#pragma once

#include "arch/x86_64/page_table.h"
#include "boot/limine.h"
#include "mm/address.h"

namespace mm
{
auto init_paging(std::span<const boot::MemoryRegion> regions) -> void;
[[nodiscard]] auto current_pml4() -> arch::PageTable*;
[[nodiscard]] auto map(arch::PageTable* pml4, VirtualAddress virt, PhysicalAddress phys, arch::PageFlags flags) -> bool;
[[nodiscard]] auto unmap(arch::PageTable* pml4, VirtualAddress virt) -> std::optional<PhysicalAddress>;
[[nodiscard]] auto translate(const arch::PageTable* pml4, VirtualAddress virt) -> std::optional<PhysicalAddress>;
[[nodiscard]] auto map_physical(PhysicalAddress phys, std::size_t size, arch::PageFlags flags) -> VirtualAddress;
}
