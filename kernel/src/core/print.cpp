#include "core/print.h"
#include <array>
#include <optional>

namespace
{
constexpr auto max_sinks = 10;
constinit auto sinks = std::array<std::optional<kernel::Sink>, max_sinks>{};
constinit auto sink_count = std::size_t{0};
}

namespace kernel
{
auto add_sink(Sink sink) -> void
{
    if (sink_count < max_sinks)
        sinks[sink_count++] = sink;
}

auto put(char c) -> void
{
    for (std::size_t i = 0; i < sink_count; i++)
    {
        if (c == '\n')
            (*sinks[i])('\r'); // terminals expect CR LF
        (*sinks[i])(c);
    }
}

auto put(std::string_view s) -> void
{
    for (char c : s)
        put(c);
}
}
