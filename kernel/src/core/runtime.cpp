#include "arch/x86_64/cpu.h"
#include "core/panic.h"
#include <cstddef>
#include <cstdint>

namespace std
{
// Called by libstdc++'s precondition checks (e.g. std::array index out of
// range); GCC enables these in unoptimized builds.
auto __glibcxx_assert_fail(const char* file, int line, const char* function, const char* condition) noexcept -> void
{
    kernel::panic("Assert fail {} {} {} {}", file, line, function, condition);
    // print file, line, function and condition with your serial output here
    arch::halt_forever();
}

[[noreturn]] auto terminate() -> void
{
    kernel::panic("Terminated");
}

[[noreturn]] auto __cxa_pure_virtual() -> void
{
    kernel::panic("Pure virtual");
}
}

// GCC may emit calls to these even in freestanding code, so they need C linkage.
extern "C"
{
auto strlen(const char* s) -> std::size_t
{
    auto len = std::size_t{};
    while (*s != '\0')
    {
        ++len;
        ++s;
    }

    return len;
}
auto memcpy(void* __restrict dest, const void* __restrict src, std::size_t n) -> void*
{
    auto d = static_cast<uint8_t*>(dest);
    auto s = static_cast<const uint8_t*>(src);
    for (std::size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

auto memset(void* s, int c, std::size_t n) -> void*
{
    auto p = static_cast<uint8_t*>(s);
    for (std::size_t i = 0; i < n; i++)
        p[i] = (uint8_t)c;
    return s;
}

auto memmove(void* dest, const void* src, std::size_t n) -> void*
{
    auto d = static_cast<uint8_t*>(dest);
    auto s = static_cast<const uint8_t*>(src);
    if (s > d)
    {
        for (std::size_t i = 0; i < n; i++)
            d[i] = s[i];
    }
    else
    {
        for (std::size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dest;
}

auto memcmp(const void* a, const void* b, std::size_t n) -> int
{
    auto p = static_cast<const uint8_t*>(a);
    auto q = static_cast<const uint8_t*>(b);
    for (std::size_t i = 0; i < n; i++)
    {
        if (p[i] != q[i])
            return p[i] < q[i] ? -1 : 1;
    }
    return 0;
}
}
