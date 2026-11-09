#include "libk/frame_bitmap.h"
#include <algorithm>
#include <cstdint>

namespace libk
{
FrameBitmap::FrameBitmap(uint8_t* storage, uint64_t frame_count)
    : storage_{storage}
    , frame_count_{frame_count}
    , free_count_{0}
    , next_free_{0}
{
}

auto FrameBitmap::init(uint8_t* storage, uint64_t frame_count) -> void
{
    storage_ = storage;
    frame_count_ = frame_count;
    free_count_ = 0;
    next_free_ = 0;
}

auto FrameBitmap::free_range(PhysRange r) -> void
{
    const auto first = (r.base + frame_size - 1) & ~(frame_size - 1);
    const auto last = (r.base + r.length) & ~(frame_size - 1);

    if (last <= first)
        return;

    const auto base = first / frame_size;
    const auto length = (last - first) / frame_size;

    for (auto frame = base; frame < std::min(base + length, frame_count_); ++frame)
    {
        free_frame(frame);
    }
}

auto FrameBitmap::reserve_range(PhysRange r) -> void
{
    const auto first = r.base & ~(frame_size - 1);
    const auto last = (r.base + r.length + frame_size - 1) & ~(frame_size - 1);

    if (last <= first)
        return;

    const auto base = first / frame_size;
    const auto length = (last - first) / frame_size;

    for (auto frame = base; frame < std::min(base + length, frame_count_); ++frame)
    {
        reserve_frame(frame);
    }
}

auto FrameBitmap::alloc() -> std::optional<uint64_t>
{
    if (free_count_ == 0)
        return std::nullopt;

    for (auto i = 0uz; i < bytes_needed(frame_count_); ++i)
    {
        const auto byte = storage_[i];
        if (byte == 0)
            continue;

        const auto idx = __builtin_ctz(byte);
        const auto frame = i * 8 + idx;
        reserve_frame(frame);
        return frame * frame_size;
    }

    return std::nullopt;
}

auto FrameBitmap::free(uint64_t phys) -> void
{
    if (phys % frame_size != 0)
        return;

    free_frame(phys / frame_size);
}

auto FrameBitmap::free_count() const -> uint64_t
{
    return free_count_;
}

auto FrameBitmap::free_frame(uint64_t frame) -> void
{
    if (frame >= frame_count_)
        return;

    const auto byte = frame / 8;
    const auto bit = frame % 8;
    auto* ptr = storage_ + byte;

    if ((*ptr & (1 << bit)))
        return;

    ++free_count_;
    *ptr |= (1 << bit);
}

auto FrameBitmap::reserve_frame(uint64_t frame) -> void
{
    if (frame >= frame_count_)
        return;

    const auto byte = frame / 8;
    const auto bit = frame % 8;
    auto* ptr = storage_ + byte;

    if ((*ptr & (1 << bit)) == 0)
        return;

    --free_count_;
    *ptr &= ~(1 << bit);
}
}
