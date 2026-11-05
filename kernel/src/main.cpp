#include "arch/x86_64/cpu.h"
#include "arch/x86_64/pit.h"
#include "boot/limine.h"
#include "core/basil.h"
#include "core/print.h"
#include "core/test.h"
#include "drivers/keyboard/keyboard.h"
#include "gfx/fonts/font8x8.h"
#include "gfx/framebuffer.h"
#include <stdbool.h>
#include <stddef.h>

// The ENTRY symbol in linker.ld; C linkage keeps the name unmangled.
extern "C" auto kmain() -> void
{
    if (!boot::base_revision_supported())
        arch::halt_forever();

    basil::init();

    gfx::clear(0x05, 0x05, 0x05, 0xff);
    kernel::kprintf("Welcome to BasilOS!\n");
    kernel::kprintf("basil: framebuffer painted\n");
    gfx::present();

#ifdef KERNEL_TESTS
    kernel::test::run_all();
#endif

    // auto last = uint64_t{};
    for (;;)
    {
        arch::wait_for_interrupt();
        while (const auto event = keyboard::poll())
        {
            if (event->pressed)
                kernel::kprintf("{}", keyboard::to_char(event->key));
        }

        gfx::present();
        // const auto now = arch::uptime_ticks();
        // if (now != last)
        // {
        //     gfx::demo_frame(now - last);
        //     gfx::present();
        //     last = now;
        // }
    }
}
