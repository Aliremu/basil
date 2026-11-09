#include "mm/paging.h"
#include "arch/x86_64/cpu.h"
#include "arch/x86_64/page_table.h"
#include "core/panic.h"
#include "mm/address.h"
#include "mm/frames.h"
#include <cassert>
#include <utility>

// x86-64 translates a virtual address by walking four tables, each one 4 KiB
// page holding 512 eight-byte entries:
//
//   63      48 47     39 38     30 29     21 20     12 11          0
//   [ sign   ] [ PML4  ] [ PDPT  ] [  PD   ] [  PT   ] [  offset   ]
//               level 3   level 2   level 1   level 0
//
// Each 9-bit field indexes one table (arch::table_index(virt, level)). The
// entry found there holds the *physical* address of the next table down; the
// level 0 entry holds the physical address of the 4 KiB frame itself, and the
// low 12 bits of the virtual address are the offset into that frame.
//
// Entries hold physical addresses, but the CPU only lets you dereference
// virtual ones, so every table is reached through the HHDM: table_at() adds
// Limine's offset. That's why none of this needs your own mapping of the
// tables to exist first.
//
// Permissions are ANDed down the walk: a page is only writable if every entry
// on the way says Writable, and only user-accessible if every entry says User.
// So intermediate entries are made permissive (Present | Writable, plus User
// if it might ever hold user pages) and the leaf entry carries the real flags.
//
// An entry with PageFlags::Huge set at level 2 or 1 is itself a leaf: it maps
// a 1 GiB or 2 MiB page directly and there is no table below it. Limine's HHDM
// uses these, so every walk has to check for them.

namespace mm
{
namespace
{
[[nodiscard]] auto table_at(PhysicalAddress phys) -> arch::PageTable*
{
    return phys.to_virtual().as<arch::PageTable>();
}

// Follows `entry` to the table it points at, one level down.
//
// - Present: return table_at(entry.address()).
// - Not present and !create: return nullptr (translate/unmap stop here).
// - Not present and create (map): grab a frame with alloc_frame(), zero all
//   4096 bytes through table_at() (a fresh table full of garbage would look
//   like 512 random mappings), then entry.set(frame, Present | Writable) and
//   return the new table.
// - Huge: there is no table below; the caller must handle this before calling.
[[nodiscard]] auto next_table(arch::PageTableEntry& entry, bool create) -> arch::PageTable*
{
    if (entry.huge())
        return nullptr;

    if (entry.present())
        return table_at(entry.address());

    if (!create)
        return nullptr;

    const auto frame = alloc_frame();
    if (!frame)
        return nullptr;

    auto* table = table_at(*frame);
    table->entries.fill({});
    entry.set(frame->value, arch::PageFlags::Present | arch::PageFlags::Writable);
    return table;
}

// Walk levels 3..1 without allocating. Returns the leaf entry for virt (level 0,
// or a Huge entry at level 2/1) and writes its level to `level`, or nullptr if a
// table on the way isn't present.
[[nodiscard]] auto find_entry(arch::PageTable* pml4, VirtualAddress virt, int& level) -> arch::PageTableEntry*
{
    auto* table = pml4;

    for (auto l = 3; l >= 0; --l)
    {
        auto& entry = table->entries[arch::table_index(virt, l)];
        if (!entry.present())
            return nullptr;

        if (l == 0 || entry.huge())
        {
            level = l;
            return &entry;
        }

        table = table_at(entry.address());
    }

    std::unreachable();
}
}

// CR3 holds the physical address of the active PML4 (its low 12 bits are
// flags/PCID, not address). Read it, mask or shift out the address, and return
// it through table_at(). Your Cr3 bitfield stores the frame *number*, so the
// address is that field << 12.
auto current_pml4() -> arch::PageTable*
{
    return table_at(PhysicalAddress{arch::read_cr3() & arch::PageTableEntry::address_mask});
}

// Walk levels 3, 2, 1 with next_table(..., true) to reach the level 0 table,
// then write the leaf:
//
//   1. For level = 3 down to 1: index the current table with
//      table_index(virt.value, level). If that entry is Huge, the region is
//      already covered by a large page; fail rather than splitting it.
//      Otherwise descend with next_table(entry, true); nullptr means out of
//      memory, so fail.
//   2. At level 0, if the entry is already present, fail: silently replacing
//      a mapping hides bugs.
//   3. entry.set(phys.value, flags | PageFlags::Present).
//
// No invlpg is needed here: the TLB never caches "not present", so a new
// mapping is picked up on first access. Changing an existing one would need it.
//
// virt and phys must both be 4 KiB aligned.
auto map(arch::PageTable* pml4, VirtualAddress virt, PhysicalAddress phys, arch::PageFlags flags) -> bool
{
    static_cast<void>(pml4);
    static_cast<void>(virt);
    static_cast<void>(phys);
    static_cast<void>(flags);
    return false;
}

// Same walk as map() but with next_table(..., false): any missing table means
// nothing is mapped, so return std::nullopt. At level 0:
//
//   1. Remember entry.address(); that's the frame you return so the caller can
//      decide whether to free_frame() it (unmap doesn't know who owns it).
//   2. Clear the entry (value = 0).
//   3. arch::invlpg(virt.value). The CPU may still have the old translation
//      cached in the TLB, and without this it keeps using it.
//
// Freeing now-empty intermediate tables is optional; skipping it just leaks a
// few frames until you care.
auto unmap(arch::PageTable* pml4, VirtualAddress virt) -> std::optional<PhysicalAddress>
{
    static_cast<void>(pml4);
    static_cast<void>(virt);
    return std::nullopt;
}

// Do in software what the MMU does in hardware. Start at level 3 and loop:
//
//   1. entry = table[table_index(virt.value, level)].
//   2. Not present: return std::nullopt.
//   3. Level 0, or Huge at level 2/1: this entry is the leaf. The page is
//      page_size(level) bytes, so offset_mask = page_size(level) - 1 and the
//      result is (entry.address() & ~offset_mask) + (virt.value & offset_mask).
//      The `& ~offset_mask` matters for huge pages, where bit 12 is the PAT
//      bit rather than part of the address.
//   4. Otherwise table = table_at(entry.address()) and go down a level.
//
// This only reads, so it takes a const table and never allocates. A good first
// test: translate(current_pml4(), {reinterpret_cast<uintptr_t>(&some_global)})
// should match where Limine loaded the kernel, and translating an HHDM address
// should give back address - hhdm_offset().
auto translate(const arch::PageTable* pml4, VirtualAddress virt) -> std::optional<PhysicalAddress>
{
    static_cast<void>(pml4);
    static_cast<void>(virt);
    return std::nullopt;
}
}
