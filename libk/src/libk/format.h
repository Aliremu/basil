#pragma once

// Formatting core shared by the kernel and host tests. Header-only so each
// user compiles it with its own flags: freestanding in the kernel, hosted in
// libk/tests.

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <meta>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace libk
{
auto format_error(const char* why) -> void;

using Writer = std::function_ref<void(char)>;

struct FormatSpec
{
    char type = '\0';
    bool alternate = false;
    bool zero_pad = false;
    bool right_align = false;
    std::size_t width = 0;
};

namespace detail
{
inline constexpr std::string_view integer_types = "bdoxX";

struct Segment
{
    std::string_view literal;
    std::optional<FormatSpec> field;
    std::string_view rest;
};

constexpr auto fail(const char* why) -> void
{
    if consteval
    {
        format_error(why);
    }
}

[[nodiscard]] constexpr auto parse_spec(std::string_view text) -> FormatSpec
{
    auto spec = FormatSpec{};
    if (text.empty())
        return spec;

    if (text.front() != ':')
        fail("replacement field must be {} or {:spec}");
    text.remove_prefix(1);

    const auto take = [&text](char c)
    {
        if (text.empty() || text.front() != c)
            return false;
        text.remove_prefix(1);
        return true;
    };

    spec.alternate = take('#');
    spec.zero_pad = take('0');
    spec.right_align = take('>');

    while (!text.empty() && text.front() >= '0' && text.front() <= '9')
    {
        spec.width = spec.width * 10 + static_cast<std::size_t>(text.front() - '0');
        text.remove_prefix(1);
    }

    if (!text.empty())
    {
        spec.type = text.front();
        text.remove_prefix(1);
    }

    if (!text.empty())
        fail("unexpected characters after the format type");

    return spec;
}

[[nodiscard]] constexpr auto next_segment(std::string_view fmt) -> Segment
{
    const auto end = fmt.end();
    for (auto it = fmt.begin(); it != end; ++it)
    {
        if (*it != '{' && *it != '}')
            continue;

        const auto next = it + 1;
        if (next != end && *next == *it)
            return {{fmt.begin(), next}, std::nullopt, {next + 1, end}};

        if (*it == '}')
            fail("unmatched '}'");

        auto close = next;
        while (close != end && *close != '}')
            ++close;

        if (close == end)
            fail("unterminated replacement field");

        return {{fmt.begin(), it}, parse_spec({next, close}), {close + 1, end}};
    }

    return {fmt, std::nullopt, {}};
}

[[nodiscard]] constexpr auto radix(char type) -> unsigned
{
    switch (type)
    {
        case 'b': return 2;
        case 'o': return 8;
        case 'x':
        case 'X': return 16;
        default: return 10;
    }
}

[[nodiscard]] constexpr auto base_prefix(const FormatSpec& spec) -> std::string_view
{
    if (!spec.alternate)
        return {};

    switch (spec.type)
    {
        case 'b': return "0b";
        case 'o': return "0";
        case 'x': return "0x";
        case 'X': return "0X";
        default: return {};
    }
}

[[nodiscard]] constexpr auto padding(const FormatSpec& spec, std::size_t length) -> std::size_t
{
    return spec.width > length ? spec.width - length : 0;
}

inline auto write(Writer out, std::string_view text) -> void
{
    for (auto c : text)
        out(c);
}

inline auto fill(Writer out, std::size_t count, char c) -> void
{
    for (auto i = 0uz; i < count; ++i)
        out(c);
}
}

template <typename T>
struct Formatter;

template <typename T>
concept Formattable = requires(const std::remove_cvref_t<T>& value, Writer out, const FormatSpec& spec) {
    { Formatter<std::remove_cvref_t<T>>::types } -> std::convertible_to<std::string_view>;
    Formatter<std::remove_cvref_t<T>>::format(value, out, spec);
};

template <>
struct Formatter<std::string_view>
{
    static constexpr auto types = std::string_view{};

    static auto format(std::string_view text, Writer out, const FormatSpec& spec) -> void
    {
        detail::write(out, text);
        detail::fill(out, detail::padding(spec, text.size()), ' ');
    }
};

template <>
struct Formatter<const char*>
{
    static constexpr auto types = std::string_view{};

    static auto format(const char* text, Writer out, const FormatSpec& spec) -> void
    {
        Formatter<std::string_view>::format(text, out, spec);
    }
};

template <>
struct Formatter<char*> : Formatter<const char*>
{
};

template <std::size_t N>
struct Formatter<char[N]>
{
    static constexpr auto types = std::string_view{};

    static auto format(const char (&text)[N], Writer out, const FormatSpec& spec) -> void
    {
        auto length = 0uz;
        while (length < N && text[length] != '\0')
            ++length;

        Formatter<std::string_view>::format({text, length}, out, spec);
    }
};

template <>
struct Formatter<char>
{
    static constexpr auto types = std::string_view{};

    static auto format(char c, Writer out, const FormatSpec& spec) -> void
    {
        Formatter<std::string_view>::format({&c, 1}, out, spec);
    }
};

template <>
struct Formatter<bool>
{
    static constexpr auto types = std::string_view{};

    static auto format(bool value, Writer out, const FormatSpec& spec) -> void
    {
        Formatter<std::string_view>::format(value ? "true" : "false", out, spec);
    }
};

template <std::integral T>
struct Formatter<T>
{
    static constexpr auto types = detail::integer_types;

    static auto format(T value, Writer out, const FormatSpec& spec) -> void
    {
        using Unsigned = std::make_unsigned_t<T>;

        const auto negative = value < 0;
        const auto base = detail::radix(spec.type);
        const auto* digits = spec.type == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
        const auto prefix = detail::base_prefix(spec);

        auto magnitude = static_cast<Unsigned>(value);
        if (negative)
            magnitude = static_cast<Unsigned>(0 - magnitude);

        auto buffer = std::array<char, std::numeric_limits<Unsigned>::digits>{};
        auto first = buffer.end();
        do
        {
            *--first = digits[magnitude % base];
            magnitude /= base;
        } while (magnitude != 0);

        const auto number = std::string_view{first, buffer.end()};
        const auto gap = detail::padding(spec, number.size() + prefix.size() + (negative ? 1 : 0));

        if (!spec.zero_pad && spec.right_align)
            detail::fill(out, gap, ' ');
        if (negative)
            out('-');
        detail::write(out, prefix);
        if (spec.zero_pad)
            detail::fill(out, gap, '0');

        detail::write(out, number);

        if (!spec.zero_pad && !spec.right_align)
            detail::fill(out, gap, ' ');
    }
};

template <typename T>
struct Formatter<T*>
{
    static constexpr auto types = std::string_view{};

    static auto format(T* pointer, Writer out, const FormatSpec& spec) -> void
    {
        Formatter<uintptr_t>::format(
            reinterpret_cast<uintptr_t>(pointer),
            out,
            {.type = 'x', .alternate = true, .zero_pad = spec.zero_pad, .width = spec.width});
    }
};

template <>
struct Formatter<std::nullptr_t> : Formatter<const void*>
{
};

template <typename T>
    requires std::is_enum_v<T>
struct Formatter<T>
{
    static constexpr auto types = detail::integer_types;

    static auto format(T value, Writer out, const FormatSpec& spec) -> void
    {
        if (spec.type == '\0')
        {
            template for (constexpr auto enumerator : std::define_static_array(std::meta::enumerators_of(^^T)))
            {
                if (value == [:enumerator:])
                {
                    Formatter<std::string_view>::format(std::meta::identifier_of(enumerator), out, spec);
                    return;
                }
            }
        }

        Formatter<std::underlying_type_t<T>>::format(std::to_underlying(value), out, spec);
    }
};

template <typename T>
    requires std::ranges::input_range<const T> && Formattable<std::ranges::range_value_t<const T>>
struct Formatter<T>
{
    static constexpr auto types = detail::integer_types;

    static auto format(const T& range, Writer out, const FormatSpec& spec) -> void
    {
        auto first = true;
        out('[');
        for (const auto& element : range)
        {
            if (!std::exchange(first, false))
                detail::write(out, ", ");
            Formatter<std::ranges::range_value_t<const T>>::format(element, out, spec);
        }
        out(']');
    }
};

template <typename T>
    requires(std::is_aggregate_v<T> && !std::ranges::range<T>)
struct Formatter<T>
{
    static constexpr auto types = detail::integer_types;

    static auto format(const T& object, Writer out, const FormatSpec& spec) -> void
    {
        constexpr auto ctx = std::meta::access_context::unchecked();
        auto first = true;

        if constexpr (std::meta::has_identifier(^^T))
        {
            detail::write(out, std::meta::identifier_of(^^T));
            out('\n');
        }

        detail::write(out, "{\n");
        template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx)))
        {
            if (!std::exchange(first, false))
                detail::write(out, ",\n");

            detail::write(out, std::meta::identifier_of(member));
            detail::write(out, ": ");
            Formatter<std::remove_cvref_t<decltype(object.[:member:])>>::format(object.[:member:], out, spec);
        }
        detail::write(out, "\n}");
    }
};

template <typename... Args>
struct FormatString
{
    std::string_view str;

    template <std::size_t N>
    consteval FormatString(const char (&s)[N])
        : str{s, N - 1}
    {
        const auto types =
            std::array<std::string_view, sizeof...(Args)>{Formatter<std::remove_cvref_t<Args>>::types...};

        auto rest = str;
        auto count = 0uz;
        while (!rest.empty())
        {
            const auto segment = detail::next_segment(rest);
            rest = segment.rest;
            if (!segment.field)
                continue;

            if (count == sizeof...(Args))
                format_error("more replacement fields than arguments");

            if (segment.field->type != '\0' && !types[count].contains(segment.field->type))
                format_error("format type is not supported by this argument");

            ++count;
        }

        if (count != sizeof...(Args))
            format_error("more arguments than replacement fields");
    }
};

namespace detail
{
struct Arg
{
    void (*print)(const void* value, Writer out, const FormatSpec& spec);
    const void* value;
};

template <typename T>
auto print_erased(const void* value, Writer out, const FormatSpec& spec) -> void
{
    Formatter<T>::format(*static_cast<const T*>(value), out, spec);
}

inline auto vformat_to(Writer out, std::string_view fmt, std::span<const Arg> args) -> void
{
    auto arg = args.begin();
    while (!fmt.empty())
    {
        const auto segment = next_segment(fmt);
        write(out, segment.literal);
        fmt = segment.rest;

        if (segment.field)
        {
            arg->print(arg->value, out, *segment.field);
            ++arg;
        }
    }
}
}

template <Formattable... Args>
inline auto format_to(Writer out, FormatString<std::type_identity_t<Args>...> fmt, const Args&... args) -> void
{
    const auto erased = std::array<detail::Arg, sizeof...(Args)>{
        detail::Arg{&detail::print_erased<std::remove_cvref_t<Args>>, &args}...,
    };
    detail::vformat_to(out, fmt.str, erased);
}
}
