#include <libk/ring_buffer.h>
#include <print>
#include <source_location>

namespace
{
// One slot stays empty to tell full from empty, so this holds 3 items.
using Buffer = libk::RingBuffer<int, 4>;

auto failures = 0;

auto check(bool ok, std::source_location where = std::source_location::current()) -> void
{
    if (ok)
        return;
    std::println("{}:{}: check failed", where.file_name(), where.line());
    ++failures;
}

auto starts_empty() -> void
{
    auto buffer = Buffer{};
    check(!buffer.pop_front());
}

auto pops_in_push_order() -> void
{
    auto buffer = Buffer{};
    buffer.push_back(1);
    buffer.push_back(2);
    buffer.push_back(3);
    check(buffer.pop_front() == 1);
    check(buffer.pop_front() == 2);
    check(buffer.pop_front() == 3);
    check(!buffer.pop_front());
}

auto drops_new_items_when_full() -> void
{
    auto buffer = Buffer{};
    for (auto value : {1, 2, 3, 4, 5, 6})
        buffer.push_back(value);
    check(buffer.pop_front() == 1);
    check(buffer.pop_front() == 2);
    check(buffer.pop_front() == 3);
    check(!buffer.pop_front());
}

auto accepts_items_after_a_pop_from_full() -> void
{
    auto buffer = Buffer{};
    for (auto value : {1, 2, 3})
        buffer.push_back(value);
    check(buffer.pop_front() == 1);
    buffer.push_back(4);
    check(buffer.pop_front() == 2);
    check(buffer.pop_front() == 3);
    check(buffer.pop_front() == 4);
    check(!buffer.pop_front());
}

auto wraps_around() -> void
{
    auto buffer = Buffer{};
    for (auto value = 0; value < 20; value++)
    {
        buffer.push_back(value);
        check(buffer.pop_front() == value);
        check(!buffer.pop_front());
    }
}
}

int main()
{
    starts_empty();
    pops_in_push_order();
    drops_new_items_when_full();
    accepts_items_after_a_pop_from_full();
    wraps_around();

    if (failures != 0)
        std::println("{} check(s) failed", failures);
    return failures == 0 ? 0 : 1;
}
