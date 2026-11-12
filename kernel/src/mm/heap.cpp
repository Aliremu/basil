#include "mm/heap.h"
#include "arch/x86_64/page_table.h"
#include "core/panic.h"
#include "libk/heap.h"
#include "mm/frames.h"
#include "mm/paging.h"
#include <utility>

namespace mm
{
namespace
{
struct InterruptGuard
{
    InterruptGuard()
    {
        __asm__ volatile(R"(pushfq
                            pop %0
                            cli
                            )"
                         : "=r"(flags_)
                         :
                         : "memory");
    }

    ~InterruptGuard()
    {
        if (flags_ & (1 << 9))
            __asm__ volatile("sti" : : : "memory");
    }

  private:
    uint64_t flags_;
};

constinit auto heap = libk::Heap{};
constinit auto heap_end = uintptr_t{0xFFFF'C000'0000'0000};

auto grow(std::size_t bytes) -> bool
{
    const auto start = heap_end;
    const auto end = heap_end + bytes;
    for (; heap_end < end; heap_end += arch::page_size(0))
    {
        const auto frame = mm::alloc_frame();
        if (!frame)
            break;

        if (!map(mm::current_pml4(), {heap_end}, *frame, arch::PageFlags::Writable | arch::PageFlags::NoExecute))
        {
            mm::free_frame(*frame);
            break;
        }
    }

    if (heap_end > start)
        heap.add_region(reinterpret_cast<void*>(start), heap_end - start);

    return heap_end == end;
}
}

auto kmalloc(std::size_t size, std::size_t align) -> void*
{
    auto lock = InterruptGuard{};

    if (auto* p = heap.alloc(size, align))
        return p;
    // grow by at least 64 KiB, rounded up to whole pages
    return grow(std::max(64 * 1024uz, (size + align + sizeof(libk::AllocHeader) + 0xFFFuz) & ~0xFFFuz))
               ? heap.alloc(size, align)
               : nullptr;
}

auto kfree(void* ptr) -> void
{
    auto lock = InterruptGuard{};

    heap.free(ptr);
}
}

auto operator new(std::size_t size) -> void*
{
    auto* p = mm::kmalloc(size, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
    if (p == nullptr)
        kernel::panic("out of memory allocating {} bytes", size);
    return p;
}

auto operator new(std::size_t size, std::align_val_t align) -> void*
{
    auto* p = mm::kmalloc(size, std::to_underlying(align));
    if (p == nullptr)
        kernel::panic("out of memory allocating {} bytes", size);
    return p;
}
auto operator new[](std::size_t size) -> void*
{
    return operator new(size);
}
auto operator new[](std::size_t size, std::align_val_t align) -> void*
{
    return operator new(size, align);
}

auto operator delete(void* p) noexcept -> void
{
    mm::kfree(p);
}
auto operator delete(void* p, std::size_t) noexcept -> void
{
    mm::kfree(p);
}
auto operator delete(void* p, std::align_val_t) noexcept -> void
{
    mm::kfree(p);
}
auto operator delete(void* p, std::size_t, std::align_val_t) noexcept -> void
{
    mm::kfree(p);
}

auto operator delete[](void* p) noexcept -> void
{
    return operator delete(p);
}
auto operator delete[](void* p, std::size_t size) noexcept -> void
{
    return operator delete(p, size);
}
auto operator delete[](void* p, std::align_val_t align) noexcept -> void
{
    return operator delete(p, align);
}
auto operator delete[](void* p, std::size_t size, std::align_val_t align) noexcept -> void
{
    return operator delete(p, size, align);
}
