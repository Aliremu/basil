#pragma once

#include "boot/limine.h"
#include <cstddef>
#include <cstdint>

namespace mm
{

[[nodiscard]] inline auto phys_to_virt(uintptr_t p) -> void*
{
    return reinterpret_cast<void*>(p + boot::hhdm_offset());
}

struct VirtualAddress
{
    uintptr_t value;

    template <typename T>
    [[nodiscard]] auto as() const noexcept -> T*
    {
        return reinterpret_cast<T*>(value);
    }

    [[nodiscard]] constexpr auto operator+(std::size_t offset) const noexcept -> VirtualAddress
    {
        return {value + offset};
    }

    [[nodiscard]] constexpr operator uintptr_t() const noexcept
    {
        return value;
    }

    [[nodiscard]] constexpr operator void*() const noexcept
    {
        return reinterpret_cast<void*>(value);
    }
};

struct PhysicalAddress
{
    uintptr_t value;

    template <typename T>
    [[nodiscard]] auto as() const noexcept -> T*
    {
        return reinterpret_cast<T*>(value);
    }

    [[nodiscard]] auto to_virtual() -> VirtualAddress
    {
        return {reinterpret_cast<uintptr_t>(phys_to_virt(value))};
    }

    [[nodiscard]] constexpr auto operator+(std::size_t offset) const noexcept -> PhysicalAddress
    {
        return {value + offset};
    }
};

}
