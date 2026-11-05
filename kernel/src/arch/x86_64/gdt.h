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

auto init_gdt() -> void;
}
