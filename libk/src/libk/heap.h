#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
namespace libk
{
struct AllocHeader
{
    uintptr_t start;
    std::size_t size;
};

class Heap
{
  public:
    constexpr Heap() = default;

    auto add_region(void* start, std::size_t size) -> void
    {
        insert(reinterpret_cast<uintptr_t>(start), size);
    }

    [[nodiscard]] auto alloc(std::size_t size, std::size_t align) -> void*
    {
        align = std::max(align, alignof(FreeBlock));
        size = (size + 15) & ~std::size_t{15};
        for (auto** link = &free_list_; *link != nullptr; link = &(*link)->next)
        {
            auto* block = *link;
            if (block->size < size + sizeof(AllocHeader))
                continue;

            const auto begin = reinterpret_cast<uintptr_t>(block);
            const auto end = begin + block->size;
            const auto user = (end - size) & ~(align - 1);
            if (user - sizeof(AllocHeader) < begin)
                continue;

            auto start = user - sizeof(AllocHeader);
            if (start - begin >= sizeof(FreeBlock))
            {
                block->size = start - begin;
            }
            else
            {
                start = begin;
                *link = block->next;
            }

            ::new (reinterpret_cast<void*>(user - sizeof(AllocHeader))) AllocHeader{start, end - start};
            return reinterpret_cast<void*>(user);
        }

        return nullptr;
    }

    auto free(void* ptr) -> void
    {
        if (ptr == nullptr)
            return;

        const auto* header =
            reinterpret_cast<const AllocHeader*>(reinterpret_cast<uintptr_t>(ptr) - sizeof(AllocHeader));
        insert(header->start, header->size);
    }

  private:
    struct FreeBlock
    {
        std::size_t size;
        FreeBlock* next;
    };

    FreeBlock* free_list_{};

    // Puts [begin, begin + size) back on the address-sorted list, merging with
    // the blocks directly before and after it when they touch.
    auto insert(uintptr_t begin, std::size_t size) -> void
    {
        auto* prev = static_cast<FreeBlock*>(nullptr);
        auto** link = &free_list_;
        while (*link != nullptr && reinterpret_cast<uintptr_t>(*link) < begin)
        {
            prev = *link;
            link = &prev->next;
        }

        auto* next = *link;
        if (next != nullptr && begin + size == reinterpret_cast<uintptr_t>(next))
        {
            size += next->size;
            next = next->next;
        }

        if (prev != nullptr && reinterpret_cast<uintptr_t>(prev) + prev->size == begin)
        {
            prev->size += size;
            prev->next = next;
            return;
        }

        *link = ::new (reinterpret_cast<void*>(begin)) FreeBlock{size, next};
    }
};
}
