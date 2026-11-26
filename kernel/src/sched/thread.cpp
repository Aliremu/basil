#include "arch/x86_64/cpu.h"
#include <cstdint>

__asm__(R"(
.pushsection .text
.global switch_context
switch_context:
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    mov %rsp, (%rdi)
    mov %rsi, %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    ret

.global thread_trampoline
thread_trampoline:
    mov %r12, %rdi
    mov %r13, %rsi
    call thread_start
    ud2
.popsection
)");

extern "C" auto switch_context(uint64_t* save_rsp, uint64_t load_rsp) -> void;
extern "C" auto thread_trampoline() -> void;
extern "C" [[noreturn]] auto thread_start(void (*fn)(void*), void* arg) -> void
{
    arch::enable_interrupt();
    fn(arg);
    arch::idle_forever();
}
