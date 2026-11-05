#include "framebuffer.h"
#include "arch/x86_64/cpu.h"
#include "boot/limine.h"
#include "core/print.h"
#include "gfx/fonts/spleen12x24.h"
#include "limine.h"
#include <ranges>

namespace gfx
{
namespace
{
constexpr auto back_buffer_capacity = 1920uz * 1200;
alignas(64) constinit auto back_buffer_storage = std::array<uint32_t, back_buffer_capacity>{};

constinit auto g_Screen = Framebuffer{};
constexpr auto margin = uint64_t{5};
constinit auto g_ScreenX = margin;
constinit auto g_ScreenY = margin;

auto draw_char(char ch, int x, int y, Framebuffer& fb, int scale = 1, std::uint32_t color = 0xFFFFFFFF) -> void
{
    constexpr auto rows = spleen12x24_height;
    constexpr auto cols = spleen12x24_width;
    auto index = static_cast<unsigned char>(ch);
    if (index >= 128)
        index = '?';

    const auto& glyph = spleen12x24[index];
    for (auto r : std::views::iota(0, rows))
    {
        for (auto c : std::views::iota(0, cols))
        {
            if (((glyph[r] >> c) & 1) == 0)
                continue;

            // Paint this font pixel as a scale×scale block.
            for (auto dy : std::views::iota(0, scale))
            {
                for (auto dx : std::views::iota(0, scale))
                    fb.at(x + c * scale + dx, y + r * scale + dy) = color;
            }
        }
    }
}
}

auto init(const Framebuffer& fb) -> void
{
    g_Screen = fb;
    if (fb.stride * fb.height > back_buffer_capacity)
    {
        kernel::kprintf("gfx: {}x{} too large for the back buffer\n", fb.width, fb.height);
        arch::halt_forever();
    }
    g_Screen.back = back_buffer_storage.data();
}

auto screen() -> Framebuffer&
{
    return g_Screen;
}

auto clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a) -> void
{
    for (uint64_t y = 0; y < g_Screen.height; y++)
    {
        for (uint64_t x = 0; x < g_Screen.width; x++)
        {
            g_Screen.at(x, y) = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

auto put(char ch) -> void
{
    constexpr auto rows = spleen12x24_height;
    constexpr auto cols = spleen12x24_width;
    constexpr auto scale = 1;
    if (ch == '\r' || ch == '\n')
    {
        g_ScreenX = margin;
        g_ScreenY += (cols)*scale;
        return;
    }

    if (g_ScreenX + cols >= g_Screen.width)
    {
        g_ScreenX = margin;
        g_ScreenY += (rows)*scale;
    }

    draw_char(ch, g_ScreenX, g_ScreenY, g_Screen, scale);
    g_ScreenX += (cols)*scale;
}

auto present() -> void
{
    auto* dst = const_cast<uint32_t*>(g_Screen.front); // rep movs is the store; volatile only guards C++ accesses
    auto* src = g_Screen.back;
    auto qwords = g_Screen.stride * g_Screen.height / 2; // two pixels per 8 bytes
    __asm__ volatile("rep movsq" : "+D"(dst), "+S"(src), "+c"(qwords) : : "memory");
}
}
