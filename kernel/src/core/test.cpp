#include "core/test.h"
#include "arch/x86_64/io.h"

// Bounds of the .kernel_tests section, defined in linker.ld.
extern "C" const kernel::test::Case __kernel_tests_start[];
extern "C" const kernel::test::Case __kernel_tests_end[];

namespace kernel::test
{
auto exit_qemu(ExitCode code) -> void
{
    static constexpr auto debug_exit_port = 0xF4; // matches -device isa-debug-exit,iobase=0xf4
    arch::outb(debug_exit_port, static_cast<uint8_t>(code));
    arch::halt_forever();
}

auto fail() -> void
{
    kprintf("test result: FAILED\n");
    exit_qemu(ExitCode::Failure);
}

auto run_all() -> void
{
    const auto count = static_cast<int>(__kernel_tests_end - __kernel_tests_start);
    kprintf("running {} kernel tests\n", count);

    for (const auto* test = __kernel_tests_start; test != __kernel_tests_end; ++test)
    {
        kprintf("test {} ... ", test->name);
        test->run(); // a failing test panics, and panic() calls fail()
        kprintf("ok\n");
    }

    kprintf("test result: ok. {} passed\n", count);
    exit_qemu(ExitCode::Success);
}
}
