#pragma once

#include <cstdint>

namespace arch
{
auto init_pit(uint32_t hz) -> void;
auto uptime_ticks() -> uint64_t;
}
