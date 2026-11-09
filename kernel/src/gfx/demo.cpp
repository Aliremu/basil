#include "gfx/framebuffer.h"
#include <array>
#include <cstdint>

namespace gfx
{
namespace
{
// The kernel has no FPU state (-mno-80387 -mno-sse), so positions and
// velocities are 16.16 fixed point: one pixel is 1 << 16.
using Fixed = int64_t;
constexpr auto frac_bits = 16;
constexpr auto one = Fixed{1} << frac_bits;

[[nodiscard]] constexpr auto to_fixed(int64_t pixels) -> Fixed
{
    return pixels * one;
}

[[nodiscard]] constexpr auto to_pixels(Fixed f) -> int64_t
{
    return f >> frac_bits;
}

[[nodiscard]] constexpr auto mul(Fixed a, Fixed b) -> Fixed
{
    return (a * b) >> frac_bits;
}

[[nodiscard]] constexpr auto div(Fixed a, Fixed b) -> Fixed
{
    return (a * one) / b;
}

// Bit-by-bit integer square root. On a fixed-point square (scale 2^32) it
// returns a fixed-point length (scale 2^16).
[[nodiscard]] constexpr auto isqrt(uint64_t n) -> uint64_t
{
    auto root = uint64_t{};
    auto bit = uint64_t{1} << 62;
    while (bit > n)
        bit >>= 2;
    while (bit != 0)
    {
        if (n >= root + bit)
        {
            n -= root + bit;
            root = (root >> 1) + bit;
        }
        else
        {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}
static_assert(isqrt(144) == 12 && isqrt(143) == 11);

// Per timer tick (100 Hz): a ball falls a full 800 px screen in about 1.5 s.
constexpr auto gravity = one * 7 / 100;
constexpr auto wall_bounce = one * 85 / 100; // fraction of speed kept off a wall
constexpr auto ball_bounce = one * 95 / 100; // ... and off another ball
constexpr auto floor_friction = one * 99 / 100;
constexpr auto kick_interval = uint64_t{600}; // ticks between random kicks
constexpr auto max_steps_per_frame = uint64_t{4};
constexpr auto background = uint32_t{0xFF050505}; // kmain's clear color

constexpr auto palette = std::array<uint32_t, 8>{
    0xFFFF5964,
    0xFFFFE74C,
    0xFF6BF178,
    0xFF35A7FF,
    0xFFB388EB,
    0xFFFF9F1C,
    0xFF2EC4B6,
    0xFFF15BB5,
};

struct Ball
{
    Fixed x, y;
    Fixed vx, vy;
    int64_t radius;
    uint32_t color;
    int64_t drawn_x, drawn_y; // where the last frame painted it, to erase it
    bool drawn;
};

constinit auto balls = std::array<Ball, 14>{};
constinit auto spawned = false;
constinit auto ticks_until_kick = kick_interval;
constinit auto rng_state = uint64_t{0x9E3779B97F4A7C15};

// xorshift64: plenty random for a demo, and needs no state beyond one word.
[[nodiscard]] auto random(int64_t lo, int64_t hi) -> int64_t
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return lo + static_cast<int64_t>(rng_state % static_cast<uint64_t>(hi - lo + 1));
}

auto spawn(int64_t width, int64_t height) -> void
{
    for (auto i = 0uz; i < balls.size(); ++i)
    {
        auto& ball = balls[i];
        ball.radius = random(14, 34);
        ball.x = to_fixed(random(ball.radius, width - ball.radius));
        ball.y = to_fixed(random(ball.radius, height / 2));
        ball.vx = random(-4 * one, 4 * one);
        ball.vy = random(-2 * one, 0);
        ball.color = palette[i % palette.size()];
    }
    spawned = true;
}

auto kick() -> void
{
    for (auto& ball : balls)
    {
        ball.vx += random(-3 * one, 3 * one);
        ball.vy -= random(4 * one, 9 * one);
    }
}

auto bounce_off_walls(Ball& ball, Fixed width, Fixed height) -> void
{
    const auto r = to_fixed(ball.radius);
    if (ball.x < r)
    {
        ball.x = r;
        ball.vx = -mul(ball.vx, wall_bounce);
    }
    else if (ball.x > width - r)
    {
        ball.x = width - r;
        ball.vx = -mul(ball.vx, wall_bounce);
    }

    if (ball.y < r)
    {
        ball.y = r;
        ball.vy = -mul(ball.vy, wall_bounce);
    }
    else if (ball.y > height - r)
    {
        ball.y = height - r;
        ball.vy = -mul(ball.vy, wall_bounce);
        ball.vx = mul(ball.vx, floor_friction);
    }
}

// Resolves an overlap between two balls as an elastic-ish collision. Mass goes
// with area, so small balls get knocked around by big ones.
auto collide(Ball& a, Ball& b) -> void
{
    const auto dx = b.x - a.x;
    const auto dy = b.y - a.y;
    const auto reach = to_fixed(a.radius + b.radius);
    const auto dist_sq = dx * dx + dy * dy; // scale 2^32, like reach * reach
    if (dist_sq == 0 || dist_sq >= reach * reach)
        return;

    const auto dist = static_cast<Fixed>(isqrt(static_cast<uint64_t>(dist_sq)));
    const auto nx = div(dx, dist); // unit normal from a to b
    const auto ny = div(dy, dist);
    const auto mass_a = a.radius * a.radius;
    const auto mass_b = b.radius * b.radius;
    const auto total = mass_a + mass_b;

    // Push them apart so they no longer overlap; the lighter ball moves more.
    const auto overlap = reach - dist;
    a.x -= mul(nx, overlap) * mass_b / total;
    a.y -= mul(ny, overlap) * mass_b / total;
    b.x += mul(nx, overlap) * mass_a / total;
    b.y += mul(ny, overlap) * mass_a / total;

    // Exchange momentum along the normal, unless they are already separating.
    const auto closing = mul(b.vx - a.vx, nx) + mul(b.vy - a.vy, ny);
    if (closing >= 0)
        return;
    const auto impulse = mul(closing, one + ball_bounce);
    a.vx += mul(impulse, nx) * mass_b / total;
    a.vy += mul(impulse, ny) * mass_b / total;
    b.vx -= mul(impulse, nx) * mass_a / total;
    b.vy -= mul(impulse, ny) * mass_a / total;
}

auto step(int64_t width, int64_t height) -> void
{
    for (auto& ball : balls)
    {
        ball.vy += gravity;
        ball.x += ball.vx;
        ball.y += ball.vy;
        bounce_off_walls(ball, to_fixed(width), to_fixed(height));
    }

    for (auto i = 0uz; i < balls.size(); ++i)
    {
        for (auto j = i + 1; j < balls.size(); ++j)
            collide(balls[i], balls[j]);
    }
}

// Scales each channel by brightness / 256, saturating, so values above 256
// blow out into a highlight.
[[nodiscard]] auto shade(uint32_t color, int64_t brightness) -> uint32_t
{
    auto channel = [&](int shift) -> uint32_t
    {
        const auto value = static_cast<int64_t>((color >> shift) & 0xFF) * brightness >> 8;
        return static_cast<uint32_t>(value > 255 ? 255 : value) << shift;
    };
    return 0xFF000000 | channel(16) | channel(8) | channel(0);
}

// Paints a filled circle, clipped to the screen. Shaded circles are lit from
// the upper left; unshaded ones are flat `color`, used to erase.
auto paint_circle(int64_t cx, int64_t cy, int64_t r, uint32_t color, bool shaded) -> void
{
    auto& fb = screen();
    const auto width = static_cast<int64_t>(fb.width);
    const auto height = static_cast<int64_t>(fb.height);
    const auto light = r / 3;
    const auto max_light_sq = (r + light) * (r + light);

    for (auto dy = -r; dy <= r; ++dy)
    {
        const auto y = cy + dy;
        if (y < 0 || y >= height)
            continue;
        for (auto dx = -r; dx <= r; ++dx)
        {
            const auto x = cx + dx;
            if (x < 0 || x >= width || dx * dx + dy * dy > r * r)
                continue;

            if (!shaded)
            {
                fb.at(x, y) = color;
                continue;
            }
            const auto lx = dx + light;
            const auto ly = dy + light;
            fb.at(x, y) = shade(color, 300 - (lx * lx + ly * ly) * 250 / max_light_sq);
        }
    }
}
}

auto demo_frame(uint64_t elapsed_ticks) -> void
{
    const auto width = static_cast<int64_t>(screen().width);
    const auto height = static_cast<int64_t>(screen().height);
    if (!spawned)
        spawn(width, height);

    // Catch up on missed ticks, but not so far that one slow frame snowballs.
    const auto steps = elapsed_ticks < max_steps_per_frame ? elapsed_ticks : max_steps_per_frame;
    for (auto i = uint64_t{}; i < steps; ++i)
    {
        step(width, height);
        if (--ticks_until_kick == 0)
        {
            kick();
            ticks_until_kick = kick_interval;
        }
    }

    // Erase everything first, then draw: erasing one ball after drawing an
    // overlapping one would cut a hole in it.
    for (auto& ball : balls)
    {
        if (ball.drawn)
            paint_circle(ball.drawn_x, ball.drawn_y, ball.radius, background, false);
    }
    for (auto& ball : balls)
    {
        ball.drawn_x = to_pixels(ball.x);
        ball.drawn_y = to_pixels(ball.y);
        ball.drawn = true;
        paint_circle(ball.drawn_x, ball.drawn_y, ball.radius, ball.color, true);
    }
}
}
