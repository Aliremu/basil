#pragma once

#include <cstdint>

namespace arch
{
struct [[gnu::packed]] IdtEntry
{
    uint16_t isr_low;   // The lower 16 bits of the ISR's address
    uint16_t kernel_cs; // The GDT segment selector that the CPU will load into CS before calling the ISR
    uint8_t ist;        // The IST in the TSS that the CPU will load into RSP; set to zero for now
    uint8_t attributes; // Type and attributes; see the IDT page
    uint16_t isr_mid;   // The higher 16 bits of the lower 32 bits of the ISR's address
    uint32_t isr_high;  // The higher 32 bits of the ISR's address
    uint32_t reserved;  // Set to zero
};

struct ScratchRegisters
{
    uint64_t r11, r10, r9, r8, rdi, rsi, rdx, rcx, rax; // pushed by isr_common
};

struct InterruptStackFrame
{
    ScratchRegisters registers;
    uint64_t vector, error_code;       // pushed by the stub
    uint64_t rip, cs, rflags, rsp, ss; // pushed by the CPU
};

using InterruptHandler = void (*)(InterruptStackFrame&);

struct InterruptDescriptorTable
{
    InterruptHandler divide_by_zero;
    InterruptHandler debug;
    InterruptHandler non_maskable_interrupt;
    InterruptHandler breakpoint;
    InterruptHandler overflow;
    InterruptHandler bound_range_exceeded;
    InterruptHandler invalid_opcode;
    InterruptHandler device_not_available;
    InterruptHandler double_fault;
    InterruptHandler invalid_tss;
    InterruptHandler segment_not_present;
    InterruptHandler stack_segment_fault;
    InterruptHandler general_protection_fault;
    InterruptHandler page_fault;
    InterruptHandler x87_floating_point;
    InterruptHandler alignment_check;
    InterruptHandler machine_check;
    InterruptHandler simd_floating_point;
    InterruptHandler virtualization;
    InterruptHandler security_exception;
};

auto init_idt() -> void;

using IrqHandler = void (*)(InterruptStackFrame&);
auto set_irq_handler(uint8_t irq, IrqHandler handler) -> void;
}
