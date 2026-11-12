#pragma once

#include <cstdint>

namespace arch
{
// Finds the Local APIC and I/O APIC in the MADT, maps their registers, enables
// this CPU's Local APIC and masks every 8259 line. Call after acpi::init and
// before routing any IRQ. Afterwards interrupts must be acknowledged with
// lapic_eoi(), not the PIC's send_eoi().
auto init_apic() -> void;

// Tells the Local APIC the current interrupt is handled. Every interrupt that
// came through the APIC needs exactly one, except spurious ones.
auto lapic_eoi() -> void;

// Sends ISA IRQ `irq` (0 = PIT, 1 = keyboard, ...) to `vector` on this CPU,
// applying the MADT's interrupt source overrides.
auto route_irq(uint8_t irq, uint8_t vector) -> void;

// Measures the Local APIC timer against the PIT and starts it firing
// `hz` times a second on vector 32, where the PIT's IRQ 0 used to arrive.
auto init_apic_timer(uint32_t hz) -> void;
}
