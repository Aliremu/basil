#pragma once

#include "gfx/framebuffer.h"
#include <inplace_vector>
#include <optional>

namespace boot
{
enum class MemoryType : uint8_t
{
    Usable,
    Reserved,
    AcpiReclaimable,
    AcpiNvs,
    BadMemory,
    BootloaderReclaimable,
    KernelAndModules,
    Framebuffer,
};

struct MemoryRegion
{
    uint64_t base;
    uint64_t length;
    MemoryType type;
};

[[nodiscard]] auto base_revision_supported() -> bool;
[[nodiscard]] auto framebuffer() -> std::optional<gfx::Framebuffer>;
auto rsdp() -> void;
[[nodiscard]] auto memory_map() -> std::inplace_vector<MemoryRegion, 128>;
[[nodiscard]] auto hhdm_offset() -> uint64_t;
}
