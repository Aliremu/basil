#pragma once

[[noreturn]] inline auto halt_forever() -> void
{
    for (;;)
        __asm__ volatile("cli; hlt");
}
