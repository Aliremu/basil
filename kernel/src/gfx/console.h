#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace gfx
{
struct Color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;

    [[nodiscard]] constexpr auto to_hex() const -> uint32_t
    {
        return 0xFF000000 | (r << 16) | (g << 8) | b;
    }
};

inline constexpr auto default_color = Color{229, 229, 229};
inline constexpr auto green = Color{13, 188, 121};
inline constexpr auto red = Color{205, 49, 49};

struct Cell
{
    char32_t ch{};
    Color color{default_color};
};

class Console
{
  public:
    auto put(char c) -> void;
    auto on_tick() -> void;

    [[nodiscard]] static auto cols() -> std::size_t;
    [[nodiscard]] static auto rows() -> std::size_t;

  private:
    enum class State : uint8_t
    {
        Text,
        Escape,
        Csi
    };

    static constexpr auto max_cols = 160uz;
    static constexpr auto max_rows = 50uz;
    static constexpr auto margin = 5;

    std::array<Cell, max_cols * max_rows> cells_{};
    std::size_t x_{};
    std::size_t y_{};
    Color color_{default_color};
    std::size_t param_{};
    State state_{State::Text};
    char32_t codepoint_{};
    int pending_{};

    auto write(char32_t c) -> void;
    auto execute(char c, std::size_t param) -> void;
    auto decode(unsigned char byte) -> void;
    auto restore() const -> void;
    auto draw(Cell cell) const -> void;
    auto next_line() -> void;

    static auto draw_at(Cell cell, std::size_t row, std::size_t col) -> void;
};
}
