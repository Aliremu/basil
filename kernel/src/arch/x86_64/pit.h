#pragma once

#include "drivers/serial/serial.h"
#include <cstdint>

namespace arch
{
inline constexpr auto pit_base_hz = 1'193'182u; // the PIT's input clock

inline constexpr auto pit_channel0 = serial::Port{0x40};
inline constexpr auto pit_channel1 = serial::Port{0x41};
inline constexpr auto pit_channel2 = serial::Port{0x42};
inline constexpr auto pit_command = serial::Port{0x43};
// System control port B: bit 0 gates PIT channel 2, bit 1 connects it to the speaker.
inline constexpr auto speaker_control = serial::Port{0x61};

auto init_pit(uint32_t hz) -> void;
auto speaker_on(uint32_t hz) -> void;
auto speaker_off() -> void;
[[nodiscard]] auto uptime_ticks() -> uint64_t;
}
