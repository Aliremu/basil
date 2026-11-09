// Example kernel tests; they run inside QEMU via `make test-kernel`. Tests
// can also live next to the code they check: include "core/test.h" there.

#include "core/test.h"

KTEST(trivial_assertion)
{
    KTEST_ASSERT(1 == 1);
}

KTEST(statics_are_zeroed)
{
    // Limine zeroes .bss when it loads the kernel.
    static int counter;
    KTEST_ASSERT(counter == 0);
}

KTEST(gdt_segments_loaded)
{
    auto cs = uint16_t{};
    auto ss = uint16_t{};
    __asm__("mov %%cs, %0" : "=r"(cs));
    __asm__("mov %%ss, %0" : "=r"(ss));
    KTEST_ASSERT(cs == 0x08);
    KTEST_ASSERT(ss == 0x10);
}

KTEST(breakpoint_exception)
{
    __asm__("int3");
}
