#pragma once

#include <cstdint>
namespace arch
{
struct [[gnu::packed]] Gdt
{
    uint32_t limit;
    uint32_t base;
    uint8_t access_byte;
    uint8_t flags;
};

auto init_gdt() -> void;
}
