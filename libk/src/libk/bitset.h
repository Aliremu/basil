#pragma once

#include <type_traits>
#include <utility>

template <typename T>
inline constexpr auto enable_bitmask_behavior = false;

template <typename E>
    requires(std::is_enum_v<E> && enable_bitmask_behavior<E>)
[[nodiscard]] constexpr auto operator|(E lhs, E rhs) -> E
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

template <typename E>
    requires(std::is_enum_v<E> && enable_bitmask_behavior<E>)
[[nodiscard]] constexpr auto operator&(E lhs, E rhs) -> E
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

template <typename E>
    requires(std::is_enum_v<E> && enable_bitmask_behavior<E>)
[[nodiscard]] constexpr auto operator~(E lhs) -> E
{
    using U = std::underlying_type_t<E>;
    return static_cast<E>(~static_cast<U>(lhs));
}

template <typename E>
    requires(std::is_enum_v<E> && enable_bitmask_behavior<E>)
[[nodiscard]] constexpr auto has(E lhs, E rhs) -> bool
{
    return std::to_underlying(lhs & rhs) == std::to_underlying(rhs);
}
