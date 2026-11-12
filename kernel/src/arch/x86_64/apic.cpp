#include "arch/x86_64/apic.h"
#include "acpi/acpi.h"
#include "arch/x86_64/cpu.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/page_table.h"
#include "arch/x86_64/pic.h"
#include "core/panic.h"
#include "core/test.h"
#include "libk/acpi.h"
#include "mm/paging.h"
#include <array>
#include <cstdint>

// Why replace the 8259 PIC at all? It only knows 15 IRQ lines, it can only
// interrupt one CPU, and PCI devices beyond the legacy ones can't reach it.
// The APIC splits the job in two:
//
//   devices ──▶ I/O APIC ──(message: "vector V for CPU N")──▶ Local APIC ──▶ CPU
//               one or more,                                    one per CPU,
//               routes pins to vectors                          also has a timer
//
// The I/O APIC has a "redirection table": one 64-bit entry per input pin that
// says which vector to raise, on which CPU, and how the pin signals. The Local
// APIC receives that, interrupts its CPU, and waits for an EOI before it
// delivers anything of equal or lower priority again.
//
// Both are programmed through memory-mapped registers (MMIO), not I/O ports.
// Map them with mm::map_physical(..., Writable | CacheDisable | NoExecute) and
// access every register through a volatile uint32_t*: always 32-bit reads and
// writes, never a memcpy or a 64-bit access, or the hardware ignores you.
//
// Vector plan. The IDT only has stubs for vectors 0-47 (idt.cpp), and
// interrupt_dispatch treats 32-47 as "IRQ vector - 32". Keep that: route ISA
// IRQ n to vector 32 + n and nothing in the dispatch table moves. The
// spurious vector (below) is the one addition.
//
// References: the APIC chapter of Intel SDM vol. 3A (Local APIC), the 82093AA I/O APIC
// datasheet, and xv6's lapic.c / ioapic.c for ~150 lines that do all of this.

namespace arch
{
namespace
{
// Local APIC register offsets (the SDM's "Local APIC Register Address Map"). Each register
// sits on a 16-byte boundary but is only 32 bits wide.
constexpr auto lapic_id = 0x020u;
constexpr auto lapic_eoi_register = 0x0B0u;
constexpr auto lapic_spurious = 0x0F0u;  // bit 8 = software enable, bits 0-7 = vector
constexpr auto lapic_lvt_timer = 0x320u; // bits 0-7 vector, bit 16 masked, bit 17 periodic
constexpr auto lapic_timer_initial = 0x380u;
constexpr auto lapic_timer_current = 0x390u;
constexpr auto lapic_timer_divide = 0x3E0u; // 0b0011 = divide by 16

constexpr auto apic_base_msr = 0x1Bu; // IA32_APIC_BASE: bit 11 = global enable

// I/O APIC: only two registers are mapped. Write a register number to IOREGSEL
// and that register appears in IOWIN.
constexpr auto ioregsel = 0x00u;
constexpr auto iowin = 0x10u;
constexpr auto ioapic_version = 0x01u; // bits 16-23: number of redirection entries - 1
// Redirection entry n is registers 0x10 + 2n (low half) and 0x11 + 2n (high).
//   low:  bits 0-7 vector, 8-10 delivery mode (000 fixed), 11 destination
//         mode (0 physical), 13 polarity (1 active low), 15 trigger (1 level),
//         16 masked
//   high: bits 24-31 destination Local APIC ID

constexpr auto spurious_vector = uint8_t{0xFF};

constinit auto lapic = static_cast<volatile uint32_t*>(nullptr);
constinit auto ioapic = static_cast<volatile uint32_t*>(nullptr);
constinit auto ioapic_gsi_base = 0u;
// Copied out of the MADT in init_apic, so route_irq doesn't walk it again.
// Index: ISA IRQ. Default {gsi = irq, flags = 0}.
struct Override
{
    uint32_t gsi;
    uint16_t flags;
};
constinit auto overrides = std::array<Override, 16>{};

// `offset` is in bytes, as in the tables above; lapic points at 32-bit words.
[[nodiscard]] auto lapic_read(uint32_t offset) -> uint32_t
{
    return lapic[offset / 4];
}

auto lapic_write(uint32_t offset, uint32_t value) -> void
{
    lapic[offset / 4] = value;
}

// Two steps: select the register, then read or write the window.
[[nodiscard]] auto ioapic_read(uint32_t reg) -> uint32_t
{
    ioapic[ioregsel / 4] = reg;
    return ioapic[iowin / 4];
}

auto ioapic_write(uint32_t reg, uint32_t value) -> void
{
    ioapic[ioregsel / 4] = reg;
    ioapic[iowin / 4] = value;
}

// Masks all 16 lines on both 8259s (write 0xFF to ports 0x21 and 0xA1).
// init_pic() already remapped them to vectors 32-47, which matters: a masked
// 8259 can still raise a spurious IRQ 7 or 15, and remapped those land on
// vectors 39 and 47 instead of on top of CPU exceptions.
auto disable_pic() -> void
{
    for (auto i = 0; i < 16; ++i)
        arch::mask(i);
}
}

// 1. auto* madt = acpi::find("APIC"); panic if nullptr. Walk it with
//    libk::acpi::for_each_madt_entry over all header.length bytes:
//      - Start with the Local APIC address from Madt::local_apic_address; a
//        LocalApicAddressOverride entry replaces it.
//      - Remember the first IoApic entry's address and gsi_base. QEMU has one
//        I/O APIC; real boards may have several, and supporting that means
//        picking the one whose GSI range covers the pin. One is fine for now.
//      - For each InterruptSourceOverride with bus 0, store {gsi, flags} in
//        overrides[source]. Fill the rest with {gsi = irq, flags = 0} first.
// 2. Map both with mm::map_physical(addr, 4096, Writable | CacheDisable |
//    NoExecute) and keep the returned pointers in `lapic` and `ioapic`.
// 3. disable_pic().
// 4. Make sure the APIC is on: set bit 11 of IA32_APIC_BASE with rdmsr/wrmsr.
//    Firmware normally leaves it set, but don't rely on it.
// 5. Software-enable this Local APIC and pick the spurious vector:
//    lapic_write(lapic_spurious, 0x100 | spurious_vector).
//    A spurious interrupt is one the APIC started delivering and then
//    withdrew. It must not get an EOI. Your IDT has no entry at 0xFF yet:
//    add a stub in idt.cpp's asm that is just `iretq` and point idt[0xFF] at
//    it, or vector 0xFF will #GP the first time it happens.
// 6. Mask every redirection entry (set bit 16 in each low half; the count is
//    ((ioapic_read(ioapic_version) >> 16) & 0xFF) + 1). Firmware may have
//    left some pointing somewhere.
//
// Then log the Local APIC ID (lapic_read(lapic_id) >> 24), the I/O APIC's
// address, GSI base and entry count, and each override. On QEMU you should
// see IRQ 0 -> GSI 2 (flags 0), plus IRQs 5, 9, 10 and 11 mapped to
// themselves with flags 0xD: level-triggered, active high (PCI interrupts).
auto init_apic() -> void
{
    static_cast<void>(lapic);
    static_cast<void>(ioapic);
    static_cast<void>(ioapic_gsi_base);
    static_cast<void>(overrides);
    static_cast<void>(lapic_read);
    static_cast<void>(lapic_write);
    static_cast<void>(ioapic_read);
    static_cast<void>(ioapic_write);
    static_cast<void>(disable_pic);
}

// Writing 0 to the EOI register is the whole protocol.
//
// Once this works, interrupt_dispatch (idt.cpp) must call lapic_eoi() instead
// of send_eoi(irq) for vectors 32-47. Sending a PIC EOI for an APIC
// interrupt does nothing useful, and the Local APIC then never delivers that
// vector again: the classic "keyboard works for exactly one key" bug.
auto lapic_eoi() -> void
{
}

// 1. const auto [gsi, flags] = overrides[irq].
// 2. pin = gsi - ioapic_gsi_base.
// 3. Build the low half: vector; delivery mode fixed (0); physical
//    destination (0); polarity bit 13 set if (flags & inti_polarity_mask) ==
//    inti_active_low; trigger bit 15 set if (flags & inti_trigger_mask) ==
//    inti_level_triggered; mask bit 16 clear. "Bus default" (00) for ISA is
//    edge-triggered, active high, which is both bits clear.
// 4. High half: this CPU's Local APIC ID in bits 24-31
//    ((lapic_read(lapic_id) >> 24) << 24).
// 5. Write the high half first, then the low half: the low write unmasks the
//    entry, so the destination has to be right before it does.
//
// keyboard::init() calls arch::unmask(1) on the PIC; replace that with
// route_irq(1, 33). The PIT gets no route: init_apic_timer takes over.
auto route_irq(uint8_t irq, uint8_t vector) -> void
{
    static_cast<void>(irq);
    static_cast<void>(vector);
}

// The Local APIC timer counts down from an initial value at the bus clock
// divided by the divide register, and nothing tells you that clock's speed.
// So measure it against a clock you know, the PIT (1.193182 MHz):
//
//   1. lapic_write(lapic_timer_divide, 0b0011) for divide by 16.
//   2. Program PIT channel 2 for a one-shot 10 ms countdown, polled, so no
//      PIT interrupt is involved:
//        - Port 0x61: clear bit 1 (speaker off), set bit 0 (gate on).
//        - Port 0x43 = 0b1011'0010: channel 2, lo/hi byte, mode 0.
//        - Port 0x42: 11932 (1193182 / 100), low byte then high byte.
//        - Restart the count by pulsing the gate: bit 0 of 0x61 low, then high.
//   3. lapic_write(lapic_timer_initial, 0xFFFF'FFFF) right after the pulse.
//   4. Spin until bit 5 of port 0x61 goes high (channel 2 reached zero).
//   5. elapsed = 0xFFFF'FFFF - lapic_read(lapic_timer_current). That's how far
//      the APIC timer counted in 10 ms.
//   6. Start it for real: lapic_write(lapic_lvt_timer, 32 | (1 << 17)) for
//      vector 32, periodic; then lapic_write(lapic_timer_initial,
//      elapsed * 100 / hz).
//
// Vector 32 is where the PIT's IRQ 0 used to arrive, so the tick handler that
// init_pit() registers with set_irq_handler(0, ...) keeps working: move that
// registration here (or into a shared timer file) and stop calling
// init_pit(). Check it: `uptime` should still advance 100 per second
// at hz = 100. Compare against a stopwatch for a minute; the PIT measurement
// is good to well under 1%.
//
// QEMU under TCG (no KVM) runs the APIC timer from host time, so a slow
// emulator still gets roughly the right rate. Good to know when numbers look
// off.
auto init_apic_timer(uint32_t hz) -> void
{
    static_cast<void>(hz);
    static_cast<void>(lapic_eoi_register);
    static_cast<void>(lapic_timer_initial);
    static_cast<void>(lapic_timer_current);
}
}

KTEST(read_ioapic_version)
{
    auto version = ((arch::ioapic_read(arch::ioapic_version) >> 16) & 0xFF) + 1;
    kernel::kprintf("ioapic version: {}", version);
}
