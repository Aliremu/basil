#pragma once

#include "libk/format.h"
#include <string_view>

namespace kernel
{
auto add_sink(libk::Writer writer) -> void;
auto put(char c) -> void;
auto put(std::string_view s) -> void;

template <libk::Formattable... Args>
inline auto kprintf(libk::FormatString<std::type_identity_t<Args>...> fmt, const Args&... args) -> void
{
    libk::format_to(static_cast<void (*)(char)>(put), fmt, args...);
}
}
