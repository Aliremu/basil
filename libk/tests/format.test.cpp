#include <libk/format.h>
#include <print>
#include <source_location>
#include <string>

namespace
{
auto failures = 0;

// Formats value with libk's Formatter, capturing the output.
template <typename T>
auto format(const T& value) -> std::string
{
    auto out = std::string{};
    auto write = [&out](char c) { out.push_back(c); };
    libk::Formatter<std::decay_t<const T&>>::format(value, write);
    return out;
}

auto check_eq(
    const std::string& got,
    const std::string& want,
    std::source_location where = std::source_location::current()) -> void
{
    if (got == want)
        return;
    std::println("{}:{}: got \"{}\", want \"{}\"", where.file_name(), where.line(), got, want);
    ++failures;
}
}

int main()
{
    check_eq(format("hello"), "hello");
    check_eq(format(""), "");

    check_eq(format(7), "7");
    check_eq(format(42), "42");
    check_eq(format(218204718), "218204718");
    check_eq(format(100), "100");
    check_eq(format(0), "0");
    check_eq(format(-5), "-5");
    check_eq(format(1000000009), "1000000009");

    if (failures != 0)
        std::println("{} check(s) failed", failures);
    return failures == 0 ? 0 : 1;
}
