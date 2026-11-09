#include "core/print.h"
#include "libk/format.h"
#include <array>
#include <cstddef>
#include <optional>

namespace
{
constexpr auto max_writers = 10uz;
constinit auto writers = std::array<std::optional<libk::Writer>, max_writers>{};
constinit auto writer_count = 0uz;
}

namespace kernel
{
auto add_sink(libk::Writer writer) -> void
{
    if (writer_count < max_writers)
    {
        writers[writer_count] = writer;
        ++writer_count;
    }
}

auto put(char c) -> void
{
    for (auto i = 0uz; i < writer_count; ++i)
        (*writers[i])(c);
}

auto put(std::string_view s) -> void
{
    for (const auto c : s)
        put(c);
}
}
