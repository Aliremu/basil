#pragma once

#include <cstdint>

namespace arch
{
auto init_pit(uint32_t hz) -> void;
[[nodiscard]] auto uptime_ticks() -> uint64_t;
}
