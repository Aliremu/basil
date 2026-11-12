#pragma once

#include <cstddef>

namespace mm
{
[[nodiscard]] auto kmalloc(std::size_t size, std::size_t align) -> void*;
auto kfree(void* ptr) -> void;
}
