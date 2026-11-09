#include "gfx/font.h"
#include "gfx/fonts/blocky6x9.h"
#include "gfx/fonts/spleen12x24.h"
#include <utility>

namespace gfx::font
{
namespace
{
template <int Width, int Height>
constexpr auto full_block = []
{
    auto glyph = std::array<uint16_t, Height>{};
    glyph.fill((1u << Width) - 1);
    return glyph;
}();

// Dot (col, row) of a braille cell covers pixel columns [col * pitch_x + x, + width)
// and rows [row * pitch_y + y, + height).
struct DotLayout
{
    int pitch_x;
    int pitch_y;
    int width;
    int height;
    int x;
    int y;
};

// Unicode orders a braille cell's eight dots down the left column, down the
// right column, then the bottom row: bit n of the pattern is dot n + 1.
template <std::size_t Height>
[[nodiscard]] constexpr auto make_braille(DotLayout layout)
{
    constexpr auto dots =
        std::array<std::array<int, 2>, 8>{{{0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2}, {0, 3}, {1, 3}}};

    auto glyphs = std::array<std::array<uint16_t, Height>, 256>{};
    for (auto pattern = 0uz; pattern < glyphs.size(); ++pattern)
    {
        for (auto bit = 0uz; bit < dots.size(); ++bit)
        {
            if (((pattern >> bit) & 1) == 0)
                continue;

            const auto [col, row] = dots[bit];
            const auto bits = ((1u << layout.width) - 1) << (col * layout.pitch_x + layout.x);
            for (auto dy = 0; dy < layout.height; ++dy)
                glyphs[pattern][row * layout.pitch_y + layout.y + dy] |= bits;
        }
    }
    return glyphs;
}

constexpr auto spleen_braille = make_braille<spleen12x24_height>({6, 6, 4, 4, 1, 1});
constexpr auto blocky_braille = make_braille<blocky6x9_height>({3, 2, 2, 2, 0, 0});

[[nodiscard]] auto spleen(char32_t ch) -> std::span<const uint16_t>
{
    if (ch < 0x100)
        return spleen12x24[ch];
    if (ch >= 0x2800 && ch < 0x2900)
        return spleen_braille[ch - 0x2800];
    if (ch == U'█')
        return full_block<spleen12x24_width, spleen12x24_height>;
    return spleen12x24['?'];
}

[[nodiscard]] auto blocky(char32_t ch) -> std::span<const uint16_t>
{
    if (ch < 0x80)
        return blocky6x9[ch];
    if (ch >= 0x2800 && ch < 0x2900)
        return blocky_braille[ch - 0x2800];
    if (ch == U'█')
        return full_block<blocky6x9_width, blocky6x9_height>;
    return blocky6x9['?'];
}
}

static_assert(get(Id::Spleen12x24).width == spleen12x24_width && get(Id::Spleen12x24).height == spleen12x24_height);
static_assert(get(Id::Blocky6x9).width == blocky6x9_width && get(Id::Blocky6x9).height == blocky6x9_height);

auto Font::glyph(char32_t ch) const -> Glyph
{
    const auto rows = [this, ch]
    {
        switch (id)
        {
            case Id::Spleen12x24: return spleen(ch);
            case Id::Blocky6x9: return blocky(ch);
        }
        std::unreachable();
    }();
    return {rows, width / scale, height / scale};
}
}
