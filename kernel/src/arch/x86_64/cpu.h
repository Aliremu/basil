#pragma once

namespace arch
{
[[noreturn]] inline auto halt_forever() -> void
{
    for (;;)
        __asm__ volatile("cli; hlt");
}

[[noreturn]] inline auto idle_forever() -> void
{
    for (;;)
        __asm__ volatile("sti; hlt");
}

inline auto wait_for_interrupt() -> void
{
    __asm__ volatile("sti; hlt");
}

}
