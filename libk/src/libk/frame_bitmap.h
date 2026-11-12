#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace libk
{

struct PhysRange
{
    uint64_t base;
    uint64_t length;
};

class FrameBitmap
{
  public:
    static constexpr uint64_t frame_size = 4096;
    static constexpr auto bytes_needed(uint64_t frames) -> std::size_t
    {
        return (frames + 7) / 8;
    }

    constexpr FrameBitmap() = default;
    FrameBitmap(uint8_t* storage, uint64_t frame_count);

    auto init(uint8_t* storage, uint64_t frame_count) -> void;

    auto free_range(PhysRange r) -> void;
    auto reserve_range(PhysRange r) -> void;

    [[nodiscard]] auto alloc() -> std::optional<uint64_t>;
    [[nodiscard]] auto alloc_contiguous(uint64_t n) -> std::optional<uint64_t>;
    auto free(uint64_t phys) -> void;
    [[nodiscard]] auto free_count() const -> uint64_t;

  private:
    uint8_t* storage_{};
    uint64_t frame_count_{};
    uint64_t free_count_{};
    uint64_t next_free_{};

    auto free_frame(uint64_t frame) -> void;
    auto reserve_frame(uint64_t frame) -> void;
};

inline FrameBitmap::FrameBitmap(uint8_t* storage, uint64_t frame_count)
    : storage_{storage}
    , frame_count_{frame_count}
    , free_count_{0}
    , next_free_{0}
{
}

inline auto FrameBitmap::init(uint8_t* storage, uint64_t frame_count) -> void
{
    storage_ = storage;
    frame_count_ = frame_count;
    free_count_ = 0;
    next_free_ = 0;
}

inline auto FrameBitmap::free_range(PhysRange r) -> void
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

inline auto FrameBitmap::reserve_range(PhysRange r) -> void
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

inline auto FrameBitmap::alloc() -> std::optional<uint64_t>
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

inline auto FrameBitmap::free(uint64_t phys) -> void
{
    if (phys % frame_size != 0)
        return;

    free_frame(phys / frame_size);
}

inline auto FrameBitmap::free_count() const -> uint64_t
{
    return free_count_;
}

inline auto FrameBitmap::free_frame(uint64_t frame) -> void
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

inline auto FrameBitmap::reserve_frame(uint64_t frame) -> void
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
