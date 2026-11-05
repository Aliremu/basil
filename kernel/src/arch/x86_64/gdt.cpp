#include "arch/x86_64/gdt.h"
#include <array>
#include <cstdint>

namespace arch
{
auto gdt_limit_too_large() -> void;
namespace
{
constexpr auto make_gdt_entry(uint8_t access, uint8_t flags, uint32_t base, uint32_t limit) -> GdtEntry
{
    if (limit > 0xFFFFF)
    {
        gdt_limit_too_large();
    }

    return {
        .limit_low = static_cast<uint16_t>(limit & 0xFFFF),
        .base_low = static_cast<uint16_t>(base & 0xFFFF),
        .base_mid = static_cast<uint8_t>((base >> 16) & 0xFF),
        .access = access,
        .granularity = static_cast<uint8_t>(((limit >> 16) & 0x0F) | ((flags & 0x0F) << 4)),
        .base_high = static_cast<uint8_t>((base >> 24) & 0xFF),
    };
}

constinit auto gdt = std::array<GdtEntry, 3>{
    make_gdt_entry(0, 0, 0, 0),            // 0x00 null descriptor (required)
    make_gdt_entry(0x9A, 0xA, 0, 0xFFFFF), // kernel code
    make_gdt_entry(0x92, 0xC, 0, 0xFFFFF), // kernel data
};

static_assert(std::bit_cast<std::uint64_t>(make_gdt_entry(0x9A, 0xA, 0, 0xFFFFF)) == 0x00AF9A000000FFFF);

struct [[gnu::packed]] Gdtr
{
    uint16_t limit; // size in bytes - 1
    uint64_t base;
};

}

auto init_gdt() -> void
{
    const auto gdtr = Gdtr{sizeof(gdt) - 1, reinterpret_cast<uint64_t>(&gdt)};

    __asm__ volatile("lgdt %0\n\t"
                     // CS can't be written with mov: push the selector and a return
                     // address, then far-return into the new code segment.
                     "pushq $0x08\n\t"
                     "leaq 1f(%%rip), %%rax\n\t"
                     "pushq %%rax\n\t"
                     "lretq\n"
                     "1:\n\t"
                     "movw $0x10, %%ax\n\t"
                     "movw %%ax, %%ds\n\t"
                     "movw %%ax, %%es\n\t"
                     "movw %%ax, %%fs\n\t"
                     "movw %%ax, %%gs\n\t"
                     "movw %%ax, %%ss\n\t"
                     :
                     : "m"(gdtr)
                     : "rax", "memory");
}
}
