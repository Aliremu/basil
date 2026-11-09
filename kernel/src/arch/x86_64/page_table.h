#pragma once

#include "libk/bitset.h"
#include "mm/address.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace arch
{
enum class PageFlags : uint64_t
{
    None = 0,
    Present = 1ull << 0,
    Writable = 1ull << 1,
    User = 1ull << 2,
    WriteThrough = 1ull << 3,
    CacheDisable = 1ull << 4,
    Accessed = 1ull << 5,
    Dirty = 1ull << 6,
    Huge = 1ull << 7,
    Global = 1ull << 8,
    NoExecute = 1ull << 63,
};

// pml4_index(virt), pdpt_index(virt), pd_index(virt), pt_index(virt)
}

template <>
inline constexpr auto enable_bitmask_behavior<arch::PageFlags> = true;

namespace arch
{
struct PageTableEntry
{
    static constexpr auto address_mask = uint64_t{0x000F'FFFF'FFFF'F000};

    // 63  62     59 58     52 51                                 12 11   9 8  7   6  5   4    3   2  1  0
    // [NX][ PKey  ][ avail  ][        physical address            ][avail][G][PS][D][A][PCD][PWT][U][W][P]
    uint64_t value;

    [[nodiscard]] constexpr auto flags() const -> PageFlags
    {
        return static_cast<PageFlags>(value & ~address_mask);
    }

    [[nodiscard]] constexpr auto present() const -> bool
    {
        return has(flags(), PageFlags::Present);
    }

    [[nodiscard]] constexpr auto huge() const -> bool
    {
        return has(flags(), PageFlags::Huge);
    }

    [[nodiscard]] constexpr auto address() const -> mm::PhysicalAddress
    {
        return mm::PhysicalAddress{value & address_mask};
    }

    constexpr auto set(uint64_t phys, PageFlags flags) -> void
    {
        value = (phys & address_mask) | std::to_underlying(flags);
    }
};

struct alignas(4096) PageTable
{
    std::array<PageTableEntry, 512> entries;
};
static_assert(sizeof(PageTable) == 4096);

[[nodiscard]] constexpr auto table_index(mm::VirtualAddress virt, int level) -> std::size_t
{
    //   63      48 47     39 38     30 29     21 20     12 11          0
    //   [ sign   ] [ PML4  ] [ PDPT  ] [  PD   ] [  PT   ] [  offset   ]
    //               level 3   level 2   level 1   level 0
    //
    // 0x1FF = 511
    return (virt.value >> (12 + 9 * level)) & 0x1FF;
}

[[nodiscard]] constexpr auto page_size(int level) -> uint64_t
{
    return uint64_t{1} << (12 + 9 * level);
}
}
