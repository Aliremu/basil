#include "arch/x86_64/pit.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/pic.h"
#include <cstdint>

namespace arch
{
namespace
{
constinit volatile auto ticks = uint64_t{}; // written by the IRQ, read by normal code
}

auto init_pit(uint32_t hz) -> void
{
    const auto divisor = static_cast<uint16_t>(pit_base_hz / hz);
    pit_command.write(0x36);
    // CC AA MMM B
    // name           |  bits | description
    // select channel |  6-7  | 00 for channel 0 (irq0 system timer), 01 for channel 1 (legacy dram refresh)
    //                |       | 10 for channel 2 (pc speaker), 11 for 8254 read-back command
    // access mode    |  4-5  | 00 to latch count value, 01 for low byte only, 10 for high byte only, 11 for low byte
    //                |       | followed by high byte
    // operating mode |  1-3  | defines modes 0 - 5
    // BCD/binary     |   0   | 0 for 160bit binary, 1 for 4-digit BCD(0-9999)
    //
    // 00 11 011 0
    pit_channel0.write(divisor & 0xFF);
    pit_channel0.write(divisor >> 8);

    arch::set_irq_handler(0, [](InterruptStackFrame&) { ticks = ticks + 1; });
    arch::unmask(0);
}
auto speaker_on(uint32_t hz) -> void
{
    const auto divisor = static_cast<uint16_t>((pit_base_hz + hz / 2) / hz); // 2712 for 440 Hz
    pit_command.write(0b10110110);
    pit_channel2.write(divisor & 0xFF);
    pit_channel2.write(divisor >> 8);
    speaker_control.write(speaker_control.read() | 0b11);
}

auto speaker_off() -> void
{
    speaker_control.write(speaker_control.read() & ~0b11);
}

auto uptime_ticks() -> uint64_t
{
    return ticks;
}
}
