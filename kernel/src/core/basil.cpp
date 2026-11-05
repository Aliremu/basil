#include "basil.h"
#include "arch/x86_64/cpu.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/io.h"
#include "arch/x86_64/pic.h"
#include "arch/x86_64/pit.h"
#include "boot/limine.h"
#include "core/print.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/serial/serial.h"
#include "gfx/framebuffer.h"

namespace basil
{
auto init() -> void
{
    if (!serial::init())
        arch::halt_forever();

    const auto fb = boot::framebuffer();
    if (!fb)
    {
        kernel::kprintf("basil: no framebuffer\n");
        arch::halt_forever();
    }
    gfx::init(*fb);

    kernel::add_sink(serial::put);
    kernel::add_sink(gfx::put);

    arch::init_gdt();
    arch::init_idt();
    arch::init_pic();
    arch::init_pit(100);

    arch::set_irq_handler(1, [](arch::InterruptStackFrame&) { keyboard::on_irq(arch::inb(0x60)); });
    // drain bytes left from boot
    while (arch::inb(0x64) & 1)
        arch::inb(0x60);

    arch::unmask(1);

    __asm__ volatile("sti");
}
}
