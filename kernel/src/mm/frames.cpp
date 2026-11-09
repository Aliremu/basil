#include "mm/frames.h"
#include "core/panic.h"
#include "libk/frame_bitmap.h"
#include "mm/address.h"

namespace mm
{
namespace
{
constinit auto frames = libk::FrameBitmap{};
}

auto init_frames(std::span<const boot::MemoryRegion> regions) -> void
{
    // Size the bitmap by the highest *usable* address, not the highest entry.
    // MMIO ranges can sit far above 4 GiB and would make the bitmap huge for nothing.
    auto top = 0uz;
    for (const auto& r : regions)
        if (r.type == boot::MemoryType::Usable)
            top = std::max(top, r.base + r.length);

    const auto frame_count = top / libk::FrameBitmap::frame_size;
    const auto bytes = libk::FrameBitmap::bytes_needed(frame_count);

    // Put the bitmap in the first usable region that's big enough.
    constexpr auto low_limit = 0x100000uz; // 1 MiB

    auto store_phys = 0uz;
    auto found = false;
    for (auto& r : regions)
    {
        if (r.type != boot::MemoryType::Usable)
            continue;
        uint64_t base = std::max(r.base, low_limit);
        uint64_t end = r.base + r.length;
        if (end > base && end - base >= bytes)
        {
            store_phys = base;
            found = true;
            break;
        }
    }
    if (!found)
        kernel::panic("no room for frame bitmap");

    auto* storage = static_cast<uint8_t*>(phys_to_virt(store_phys));
    std::ranges::fill(std::span{storage, bytes}, uint8_t{0});
    frames.init(storage, frame_count);

    for (auto& r : regions)
        if (r.type == boot::MemoryType::Usable)
            frames.free_range({r.base, r.length});

    // Order matters: reserve *after* freeing, or these get freed again.
    frames.reserve_range({store_phys, bytes}); // the bitmap's own frames
    frames.reserve_range({0, 4096});           // frame 0
}

auto alloc_frame() -> std::optional<PhysicalAddress>
{
    return frames.alloc().transform([](uint64_t phys) { return PhysicalAddress{phys}; });
}

auto free_frame(PhysicalAddress frame) -> void
{
    frames.free(frame.value);
}
}
