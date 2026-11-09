#include "libk/format.h"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace
{
enum class Color
{
    Red,
    Green,
};

struct Point
{
    int x;
    int y;
};

struct Named
{
    char label[8];
    const char* text;
    std::array<int, 2> values;
    Color color;
};

template <typename T>
struct Box
{
    T value;
};

template <libk::Formattable... Args>
[[nodiscard]] auto render(libk::FormatString<std::type_identity_t<Args>...> fmt, const Args&... args) -> std::string
{
    auto out = std::string{};
    auto write = [&out](char c) { out.push_back(c); };
    libk::format_to(write, fmt, args...);
    return out;
}
}

TEST(Format, Literals)
{
    EXPECT_EQ(render(""), "");
    EXPECT_EQ(render("hello"), "hello");
    EXPECT_EQ(render("{{}}"), "{}");
    EXPECT_EQ(render("{{{}}}", 1), "{1}");
    EXPECT_EQ(render("a{}b{}c", 1, 2), "a1b2c");
}

TEST(Format, Strings)
{
    char mutable_text[] = "mutable";
    auto* mutable_pointer = mutable_text;
    const auto* pointer = "pointer";

    EXPECT_EQ(render("{}", "hello"), "hello");
    EXPECT_EQ(render("{}", ""), "");
    EXPECT_EQ(render("{}", pointer), "pointer");
    EXPECT_EQ(render("{}", mutable_pointer), "mutable");
    EXPECT_EQ(render("{}", std::string_view{"view"}), "view");
    EXPECT_EQ(render("{}", 'c'), "c");
    EXPECT_EQ(render("{:5}|", "ab"), "ab   |");
    EXPECT_EQ(render("{:3}|", 'c'), "c  |");
}

TEST(Format, Bools)
{
    EXPECT_EQ(render("{}", true), "true");
    EXPECT_EQ(render("{}", false), "false");
}

TEST(Format, Decimal)
{
    EXPECT_EQ(render("{}", 0), "0");
    EXPECT_EQ(render("{}", 7), "7");
    EXPECT_EQ(render("{}", 100), "100");
    EXPECT_EQ(render("{}", 218204718), "218204718");
    EXPECT_EQ(render("{}", -5), "-5");
    EXPECT_EQ(render("{:d}", 42), "42");
    EXPECT_EQ(render("{}", uint8_t{255}), "255");
    EXPECT_EQ(render("{}", int8_t{-128}), "-128");
    EXPECT_EQ(render("{}", std::numeric_limits<uint64_t>::max()), "18446744073709551615");
    EXPECT_EQ(render("{}", std::numeric_limits<int64_t>::min()), "-9223372036854775808");
}

TEST(Format, OtherBases)
{
    EXPECT_EQ(render("{:x}", 255), "ff");
    EXPECT_EQ(render("{:X}", 255), "FF");
    EXPECT_EQ(render("{:#x}", 255), "0xff");
    EXPECT_EQ(render("{:#X}", 255), "0XFF");
    EXPECT_EQ(render("{:b}", 5), "101");
    EXPECT_EQ(render("{:#b}", 5), "0b101");
    EXPECT_EQ(render("{:o}", 8), "10");
    EXPECT_EQ(render("{:#o}", 8), "010");
    EXPECT_EQ(render("{:x}", -255), "-ff");
    EXPECT_EQ(render("{:b}", std::numeric_limits<uint64_t>::max()), std::string(64, '1'));
}

TEST(Format, PadsNumbers)
{
    EXPECT_EQ(render("{:>5}", 42), "   42");
    EXPECT_EQ(render("{:05}", 42), "00042");
    EXPECT_EQ(render("{:05}", -42), "-0042");
    EXPECT_EQ(render("{:#010x}", 0xBEEF), "0x0000beef");
    EXPECT_EQ(render("{:1}", 12345), "12345");
}

TEST(Format, Pointers)
{
    EXPECT_EQ(render("{}", reinterpret_cast<void*>(0x1000)), "0x1000");
    EXPECT_EQ(render("{:018}", reinterpret_cast<const int*>(0x1000)), "0x0000000000001000");
    EXPECT_EQ(render("{}", nullptr), "0x0");
}

TEST(Format, Enums)
{
    EXPECT_EQ(render("{}", Color::Green), "Green");
    EXPECT_EQ(render("{:d}", Color::Green), "1");
    EXPECT_EQ(render("{}", static_cast<Color>(9)), "9");
}

TEST(Format, Ranges)
{
    const int values[] = {1, 2, 3};

    EXPECT_EQ(render("{}", values), "[1, 2, 3]");
    EXPECT_EQ(render("{}", std::array{1, 2, 3}), "[1, 2, 3]");
    EXPECT_EQ(render("{}", std::span<const int>{values}), "[1, 2, 3]");
    EXPECT_EQ(render("{:#x}", std::array{10, 255}), "[0xa, 0xff]");
    EXPECT_EQ(render("{}", std::array<int, 0>{}), "[]");
}

TEST(Format, Aggregates)
{
    EXPECT_EQ(render("{}", Point{1, 2}), "Point\n{\nx: 1,\ny: 2\n}");
    EXPECT_EQ(render("{:x}", Point{10, 255}), "Point\n{\nx: a,\ny: ff\n}");
    EXPECT_EQ(render("{}", Box<int>{5}), "{\nvalue: 5\n}");
    EXPECT_EQ(
        render("{}", Named{"label", "text", {1, 2}, Color::Red}),
        "Named\n{\nlabel: label,\ntext: text,\nvalues: [1, 2],\ncolor: Red\n}");
}
