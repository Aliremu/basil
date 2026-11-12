#include "drivers/pci/pci.h"
#include "acpi/acpi.h"
#include "arch/x86_64/page_table.h"
#include "core/panic.h"
#include "libk/acpi.h"
#include "mm/paging.h"
#include <cstdint>
#include <inplace_vector>

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
constinit auto ecam_base = uint64_t{}; // virtual, after map_physical
constinit auto start_bus = uint8_t{};
constinit auto end_bus = uint8_t{};

// Address of the function's config space, as a pointer to 32-bit words.
// base_address in the MCFG is where bus 0 *would* be even when start_bus > 0,
// so don't subtract start_bus.
[[nodiscard]] auto config(uint8_t bus, uint8_t device, uint8_t function) -> volatile uint32_t*
{
    static_cast<void>(bus);
    static_cast<void>(device);
    static_cast<void>(function);
    return nullptr;
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
//    ecam_base = base_address + hhdm_offset() so config() can use the formula
//    directly.
auto init() -> void
{
    static_cast<void>(ecam_base);
    static_cast<void>(start_bus);
    static_cast<void>(end_bus);
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
    return {};
}

// config(bus, device, function)[offset / 4]. Bounds: offset < 4096.
auto read32(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) -> uint32_t
{
    static_cast<void>(config);
    static_cast<void>(bus);
    static_cast<void>(device);
    static_cast<void>(function);
    static_cast<void>(offset);
    return 0xFFFF'FFFF;
}

auto write32(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint32_t value) -> void
{
    static_cast<void>(bus);
    static_cast<void>(device);
    static_cast<void>(function);
    static_cast<void>(offset);
    static_cast<void>(value);
}
}
