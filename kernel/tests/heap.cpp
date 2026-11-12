#include "core/test.h"
#include "mm/paging.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

namespace
{
struct alignas(4096) Page
{
    std::array<std::byte, 4096> bytes;
};

[[nodiscard]] auto address(const void* p) -> uintptr_t
{
    return reinterpret_cast<uintptr_t>(p);
}
}

KTEST(new_and_delete_round_trip)
{
    auto* value = new uint64_t{42};
    KTEST_ASSERT(*value == 42);
    delete value;
}

KTEST(heap_memory_is_mapped)
{
    auto* value = new uint64_t{};
    KTEST_ASSERT(mm::translate(mm::current_pml4(), {address(value)}).has_value());
    delete value;
}

KTEST(new_is_sixteen_byte_aligned)
{
    auto* a = new uint8_t{};
    auto* b = new uint8_t{};
    KTEST_ASSERT(address(a) % 16 == 0);
    KTEST_ASSERT(address(b) % 16 == 0);
    KTEST_ASSERT(a != b);
    delete a;
    delete b;
}

KTEST(aligned_new_honours_alignas)
{
    auto* page = new Page{};
    KTEST_ASSERT(address(page) % alignof(Page) == 0);
    page->bytes.back() = std::byte{1};
    delete page;
}

KTEST(freed_memory_is_reused)
{
    auto* first = new uint64_t{};
    const auto first_address = address(first);
    delete first;

    auto* second = new uint64_t{};
    KTEST_ASSERT(address(second) == first_address);
    delete second;
}

KTEST(unique_ptr_frees_on_scope_exit)
{
    auto owned_address = uintptr_t{};
    {
        auto owned = std::make_unique<std::array<uint64_t, 64>>();
        owned->fill(7);
        owned_address = address(owned.get());
    }
    auto again = std::make_unique<std::array<uint64_t, 64>>();
    KTEST_ASSERT(address(again.get()) == owned_address);
}

KTEST(many_small_allocations_keep_their_values)
{
    constexpr auto count = 1000uz;
    auto pointers = std::array<uint64_t*, count>{};
    for (auto i = 0uz; i < count; ++i)
        pointers[i] = new uint64_t{i};

    for (auto i = 0uz; i < count; ++i)
        KTEST_ASSERT(*pointers[i] == i);

    for (auto* p : pointers)
        delete p;
}

KTEST(large_allocation_grows_the_heap)
{
    constexpr auto size = 256uz * 1024;
    auto* bytes = new std::byte[size];
    bytes[0] = std::byte{1};
    bytes[size - 1] = std::byte{2};
    KTEST_ASSERT(bytes[0] == std::byte{1} && bytes[size - 1] == std::byte{2});
    delete[] bytes;
}
