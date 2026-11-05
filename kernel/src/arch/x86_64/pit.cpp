#include "pit.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/pic.h"

namespace arch
{
namespace
{
constexpr auto PIT_CHANNEL0 = 0x40;
constexpr auto PIT_COMMAND = 0x43;
constexpr auto PIT_BASE_HZ = 1'193'182u;

constinit volatile auto ticks = uint64_t{}; // written by the IRQ, read by normal code
}

auto init_pit(uint32_t hz) -> void
{
    const auto divisor = static_cast<uint16_t>(PIT_BASE_HZ / hz);
    arch::outb(PIT_COMMAND, 0x36); // channel 0, lo/hi byte, mode 3 (square wave)
    arch::outb(PIT_CHANNEL0, divisor & 0xFF);
    arch::outb(PIT_CHANNEL0, divisor >> 8);

    arch::set_irq_handler(0, [](InterruptStackFrame&) { ticks = ticks + 1; });
    arch::unmask(0);
}

auto uptime_ticks() -> uint64_t
{
    return ticks;
}
}
