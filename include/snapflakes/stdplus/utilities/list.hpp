#pragma once

#include <utility>
#include <type_traits>
#include <snapflakes/stdplus/utilities/concepts.hpp>
#include <snapflakes/stdplus/utilities/initialization.hpp>

namespace snapflakes {
namespace stdplus {
namespace utils {

class static_double_linked_list_element: public stdplus::utils::immoveable {
public:
    constexpr static_double_linked_list_element() = default;
    constexpr ~static_double_linked_list_element() = default;

    constexpr auto* get_next() const noexcept {
        return next_;
    }

    constexpr auto* get_prev() const noexcept {
        return prev_;
    }

    constexpr void set_next(static_double_linked_list_element* next) noexcept {
        next_ = next;
    }

    constexpr void set_prev(static_double_linked_list_element* prev) noexcept {
        prev_ = prev;
    }

private:
    static_double_linked_list_element* next_ = nullptr;
    static_double_linked_list_element* prev_ = nullptr;
};

template<typename Ret, typename ...Args>
struct invokeable_list_element_base: public static_double_linked_list_element {
    using f = Ret (*)(invokeable_list_element_base<Ret, Args...>*, Args...);

    constexpr invokeable_list_element_base() = default;

    constexpr explicit invokeable_list_element_base(f f): static_double_linked_list_element(), f_(f) {};

    constexpr ~invokeable_list_element_base() = default;

    template<typename ...CallArgs>
    constexpr Ret invoke(CallArgs&& ...args) noexcept {
        if constexpr (std::is_void_v<Ret>) {
            (*f_)(this, std::forward<CallArgs>(args)...);
            return;
        }
        return (*f_)(this, std::forward<CallArgs>(args)...);
    }

private:
    f f_ = nullptr;
};

template<typename Ele>
requires concepts::static_double_linked_listable<Ele>
class static_double_linked_list {
public:

    template<typename T>
    requires std::is_same_v<T, Ele> || std::derived_from<T, Ele>
    constexpr explicit static_double_linked_list(T* sentinel) noexcept {
        set_sentinel(sentinel);
    }

    constexpr ~static_double_linked_list() = default;

    template<typename T>
    requires std::is_same_v<T, Ele> || std::derived_from<T, Ele>
    constexpr void set_sentinel(T* sentinel) noexcept {
        sentinel_ = static_cast<Ele*>(sentinel);
        sentinel_->set_next(sentinel_);
        sentinel_->set_prev(sentinel_);
        size_ = 0;
    }

    constexpr std::size_t size() const noexcept {
        return size_;
    }

    constexpr bool empty() const noexcept {
        return size_ == 0;
    }

    constexpr void pop_front() noexcept {
        erase(front());
    }

    constexpr Ele* pop_front_value() noexcept {
        Ele* e = front();
        pop_front();
        return e;
    }

    constexpr void push_back(Ele* e) noexcept {
        Ele* tail = back();
        e->set_next(sentinel_);
        e->set_prev(tail);
        tail->set_next(e);
        sentinel_->set_prev(e);
        size_++;
    }

    constexpr void push_back(Ele& e) noexcept {
        push_back(&e);
    }

    constexpr void push_front(Ele* e) noexcept {
        Ele* old_h = front();
        e->set_next(old_h);
        e->set_prev(sentinel_);
        sentinel_->set_next(e);
        old_h->set_prev(e);
        size_++;
    }

    constexpr void push_front(Ele& e) noexcept {
        push_front(&e);
    }

    constexpr Ele* front() const noexcept {
        return static_cast<Ele*>(sentinel_->get_next());
    }

    constexpr Ele* back() const noexcept {
        return static_cast<Ele*>(sentinel_->get_prev());
    }

    constexpr void erase(Ele* e) noexcept {
        Ele* prev = static_cast<Ele*>(e->get_prev());
        Ele* next = static_cast<Ele*>(e->get_next());
        prev->set_next(next);
        next->set_prev(prev);
        e->set_next(nullptr);
        e->set_prev(nullptr);
        size_--;
    }

private:
    Ele* sentinel_ = nullptr;

    std::size_t size_ = 0;
};

}
}
}
