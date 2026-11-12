#pragma once

#include <cstdint>
namespace arch
{
struct [[gnu::packed]] Cr3
{
    uint64_t pcid : 12;
    uint64_t phyiscal_address : 48;
};

static_assert(sizeof(Cr3) == 8);

[[noreturn]] inline auto halt_forever() -> void
{
    for (;;)
        __asm__ volatile("cli; hlt");
}

inline auto wait_for_interrupt() -> void
{
    __asm__ volatile("sti; hlt");
}

[[noreturn]] inline auto idle_forever() -> void
{
    for (;;)
        wait_for_interrupt();
}

[[nodiscard]] inline auto read_cr2() -> uint64_t
{
    auto value = uint64_t{};
    __asm__ volatile("mov %%cr2, %0" : "=r"(value));
    return value;
}

[[nodiscard]] inline auto read_cr3() -> uint64_t
{
    auto value = uint64_t{};
    __asm__ volatile("mov %%cr3, %0" : "=r"(value));
    return value;
}

inline auto write_cr3(uint64_t value) -> void
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

inline auto invlpg(uint64_t addr) -> void
{
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

[[nodiscard]] inline auto rdmsr(uint64_t msr) noexcept -> uint64_t
{
    auto low = uint32_t{};
    auto high = uint32_t{};

    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));

    return (static_cast<uint64_t>(high) << 32) | low;
}

inline auto wrmsr(uint64_t msr, uint64_t value) -> void
{
    auto low = static_cast<uint32_t>(value & 0xFFFFFFFF);
    auto high = static_cast<uint32_t>(value >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high) : "memory");
}

inline auto enable_nx() -> void
{
    constexpr auto efer = uint32_t{0xC000'0080};
    arch::wrmsr(efer, arch::rdmsr(efer) | (1ull << 11));
}
}
