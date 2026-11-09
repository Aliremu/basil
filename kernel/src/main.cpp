#include "arch/x86_64/cpu.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/page_table.h"
#include "arch/x86_64/pic.h"
#include "arch/x86_64/pit.h"
#include "boot/limine.h"
#include "core/log.h"
#include "core/panic.h"
#include "core/print.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/serial/serial.h"
#include "gfx/console.h"
#include "gfx/fonts/font8x8.h"
#include "gfx/framebuffer.h"
#include "mm/frames.h"
#include "mm/paging.h"
#include "shell.h"
#include <algorithm>
#include <functional>

#ifdef KERNEL_TESTS
#include "core/test.h"
#endif

// The ENTRY symbol in linker.ld; C linkage keeps the name unmangled.
extern "C" auto kmain() -> void
{
    if (!boot::base_revision_supported())
        arch::halt_forever();

    if (!serial::init())
        arch::halt_forever();

    const auto fb = boot::framebuffer();
    if (!fb)
    {
        kernel::panic("basil: no framebuffer\n");
        arch::halt_forever();
    }
    gfx::init(*fb);
    gfx::clear(0x05, 0x05, 0x05, 0xFF);

    kernel::add_sink(serial::put);
    static auto console = gfx::Console{};
    kernel::add_sink([&](char c) { console.put(c); });

    arch::init_gdt();
    log::info("initialized gdt");
    arch::init_tss();
    log::info("initialized tss");
    arch::init_idt();
    log::info("initialized idt");
    arch::init_pic();
    log::info("initialized pic");
    arch::init_pit(100);
    log::info("initialized pit");
    keyboard::init();
    log::info("initialized keyboard drivers");

    auto usable = 0uz;
    auto total = 0uz;
    const auto regions = boot::memory_map();
    for (const auto& region : regions)
    {
        log::info("{:#016x} {:>11} {}", region.base, region.length, region.type);
        if (region.type == boot::MemoryType::Usable)
            usable += region.length;

        total += region.length;
    }
    log::info("usable memory: {} / {} MiB", usable / (1024 * 1024), total / (1024 * 1024));

    mm::init_frames(regions);
    log::info("initialized memory frames");

    auto* pml4 = mm::current_pml4();
    const auto virt = mm::VirtualAddress{0xFFFF'FF00'0000'0000};
    const auto frame = mm::alloc_frame();
    if (!frame)
        kernel::panic("out of memory");

    if (!mm::map(pml4, virt, *frame, arch::PageFlags::Writable | arch::PageFlags::NoExecute))
        kernel::panic("map failed");
    *virt.as<uint64_t>() = 0xDEAD'BEEF;
    log::info(
        "{:#x} -> {:#x}, reads {:#x}",
        virt.value,
        mm::translate(pml4, virt)->value,
        *static_cast<uint64_t*>(mm::phys_to_virt(frame->value))); // same frame through the HHDM
    log::info("unmapped: {}", mm::unmap(pml4, virt).has_value() && !mm::translate(pml4, virt));

    __asm__ volatile("sti");

    kernel::kprintf("Welcome to BasilOS!\n");
    gfx::present();

#ifdef KERNEL_TESTS
    kernel::test::run_all();
#endif

    static auto shell = basil::Shell{std::min(gfx::Console::cols(), 300uz)};

    for (;;)
    {
        arch::wait_for_interrupt();
        while (const auto event = keyboard::poll())
        {
            shell.on_key(*event);
        }
        console.on_tick();

        gfx::present();
    }
}
