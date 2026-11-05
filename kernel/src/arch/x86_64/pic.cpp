#include "pic.h"
#include "arch/x86_64/io.h"

namespace arch
{
namespace
{
constexpr auto PIC1_COMMAND = 0x20;
constexpr auto PIC1_DATA = 0x21;
constexpr auto PIC2_COMMAND = 0xA0;
constexpr auto PIC2_DATA = 0xA1;

constexpr auto ICW1_INIT = 0x10;
constexpr auto ICW1_ICW4 = 0x01;

constexpr auto ICW4_8086 = 0x01;

constexpr auto PIC_EOI = 0x20;

auto data_port(uint8_t irq) -> uint16_t
{
    return irq < 8 ? PIC1_DATA : PIC2_DATA;
}

auto mask_bit(uint8_t irq) -> uint8_t
{
    return static_cast<uint8_t>(1u << (irq % 8));
}
}

auto init_pic() -> void
{
    const auto offset1 = 0x20;
    const auto offset2 = 0x28;
    __asm__ volatile("cli");

    arch::outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    arch::outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    arch::outb(PIC1_DATA, offset1);
    arch::outb(PIC2_DATA, offset2);
    io_wait();

    arch::outb(PIC1_DATA, 0x04);
    arch::outb(PIC2_DATA, 0x02);
    io_wait();

    arch::outb(PIC1_DATA, ICW4_8086);
    arch::outb(PIC2_DATA, ICW4_8086);
    io_wait();

    arch::outb(PIC1_DATA, 0xFF);
    arch::outb(PIC2_DATA, 0xFF);
    io_wait();
}

auto io_wait() -> void
{
    arch::outb(0x80, 0);
}

auto mask(uint8_t irq) -> void
{
    const auto port = data_port(irq);
    arch::outb(port, arch::inb(port) | mask_bit(irq));
}

auto unmask(uint8_t irq) -> void
{
    const auto port = data_port(irq);
    arch::outb(port, arch::inb(port) & static_cast<uint8_t>(~mask_bit(irq)));
}
auto send_eoi(uint8_t irq) -> void
{
    if (irq >= 8)
        arch::outb(PIC2_COMMAND, PIC_EOI);

    arch::outb(PIC1_COMMAND, PIC_EOI);
}
}
