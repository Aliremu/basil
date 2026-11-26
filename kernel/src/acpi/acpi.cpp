#include "acpi/acpi.h"
#include "arch/x86_64/page_table.h"
#include "core/panic.h"
#include "libk/acpi.h"
#include "mm/address.h"
#include "mm/paging.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

// ACPI is how firmware describes the machine to you. You only need the static
// tables for phase 3; the AML bytecode in the DSDT (power buttons, sleep
// states) is a separate, much bigger job you can skip for a long time.
//
//   Limine ──▶ RSDP ──▶ XSDT ──┬──▶ "APIC" (MADT): CPUs, I/O APICs, IRQ overrides
//    (phys)                    ├──▶ "MCFG": PCIe config space windows
//                              ├──▶ "FACP" (FADT): power management, the DSDT
//                              └──▶ "HPET", ...
//
// Every pointer in this chain is *physical*. Before reading anything, make it
// reachable with mm::map_physical (read-only: NoExecute) and read through the
// virtual address it returns.
//
// A trap that catches everyone once: you don't know a table's length until
// you've read its header, so map the header first (sizeof(SdtHeader)), read
// header.length, then map the full length before touching the body.

namespace acpi
{
namespace
{
// Set by init(): the root table's virtual address, and how wide its entries
// are (8 bytes for the XSDT, 4 for the RSDT).
constinit auto root = static_cast<const libk::acpi::SdtHeader*>(nullptr);
constinit auto entry_size = 0uz;

// Maps the whole table at `phys` and returns it, or nullptr when its checksum
// is wrong.
//
//   1. map_physical(phys, sizeof(SdtHeader), arch::PageFlags::NoExecute) and
//      read the header's length.
//   2. map_physical(phys, length, NoExecute) again for the full table.
//      map_physical skips pages that are already mapped, so mapping the first
//      page twice is fine.
//   3. Run libk::acpi::checksum_ok over all `length` bytes; on failure return
//      nullptr (log the signature so you can see which one was bad).
[[nodiscard]] auto map_table(mm::PhysicalAddress phys) -> const libk::acpi::SdtHeader*
{
    using libk::acpi::SdtHeader;

    const auto* header = mm::map_physical(phys, sizeof(SdtHeader), arch::PageFlags::NoExecute).view<SdtHeader>();
    if (header->length < sizeof(SdtHeader))
        return nullptr;

    const auto* table = mm::map_physical(phys, header->length, arch::PageFlags::NoExecute).view<SdtHeader>();
    if (!libk::acpi::checksum_ok(libk::acpi::table_bytes(*table)))
        return nullptr;

    return table;
}
}

// 1. Map sizeof(Rsdp) bytes at `rsdp` (NoExecute) and view them as an Rsdp.
// 2. Check the signature is "RSD PTR " and the checksum of the first 20 bytes.
//    Those 20 bytes are the ACPI 1.0 structure; the checksum covers only them.
// 3. If revision >= 2, also check extended_checksum over all 36 bytes and use
//    xsdt_address with entry_size = 8. Otherwise use rsdt_address with
//    entry_size = 4. The spec says to prefer the XSDT whenever it exists.
//    (QEMU with SeaBIOS reports revision 0, so `make run-bios` exercises the
//    RSDT path and `make run` the XSDT path. Try both.)
// 4. root = map_table(that address); panic if it's nullptr or the signature
//    isn't "XSDT"/"RSDT" as expected.
//
// A good first log line: print the RSDP revision, the OEM ID and every table
// signature in the root. On QEMU q35 you should see FACP, APIC, HPET, MCFG,
// WAET (and maybe more); if that list looks right, the rest of ACPI is easy.
auto init(mm::PhysicalAddress phys) -> void
{
    using libk::acpi::Rsdp;
    const auto* rsdp = mm::map_physical(phys, sizeof(Rsdp), arch::PageFlags::NoExecute).view<Rsdp>();
    if (std::string_view{rsdp->signature} != "RSD PTR ")
        kernel::panic("acpi: bad RSDP magic signature");

    const auto bytes = std::as_bytes(std::span{rsdp, 1});
    constexpr auto v1_size = offsetof(Rsdp, length);

    if (!libk::acpi::checksum_ok(bytes.first(v1_size)))
        kernel::panic("acpi: bad RSDP checksum");

    if (rsdp->revision >= 2 && !libk::acpi::checksum_ok(bytes))
        kernel::panic("acpi: bad RSDP extended checksum");

    const auto is_xsdt = rsdp->revision >= 2 && rsdp->xsdt_address != 0;
    root = map_table({is_xsdt ? rsdp->xsdt_address : rsdp->rsdt_address});
    entry_size = is_xsdt ? 8 : 4;

    if (root == nullptr || std::string_view{root->signature} != (is_xsdt ? "XSDT" : "RSDT"))
        kernel::panic("acpi: bad root table");
}

// Walk the root table's entries and return the first whose signature matches.
//
//   1. The entries are the root's bytes after its header:
//      table_bytes(*root).subspan(sizeof(SdtHeader)), entry_size each.
//   2. Read entry i with std::memcpy into a uint64_t (zeroed first, so the
//      RSDT's 4-byte entries come out right). It's packed, so it may be
//      misaligned: don't cast to uint64_t* and dereference.
//   3. map_table(that address); skip nullptr (bad checksum) and return the
//      first table whose signature equals `signature`.
//
// Comparing: std::string_view{header->signature.data(), 4} == signature.
//
// Mapping every table just to read its signature is wasteful but harmless;
// there are about ten of them and you call this a handful of times at boot.
auto find(std::string_view signature) -> const libk::acpi::SdtHeader*
{
    const auto entries = libk::acpi::table_bytes(*root).subspan(sizeof(libk::acpi::SdtHeader));

    for (auto offset = 0uz; offset + entry_size <= entries.size(); offset += entry_size)
    {
        auto address = uint64_t{};
        std::memcpy(&address, entries.data() + offset, entry_size);
        const auto* header = map_table({address});
        if (header == nullptr)
            continue;

        if (std::string_view{header->signature} == signature)
            return header;
    }

    return nullptr;
}
}
