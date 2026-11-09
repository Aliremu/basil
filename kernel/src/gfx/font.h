#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <utility>

namespace gfx::font
{
// One uint16_t per row, top to bottom; bit c of a row is pixel column c (LSB = leftmost).
struct Glyph
{
    std::span<const uint16_t> rows;
    int width;
    int height;

    [[nodiscard]] constexpr auto pixel(int x, int y) const -> bool
    {
        return ((rows[y] >> x) & 1) != 0;
    }
};

enum class Id : uint8_t
{
    Spleen12x24,
    Blocky6x9,
};

struct Font
{
    Id id;
    int width;
    int height;
    int scale{1};

    [[nodiscard]] auto glyph(char32_t ch) const -> Glyph;
};

inline constexpr auto fonts = std::array{
    Font{Id::Spleen12x24, 12, 24},
    Font{Id::Blocky6x9, 6, 9},
};

[[nodiscard]] constexpr auto get(Id id, int scale = 1) -> Font
{
    auto font = fonts[std::to_underlying(id)];
    font.scale = scale;
    font.width *= scale;
    font.height *= scale;
    return font;
}
}
