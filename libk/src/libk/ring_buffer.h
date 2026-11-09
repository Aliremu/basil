#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <iterator>
#include <optional>

namespace libk
{
template <typename T, std::size_t N>
class RingBuffer
{
  public:
    auto push_back(T value) -> void
    {
        const auto idx = tail_.load();
        const auto next = (idx + 1) % N;
        if (next == head_.load())
            return;

        data_[idx] = std::move(value);
        tail_.store(next);
    }

    auto pop_front() -> std::optional<T>
    {
        const auto idx = head_.load();
        if (idx == tail_.load())
            return std::nullopt;

        auto val = std::move(data_[idx]);
        head_.store((idx + 1) % N);
        return val;
    }

    [[nodiscard]] auto size() const -> std::size_t
    {
        return (tail_.load() + N - head_.load()) % N;
    }

    [[nodiscard]] auto full() const -> bool
    {
        return size() == N - 1;
    }

    class iterator
    {
      public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        iterator(T* ptr, std::size_t idx)
            : ptr_{ptr}
            , idx_{idx}
        {
        }

        [[nodiscard]] auto operator*() const -> reference
        {
            return ptr_[idx_];
        }

        [[nodiscard]] auto operator->() const -> pointer
        {
            return &ptr_[idx_];
        }

        auto operator++() noexcept -> iterator&
        {
            idx_ = (idx_ + 1) % N;
            return *this;
        }

        auto operator++(int) noexcept -> iterator
        {
            auto temp = *this;
            ++(*this);
            return temp;
        }

        auto operator--() noexcept -> iterator&
        {
            idx_ = (idx_ + N - 1) % N;
            return *this;
        }

        auto operator--(int) noexcept -> iterator
        {
            auto temp = *this;
            --(*this);
            return temp;
        }

        [[nodiscard]] friend auto operator==(const iterator& lhs, const iterator& rhs) noexcept -> bool
        {
            return lhs.idx_ == rhs.idx_;
        }

      private:
        T* ptr_;
        std::size_t idx_;
    };

    [[nodiscard]] auto begin() -> iterator
    {
        return iterator{data_.data(), head_.load()};
    }

    [[nodiscard]] auto end() -> iterator
    {
        return iterator{data_.data(), tail_.load()};
    }

    [[nodiscard]] auto rbegin() -> std::reverse_iterator<iterator>
    {
        return std::reverse_iterator{end()};
    }

    [[nodiscard]] auto rend() -> std::reverse_iterator<iterator>
    {
        return std::reverse_iterator{begin()};
    }

  private:
    std::array<T, N> data_{};
    std::atomic<std::size_t> head_{};
    std::atomic<std::size_t> tail_{};
};
}
