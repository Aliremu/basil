#pragma once

#include "core/panic.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ranges>
#include <span>

// ACPI table layouts and the parsing that needs no hardware, so host tests can
// run it on byte arrays. The kernel side (finding tables in physical memory)
// lives in kernel/src/acpi/.
//
// Firmware writes these tables byte for byte as the spec lays them out, with
// no padding, so every struct is [[gnu::packed]] and checked with a
// static_assert against the spec's size. Field names follow ACPI 6.5 chapter
// 5.2 so you can read the two side by side.
//
// Packed members can be misaligned (an XSDT pointer can sit at an address
// that's 4 mod 8). Reading a member through the struct is fine: the compiler
// knows it's packed and emits safe loads. Taking a pointer or reference to a
// packed member is not, which is why GCC warns about it.

namespace libk::acpi
{
// Root System Description Pointer: the only structure you find directly
// (Limine hands you its address). Everything else hangs off it.
//
// Revision 0 (ACPI 1.0) stops after rsdt_address: 20 bytes, covered by
// `checksum`. Revision 2+ adds the rest: `length` (36), the 64-bit
// xsdt_address, and `extended_checksum` over all 36 bytes.
struct [[gnu::packed]] Rsdp
{
    std::array<char, 8> signature; // "RSD PTR " (trailing space)
    uint8_t checksum;
    std::array<char, 6> oem_id;
    uint8_t revision;
    uint32_t rsdt_address;
    // Revision 2+ only:
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    std::array<uint8_t, 3> reserved;
};
static_assert(sizeof(Rsdp) == 36);

// Every table other than the RSDP starts with this header. `length` covers
// the header and the body, and all `length` bytes sum to zero.
struct [[gnu::packed]] SdtHeader
{
    std::array<char, 4> signature; // "XSDT", "APIC" (the MADT), "MCFG", "FACP", "HPET", ...
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    std::array<char, 6> oem_id;
    std::array<char, 8> oem_table_id;
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};
static_assert(sizeof(SdtHeader) == 36);

// The XSDT body is (length - 36) / 8 physical addresses of other tables; the
// RSDT's is (length - 36) / 4 32-bit ones. Both are packed after the header,
// so read them with std::memcpy (or a packed struct), not through uint64_t*.

// MADT, signature "APIC": the interrupt controllers in the machine.
struct [[gnu::packed]] Madt
{
    SdtHeader header;
    uint32_t local_apic_address; // physical; a type 5 entry overrides it with 64 bits
    uint32_t flags;              // bit 0, PCAT_COMPAT: 8259 PICs are present too
    // Variable-length entries follow, up to header.length.
};
static_assert(sizeof(Madt) == 44);

enum class MadtType : uint8_t
{
    LocalApic = 0,
    IoApic = 1,
    InterruptSourceOverride = 2,
    NmiSource = 3,
    LocalApicNmi = 4,
    LocalApicAddressOverride = 5,
    LocalX2Apic = 9,
};

// Every MADT entry starts with these two bytes. `length` includes them, so the
// next entry is always `length` bytes on, even for types you don't know.
struct [[gnu::packed]] MadtEntryHeader
{
    MadtType type;
    uint8_t length;
};

// One per CPU. Phase 4's "bring up the other cores" walks these.
struct [[gnu::packed]] MadtLocalApic
{
    MadtEntryHeader header;
    uint8_t processor_uid;
    uint8_t apic_id;
    uint32_t flags; // bit 0 enabled, bit 1 online capable
};
static_assert(sizeof(MadtLocalApic) == 8);

// One per I/O APIC. It handles GSIs gsi_base .. gsi_base + (redirection
// entries - 1); the count comes from its version register, not from here.
struct [[gnu::packed]] MadtIoApic
{
    MadtEntryHeader header;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t address; // physical, MMIO
    uint32_t gsi_base;
};
static_assert(sizeof(MadtIoApic) == 12);

// "ISA IRQ `source` arrives on GSI `gsi`, not GSI `source`." Without one of
// these, ISA IRQ n is GSI n. On QEMU the PIT's IRQ 0 is overridden to GSI 2.
struct [[gnu::packed]] MadtInterruptSourceOverride
{
    MadtEntryHeader header;
    uint8_t bus;    // always 0, ISA
    uint8_t source; // ISA IRQ
    uint32_t gsi;
    uint16_t flags; // MPS INTI flags, see below
};
static_assert(sizeof(MadtInterruptSourceOverride) == 10);

// MPS INTI flags, used by type 2, 3 and 4 entries.
// Bits 0-1, polarity: 00 bus default (ISA: active high), 01 high, 11 low.
// Bits 2-3, trigger:  00 bus default (ISA: edge), 01 edge, 11 level.
inline constexpr auto inti_polarity_mask = uint16_t{0b0011};
inline constexpr auto inti_active_low = uint16_t{0b0011};
inline constexpr auto inti_trigger_mask = uint16_t{0b1100};
inline constexpr auto inti_level_triggered = uint16_t{0b1100};

struct [[gnu::packed]] MadtLocalApicAddressOverride
{
    MadtEntryHeader header;
    uint16_t reserved;
    uint64_t address;
};
static_assert(sizeof(MadtLocalApicAddressOverride) == 12);

// MCFG: where PCIe configuration space is memory-mapped (ECAM).
struct [[gnu::packed]] Mcfg
{
    SdtHeader header;
    uint64_t reserved;
    // (header.length - 44) / 16 McfgAllocation entries follow.
};
static_assert(sizeof(Mcfg) == 44);

// One ECAM window. base_address is where *bus 0* would start, even when
// start_bus isn't 0: function (bus, dev, fn) is at
// base_address + (bus << 20 | dev << 15 | fn << 12).
struct [[gnu::packed]] McfgAllocation
{
    uint64_t base_address;
    uint16_t segment_group;
    uint8_t start_bus;
    uint8_t end_bus;
    uint32_t reserved;
};
static_assert(sizeof(McfgAllocation) == 16);

// True when the bytes sum to 0 mod 256. That's every ACPI checksum: the
// firmware picks the checksum byte so the total wraps to zero, which means you
// don't skip the checksum field, you just add everything. Use it for the RSDP
// (first 20 bytes, then all 36 for revision 2+) and every table (all `length`
// bytes).
//
// Accumulate into a uint8_t so it wraps for you, then compare with 0.
[[nodiscard]] constexpr auto checksum_ok(std::span<const std::byte> bytes) -> bool
{
    return std::ranges::fold_left(bytes | std::views::transform(std::to_integer<unsigned>), 0u, std::plus{}) % 256 == 0;
}

// Calls fn(const MadtEntryHeader&) for each entry of a MADT, where `table` is
// all header.length bytes of it.
//
//   1. Start at offset sizeof(Madt) (44); the entries begin right after the
//      fixed fields.
//   2. While offset + sizeof(MadtEntryHeader) <= table.size(): read the
//      header there. If its length is < 2 or runs past the end, stop: the
//      table is corrupt, and a zero length would loop forever.
//   3. Call fn with it, then offset += length.
//
// The callback switches on header.type and reinterprets the header as the
// matching struct (MadtIoApic and so on). Skip types you don't handle; their
// length still gets you to the next entry.
//
// Host test idea: build a byte buffer with a Madt, an I/O APIC entry and two
// overrides, and check the callback sees exactly those three in order; then
// one with a zero-length entry, which must stop instead of hanging.
template <typename F>
auto for_each_madt_entry(std::span<const std::byte> table, F&& fn) -> void
{
    for (auto offset = sizeof(Madt); offset + sizeof(MadtEntryHeader) <= table.size();)
    {
        auto& header = *std::start_lifetime_as<MadtEntryHeader>(table.subspan(offset, sizeof(MadtEntryHeader)).data());
        if (header.length < 2 || offset + header.length >= table.size())
            kernel::panic("table is corrupt");

        fn(header);
        offset += header.length;
    }
}
}
