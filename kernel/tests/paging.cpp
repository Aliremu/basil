#include "mm/paging.h"
#include "arch/x86_64/cpu.h"
#include "arch/x86_64/page_table.h"
#include "core/test.h"
#include "mm/address.h"
#include "mm/frames.h"

namespace
{
using arch::PageFlags;

// PML4 slot 510 is unused by Limine, so each test owns a page in it.
constexpr auto scratch_base = uintptr_t{0xFFFF'FF00'0000'0000};

// PML4 slot 509 is never mapped, so a walk stops at the top level.
constexpr auto unmapped = mm::VirtualAddress{0xFFFF'FE80'0000'0000};

[[nodiscard]] auto scratch(std::size_t page) -> mm::VirtualAddress
{
    return {scratch_base + (page + 1) * arch::page_size(0)};
}

[[nodiscard]] auto hhdm(uint64_t phys) -> mm::VirtualAddress
{
    return {reinterpret_cast<uintptr_t>(mm::phys_to_virt(phys))};
}

[[nodiscard]] auto alloc() -> mm::PhysicalAddress
{
    const auto frame = mm::alloc_frame();
    KTEST_ASSERT(frame.has_value());
    return *frame;
}

auto release(mm::VirtualAddress virt) -> void
{
    if (const auto frame = mm::unmap(mm::current_pml4(), virt))
        mm::free_frame(*frame);
}
}

KTEST(nx_is_enabled)
{
    KTEST_ASSERT(arch::rdmsr(0xC000'0080) & (1 << 11));
}

KTEST(current_pml4_matches_cr3)
{
    auto* pml4 = mm::current_pml4();
    KTEST_ASSERT(reinterpret_cast<uintptr_t>(pml4) % arch::page_size(0) == 0);

    const auto phys = mm::translate(pml4, {reinterpret_cast<uintptr_t>(pml4)});
    KTEST_ASSERT(phys && phys->value == (arch::read_cr3() & arch::PageTableEntry::address_mask));
}

KTEST(translate_hhdm_address)
{
    const auto phys = mm::translate(mm::current_pml4(), hhdm(0x12'3456));
    KTEST_ASSERT(phys && phys->value == 0x12'3456);
}

KTEST(translate_kernel_static)
{
    static auto value = uint64_t{};
    value = 0x1234'5678;

    const auto phys = mm::translate(mm::current_pml4(), {reinterpret_cast<uintptr_t>(&value)});
    KTEST_ASSERT(phys.has_value());
    KTEST_ASSERT(*hhdm(phys->value).as<volatile uint64_t>() == 0x1234'5678);
}

KTEST(translate_unmapped_is_nullopt)
{
    auto* pml4 = mm::current_pml4();
    KTEST_ASSERT(!mm::translate(pml4, unmapped));
    KTEST_ASSERT(!mm::translate(pml4, scratch(0)));
}

KTEST(map_is_visible_through_hhdm)
{
    const auto virt = scratch(1);
    const auto frame = alloc();
    KTEST_ASSERT(mm::map(mm::current_pml4(), virt, frame, PageFlags::Writable));

    *virt.as<volatile uint64_t>() = 0xDEAD'BEEF;
    KTEST_ASSERT(*hhdm(frame.value).as<volatile uint64_t>() == 0xDEAD'BEEF);

    release(virt);
}

KTEST(translate_mapped_page_keeps_offset)
{
    auto* pml4 = mm::current_pml4();
    const auto virt = scratch(2);
    const auto frame = alloc();
    KTEST_ASSERT(mm::map(pml4, virt, frame, PageFlags::Writable));

    const auto phys = mm::translate(pml4, virt + 0x123uz);
    KTEST_ASSERT(phys && phys->value == frame.value + 0x123);

    release(virt);
}

KTEST(map_refuses_mapped_page)
{
    auto* pml4 = mm::current_pml4();
    const auto virt = scratch(3);
    const auto first = alloc();
    const auto second = alloc();
    KTEST_ASSERT(mm::map(pml4, virt, first, PageFlags::Writable));
    KTEST_ASSERT(!mm::map(pml4, virt, second, PageFlags::Writable));

    const auto phys = mm::translate(pml4, virt);
    KTEST_ASSERT(phys && phys->value == first.value);

    release(virt);
    mm::free_frame(second);
}

KTEST(map_refuses_hhdm_page)
{
    auto* pml4 = mm::current_pml4();
    const auto frame = alloc();
    KTEST_ASSERT(!mm::map(pml4, hhdm(0x20'0000), frame, PageFlags::Writable));

    const auto phys = mm::translate(pml4, hhdm(0x20'0000));
    KTEST_ASSERT(phys && phys->value == 0x20'0000);

    mm::free_frame(frame);
}

KTEST(map_rejects_unaligned)
{
    auto* pml4 = mm::current_pml4();
    const auto virt = scratch(4);
    const auto frame = alloc();
    KTEST_ASSERT(!mm::map(pml4, virt + 1uz, frame, PageFlags::Writable));
    KTEST_ASSERT(!mm::map(pml4, virt, frame + 1, PageFlags::Writable));
    KTEST_ASSERT(!mm::translate(pml4, virt));

    mm::free_frame(frame);
}

KTEST(unmap_returns_the_frame)
{
    auto* pml4 = mm::current_pml4();
    const auto virt = scratch(5);
    const auto frame = alloc();
    KTEST_ASSERT(mm::map(pml4, virt, frame, PageFlags::Writable));

    const auto unmapped_frame = mm::unmap(pml4, virt);
    KTEST_ASSERT(unmapped_frame && unmapped_frame->value == frame.value);
    KTEST_ASSERT(!mm::translate(pml4, virt));

    mm::free_frame(frame);
}

KTEST(unmap_twice_is_nullopt)
{
    auto* pml4 = mm::current_pml4();
    const auto virt = scratch(6);
    const auto frame = alloc();
    KTEST_ASSERT(mm::map(pml4, virt, frame, PageFlags::Writable));
    KTEST_ASSERT(mm::unmap(pml4, virt).has_value());
    KTEST_ASSERT(!mm::unmap(pml4, virt));

    mm::free_frame(frame);
}

KTEST(unmap_unmapped_is_nullopt)
{
    auto* pml4 = mm::current_pml4();
    KTEST_ASSERT(!mm::unmap(pml4, unmapped));
    KTEST_ASSERT(!mm::unmap(pml4, scratch(7)));
}

KTEST(unmap_rejects_unaligned)
{
    auto* pml4 = mm::current_pml4();
    const auto virt = scratch(8);
    const auto frame = alloc();
    KTEST_ASSERT(mm::map(pml4, virt, frame, PageFlags::Writable));
    KTEST_ASSERT(!mm::unmap(pml4, virt + 1uz));
    KTEST_ASSERT(mm::translate(pml4, virt).has_value());

    release(virt);
}

// Reading through virt before unmapping caches the old translation in the TLB;
// without invlpg the final read would still see the first frame.
KTEST(remap_flushes_the_tlb)
{
    auto* pml4 = mm::current_pml4();
    const auto virt = scratch(9);
    const auto first = alloc();
    const auto second = alloc();
    *hhdm(first.value).as<volatile uint64_t>() = 1;
    *hhdm(second.value).as<volatile uint64_t>() = 2;

    KTEST_ASSERT(mm::map(pml4, virt, first, PageFlags::Writable));
    KTEST_ASSERT(*virt.as<volatile uint64_t>() == 1);
    KTEST_ASSERT(mm::unmap(pml4, virt).has_value());

    KTEST_ASSERT(mm::map(pml4, virt, second, PageFlags::Writable));
    KTEST_ASSERT(*virt.as<volatile uint64_t>() == 2);

    release(virt);
    mm::free_frame(first);
}
