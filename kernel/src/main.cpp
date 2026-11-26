#include "acpi/acpi.h"
#include "arch/x86_64/apic.h"
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
#include "drivers/pci/pci.h"
#include "drivers/serial/serial.h"
#include "gfx/console.h"
#include "gfx/fonts/font8x8.h"
#include "gfx/framebuffer.h"
#include "mm/address.h"
#include "mm/frames.h"
#include "mm/paging.h"
#include "sched/scheduler.h"
#include "sched/thread.h"
#include "shell.h"
#include <algorithm>
#include <cassert>
#include <cstdint>

#ifdef KERNEL_TESTS
#include "core/test.h"
#endif

constinit sched::Thread thread_a;
constinit sched::Thread thread_b;

auto init_thread(sched::Thread& t, void (*fn)(void*), void* arg) -> void
{
    t.stack_size = 16 * 1024;
    t.stack = new std::byte[t.stack_size];

    auto top = reinterpret_cast<uintptr_t>(t.stack + t.stack_size) & ~uintptr_t{0xF}; // 16-byte aligned
    auto* sp = reinterpret_cast<uint64_t*>(top);

    *--sp = 0;                                                     // padding
    *--sp = 0;                                                     // padding
    *--sp = reinterpret_cast<uint64_t>(&sched::thread_trampoline); // ret jumps here
    *--sp = 0;                                                     // rbp
    *--sp = 0;                                                     // rbx
    *--sp = reinterpret_cast<uint64_t>(fn);                        // r12
    *--sp = reinterpret_cast<uint64_t>(arg);                       // r13
    *--sp = 0;                                                     // r14
    *--sp = 0;                                                     // r15

    assert(reinterpret_cast<uintptr_t>(sp + 7) % 16 == 0);
    t.rsp = reinterpret_cast<uint64_t>(sp);
}

auto b_main(void*) -> void
{
    for (int i = 0;; ++i)
    {
        kernel::kprintf("B{} ", i);
        sched::switch_context(&thread_b.rsp, thread_a.rsp);
    }
}

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

    mm::init_paging(regions);
    log::info("initialized memory paging");

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
        *frame->to_virtual().as<uint64_t>()); // same frame through the HHDM
    log::info("unmapped: {}", mm::unmap(pml4, virt).has_value() && !mm::translate(pml4, virt));

    acpi::init(mm::PhysicalAddress{*boot::rsdp()});
    pci::init();

    arch::init_apic();
    log::info("initialized apic");

    arch::init_apic_timer(100);

    keyboard::init();
    log::info("initialized keyboard drivers");

    __asm__ volatile("sti");

    kernel::kprintf("Welcome to BasilOS!\n");

#ifdef KERNEL_TESTS
    kernel::test::run_all();
#endif

    static auto shell = basil::Shell{std::min(gfx::Console::cols(), 300uz)};

    gfx::present();

    init_thread(thread_b, b_main, nullptr);
    for (int i = 0; i < 5; ++i)
    {
        kernel::kprintf("A{} ", i);
        sched::switch_context(&thread_a.rsp, thread_b.rsp);
    }

    for (;;)
    {
        arch::wait_for_interrupt();
        while (const auto event = keyboard::poll())
        {
            shell.on_key(*event);
        }
        shell.on_tick();
        console.on_tick();

        gfx::present();
    }
}
