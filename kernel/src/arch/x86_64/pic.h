#pragma once

#include <cstdint>

namespace arch
{
auto init_pic() -> void;
auto io_wait() -> void;
auto mask(uint8_t irq) -> void;
auto unmask(uint8_t irq) -> void;
auto send_eoi(uint8_t irq) -> void;
}
