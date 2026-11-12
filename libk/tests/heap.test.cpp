#include "libk/heap.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <random>
#include <vector>

namespace
{
using libk::Heap;

// Every allocation carries a 16-byte header in front of it.
constexpr auto header = 16uz;

[[nodiscard]] auto address(const void* p) -> uintptr_t
{
    return reinterpret_cast<uintptr_t>(p);
}

class HeapTest : public testing::Test
{
  protected:
    static constexpr auto region = 4096uz;

    alignas(4096) std::array<std::byte, 16 * region> buffer_{};
    Heap heap_;

    auto add(std::size_t offset, std::size_t size) -> void
    {
        heap_.add_region(buffer_.data() + offset, size);
    }

    [[nodiscard]] auto inside(const void* p, std::size_t size) const -> bool
    {
        return address(p) >= address(buffer_.data()) && address(p) + size <= address(buffer_.data() + buffer_.size());
    }
};
}

TEST(Heap, AllocWithoutRegionsFails)
{
    auto heap = Heap{};
    EXPECT_EQ(heap.alloc(16, 16), nullptr);
}

TEST_F(HeapTest, AllocationsDoNotOverlap)
{
    add(0, region);
    auto* a = static_cast<std::byte*>(heap_.alloc(100, 16));
    auto* b = static_cast<std::byte*>(heap_.alloc(100, 16));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(a + 100 <= b || b + 100 <= a);
    EXPECT_TRUE(inside(a, 100));
    EXPECT_TRUE(inside(b, 100));
}

TEST_F(HeapTest, AllocationsKeepTheirContents)
{
    add(0, region);
    auto blocks = std::vector<std::byte*>{};
    for (auto i = 0; i < 8; ++i)
    {
        auto* p = static_cast<std::byte*>(heap_.alloc(64, 16));
        ASSERT_NE(p, nullptr);
        std::fill_n(p, 64, std::byte(i));
        blocks.push_back(p);
    }

    for (auto i = 0; i < 8; ++i)
        EXPECT_TRUE(std::all_of(blocks[i], blocks[i] + 64, [i](std::byte b) { return b == std::byte(i); }));
}

TEST_F(HeapTest, AllocationsAreAtLeastSixteenByteAligned)
{
    add(0, region);
    for (auto size : {1uz, 7uz, 17uz, 33uz})
        EXPECT_EQ(address(heap_.alloc(size, 1)) % 16, 0) << "size " << size;
}

TEST_F(HeapTest, HonoursLargeAlignments)
{
    add(0, 8 * region);
    for (auto align : {32uz, 64uz, 256uz, 4096uz})
    {
        auto* p = heap_.alloc(24, align);
        ASSERT_NE(p, nullptr) << "align " << align;
        EXPECT_EQ(address(p) % align, 0) << "align " << align;
    }
}

TEST_F(HeapTest, ZeroSizeAllocationsAreDistinct)
{
    add(0, region);
    auto* a = heap_.alloc(0, 16);
    auto* b = heap_.alloc(0, 16);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);
}

TEST_F(HeapTest, AllocFailsWhenTooBig)
{
    add(0, region);
    EXPECT_EQ(heap_.alloc(region, 16), nullptr);
}

TEST_F(HeapTest, WholeRegionCanBeAllocatedOnce)
{
    add(0, region);
    EXPECT_NE(heap_.alloc(region - header, 16), nullptr);
    EXPECT_EQ(heap_.alloc(1, 16), nullptr);
}

TEST_F(HeapTest, FreeNullDoesNothing)
{
    add(0, region);
    heap_.free(nullptr);
    EXPECT_NE(heap_.alloc(region - header, 16), nullptr);
}

TEST_F(HeapTest, FreedMemoryIsReused)
{
    add(0, region);
    auto* p = heap_.alloc(region - header, 16);
    ASSERT_NE(p, nullptr);
    heap_.free(p);
    EXPECT_EQ(heap_.alloc(region - header, 16), p);
}

TEST_F(HeapTest, FreeingInAllocationOrderCoalesces)
{
    add(0, region);
    auto blocks = std::vector<void*>{};
    while (auto* p = heap_.alloc(48, 16))
        blocks.push_back(p);
    ASSERT_GT(blocks.size(), 2uz);

    for (auto* p : blocks)
        heap_.free(p);
    EXPECT_NE(heap_.alloc(region - header, 16), nullptr);
}

TEST_F(HeapTest, FreeingInReverseOrderCoalesces)
{
    add(0, region);
    auto blocks = std::vector<void*>{};
    while (auto* p = heap_.alloc(48, 16))
        blocks.push_back(p);

    std::ranges::reverse(blocks);
    for (auto* p : blocks)
        heap_.free(p);
    EXPECT_NE(heap_.alloc(region - header, 16), nullptr);
}

TEST_F(HeapTest, FreeingInRandomOrderCoalesces)
{
    add(0, 16 * region);
    auto rng = std::mt19937{1};
    auto blocks = std::vector<void*>{};
    for (auto i = 0; i < 200; ++i)
        if (auto* p = heap_.alloc(rng() % 300 + 1, 16uz << (rng() % 4)))
            blocks.push_back(p);
    ASSERT_GT(blocks.size(), 100uz);

    std::ranges::shuffle(blocks, rng);
    for (auto* p : blocks)
        heap_.free(p);
    EXPECT_NE(heap_.alloc(16 * region - header, 16), nullptr);
}

TEST_F(HeapTest, FreeingAFullHeapsMiddleBlockLeavesAHole)
{
    add(0, region);
    constexpr auto quarter = region / 4 - header;
    auto blocks = std::array<void*, 4>{};
    for (auto& p : blocks)
    {
        p = heap_.alloc(quarter, 16);
        ASSERT_NE(p, nullptr);
    }
    ASSERT_EQ(heap_.alloc(1, 16), nullptr);

    heap_.free(blocks[1]);
    EXPECT_EQ(heap_.alloc(quarter + 1, 16), nullptr);
    EXPECT_EQ(heap_.alloc(quarter, 16), blocks[1]);
}

TEST_F(HeapTest, AdjacentRegionsMerge)
{
    add(0, region / 2);
    add(region / 2, region / 2);
    EXPECT_NE(heap_.alloc(region - header, 16), nullptr);
}

TEST_F(HeapTest, AdjacentRegionsMergeWhenAddedBackwards)
{
    add(region / 2, region / 2);
    add(0, region / 2);
    EXPECT_NE(heap_.alloc(region - header, 16), nullptr);
}

TEST_F(HeapTest, SeparateRegionsStaySeparate)
{
    add(0, region);
    add(2 * region, region);
    EXPECT_EQ(heap_.alloc(region, 16), nullptr);
    EXPECT_NE(heap_.alloc(region - header, 16), nullptr);
    EXPECT_NE(heap_.alloc(region - header, 16), nullptr);
}
