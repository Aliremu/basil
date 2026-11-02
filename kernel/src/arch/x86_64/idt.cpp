#include "arch/x86_64/idt.h"
#include "core/print.h"
#include <array>
#include <cstdint>
#include <ranges>

// Exception entry points. These live at file scope, not in a function: the CPU
// jumps to them on its own, so they must not sit inside another function's code.
// Each stub pushes a vector number (and a dummy error code when the CPU doesn't
// push one), so every frame has the same layout when it reaches isr_common.
__asm__(R"(
.pushsection .text

.macro isr_err_stub n
isr_stub_\n:
    pushq $\n                  # vector (the CPU already pushed the error code)
    jmp isr_common
.endm

.macro isr_no_err_stub n
isr_stub_\n:
    pushq $0                   # dummy error code
    pushq $\n                  # vector
    jmp isr_common
.endm

# Saves the scratch registers, so a handler that returns doesn't corrupt the
# interrupted code. 48-byte CPU frame + 8 vector + 72 registers = 128, so the
# stack is 16-byte aligned at the call, as the System V ABI requires.
isr_common:
    push %rax; push %rcx; push %rdx; push %rsi; push %rdi
    push %r8;  push %r9;  push %r10; push %r11
    move %rsp, %rdi
    call interrupt_dispatch
    pop %r11;  pop %r10;  pop %r9;  pop %r8
    pop %rdi;  pop %rsi;  pop %rdx; pop %rcx; pop %rax
    add $16, %rsp              # drop vector + error code
    iretq

isr_no_err_stub 0              # divide error
isr_no_err_stub 1              # debug
isr_no_err_stub 2              # NMI
isr_no_err_stub 3              # breakpoint
isr_no_err_stub 4              # overflow
isr_no_err_stub 5              # bound range exceeded
isr_no_err_stub 6              # invalid opcode
isr_no_err_stub 7              # device not available
isr_err_stub    8              # double fault
isr_no_err_stub 9              # coprocessor segment overrun (legacy)
isr_err_stub    10             # invalid TSS
isr_err_stub    11             # segment not present
isr_err_stub    12             # stack-segment fault
isr_err_stub    13             # general protection fault
isr_err_stub    14             # page fault
isr_no_err_stub 15             # reserved
isr_no_err_stub 16             # x87 floating-point
isr_err_stub    17             # alignment check
isr_no_err_stub 18             # machine check
isr_no_err_stub 19             # SIMD floating-point
isr_no_err_stub 20             # virtualization
isr_err_stub    21             # control protection
isr_no_err_stub 22
isr_no_err_stub 23
isr_no_err_stub 24
isr_no_err_stub 25
isr_no_err_stub 26
isr_no_err_stub 27
isr_no_err_stub 28             # hypervisor injection
isr_err_stub    29             # VMM communication
isr_err_stub    30             # security
isr_no_err_stub 31

.popsection

# isr_stub_table[i] = address of isr_stub_i, read by init_idt.
.pushsection .rodata
.balign 8
isr_stub_table:
.irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    .quad isr_stub_\n
.endr
.popsection
)");

namespace arch
{
namespace
{
alignas(0x10) constinit auto idt = std::array<IdtEntry, 256>{};
constinit auto handlers = InterruptDescriptorTable{};

struct [[gnu::packed]] Idtr
{
    uint16_t limit; // size in bytes - 1
    uint64_t base;
};

// 0x8E: present, ring 0, 64-bit interrupt gate (clears IF on entry).
constexpr uint8_t interrupt_gate = 0x8E;
constexpr uint16_t kernel_code_selector = 0x08;

auto make_idt_entry(uint64_t isr, uint8_t attributes) -> IdtEntry
{
    return {
        .isr_low = static_cast<uint16_t>(isr & 0xFFFF),
        .kernel_cs = kernel_code_selector,
        .ist = 0,
        .attributes = attributes,
        .isr_mid = static_cast<uint16_t>((isr >> 16) & 0xFFFF),
        .isr_high = static_cast<uint32_t>(isr >> 32),
        .reserved = 0,
    };
}
}

extern "C"
{
// Defined in the asm above. The linker matches it by name only, so any type
// with the same layout works; std::array<uint64_t, 32> is exactly 32 quads.
extern const std::array<uint64_t, 32> isr_stub_table;

// Called from isr_common. Only the asm refers to it, so [[gnu::used]] keeps
// the compiler from discarding it.
[[gnu::used, noreturn]] void interrupt_dispatch(arch::InterruptStackFrame* frame)
{
}
}

auto init_idt() -> void
{
    static_assert(sizeof(isr_stub_table) == 32 * sizeof(uint64_t));
    for (auto vector : std::views::iota(0uz, isr_stub_table.size()))
    {
        idt[vector] = make_idt_entry(isr_stub_table.at(vector), interrupt_gate);
    }

    const auto idtr = Idtr{sizeof(idt) - 1, reinterpret_cast<uint64_t>(&idt)};
    __asm__ volatile("lidt %0" : : "m"(idtr) : "memory");
    __asm__ volatile("sti");
}
}
