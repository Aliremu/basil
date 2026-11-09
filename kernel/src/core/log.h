#pragma once

#include "core/print.h"
#include "libk/format.h"
#include <cstddef>
#include <cstdint>
#include <source_location>

namespace log
{
enum class Level : uint8_t
{
    Info,
    Warn,
    Error,
};

// A format string that also records where it was written. The default argument
// is evaluated at the call site, so `location` is the caller's, and it rides in
// the first parameter instead of after the argument pack, where it would stop
// the pack from being deduced.
template <typename... Args>
struct LocatedFormat
{
    libk::FormatString<Args...> fmt;
    std::source_location location;

    template <std::size_t N>
    consteval LocatedFormat(const char (&s)[N], std::source_location location = std::source_location::current())
        : fmt{s}
        , location{location}
    {
    }
};

inline auto prefix(Level level, std::source_location location) -> void
{
    switch (level)
    {
        case Level::Info: kernel::kprintf("[\x1b[32minfo\x1b[37m] "); break;
        case Level::Warn: kernel::kprintf("[\x1b[33mwarn\x1b[37m] "); break;
        case Level::Error: kernel::kprintf("[\x1b[31merror\x1b[37m] "); break;
    }
    kernel::kprintf("[{}:{}] ", location.file_name(), location.line());
}

template <libk::Formattable... Args>
auto write(Level level, LocatedFormat<std::type_identity_t<Args>...> fmt, const Args&... args) -> void
{
    prefix(level, fmt.location);
    kernel::kprintf(fmt.fmt, args...);
    kernel::kprintf("\n");
}

template <libk::Formattable... Args>
auto info(LocatedFormat<std::type_identity_t<Args>...> fmt, const Args&... args) -> void
{
    write<Args...>(Level::Info, fmt, args...);
}

template <libk::Formattable... Args>
auto warn(LocatedFormat<std::type_identity_t<Args>...> fmt, const Args&... args) -> void
{
    write<Args...>(Level::Warn, fmt, args...);
}

template <libk::Formattable... Args>
auto error(LocatedFormat<std::type_identity_t<Args>...> fmt, const Args&... args) -> void
{
    write<Args...>(Level::Error, fmt, args...);
}
}
