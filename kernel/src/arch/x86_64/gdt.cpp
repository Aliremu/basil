#include "arch/x86_64/gdt.h"
#include <array>
#include <cstdint>

namespace arch
{
auto gdt_limit_too_large() -> void;
namespace
{
constexpr auto make_gdt_entry(const Gdt& entry) -> uint64_t
{
    alignas(uint64_t) std::array<uint8_t, sizeof(uint64_t)> target{};

    // Check the limit to make sure that it can be encoded
    if (entry.limit > 0xFFFFF)
    {
        gdt_limit_too_large();
    }

    // Encode the limit
    target[0] = entry.limit & 0xFF;
    target[1] = (entry.limit >> 8) & 0xFF;
    target[6] = (entry.limit >> 16) & 0x0F;

    // Encode the base
    target[2] = entry.base & 0xFF;
    target[3] = (entry.base >> 8) & 0xFF;
    target[4] = (entry.base >> 16) & 0xFF;
    target[7] = (entry.base >> 24) & 0xFF;

    // Encode the access byte
    target[5] = entry.access_byte;

    // Encode the flags
    target[6] |= (entry.flags << 4);

    return std::bit_cast<uint64_t>(target);
}

constinit auto gdt = std::array<uint64_t, 3>{
    0, // 0x00 null descriptor (required)
    make_gdt_entry({
        .limit = 0xFFFFF,
        .base = 0,
        .access_byte = 0x9A,
        .flags = 0xA,
    }), // kernel code
    make_gdt_entry({
        .limit = 0xFFFFF,
        .base = 0,
        .access_byte = 0x92,
        .flags = 0xC,
    }), // kernel data
};

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
