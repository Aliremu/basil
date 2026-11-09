#pragma once

#include "boot/limine.h"
#include "mm/address.h"
namespace mm
{
auto init_frames(std::span<const boot::MemoryRegion> regions) -> void;

[[nodiscard]] auto alloc_frame() -> std::optional<PhysicalAddress>;
auto free_frame(PhysicalAddress frame) -> void;
}
