#pragma once

#include "arch/x86_64/io.h"
#include <cstdint>
namespace serial
{
struct Port
{
    uint16_t port;

    [[nodiscard]] auto read() const -> uint8_t
    {
        return arch::inb(port);
    }

    auto write(uint8_t value) const -> void
    {
        arch::outb(port, value);
    }
};

[[nodiscard]] auto init() -> bool;
auto put(char c) -> void;
}
