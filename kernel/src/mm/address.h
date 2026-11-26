#pragma once

#include "boot/limine.h"
#include <cstddef>
#include <cstdint>
#include <memory>

// Typed addresses. Physical and virtual addresses are both plain integers to
// the CPU, but mixing them up is the classic paging bug, so each gets its own
// type and the only way across is PhysicalAddress::to_virtual() (the HHDM).
// Neither converts implicitly to an integer or a pointer; use .value, as<T>()
// or view<T>().
//
// Getting a pointer out of a VirtualAddress:
//   - as<T>()   when a T object already lives there because C++ created it
//               (a global, a new'd object, a page table made with placement
//               new), or for volatile device registers.
//   - view<T>() for bytes C++ never created as a T: firmware tables, structs
//               laid over a buffer. It starts a T's lifetime over the existing
//               bytes (std::start_lifetime_as) without copying them, which is
//               what makes reading them defined.

namespace mm
{
struct VirtualAddress
{
    uintptr_t value;

    [[nodiscard]] static auto of(const volatile void* pointer) noexcept -> VirtualAddress
    {
        return {reinterpret_cast<uintptr_t>(pointer)};
    }

    template <typename T>
    [[nodiscard]] auto as() const noexcept -> T*
    {
        return reinterpret_cast<T*>(value);
    }

    template <typename T>
    [[nodiscard]] auto view() const noexcept -> const T*
    {
        return std::start_lifetime_as<T>(as<const void>());
    }

    [[nodiscard]] constexpr auto is_aligned(std::size_t alignment) const noexcept -> bool
    {
        return value % alignment == 0;
    }

    [[nodiscard]] constexpr auto operator+(std::size_t offset) const noexcept -> VirtualAddress
    {
        return {value + offset};
    }

    constexpr auto operator+=(std::size_t offset) noexcept -> VirtualAddress&
    {
        value += offset;
        return *this;
    }

    [[nodiscard]] constexpr auto operator<=>(const VirtualAddress&) const noexcept = default;
};

struct PhysicalAddress
{
    uintptr_t value;

    // Where this frame appears in the higher half direct map.
    [[nodiscard]] auto to_virtual() const noexcept -> VirtualAddress
    {
        return {value + boot::hhdm_offset()};
    }

    [[nodiscard]] constexpr auto is_aligned(std::size_t alignment) const noexcept -> bool
    {
        return value % alignment == 0;
    }

    // `alignment` must be a power of two.
    [[nodiscard]] constexpr auto align_down(std::size_t alignment) const noexcept -> PhysicalAddress
    {
        return {value & ~(alignment - 1)};
    }

    [[nodiscard]] constexpr auto align_up(std::size_t alignment) const noexcept -> PhysicalAddress
    {
        return {(value + alignment - 1) & ~(alignment - 1)};
    }

    [[nodiscard]] constexpr auto operator+(std::size_t offset) const noexcept -> PhysicalAddress
    {
        return {value + offset};
    }

    constexpr auto operator+=(std::size_t offset) noexcept -> PhysicalAddress&
    {
        value += offset;
        return *this;
    }

    [[nodiscard]] constexpr auto operator<=>(const PhysicalAddress&) const noexcept = default;
};
}
