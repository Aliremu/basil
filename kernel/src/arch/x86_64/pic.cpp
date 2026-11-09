#include "arch/x86_64/pic.h"
#include "arch/x86_64/io.h"
#include <cstdint>

namespace arch
{
namespace
{
constexpr auto pic1_command = 0x20;
constexpr auto pic1_data = 0x21;
constexpr auto pic2_command = 0xA0;
constexpr auto pic2_data = 0xA1;

constexpr auto icw1_init = 0x10;
constexpr auto icw1_icw4 = 0x01;

constexpr auto icw4_8086 = 0x01;

constexpr auto pic_eoi = 0x20;

[[nodiscard]] constexpr auto data_port(uint8_t irq) -> uint16_t
{
    return irq < 8 ? pic1_data : pic2_data;
}

[[nodiscard]] constexpr auto mask_bit(uint8_t irq) -> uint8_t
{
    return static_cast<uint8_t>(1u << (irq % 8));
}
}

auto init_pic() -> void
{
    const auto offset1 = 0x20;
    const auto offset2 = 0x28;

    __asm__ volatile("cli");

    arch::outb(pic1_command, icw1_init | icw1_icw4);
    arch::outb(pic2_command, icw1_init | icw1_icw4);
    io_wait();

    arch::outb(pic1_data, offset1);
    arch::outb(pic2_data, offset2);
    io_wait();

    arch::outb(pic1_data, 0x04);
    arch::outb(pic2_data, 0x02);
    io_wait();

    arch::outb(pic1_data, icw4_8086);
    arch::outb(pic2_data, icw4_8086);
    io_wait();

    arch::outb(pic1_data, 0xFF);
    arch::outb(pic2_data, 0xFF);
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
        arch::outb(pic2_command, pic_eoi);

    arch::outb(pic1_command, pic_eoi);
}
}
