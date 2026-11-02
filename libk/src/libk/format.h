#pragma once

// Formatting core shared by the kernel and host tests. Header-only so each
// user compiles it with its own flags: freestanding in the kernel, hosted in
// libk/tests.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <ranges>
#include <string_view>
#include <type_traits>

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
struct Formatter<int>
{
    static auto format(int n, Writer out) -> void
    {
        auto digits = std::array<char, 32>{0};
        auto len = 0;
        auto negative = n < 0;
        if (negative)
            n = 0 - static_cast<unsigned int>(n);
        do
        {
            digits[len++] = n % 10;
            n /= 10;
        } while (n > 0);

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
}
