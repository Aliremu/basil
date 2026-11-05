#pragma once
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

    auto at(uint64_t x, uint64_t y) -> volatile uint32_t&
    {
        return back[y * stride + x];
    }
};

auto init(const Framebuffer& fb) -> void;
auto clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a) -> void;
auto put(char c) -> void;
auto screen() -> Framebuffer&;
auto present() -> void;

// Test scene: shaded balls bouncing and colliding under gravity, with a random
// kick every few seconds. Call from normal (non-interrupt) code with the number
// of timer ticks since the previous call.
auto demo_frame(uint64_t elapsed_ticks) -> void;

inline auto rainbow(uint64_t tick) -> void
{
    auto r = (tick / 100) % 256;
    auto g = (tick / 10) % 256;
    auto b = (tick / 200) % 256;
    clear(r, g, b, 0xFF);
}
}
