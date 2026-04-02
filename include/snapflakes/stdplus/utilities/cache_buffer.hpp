#pragma once

#include <atomic>

#include <concepts>
#include <optional>
#include <array>

namespace snapflakes {
namespace stdplus {
namespace utils {

template<typename Data, typename Atomic = std::atomic<int>>
class double_cached_buffer {
public:
    double_cached_buffer() = default;
    
    ~double_cached_buffer() = default;

    template<typename T>
    requires std::constructible_from<Data, T>
    constexpr void push_back(T&& value) noexcept {
        auto old = back_ind_.load(std::memory_order_relaxed);
        buffer_[old].emplace(std::forward<T>(value));
        back_ind_.store(old ^ 1, std::memory_order_release);
    }

    template<typename ...Args>
    requires std::constructible_from<Data, Args...>
    constexpr void emplace_back(Args&& ...args) noexcept {
        auto old = back_ind_.load(std::memory_order_relaxed);
        buffer_[old].emplace(std::forward<Args>(args)...);
        back_ind_.store(old ^ 1, std::memory_order_release);
    }

    constexpr Data& front() noexcept {
        auto idx = front_ind_.load(std::memory_order_acquire);
        return buffer_[idx].value();
    }

    constexpr void pop_front() noexcept {
        auto old = front_ind_.load(std::memory_order_relaxed);
        buffer_[old].reset();
        front_ind_.store(old ^ 1, std::memory_order_release);
    }

    constexpr bool has_value_front() const noexcept {
        auto idx = front_ind_.load(std::memory_order_acquire);
        return buffer_[idx].has_value();
    }

    constexpr bool has_value_back() const noexcept {
        auto idx = back_ind_.load(std::memory_order_acquire);
        return buffer_[idx].has_value();
    }

    // not accurate
    constexpr bool is_full() const noexcept {
        auto old_front = front_ind_.load(std::memory_order_acq_rel);
        auto old_back = back_ind_.load(std::memory_order_acq_rel);
        return buffer_[old_front].has_value() && buffer_[old_back].has_value() && old_front != old_back;
    }

private:
    Atomic front_ind_ = 0;
    Atomic back_ind_ = 0;
    std::array<std::optional<Data>, 2> buffer_ = {std::nullopt, std::nullopt};
};

}
}
}
