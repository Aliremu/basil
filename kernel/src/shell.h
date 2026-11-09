#pragma once

#include "drivers/keyboard/keyboard.h"
#include "libk/ring_buffer.h"
#include <array>
#include <cstddef>
#include <inplace_vector>
#include <string_view>

namespace basil
{
template <std::size_t N>
class LineEditor
{
  public:
    auto insert(char c) -> void
    {
        if (text_.size() == text_.capacity())
            return;

        text_.insert(text_.begin() + cursor_, c);
        ++cursor_;
    }

    auto backspace() -> void
    {
        if (cursor_ == 0)
            return;

        --cursor_;
        text_.erase(text_.begin() + cursor_);
    }

    auto left() -> void
    {
        if (cursor_ > 0)
            --cursor_;
    }

    auto right() -> void
    {
        if (cursor_ < text_.size())
            ++cursor_;
    }

    auto clear() -> void
    {
        text_.clear();
        cursor_ = 0;
    }

    auto end() -> void
    {
        cursor_ = text_.size();
    }

    [[nodiscard]] auto text() const -> std::string_view
    {
        return std::string_view{text_.data(), text_.size()};
    }

    [[nodiscard]] auto cursor() const -> std::size_t
    {
        return cursor_;
    }

  private:
    std::inplace_vector<char, N> text_;
    std::size_t cursor_{};
};

class Shell
{
  public:
    Shell(std::size_t cols);

    auto on_key(const keyboard::KeyEvent& key) -> void;

  private:
    static constexpr auto prompt = std::string_view{"root@basil \x1b[32m$\x1b[37m "};

    LineEditor<256> line_;
    libk::RingBuffer<std::array<char, 256>, 100> history_;
    std::size_t history_idx_{};
    std::size_t cols_;
    std::size_t offset_{};

    auto run(std::string_view line) -> void;
    auto refresh() -> void;
    auto submit() -> void;

    static auto neofetch() -> void;
};
}
