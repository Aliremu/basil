#include "libk/frame_bitmap.h"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <set>

namespace
{
using libk::FrameBitmap;

[[nodiscard]] constexpr auto page(uint64_t n) -> uint64_t
{
    return n * FrameBitmap::frame_size;
}

class FrameBitmapTest : public testing::Test
{
  protected:
    // Not a multiple of 8, so the last storage byte is only partly used.
    static constexpr auto frames = 20uz;
    static constexpr auto storage_bytes = FrameBitmap::bytes_needed(frames);

    // One extra byte past the bitmap catches writes out of bounds.
    std::array<uint8_t, storage_bytes + 1> storage_{};
    FrameBitmap bitmap_{storage_.data(), frames};

    [[nodiscard]] auto guard() const -> uint8_t
    {
        return storage_[storage_bytes];
    }
};
}

TEST(FrameBitmap, BytesNeededRoundsUp)
{
    EXPECT_EQ(FrameBitmap::bytes_needed(0), 0);
    EXPECT_EQ(FrameBitmap::bytes_needed(1), 1);
    EXPECT_EQ(FrameBitmap::bytes_needed(8), 1);
    EXPECT_EQ(FrameBitmap::bytes_needed(9), 2);
}

TEST_F(FrameBitmapTest, StartsWithNothingFree)
{
    EXPECT_EQ(bitmap_.free_count(), 0);
}

TEST_F(FrameBitmapTest, FreeRangeFreesWholeFrames)
{
    bitmap_.free_range({page(2), page(3)});
    EXPECT_EQ(bitmap_.free_count(), 3);
}

TEST_F(FrameBitmapTest, FreeRangeSkipsPartialFrames)
{
    // Covers the tail of frame 2, frames 3 and 4, and the head of frame 5.
    bitmap_.free_range({page(2) + 1, page(3)});
    EXPECT_EQ(bitmap_.free_count(), 2);
}

TEST_F(FrameBitmapTest, FreeRangeInsideOneFrameFreesNothing)
{
    bitmap_.free_range({page(2) + 0x800, 0x400});
    EXPECT_EQ(bitmap_.free_count(), 0);
}

TEST_F(FrameBitmapTest, FreeRangeAcrossAFrameBoundaryFreesNothing)
{
    bitmap_.free_range({page(2) + 0x800, 0x1000});
    EXPECT_EQ(bitmap_.free_count(), 0);
}

TEST_F(FrameBitmapTest, ReserveRangeRemovesFrames)
{
    bitmap_.free_range({page(0), page(8)});
    bitmap_.reserve_range({page(2), page(2)});
    EXPECT_EQ(bitmap_.free_count(), 6);
}

TEST_F(FrameBitmapTest, ReservingAReservedFrameChangesNothing)
{
    bitmap_.reserve_range({page(3), page(1)});
    EXPECT_EQ(bitmap_.free_count(), 0);
}

TEST_F(FrameBitmapTest, AllocReturnsTheFreeFrame)
{
    bitmap_.free_range({page(5), page(1)});
    EXPECT_EQ(bitmap_.alloc(), page(5));
    EXPECT_EQ(bitmap_.free_count(), 0);
}

TEST_F(FrameBitmapTest, AllocNeverReturnsAFrameTwice)
{
    bitmap_.free_range({page(3), page(4)});

    auto allocated = std::set<uint64_t>{};
    for (auto i = 0; i < 4; ++i)
        allocated.insert(*bitmap_.alloc());

    EXPECT_EQ(allocated, (std::set<uint64_t>{page(3), page(4), page(5), page(6)}));
    EXPECT_EQ(bitmap_.free_count(), 0);
}

TEST_F(FrameBitmapTest, AllocFailsWhenNothingIsFree)
{
    EXPECT_EQ(bitmap_.alloc(), std::nullopt);
}

TEST_F(FrameBitmapTest, AllocFailsOnceExhausted)
{
    bitmap_.free_range({page(4), page(1)});
    EXPECT_EQ(bitmap_.alloc(), page(4));
    EXPECT_EQ(bitmap_.alloc(), std::nullopt);
}

TEST_F(FrameBitmapTest, AllocFindsFramesInThePartialLastByte)
{
    bitmap_.free_range({page(17), page(1)});
    EXPECT_EQ(bitmap_.alloc(), page(17));
}

TEST_F(FrameBitmapTest, FreedFrameCanBeAllocatedAgain)
{
    bitmap_.free_range({page(9), page(1)});
    const auto frame = bitmap_.alloc();
    bitmap_.free(*frame);
    EXPECT_EQ(bitmap_.free_count(), 1);
    EXPECT_EQ(bitmap_.alloc(), frame);
}

TEST_F(FrameBitmapTest, FreeIgnoresUnalignedAddresses)
{
    bitmap_.free(page(3) + 1);
    EXPECT_EQ(bitmap_.free_count(), 0);
}

TEST_F(FrameBitmapTest, DoubleFreeCountsOnce)
{
    bitmap_.free(page(3));
    bitmap_.free(page(3));
    EXPECT_EQ(bitmap_.free_count(), 1);
}

TEST_F(FrameBitmapTest, FreeIgnoresFramesPastTheEnd)
{
    bitmap_.free(page(frames));
    bitmap_.free(page(storage_bytes * 8));
    EXPECT_EQ(bitmap_.free_count(), 0);
    EXPECT_EQ(guard(), 0);
}

TEST_F(FrameBitmapTest, FreeRangeStopsAtTheEnd)
{
    bitmap_.free_range({page(16), page(16)});
    EXPECT_EQ(bitmap_.free_count(), frames - 16);
    EXPECT_EQ(guard(), 0);
}
