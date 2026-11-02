#include "arch/x86_64/cpu.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "core/basil.h"
#include "core/panic.h"
#include "core/print.h"
#include "core/test.h"
#include "drivers/serial/serial.h"
#include "gfx/fonts/font8x8.h"
#include "gfx/fonts/spleen12x24.h"
#include <cstdint>
#include <functional>
#include <limine.h>
#include <ranges>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <tuple>
#include <utility>

__attribute__((used, section(".limine_requests"))) static volatile uint64_t limine_base_revision[] =
    LIMINE_BASE_REVISION(3);

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

__attribute__((used, section(".limine_requests_start"))) static volatile uint64_t limine_requests_start_marker[] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end"))) static volatile uint64_t limine_requests_end_marker[] =
    LIMINE_REQUESTS_END_MARKER;

#define KASSERT(c) ((c) ? void(0) : assert_fail(#c, std::source_location::current()))

auto draw_char(char ch, int x, int y, limine_framebuffer* fb, int scale = 1, std::uint32_t color = 0xFFFFFFFF) -> void
{
    constexpr auto rows = spleen12x24_height;
    constexpr auto cols = spleen12x24_width;
    auto index = static_cast<unsigned char>(ch);
    if (index >= 128)
        index = '?';

    const auto& glyph = spleen12x24[index];
    for (auto r : std::views::iota(0, rows))
    {
        for (auto c : std::views::iota(0, cols))
        {
            if (((glyph[r] >> c) & 1) == 0)
                continue;

            // Paint this font pixel as a scale×scale block.
            for (auto dy : std::views::iota(0, scale))
            {
                auto* const row = reinterpret_cast<volatile uint32_t*>(
                    static_cast<std::uint8_t*>(fb->address) + (y + r * scale + dy) * fb->pitch);
                for (auto dx : std::views::iota(0, scale))
                    row[x + c * scale + dx] = color;
            }
        }
    }
}

auto draw_string(
    std::string_view str,
    int x,
    int y,
    limine_framebuffer* fb,
    int scale = 1,
    std::uint32_t color = 0xFFFFFFFF) -> std::pair<int, int>
{
    constexpr auto rows = spleen12x24_height;
    constexpr auto cols = spleen12x24_width;
    auto originalX = x;
    for (char ch : str)
    {
        if (ch == '\r' || ch == '\n')
        {
            x = originalX;
            y += (rows)*scale;
            continue;
        }
        draw_char(ch, x, y, fb, scale, color);

        x += (cols)*scale; // one scaled column of spacing
    }

    return {x, y};
}

auto screen_out(limine_framebuffer* fb, char ch) -> void
{
    static auto x = 5;
    static auto y = 5;

    constexpr auto cols = spleen12x24_width;
    constexpr auto scale = 1;
    if (ch == '\r' || ch == '\n')
    {
        x = 5;
        y += (cols)*scale;
        return;
    }
    draw_char(ch, x, y, fb, scale);
    x += (cols)*scale;
}

struct InterruptDescriptorTable
{
};

// The ENTRY symbol in linker.ld; C linkage keeps the name unmangled.
extern "C" auto kmain() -> void
{
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision))
        ::halt_forever();

    basil::init();

    kernel::kprintf("basil: hello from the kernel\n");

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1)
    {
        kernel::kprintf("basil: no framebuffer\n");
        ::halt_forever();
    }

    struct limine_framebuffer* fb = framebuffer_request.response->framebuffers[0];
    kernel::add_sink(std::bind_front<&screen_out>(fb));
    for (uint64_t y = 0; y < fb->height; y++)
    {
        volatile uint32_t* row = (volatile uint32_t*)((uint8_t*)fb->address + y * fb->pitch);
        for (uint64_t x = 0; x < fb->width; x++)
        {
            row[x] = 0xFF050505; // (uint32_t)((x * 255 / fb->width) << 16 | (y * 255 / fb->height) << 8 | 0x60);
        }
    }

    kernel::kprintf("Welcome to BasilOS!\n");
    kernel::kprintf("basil: framebuffer painted\n");
    __asm__ volatile("int3");

#ifdef KERNEL_TESTS
    kernel::test::run_all(); // `test` profile: run the KTESTs and exit QEMU
#endif

    ::halt_forever();
}
