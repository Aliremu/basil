#pragma once

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

}
