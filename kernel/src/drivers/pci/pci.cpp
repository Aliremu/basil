#include "drivers/pci/pci.h"
#include "acpi/acpi.h"
#include "arch/x86_64/page_table.h"
#include "core/panic.h"
#include "libk/acpi.h"
#include "mm/address.h"
#include "mm/paging.h"
#include <cstdint>
#include <inplace_vector>
#include <ranges>

// PCI is how the machine tells you what devices exist and where their
// registers are. Every function has a 256-byte (PCIe: 4 KiB) configuration
// space with a standard header:
//
//   offset  0x00  vendor ID (16)   device ID (16)       0xFFFF vendor = nothing here
//           0x04  command (16)     status (16)          command bit 1 memory, bit 2 bus master
//           0x08  revision (8) prog IF (8) subclass (8) class (8)
//           0x0C  cache line, latency, header type (8), BIST
//                   header type bit 7 = multi-function; bits 0-6: 0 device, 1 PCI bridge
//           0x10  BAR0 .. 0x24 BAR5                     where the device's registers are
//           0x2C  subsystem vendor/device
//           0x34  capabilities pointer                  MSI, MSI-X, virtio config ...
//           0x3C  interrupt line, interrupt pin
//
// Read it with 32-bit accesses and pull fields out with shifts: vendor is
// read32(..., 0x00) & 0xFFFF, class is read32(..., 0x08) >> 24, and so on.
//
// Two ways in:
//   - Legacy: write an address to I/O port 0xCF8, read data from 0xCFC. Only
//     the first 256 bytes, and it works everywhere, including QEMU's old
//     `-M pc`.
//   - ECAM (PCIe, what you'll do here): all of config space is plain memory.
//     The MCFG table says where, and function (bus, dev, fn) is the 4 KiB at
//     base + (bus << 20 | dev << 15 | fn << 12). Your Makefile boots `-M
//     q35`, which has ECAM at 0xB000'0000.
//
// On q35 with your current flags, expect something like:
//   00:00.0 8086:29c0 class 06/00 host bridge
//   00:01.0 1234:1111 class 03/00 VGA (QEMU's std VGA)
//   00:1f.0 8086:2918 class 06/01 ISA bridge (LPC)
//   00:1f.2 8086:2922 class 01/06 SATA (AHCI)
//   00:1f.3 8086:2930 class 0c/05 SMBus
// plus a network card. Phase 6 adds `-device virtio-blk-pci`, vendor 1af4.

namespace pci
{
namespace
{
// The MCFG window you're using. QEMU has exactly one, segment 0; supporting
// several segments can wait for real hardware.
constinit auto ecam_base = mm::VirtualAddress{}; // where bus 0 would be mapped
constinit auto start_bus = uint8_t{};
constinit auto end_bus = uint8_t{};

// Address of the function's config space, as a pointer to 32-bit words.
// base_address in the MCFG is where bus 0 *would* be even when start_bus > 0,
// so don't subtract start_bus.
[[nodiscard]] auto config(uint8_t bus, uint8_t device, uint8_t function) -> volatile uint32_t*
{
    // Outside the mapped buses is an unmapped page (a page fault), and a
    // device or function number too big for its bit field lands in a
    // neighbour's config space.
    if (bus < start_bus || bus > end_bus || device >= 32 || function >= 8)
        kernel::panic("pci: no such function {:02x}:{:02x}.{}", unsigned{bus}, unsigned{device}, unsigned{function});

    return (ecam_base + (bus << 20) + (device << 15) + (function << 12)).as<volatile uint32_t>();
}

// The 32-bit register at `offset` in the function's config space. Offsets
// must be dword-aligned: offset / 4 would otherwise round down silently and
// touch a different register than the caller asked for.
[[nodiscard]] auto register_at(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) -> volatile uint32_t*
{
    if (offset >= 4096 || offset % 4 != 0)
        kernel::panic("pci: bad config offset {:#x}", offset);

    return config(bus, device, function) + offset / 4;
}
}

// 1. auto* mcfg = acpi::find("MCFG"); panic (or log and return) if nullptr.
// 2. The allocations start at sizeof(libk::acpi::Mcfg); there are
//    (length - 44) / 16 of them. Take the first with segment_group 0 and
//    std::memcpy it out (packed, possibly misaligned).
// 3. The window covers (end_bus - start_bus + 1) MiB: 256 functions per bus
//    times 4 KiB. That's 256 MiB for buses 0-255, which is 65536 page table
//    entries if you map it all with 4 KiB pages. Fine for now (about 512 KiB
//    of page tables); later you can map only the buses that exist, or teach
//    map() 2 MiB pages.
//    map_physical(base_address + (start_bus << 20), size,
//                 Writable | CacheDisable | NoExecute), and keep
//    ecam_base = mm::PhysicalAddress{base_address}.to_virtual() so config()
//    can use the formula directly.
auto init() -> void
{
    using namespace libk::acpi;
    const auto* mcfg = acpi::find<Mcfg>();
    if (mcfg == nullptr)
        kernel::panic("pci: couldn't find MCFG");

    const auto allocation_count = (mcfg->header.length - sizeof(Mcfg)) / sizeof(McfgAllocation);
    const auto* allocations = std::start_lifetime_as_array<McfgAllocation>(mcfg + 1, allocation_count);

    for (const auto& alloc : std::span{allocations, allocation_count})
    {
        if (alloc.segment_group != 0)
            continue;

        const auto base = mm::PhysicalAddress{alloc.base_address};
        const auto size = std::size_t{alloc.end_bus - alloc.start_bus + 1u} << 20;
        using enum arch::PageFlags;
        std::ignore = mm::map_physical(base + (alloc.start_bus << 20u), size, Writable | CacheDisable | NoExecute);
        ecam_base = base.to_virtual();
        start_bus = alloc.start_bus;
        end_bus = alloc.end_bus;
        return;
    }

    kernel::panic("pci: no segment 0 in MCFG");
}

// Brute force is fine and simplest: for every bus in [start_bus, end_bus],
// device 0-31, function 0-7:
//
//   1. Vendor ID 0xFFFF means nothing is there; skip. If it's function 0,
//      skip the whole device (functions 1-7 can't exist without 0).
//   2. Otherwise record a Function from offsets 0x00 and 0x08.
//   3. If function 0's header type (offset 0x0C, bits 16-23) doesn't have bit
//      7 set, it's single-function: don't probe 1-7. Some devices answer on
//      every function number when they aren't multi-function, so you'd list
//      them eight times.
//
// The smarter way, following PCI-to-PCI bridges from bus 0 (header type 1,
// secondary bus number at 0x18 bits 8-15), visits only buses that exist. Do
// it later if boot time ever matters; on QEMU brute force takes a moment.
//
// Then add `lspci` to the shell: one line per function, formatted as
// "{:02x}:{:02x}.{} {:04x}:{:04x} class {:02x}/{:02x}". The vendor and
// class names come from https://pci-ids.ucw.cz; a small table of the classes
// above is plenty.
auto enumerate() -> std::inplace_vector<Function, 64>
{
    const auto buses = std::views::iota(unsigned{start_bus}, unsigned{end_bus} + 1u);
    const auto devices = std::views::iota(0, 32);

    auto res = std::inplace_vector<Function, 64>{};
    const auto enumerate_device = [&res](uint8_t bus, uint8_t device)
    {
        const auto id = read32(bus, device, 0, 0x00);
        // const auto cls = read32(bus, device, 0, 0x08);
        const auto vendor_id = id & 0xFFFF;
        if (vendor_id == 0xFFFF)
            return;

        const auto functions = std::views::iota(0u, 8u);
        for (auto&& function : functions)
        {
            const auto id = read32(bus, device, function, 0x00);
            const auto cls = read32(bus, device, function, 0x08);
            const auto vendor_id = static_cast<uint16_t>(id & 0xFFFF);
            if (vendor_id == 0xFFFF)
                continue;

            const auto device_id = static_cast<uint16_t>(id >> 16u);
            const auto class_code = static_cast<uint8_t>(cls >> 24u);
            const auto subclass = static_cast<uint8_t>(cls >> 16u & 0xFF);
            const auto prog_if = static_cast<uint8_t>(cls >> 8u & 0xFF);

            res.try_emplace_back(
                Function{
                    .bus = bus,
                    .device = device,
                    .function = static_cast<uint8_t>(function),
                    .vendor_id = vendor_id,
                    .device_id = device_id,
                    .class_code = class_code,
                    .subclass = subclass,
                    .prog_if = prog_if,
                });

            if (function == 0 && (read32(bus, device, 0, 0x0C) >> 16 & 0x80) == 0)
                break;
        }
    };

    for (auto&& [bus, device] : std::views::cartesian_product(buses, devices))
        enumerate_device(bus, device);

    return res;
}

auto read32(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) -> uint32_t
{
    return *register_at(bus, device, function, offset);
}

auto write32(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint32_t value) -> void
{
    *register_at(bus, device, function, offset) = value;
}
}
