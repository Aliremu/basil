#pragma once

#include "arch/x86_64/cpu.h"
#include "core/print.h"

namespace kernel
{
#ifdef KERNEL_TESTS
namespace test
{
[[noreturn]] auto fail() -> void; // test.cpp
}
#endif

template <libk::Formattable... Args>
[[noreturn]] auto panic(libk::FormatString<std::type_identity_t<Args>...> fmt, const Args&... args) -> void
{
    kprintf("*** basil panic: ");
    kprintf(fmt, args...);
#ifdef KERNEL_TESTS
    // A panic during the test run is a failed test.
    test::fail();
#endif
    arch::halt_forever();
}
}
