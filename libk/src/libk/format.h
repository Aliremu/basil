#pragma once

// Formatting core shared by the kernel and host tests. Header-only so each
// user compiles it with its own flags: freestanding in the kernel, hosted in
// libk/tests.

#include <array>
#include <cstddef>
#include <functional>
#include <meta>
#include <string_view>
#include <type_traits>
#include <utility>

namespace libk
{
auto format_error(const char* why) -> void;

template <typename... Args>
struct FormatString
{
    std::string_view str;

    template <std::size_t N>
    consteval FormatString(const char (&s)[N])
        : str{s, N - 1}
    {
        auto isCapture = false;
        auto idx = std::size_t{};

        for (auto c : str)
        {
            if (c == '}' && isCapture)
            {
                ++idx;
                isCapture = false;
            }
            else
            {
                isCapture = c == '{';
            }
        }

        if (idx != sizeof...(Args))
            format_error("argument count does not match the format string");
    }
};

using Writer = std::function_ref<void(char)>;

template <typename T>
struct Formatter; // no primary definition: unformattable types fail to compile

template <typename T>
concept Formattable = requires(const T& v, Writer out) { Formatter<std::decay_t<const T&>>::format(v, out); };

template <>
struct Formatter<char>
{
    static auto format(char c, Writer out) -> void
    {
        out(c);
    }
};

template <std::integral T>
struct Formatter<T>
{
    static auto format(T n, Writer out) -> void
    {
        using U = std::make_unsigned_t<T>;

        auto digits = std::array<char, 32>{0};
        auto len = 0;
        auto negative = n < 0;
        auto magnitude = static_cast<U>(n);
        if (negative)
            magnitude = static_cast<U>(0 - magnitude);
        do
        {
            digits[len++] = magnitude % 10;
            magnitude /= 10;
        } while (magnitude > 0);

        if (negative)
            out('-');

        while (len > 0)
            out('0' + digits[--len]);
    }
};

template <>
struct Formatter<const char*>
{
    static auto format(const char* s, Writer out) -> void
    {
        while (*s != '\0')
        {
            out(*s);
            ++s;
        }
    }
};

template <>
struct Formatter<std::string_view>
{
    static auto format(std::string_view s, Writer out) -> void
    {
        for (auto c : s)
        {
            out(c);
        }
    }
};

template <typename T>
    requires std::is_aggregate_v<T>
struct Formatter<T>
{
    static auto format(const T& obj, Writer out) -> void
    {
        constexpr auto ctx = std::meta::access_context::unchecked();
        auto first = true;
        for (auto c : std::meta::identifier_of(^^T))
            out(c);

        out('\n');
        out('{');
        out('\n');
        template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx)))
        {
            if (!std::exchange(first, false))
            {
                out(',');
                out('\n');
            }
            constexpr auto name = std::meta::identifier_of(member);
            for (auto c : name)
                out(c);
            out(':');
            out(' ');
            Formatter<std::remove_cvref_t<decltype(obj.[:member:])>>::format(obj.[:member:], out);
        }
        out('\n');
        out('}');
    }
};
}
