#include "drivers/keyboard/keyboard.h"
#include "core/test.h"
#include "libk/ring_buffer.h"

namespace keyboard
{
namespace
{
constexpr std::uint8_t release_bit = 0x80;
constinit auto queue = libk::RingBuffer<KeyEvent, 10>{};

constexpr auto is_key(uint8_t make) -> bool
{
    const auto last_contiguous = static_cast<std::uint8_t>(Scancode::KeypadPeriod);
    return (make >= 0x01 && make <= last_contiguous) || make == static_cast<std::uint8_t>(Scancode::F11) ||
           make == static_cast<std::uint8_t>(Scancode::F12);
}
}

auto translate(uint8_t scancode) -> KeyEvent
{
    const auto make = static_cast<std::uint8_t>(scancode & ~release_bit);
    return {
        .key = is_key(make) ? static_cast<Scancode>(make) : Scancode::Unknown,
        .pressed = (scancode & release_bit) == 0,
    };
}

auto to_char(Scancode key) -> char
{
    switch (key)
    {
        case Scancode::Num0: return '0';
        case Scancode::Num1: return '1';
        case Scancode::Num2: return '2';
        case Scancode::Num3: return '3';
        case Scancode::Num4: return '4';
        case Scancode::Num5: return '5';
        case Scancode::Num6: return '6';
        case Scancode::Num7: return '7';
        case Scancode::Num8: return '8';
        case Scancode::Num9: return '9';
        case Scancode::A: return 'a';
        case Scancode::B: return 'b';
        case Scancode::C: return 'c';
        case Scancode::D: return 'd';
        case Scancode::E: return 'e';
        case Scancode::F: return 'f';
        case Scancode::G: return 'g';
        case Scancode::H: return 'h';
        case Scancode::I: return 'i';
        case Scancode::J: return 'j';
        case Scancode::K: return 'k';
        case Scancode::L: return 'l';
        case Scancode::M: return 'm';
        case Scancode::N: return 'n';
        case Scancode::O: return 'o';
        case Scancode::P: return 'p';
        case Scancode::Q: return 'q';
        case Scancode::R: return 'r';
        case Scancode::S: return 's';
        case Scancode::T: return 't';
        case Scancode::U: return 'u';
        case Scancode::V: return 'v';
        case Scancode::W: return 'w';
        case Scancode::X: return 'x';
        case Scancode::Y: return 'y';
        case Scancode::Z: return 'z';
        case Scancode::Minus: return '-';
        case Scancode::Equals: return '=';
        case Scancode::LeftBracket: return '[';
        case Scancode::RightBracket: return ']';
        case Scancode::Backslash: return '\\';
        case Scancode::Semicolon: return ';';
        case Scancode::Apostrophe: return '\'';
        case Scancode::Backtick: return '`';
        case Scancode::Comma: return ',';
        case Scancode::Period: return '.';
        case Scancode::Slash: return '/';
        case Scancode::Space: return ' ';
        case Scancode::Tab: return '\t';
        case Scancode::Enter: return '\n';
        case Scancode::Backspace: return '\b';
        case Scancode::Keypad0: return '0';
        case Scancode::Keypad1: return '1';
        case Scancode::Keypad2: return '2';
        case Scancode::Keypad3: return '3';
        case Scancode::Keypad4: return '4';
        case Scancode::Keypad5: return '5';
        case Scancode::Keypad6: return '6';
        case Scancode::Keypad7: return '7';
        case Scancode::Keypad8: return '8';
        case Scancode::Keypad9: return '9';
        case Scancode::KeypadPeriod: return '.';
        case Scancode::KeypadPlus: return '+';
        case Scancode::KeypadMinus: return '-';
        case Scancode::KeypadMultiply: return '*';
        default: return '?';
    }
}

auto on_irq(uint8_t scancode) -> void
{
    auto event = translate(scancode);
    queue.push_back(std::move(event));
}

auto poll() -> std::optional<KeyEvent>
{
    return queue.pop_front();
}
}

KTEST(keyboard_translates_press)
{
    const auto event = keyboard::translate(0x1E);
    KTEST_ASSERT(event.key == keyboard::Scancode::A);
    KTEST_ASSERT(event.pressed);
}

KTEST(keyboard_translates_release)
{
    const auto event = keyboard::translate(0x9E);
    KTEST_ASSERT(event.key == keyboard::Scancode::A);
    KTEST_ASSERT(!event.pressed);
}

KTEST(keyboard_translates_keys_past_the_gap)
{
    KTEST_ASSERT(keyboard::translate(0x58).key == keyboard::Scancode::F12);
    KTEST_ASSERT(keyboard::translate(0x54).key == keyboard::Scancode::Unknown);
}

KTEST(keyboard_rejects_non_key_bytes)
{
    KTEST_ASSERT(keyboard::translate(0x00).key == keyboard::Scancode::Unknown);
    KTEST_ASSERT(keyboard::translate(0xE0).key == keyboard::Scancode::Unknown);
    KTEST_ASSERT(keyboard::translate(0xE1).key == keyboard::Scancode::Unknown);
    KTEST_ASSERT(keyboard::translate(0xFA).key == keyboard::Scancode::Unknown);
}

KTEST(keyboard_maps_keys_to_chars)
{
    KTEST_ASSERT(keyboard::to_char(keyboard::Scancode::Num0) == '0');
    KTEST_ASSERT(keyboard::to_char(keyboard::Scancode::Z) == 'z');
    KTEST_ASSERT(keyboard::to_char(keyboard::Scancode::Backslash) == '\\');
    KTEST_ASSERT(keyboard::to_char(keyboard::Scancode::Apostrophe) == '\'');
    KTEST_ASSERT(keyboard::to_char(keyboard::Scancode::Enter) == '\n');
    KTEST_ASSERT(keyboard::to_char(keyboard::Scancode::LeftShift) == '?');
}

KTEST(keyboard_queues_events_in_order)
{
    while (keyboard::poll())
    {
    }

    keyboard::on_irq(0x1E);
    keyboard::on_irq(0x9E);

    const auto press = keyboard::poll();
    KTEST_ASSERT(press && press->key == keyboard::Scancode::A && press->pressed);
    const auto release = keyboard::poll();
    KTEST_ASSERT(release && release->key == keyboard::Scancode::A && !release->pressed);
    KTEST_ASSERT(!keyboard::poll());
}

KTEST(keyboard_keeps_queued_events_when_full)
{
    while (keyboard::poll())
    {
    }

    keyboard::on_irq(0x10);
    for (auto i = 0; i < 20; i++)
        keyboard::on_irq(0x1E);

    const auto first = keyboard::poll();
    KTEST_ASSERT(first && first->key == keyboard::Scancode::Q);
    while (keyboard::poll())
    {
    }
}
