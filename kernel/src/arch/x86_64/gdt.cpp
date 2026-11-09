#include "arch/x86_64/gdt.h"
#include <array>
#include <bit>
#include <cstdint>

namespace arch
{
auto gdt_limit_too_large() -> void;

namespace
{
[[nodiscard]] constexpr auto make_gdt_entry(uint8_t access, uint8_t flags, uint32_t base, uint32_t limit) -> GdtEntry
{
    if (limit > 0xFFFFF)
        gdt_limit_too_large();

    return {
        .limit_low = static_cast<uint16_t>(limit & 0xFFFF),
        .base_low = static_cast<uint16_t>(base & 0xFFFF),
        .base_mid = static_cast<uint8_t>((base >> 16) & 0xFF),
        .access = access,
        .granularity = static_cast<uint8_t>(((limit >> 16) & 0x0F) | ((flags & 0x0F) << 4)),
        .base_high = static_cast<uint8_t>((base >> 24) & 0xFF),
    };
}

constinit auto gdt = std::array<GdtEntry, 7>{
    make_gdt_entry(0, 0, 0, 0),            // 0x00 null descriptor (required)
    make_gdt_entry(0x9A, 0xA, 0, 0xFFFFF), // kernel code segment
    make_gdt_entry(0x92, 0xC, 0, 0xFFFFF), // kernel data segment
    make_gdt_entry(0xF2, 0xC, 0, 0xFFFFF), // usermode data segment
    make_gdt_entry(0xFA, 0xA, 0, 0xFFFFF), // usermode code segment
};

static_assert(std::bit_cast<uint64_t>(make_gdt_entry(0x9A, 0xA, 0, 0xFFFFF)) == 0x00AF9A000000FFFF);

constinit auto tss = Tss{};

struct [[gnu::packed]] Gdtr
{
    uint16_t limit; // size in bytes - 1
    uint64_t base;
};

}

auto init_gdt() -> void
{
    const auto base = reinterpret_cast<uint64_t>(&arch::tss);
    gdt[5] = make_gdt_entry(0x89, 0, static_cast<uint32_t>(base), sizeof(Tss) - 1);
    gdt[6] = std::bit_cast<GdtEntry>(base >> 32);
    const auto gdtr = Gdtr{sizeof(gdt) - 1, reinterpret_cast<uint64_t>(&gdt)};

    __asm__ volatile(R"(lgdt %0
                     pushq $0x08
                     leaq 1f(%%rip), %%rax
                     pushq %%rax
                     lretq
                     1:
                     movw $0x10, %%ax
                     movw %%ax, %%ds
                     movw %%ax, %%es
                     movw %%ax, %%fs
                     movw %%ax, %%gs
                     movw %%ax, %%ss)"
                     :
                     : "m"(gdtr)
                     : "rax", "memory");
}

alignas(16) constinit auto double_fault_stack = std::array<std::byte, 16 * 1024>{};

auto init_tss() -> void
{
    tss.ist1 = reinterpret_cast<uint64_t>(double_fault_stack.data() + double_fault_stack.size());
    tss.iopb = sizeof(Tss);
    __asm__ volatile("ltr %w0" ::"r"(0x28));
}
}
