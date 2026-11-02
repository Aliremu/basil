#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

#include <libk/format.h>

namespace kernel
{
using Sink = std::function_ref<void(char)>;
auto add_sink(Sink sink) -> void;
auto put(char c) -> void;
auto put(std::string_view s) -> void;
// auto put_unsigned(uint32_t value, ) -> void;
// auto put_signed(char c) -> void;
enum class Spec : uint8_t
{
    Plain,
    Hex
}; // {} or {:x}

struct Arg // one argument with its type erased
{
    void (*print)(const void* value, Spec spec);
    const void* value;
};

using libk::FormatString;
using libk::Formattable;
using libk::Formatter;
using libk::Writer;

auto vprint(std::string_view fmt, const Arg* args, std::size_t count) -> void;

template <Formattable... Args>
inline auto kprintf(FormatString<std::type_identity_t<Args>...> fmt, const Args&... args) -> void
{
    enum class State : uint8_t
    {
        Normal,
        Primed,
    } state = State::Normal;

    auto format_at = [&]<typename... Ts>(this auto&& self, std::size_t i, const Ts&... values) -> void
    {
        if constexpr (sizeof...(Ts) > 0)
        {
            [&]<typename T, typename... Rest>(const T& first, const Rest&... rest)
            {
                if (i == 0)
                    Formatter<std::decay_t<const T&>>::format(first, static_cast<void (*)(char)>(put));
                else if constexpr (sizeof...(Rest) > 0)
                    self(i - 1, rest...);
            }(values...);
        }
    };

    auto idx = std::size_t{};
    for (auto c : fmt.str)
    {
        switch (state)
        {
            case State::Normal:
                if (c == '{')
                    state = State::Primed;
                else
                    put(c);
                break;
            case State::Primed:
                if (c == '}')
                    format_at(idx++, args...);
                else
                    put(c);

                state = State::Normal;
                break;
        }
    }
}
}
