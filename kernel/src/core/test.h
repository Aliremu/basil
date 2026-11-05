#pragma once

// In-kernel tests, the C++ take on Rust's custom_test_frameworks: KTEST puts
// a {name, function} record in the .kernel_tests section, and run_all() walks
// the section between the linker script's start/end symbols. Only the `test`
// profile (KERNEL_TESTS) registers tests; other builds compile each body and
// discard it, so tests can sit next to the code they check.

#include "core/panic.h"

namespace kernel::test
{
struct Case
{
    const char* name;
    void (*run)();
};

// Values for QEMU's isa-debug-exit device; QEMU exits with (code << 1) | 1,
// so success is 33 and failure 35 (`make test` checks for 33).
enum class ExitCode : uint8_t
{
    Success = 0x10,
    Failure = 0x11,
};

[[noreturn]] auto exit_qemu(ExitCode code) -> void;

// Ends the run as failed; panic() calls this in test builds.
[[noreturn]] auto fail() -> void;

// Runs every registered test, then exits QEMU.
[[noreturn]] auto run_all() -> void;
}

#ifdef KERNEL_TESTS
#define KTEST_REGISTER(fn)                                                                                             \
    [[gnu::used, gnu::section(".kernel_tests")]] static const kernel::test::Case fn##_case{#fn, fn};
#else
#define KTEST_REGISTER(fn)
#endif

#define KTEST(fn)                                                                                                      \
    [[maybe_unused]] static auto fn() -> void;                                                                         \
    KTEST_REGISTER(fn)                                                                                                 \
    static auto fn() -> void

// Fails the running test (or panics outside one) when cond is false.
#define KTEST_ASSERT(cond)                                                                                             \
    ((cond) ? void(0) : kernel::panic("assertion failed: {} ({}:{})\n", #cond, __FILE__, static_cast<int>(__LINE__)))
