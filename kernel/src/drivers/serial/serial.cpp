#include "drivers/serial/serial.h"
#include "arch/x86_64/io.h"
#include <cstdint>

namespace serial
{
namespace
{
constexpr auto com1 = uint16_t{0x3F8};

// Register offsets from the port's base address.
constexpr auto data = uint16_t{0};             // byte in/out; divisor low byte while DLAB is set
constexpr auto interrupt_enable = uint16_t{1}; // divisor high byte while DLAB is set
constexpr auto fifo_control = uint16_t{2};
constexpr auto line_control = uint16_t{3};
constexpr auto modem_control = uint16_t{4};
constexpr auto line_status = uint16_t{5};

constexpr auto dlab = uint8_t{0x80};           // line_control: next two writes set the divisor
constexpr auto line_8n1 = uint8_t{0x03};       // 8 data bits, no parity, 1 stop bit
constexpr auto transmit_empty = uint8_t{0x20}; // line_status: ready for another byte

auto write_byte(char c) -> void
{
    while ((arch::inb(com1 + line_status) & transmit_empty) == 0)
    {
    }
    arch::outb(com1 + data, static_cast<uint8_t>(c));
}
}

auto init() -> bool
{
    arch::outb(com1 + interrupt_enable, 0x00); // we poll; no serial interrupts
    arch::outb(com1 + line_control, dlab);
    arch::outb(com1 + data, 3); // divisor 3: 115200 / 3 = 38400 baud
    arch::outb(com1 + interrupt_enable, 0);
    arch::outb(com1 + line_control, line_8n1);
    arch::outb(com1 + fifo_control, 0xC7); // enable and clear FIFOs, 14-byte threshold

    // Loopback self-test: anything sent comes straight back.
    arch::outb(com1 + modem_control, 0x1E);
    arch::outb(com1 + data, 0xAE);
    if (arch::inb(com1 + data) != 0xAE)
        return false;

    arch::outb(com1 + modem_control, 0x0F); // normal operation
    for (const auto c : "\x1b[0m\x1b[2J\x1b[H")
    {
        if (c)
            put(c);
    }

    return true;
}

auto put(char c) -> void
{
    if (c == '\n')
        write_byte('\r'); // terminals expect CR LF
    write_byte(c);
}
}
