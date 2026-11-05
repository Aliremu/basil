#include "boot/limine.h"
#include <cstdint>

// Limine scans the .limine_requests section (laid out by linker.ld) for these
// objects and fills in their responses before jumping to kmain. Nothing in C++
// reads them, so `gnu::used` keeps the compiler from dropping them.
namespace
{
[[gnu::used, gnu::section(".limine_requests")]] volatile std::uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(3);

[[gnu::used, gnu::section(".limine_requests")]] volatile limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

[[gnu::used, gnu::section(".limine_requests")]] volatile limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

[[gnu::used, gnu::section(".limine_requests_start")]] volatile std::uint64_t limine_requests_start_marker[] =
    LIMINE_REQUESTS_START_MARKER;

[[gnu::used, gnu::section(".limine_requests_end")]] volatile std::uint64_t limine_requests_end_marker[] =
    LIMINE_REQUESTS_END_MARKER;
}

namespace boot
{
auto base_revision_supported() -> bool
{
    return LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision);
}

auto framebuffer() -> std::optional<gfx::Framebuffer>
{
    const auto* response = framebuffer_request.response;
    if (response == nullptr || response->framebuffer_count < 1)
        return std::nullopt;
    auto* fb = response->framebuffers[0];

    if (fb->bpp != 32 || fb->memory_model != LIMINE_FRAMEBUFFER_RGB || fb->pitch % sizeof(uint32_t) != 0)
        return std::nullopt;

    return std::optional<gfx::Framebuffer>{
        std::in_place,
        static_cast<volatile uint32_t*>(fb->address),
        nullptr,
        fb->width,
        fb->height,
        fb->pitch / sizeof(uint32_t)};
}

auto rsdp() -> void
{
    const auto* response = rsdp_request.response;
}
}
