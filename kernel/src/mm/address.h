#pragma once

#include <cstddef>
#include <cstdint>

namespace kernel
{
struct PhysicalAddress
{
    std::uintptr_t Value;

    template <typename T>
    [[nodiscard]] auto as() const noexcept -> T*
    {
        return reinterpret_cast<T*>(Value);
    }

    [[nodiscard]] constexpr auto operator+(std::size_t offset) const noexcept -> PhysicalAddress
    {
        return {Value + offset};
    }
};

struct VirtualAddress
{
    std::uintptr_t Value;

    template <typename T>
    [[nodiscard]] auto as() const noexcept -> T*
    {
        return reinterpret_cast<T*>(Value);
    }

    [[nodiscard]] constexpr auto operator+(std::size_t offset) const noexcept -> VirtualAddress
    {
        return {Value + offset};
    }
};
}
