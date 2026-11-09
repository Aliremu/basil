#pragma once

namespace serial
{
[[nodiscard]] auto init() -> bool;
auto put(char c) -> void;
}
