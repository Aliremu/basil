#pragma once

#include "gfx/font.h"
#include <cstdint>

namespace gfx
{
struct Framebuffer
{
    volatile uint32_t* front;
    volatile uint32_t* back;
    uint64_t width;
    uint64_t height;
    uint64_t stride;

    [[nodiscard]] auto at(uint64_t x, uint64_t y) -> volatile uint32_t&
    {
        return back[y * stride + x];
    }

    [[nodiscard]] auto at(uint64_t x, uint64_t y) const -> const volatile uint32_t&
    {
        return back[y * stride + x];
    }
};

auto init(const Framebuffer& fb) -> void;
auto clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a) -> void;
auto draw_char(
    const font::Glyph& glyph,
    int x,
    int y,
    int scale = 1,
    uint32_t color = 0xFFFFFFFF,
    uint32_t background = 0xFF050505) -> void;
auto put(char c) -> void;
[[nodiscard]] auto screen() -> Framebuffer&;
auto present() -> void;
}
