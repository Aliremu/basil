#include "basil.h"
#include "arch/x86_64/cpu.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "core/print.h"
#include "drivers/serial/serial.h"

namespace basil
{
auto init() -> void
{
    if (!serial::init())
        ::halt_forever();

    kernel::add_sink(serial::put);

    arch::init_gdt();
    arch::init_idt();
}
}
