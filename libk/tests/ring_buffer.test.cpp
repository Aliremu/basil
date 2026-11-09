#include "libk/ring_buffer.h"
#include <gtest/gtest.h>

namespace
{
// One slot stays empty to tell full from empty, so this holds 3 items.
using Buffer = libk::RingBuffer<int, 4>;
}

TEST(RingBuffer, StartsEmpty)
{
    auto buffer = Buffer{};
    EXPECT_FALSE(buffer.pop_front());
}

TEST(RingBuffer, PopsInPushOrder)
{
    auto buffer = Buffer{};
    buffer.push_back(1);
    buffer.push_back(2);
    buffer.push_back(3);
    EXPECT_EQ(buffer.pop_front(), 1);
    EXPECT_EQ(buffer.pop_front(), 2);
    EXPECT_EQ(buffer.pop_front(), 3);
    EXPECT_FALSE(buffer.pop_front());
}

TEST(RingBuffer, DropsNewItemsWhenFull)
{
    auto buffer = Buffer{};
    for (auto value : {1, 2, 3, 4, 5, 6})
        buffer.push_back(value);
    EXPECT_EQ(buffer.pop_front(), 1);
    EXPECT_EQ(buffer.pop_front(), 2);
    EXPECT_EQ(buffer.pop_front(), 3);
    EXPECT_FALSE(buffer.pop_front());
}

TEST(RingBuffer, AcceptsItemsAfterAPopFromFull)
{
    auto buffer = Buffer{};
    for (auto value : {1, 2, 3})
        buffer.push_back(value);
    EXPECT_EQ(buffer.pop_front(), 1);
    buffer.push_back(4);
    EXPECT_EQ(buffer.pop_front(), 2);
    EXPECT_EQ(buffer.pop_front(), 3);
    EXPECT_EQ(buffer.pop_front(), 4);
    EXPECT_FALSE(buffer.pop_front());
}

TEST(RingBuffer, WrapsAround)
{
    auto buffer = Buffer{};
    for (auto value = 0; value < 20; ++value)
    {
        buffer.push_back(value);
        EXPECT_EQ(buffer.pop_front(), value);
        EXPECT_FALSE(buffer.pop_front());
    }
}
