#pragma once

#include <cstdint>

namespace arch
{
struct [[gnu::packed]] GdtEntry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
};

struct [[gnu::packed]] Tss
{
    uint32_t reserved0;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t reserved1;
    uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb;
};

static_assert(sizeof(Tss) == 104);

auto init_gdt() -> void;
auto init_tss() -> void;
}
