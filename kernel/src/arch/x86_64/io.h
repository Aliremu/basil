#pragma once

#include <cstdint>
namespace arch
{
inline auto outb(uint16_t port, uint8_t value) -> void
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline auto inb(uint16_t port) -> uint8_t
{
    auto value = uint8_t{};
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
}
