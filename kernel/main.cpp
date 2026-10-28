#include <limine.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

// GCC may emit calls to these even in freestanding code, so they need C linkage.
extern "C"
{
auto memcpy(void* __restrict dest, const void* __restrict src, size_t n) -> void*
{
    auto d = static_cast<uint8_t*>(dest);
    auto s = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

auto memset(void* s, int c, size_t n) -> void*
{
    auto p = static_cast<uint8_t*>(s);
    for (size_t i = 0; i < n; i++)
        p[i] = (uint8_t)c;
    return s;
}

auto memmove(void* dest, const void* src, size_t n) -> void*
{
    auto d = static_cast<uint8_t*>(dest);
    auto s = static_cast<const uint8_t*>(src);
    if (s > d)
    {
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    }
    else
    {
        for (size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dest;
}

auto memcmp(const void* a, const void* b, size_t n) -> int
{
    auto p = static_cast<const uint8_t*>(a);
    auto q = static_cast<const uint8_t*>(b);
    for (size_t i = 0; i < n; i++)
    {
        if (p[i] != q[i])
            return p[i] < q[i] ? -1 : 1;
    }
    return 0;
}
}

static inline auto outb(uint16_t port, uint8_t val) -> void
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// COM1, already initialised enough by firmware/QEMU for `-serial stdio`.
static auto serial_puts(const char* s) -> void
{
    for (; *s; s++)
    {
        if (*s == '\n')
            outb(0x3f8, '\r');
        outb(0x3f8, (uint8_t)*s);
    }
}

static void hcf(void)
{
    for (;;)
        __asm__ volatile("cli; hlt");
}

// The ENTRY symbol in linker.ld; C linkage keeps the name unmangled.
extern "C" auto kmain() -> void
{
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision))
        hcf();

    serial_puts("basil: hello from the kernel\n");

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1)
    {
        serial_puts("basil: no framebuffer\n");
        hcf();
    }

    struct limine_framebuffer* fb = framebuffer_request.response->framebuffers[0];
    for (uint64_t y = 0; y < fb->height; y++)
    {
        volatile uint32_t* row = (volatile uint32_t*)((uint8_t*)fb->address + y * fb->pitch);
        for (uint64_t x = 0; x < fb->width; x++)
        {
            row[x] = 0xFF00FFFF; // (uint32_t)((x * 255 / fb->width) << 16 | (y * 255 / fb->height) << 8 | 0x60);
        }
    }
    serial_puts("basil: framebuffer painted\n");

    hcf();
}
