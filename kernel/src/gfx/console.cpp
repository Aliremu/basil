#include "gfx/console.h"
#include "arch/x86_64/pit.h"
#include "gfx/font.h"
#include "gfx/framebuffer.h"
#include <algorithm>
#include <functional>
#include <mdspan>
#include <span>

namespace gfx
{
namespace
{
constexpr auto console_font = font::get(font::Id::Blocky6x9, 2);
}

auto Console::put(char c) -> void
{
    switch (state_)
    {
        case State::Text:
            if (c == '\x1b')
                state_ = State::Escape;
            else
                decode(static_cast<unsigned char>(c));
            break;
        case State::Escape: state_ = c == '[' ? State::Csi : State::Text; break;
        case State::Csi:
            if (c >= '0' && c <= '9')
            {
                param_ = param_ * 10 + (c - '0');
            }
            else
            {
                execute(c, param_);
                param_ = 0;
                state_ = State::Text;
            }
            break;
    }
}

auto Console::on_tick() -> void
{
    const auto ticks = arch::uptime_ticks();
    if (ticks % 120 < 60)
        draw({U'█', color_});
    else
        restore();
}

auto Console::cols() -> std::size_t
{
    return screen().width / console_font.width;
}

auto Console::rows() -> std::size_t
{
    return std::min((screen().height - margin) / console_font.height, max_rows);
}

auto Console::write(char32_t c) -> void
{
    const auto cols = Console::cols();
    auto span = std::mdspan{cells_.data(), max_rows, cols};

    if (c == '\n')
    {
        restore();
        span[y_, x_] = {c, color_};
        x_ = 0;
        next_line();
    }
    else if (c == '\t')
    {
        auto nearestX = (x_ + 4) & ~3;
        if (nearestX >= cols)
        {
            nearestX = 0;
            next_line();
        }
        x_ = nearestX;
    }
    else if (c == '\b')
    {
        if (x_ == 0 && y_ == 0)
            return;

        restore();

        if (x_ == 0)
        {
            --y_;
            x_ = cols - 1;

            while (x_ > 0 && span[y_, x_].ch == '\0')
                --x_;
        }
        else
        {
            --x_;
        }

        span[y_, x_] = {};
    }
    else if (c == '\r')
    {
        restore();
        x_ = 0;
    }
    else
    {
        span[y_, x_] = {c, color_};
        draw(span[y_, x_]);
        if (++x_ == cols)
        {
            x_ = 0;
            next_line();
        }
    }
}

auto Console::execute(char c, std::size_t param) -> void
{
    const auto cols = Console::cols();
    auto span = std::mdspan{cells_.data(), max_rows, cols};
    const auto count = std::max(param, 1uz);

    restore();

    switch (c)
    {
        case 'm':
        {
            color_ = std::invoke(
                [param] -> Color
                {
                    switch (param)
                    {
                        case 30: return {0, 0, 0};
                        case 31: return {205, 49, 49};
                        case 32: return {13, 188, 121};
                        case 33: return {229, 229, 16};
                        case 34: return {36, 114, 200};
                        case 35: return {188, 63, 188};
                        case 36: return {17, 168, 205};
                        case 37: return {229, 229, 229};
                        default: return default_color;
                    }
                });
            break;
        }
        case 'C': x_ = std::min(x_ + count, cols - 1); break;
        case 'D': x_ -= std::min(x_, count); break;
        case 'A': y_ -= std::min(y_, count); break;
        case 'J':
        case 'K':
        {
            const auto last_row = c == 'J' ? rows() : y_ + 1;
            for (auto row = y_; row < last_row; ++row)
            {
                for (auto col = row == y_ ? x_ : 0; col < cols; ++col)
                {
                    span[row, col] = {};
                    draw_at(span[row, col], row, col);
                }
            }
            break;
        }
        case 'H':
        {
            cells_.fill({});
            x_ = 0;
            y_ = 0;
            gfx::clear(0x05, 0x05, 0x05, 0xFF);
        }

        default: break;
    }
}

auto Console::decode(unsigned char byte) -> void
{
    if (pending_ > 0 && (byte & 0xC0) == 0x80)
    {
        codepoint_ = (codepoint_ << 6) | (byte & 0x3F);
        if (--pending_ == 0)
            write(codepoint_);
        return;
    }

    if (pending_ > 0)
    {
        pending_ = 0;
        write(U'?');
    }

    if (byte < 0x80)
    {
        write(byte);
        return;
    }

    pending_ = (byte & 0xE0) == 0xC0 ? 1 : (byte & 0xF0) == 0xE0 ? 2 : (byte & 0xF8) == 0xF0 ? 3 : 0;
    if (pending_ == 0)
        write(U'?');

    codepoint_ = byte & (0x3F >> pending_);
}

auto Console::restore() const -> void
{
    draw(std::mdspan{cells_.data(), max_rows, cols()}[y_, x_]);
}

auto Console::draw(Cell cell) const -> void
{
    draw_at(cell, y_, x_);
}

auto Console::next_line() -> void
{
    const auto cols = Console::cols();
    const auto rows = Console::rows();

    if (++y_ < rows)
        return;

    const auto screen = std::span{cells_}.first(rows * cols);
    std::ranges::copy(screen.subspan(cols), screen.begin());
    std::ranges::fill(screen.last(cols), Cell{});
    --y_;

    for (auto row = 0uz; row < rows; ++row)
    {
        for (auto col = 0uz; col < cols; ++col)
            draw_at(screen[row * cols + col], row, col);
    }
}

auto Console::draw_at(Cell cell, std::size_t row, std::size_t col) -> void
{
    const auto c = cell.ch == U'\0' || cell.ch == U'\n' ? U' ' : cell.ch;
    draw_char(
        console_font.glyph(c),
        col * console_font.width,
        row * console_font.height + margin,
        console_font.scale,
        cell.color.to_hex());
}
}
