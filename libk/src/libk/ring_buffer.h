#pragma once

#include <array>
#include <atomic>
#include <optional>

namespace libk
{
template <typename T, std::size_t N>
struct RingBuffer
{
    std::array<T, N> _data;
    std::atomic<std::size_t> _head;
    std::atomic<std::size_t> _tail;

    auto push_back(T value) -> void
    {
        auto idx = _tail.load();
        auto next = (idx + 1) % N;
        if (next == _head.load())
            return;

        _data[idx] = std::move(value);
        _tail.store((idx + 1) % N);
    }

    auto pop_front() -> std::optional<T>
    {
        auto idx = _head.load();
        if (idx == _tail.load())
            return std::nullopt;

        auto val = std::move(_data[idx]);
        _head.store((idx + 1) % N);
        return std::move(val);
    }
};
}
