#include "drivers/serial/serial.h"
#include "arch/x86_64/io.h"

namespace serial
{
namespace
{
constexpr std::uint16_t com1 = 0x3f8;

// Register offsets from the port's base address.
constexpr std::uint16_t data = 0;             // byte in/out; divisor low byte while DLAB is set
constexpr std::uint16_t interrupt_enable = 1; // divisor high byte while DLAB is set
constexpr std::uint16_t fifo_control = 2;
constexpr std::uint16_t line_control = 3;
constexpr std::uint16_t modem_control = 4;
constexpr std::uint16_t line_status = 5;

constexpr std::uint8_t dlab = 0x80;           // line_control: next two writes set the divisor
constexpr std::uint8_t line_8n1 = 0x03;       // 8 data bits, no parity, 1 stop bit
constexpr std::uint8_t transmit_empty = 0x20; // line_status: ready for another byte
}

auto init() -> bool
{
    arch::outb(com1 + interrupt_enable, 0x00); // we poll; no serial interrupts
    arch::outb(com1 + line_control, dlab);
    arch::outb(com1 + data, 3); // divisor 3: 115200 / 3 = 38400 baud
    arch::outb(com1 + interrupt_enable, 0);
    arch::outb(com1 + line_control, line_8n1);
    arch::outb(com1 + fifo_control, 0xc7); // enable and clear FIFOs, 14-byte threshold

    // Loopback self-test: anything sent comes straight back.
    arch::outb(com1 + modem_control, 0x1e);
    arch::outb(com1 + data, 0xae);
    if (arch::inb(com1 + data) != 0xae)
        return false;

    arch::outb(com1 + modem_control, 0x0f); // normal operation
    for (char c : "\x1b[0m\x1b[2J\x1b[H")
        if (c)
            put(c);

    return true;
}

auto put(char c) -> void
{
    while ((arch::inb(com1 + line_status) & transmit_empty) == 0)
    {
    }
    arch::outb(com1 + data, static_cast<uint8_t>(c));
}
}
