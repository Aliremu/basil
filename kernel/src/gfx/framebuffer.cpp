#include "gfx/framebuffer.h"
#include "arch/x86_64/cpu.h"
#include "boot/limine.h"
#include "core/log.h"
#include "core/print.h"
#include <array>
#include <ranges>

namespace gfx
{
namespace
{
constexpr auto back_buffer_capacity = 1920uz * 1200;
alignas(64) constinit auto back_buffer_storage = std::array<uint32_t, back_buffer_capacity>{};

constexpr auto margin = uint64_t{5};
constinit auto screen_ = Framebuffer{};
constinit auto cursor_x = margin;
constinit auto cursor_y = margin;
}

auto init(const Framebuffer& fb) -> void
{
    screen_ = fb;
    if (fb.stride * fb.height > back_buffer_capacity)
    {
        kernel::kprintf("gfx: {}x{} too large for the back buffer\n", fb.width, fb.height);
        arch::halt_forever();
    }
    screen_.back = back_buffer_storage.data();
}

auto screen() -> Framebuffer&
{
    return screen_;
}

auto clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a) -> void
{
    for (auto y = uint64_t{}; y < screen_.height; ++y)
    {
        for (auto x = uint64_t{}; x < screen_.width; ++x)
            screen_.at(x, y) = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

auto draw_char(const font::Glyph& glyph, int x, int y, int scale, uint32_t color, uint32_t background) -> void
{
    for (const auto r : std::views::iota(0, glyph.height))
    {
        for (const auto c : std::views::iota(0, glyph.width))
        {
            const auto pixel = glyph.pixel(c, r) ? color : background;

            // Paint this font pixel as a scale×scale block.
            for (const auto dy : std::views::iota(0, scale))
            {
                for (const auto dx : std::views::iota(0, scale))
                    screen_.at(x + c * scale + dx, y + r * scale + dy) = pixel;
            }
        }
    }
}

auto put(char ch) -> void
{
    constexpr auto boot_font = font::get(font::Id::Spleen12x24);
    constexpr auto rows = boot_font.height;
    constexpr auto cols = boot_font.width;
    constexpr auto scale = 1;
    if (ch == '\r')
    {
        cursor_x = margin;
        return;
    }

    if (ch == '\n')
    {
        cursor_x = margin;
        cursor_y += rows * scale;
        return;
    }

    if (cursor_x + cols >= screen_.width)
    {
        cursor_x = margin;
        cursor_y += rows * scale;
    }

    draw_char(boot_font.glyph(static_cast<unsigned char>(ch)), cursor_x, cursor_y, scale);
    cursor_x += cols * scale;
}

auto present() -> void
{
    auto* dst = const_cast<uint32_t*>(screen_.front); // rep movs is the store; volatile only guards C++ accesses
    auto* src = screen_.back;
    auto qwords = screen_.stride * screen_.height / 2; // two pixels per 8 bytes
    __asm__ volatile("rep movsq" : "+D"(dst), "+S"(src), "+c"(qwords) : : "memory");
}
}
