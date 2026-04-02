#pragma once

#include <cassert>
#include <cstddef>
#include <format>
#include <type_traits>
#include <ranges>
#include <snapflakes/stdplus/exec/concepts.hpp>
#include <snapflakes/stdplus/exec/std.hpp>


namespace snapflakes {
namespace stdplus {
namespace execution {
namespace utils {

template<typename Variant, typename Visitor>
requires concepts::variant_type<Variant>
constexpr void visit_types(Visitor&& visitor) {
    []<std::size_t... Is>(std::index_sequence<Is...>, Visitor&& v) {
        (v.template operator()<std::variant_alternative_t<Is, Variant>>(), ...);
    }(std::make_index_sequence<std::variant_size_v<Variant>>{}, 
      std::forward<Visitor>(visitor));
}

template<typename Variant, typename ...Args>
requires concepts::variant_type<Variant>
Variant to_variant(Args&& ...args) noexcept {
    if constexpr (sizeof...(Args) == 0) {
        return std::monostate{};
    } else if constexpr (sizeof...(Args) == 1) {
        return std::forward<std::tuple_element_t<0, std::tuple<Args...>>>(
            std::get<0>(std::tuple<Args...>(std::forward<Args>(args)...))
        );
    } else {
        return std::make_tuple(std::forward<Args>(args)...);
    }
}

struct immoveable {
    constexpr immoveable() = default;
    immoveable(immoveable&&) = delete;
    immoveable& operator=(immoveable&&) = delete;
    constexpr ~immoveable() = default;
};

struct immoveable_and_noncopyable {
    constexpr immoveable_and_noncopyable() = default;
    immoveable_and_noncopyable(const immoveable_and_noncopyable&) = delete;
    immoveable_and_noncopyable& operator=(const immoveable_and_noncopyable&) = delete;
    immoveable_and_noncopyable(immoveable_and_noncopyable&&) = delete;
    immoveable_and_noncopyable& operator=(immoveable_and_noncopyable&&) = delete;
    constexpr ~immoveable_and_noncopyable() = default;
};

class non_mutex {
    constexpr non_mutex() = default;
    constexpr ~non_mutex() = default;

    constexpr void lock() noexcept {}

    constexpr void unlock() noexcept {}
};

template<typename F>
class guard: public immoveable_and_noncopyable {
public:
    guard() = delete;

    constexpr explicit guard(F&& f): f_(std::move(f)) {}

    constexpr ~guard() noexcept {
        f_();
    }

private:
    F f_;
};

class static_double_linked_list_element: public immoveable {
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

    constexpr Ret invoke(Args&& ...args) noexcept {
        if constexpr (std::is_void_v<Ret>) {
            (*f_)(this, std::forward<Args>(args)...);
            return;
        }
        return (*f_)(this, std::forward<Args>(args)...);
    }

private:
    f f_ = nullptr;
};

template<typename Ele>
requires concepts::static_double_linked_listable<Ele>
class static_double_linked_list {
public:
    constexpr static_double_linked_list() = default;

    constexpr ~static_double_linked_list() = default;

    constexpr size_t size() const noexcept {
        return size_;
    }

    constexpr bool empty() const noexcept {
        return size_ == 0;
    }

    constexpr void pop_front() noexcept {
        Ele* e = head_;
        head_ = static_cast<Ele*>(head_->get_next());
        e->set_next(nullptr);
        e->set_prev(nullptr);
        size_--;
    }

    constexpr void push_back(Ele* e) noexcept {
        e->set_next(nullptr);
        e->set_prev(nullptr);
        if (empty()) {
            head_ = e;
            tail_ = e;
        } else {
            tail_->set_next(e);
            e->set_prev(tail_);
            tail_ = e;
        }
        size_++;
    }

    constexpr void push_back(Ele& e) noexcept {
        push_back(&e);
    }

    constexpr void push_front(Ele* e) noexcept {
        e->set_next(nullptr);
        e->set_prev(nullptr);
        if (empty()) {
            head_ = e;
            tail_ = e;
        } else {
            e->set_next(head_);
            head_->set_prev(e);
            head_ = e;
        }
        size_++;
    }

    constexpr void push_front(Ele& e) noexcept {
        push_front(&e);
    }

    constexpr Ele* front() const noexcept {
        return head_;
    }

    constexpr Ele* back() const noexcept {
        return tail_;
    }

private:
    Ele* head_ = nullptr;
    Ele* tail_ = nullptr;

    size_t size_ = 0;
};

template<typename Ele>
class random_access_ring_buffer {
public:
    using value_type = Ele;

    constexpr random_access_ring_buffer() = default;

    constexpr explicit random_access_ring_buffer(size_t size): cap_(size), buffer_() {
        if (size == 0)
            return;

        buffer_.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            buffer_.emplace_back(std::nullopt);
        }
    }

    constexpr bool is_full() const noexcept {
        return sz_ == cap_;
    }

    constexpr bool is_empty() const noexcept {
        return sz_ == 0;
    }

    constexpr size_t size() const noexcept {
        return sz_;
    }

    constexpr const Ele& front() const {
        __throw_empty();
        return *buffer_[head_];
    }

    constexpr Ele& front() noexcept {
        __throw_empty();
        return *buffer_[head_];
    }

    constexpr const Ele& back() const {
        __throw_empty();
        return *buffer_[tail_];
    }

    constexpr Ele& back() {
        __throw_empty();
        return *buffer_[tail_];
    }

    constexpr Ele& operator[](size_t n) noexcept {
        return *buffer_[__at_index_no_throw(head_, n)];
    }

    constexpr const Ele& operator[](size_t n) const noexcept {
        return *buffer_[__at_index_no_throw(head_, n)];
    }

    constexpr const Ele& at(size_t n) const {
        return *buffer_[__at_index(head_, n)];
    }

    constexpr Ele& at(size_t n) {
        return *buffer_[__at_index(head_, n)];
    }

    constexpr void pop_front() {
        __throw_empty();
        auto target = head_;
        head_ = __next(head_);
        sz_--;
        buffer_[target].emplace(std::nullopt);
    }

    constexpr Ele&& pop_front_with_value() {
        __throw_empty();
        auto target = head_;
        head_ = __next(head_);
        sz_--;
        return std::move(*buffer_[target]);
    }

    constexpr void push_back(Ele&& e) {
        __emplace_back(std::move(e));
    }

    constexpr void push_back(const Ele& e) {
        __emplace_back(e);
    }

    template<typename ...Args>
    constexpr void emplace_back(Args&&... args) {
        __emplace_back(std::forward<Args>(args)...);
    }

    template<typename ...Args>
    constexpr void emplace(size_t ind, Args&&... args) {
        buffer_[__at_index(head_, ind)].emplace(std::forward<Args>(args)...);
    }

    template<typename RB>
    friend struct iterator;

    template<typename RB>
    requires std::is_same_v<random_access_ring_buffer, std::remove_cvref_t<RB>>
    struct iterator {
        friend class random_access_ring_buffer;

        using iterator_category = std::random_access_iterator_tag;
        using reference = decltype(std::declval<RB>().front());
        using value_type = std::remove_reference_t<reference>;
        using difference_type = std::ptrdiff_t;
        using pointer = std::add_pointer_t<value_type>;

        constexpr iterator(size_t pos, RB* buffer): pos_(pos), rb_(buffer) {}

        constexpr reference operator*() const {
            return rb_->at(pos_);
        }
        
        constexpr pointer operator->() const {
            return &(rb_->at(pos_));
        }

        constexpr reference operator[](difference_type n) const {
            return rb_->buffer_[rb_->__at_index(pos_, n)];
        }

        constexpr iterator& operator++() noexcept {
            pos_ = rb_->__next(pos_);
            return *this;
        }

        constexpr iterator operator++(int) noexcept {
            auto tmp = *this;
            pos_ = rb_->__next(pos_);
            return tmp;
        }

        constexpr iterator& operator--() noexcept {
            pos_ = rb_->__prev(pos_);
            return *this;
        }
        
        constexpr iterator operator--(int) noexcept {
            auto tmp = *this;
            pos_ = rb_->__prev(pos_);
            return tmp;
        }

        constexpr iterator& operator+=(difference_type n) noexcept {
            pos_ = rb_->__at_index_no_throw(pos_, n);
            return *this;
        }

        constexpr iterator& operator-=(difference_type n) noexcept {
            pos_ = rb_->__at_index_no_throw(pos_ , -n);
            return *this;
        }

        constexpr iterator operator+(difference_type n) const noexcept {
            return iterator(rb_->__at_index_no_throw(pos_, n), rb_);
        }
        
        constexpr iterator operator-(difference_type n) const noexcept {
            return iterator(rb_->__at_index_no_throw(pos_ , -n), rb_);
        }

        friend constexpr iterator operator+(difference_type n, const iterator& it) noexcept {
            return iterator(it.rb_->__at_index_no_throw(it.pos_, n), it.rb_);
        }

        friend constexpr iterator operator-(difference_type n, const iterator& it) noexcept {
            return iterator(it.rb_->__at_index_no_throw(it.pos_ , -n), it.rb_);
        }

        constexpr difference_type operator-(const iterator& other) const noexcept {
            return this->rb_->__distance_no_throw(pos_, other.pos_);
        }

        constexpr bool operator==(const iterator& other) const noexcept {
            return pos_ == other.pos_;
        }

        constexpr auto operator<=>(const iterator& other) const noexcept {
            return rb_->__convert_to_comparison_index_space(pos_) <=> rb_->__convert_to_comparison_index_space(other.pos_);
        }

    private:
        std::ptrdiff_t pos_ = 0;
        RB* rb_ = nullptr;
    };

    using iterator_type = iterator<random_access_ring_buffer>;
    using const_iterator_type = iterator<const random_access_ring_buffer>;

    constexpr iterator_type begin() noexcept {
        return iterator_type(head_, this);
    }

    constexpr iterator_type end() noexcept {
        return iterator_type(tail_, this);
    }

    constexpr const_iterator_type cbegin() const noexcept {
        return const_iterator_type(head_, this);
    }

    constexpr const_iterator_type cend() const noexcept {
        return const_iterator_type(tail_, this);
    }

    auto view(size_t ind, size_t n) noexcept {
        return std::ranges::subrange(begin() + ind, begin() + (ind + n));
    }

    auto view(size_t ind, size_t n) const noexcept {
        return std::ranges::subrange(cbegin() + ind, cbegin() + (ind + n));
    }

    constexpr bool has_value(size_t ind) const {
        return buffer_[__at_index(head_, ind)].has_value();
    }

private:
    constexpr std::ptrdiff_t __next(std::ptrdiff_t pos) const noexcept {
        return ++pos < cap_ ? pos : 0;
    }

    constexpr std::ptrdiff_t __prev(std::ptrdiff_t pos) const noexcept {
        return pos == 0 ? std::max<std::ptrdiff_t>(cap_, 1) - 1 : --pos;
    }

    constexpr void __throw_empty() const {
        if (sz_ == 0)
            throw std::out_of_range(
                    std::format("snapflakes::stdplus::execution::utils::ring_buffer __throw_empty: buffer is empty with rb pos range [{}, {})", head_, tail_
                ));
    }

    constexpr void __throw_full() const {
        if (sz_ == cap_)
            throw std::out_of_range(
                    std::format("snapflakes::stdplus::execution::utils::ring_buffer __throw_full: buffer is full with rb pos range [{}, {}) + [{}, {})", head_, sz_, 0, tail_
                ));
    }

    const std::ptrdiff_t __at_index_no_throw(std::ptrdiff_t pos, std::ptrdiff_t n) const noexcept {
        pos += n;
        while (cap_ > 0 && pos < 0)
            pos += cap_;

        while (cap_ > 0 && pos >= cap_)
            pos -= cap_;

        return pos;
    }

    constexpr std::ptrdiff_t __at_index(std::ptrdiff_t pos, std::ptrdiff_t n) const {
        auto d = __distance(head_, pos) + n;
        if (d >= sz_ || d < 0)
            throw std::out_of_range(
                    std::format("snapflakes::stdplus::execution::utils::ring_buffer at_index: rb pos {} is out of range [{}, {}) after moving forward {}", 
                    pos, head_, tail_, n
                ));

        return __at_index_no_throw(pos, n);
    }

    constexpr std::ptrdiff_t __convert_to_comparison_index_space(std::ptrdiff_t pos) const noexcept {
        if (head_ > tail_)
            return pos < head_ ? pos + cap_ : pos;

        return pos;
    }

    // calculate rh - lh
    constexpr std::ptrdiff_t __distance(std::ptrdiff_t lh, std::ptrdiff_t rh) const noexcept {
        if (head_ > tail_) {
            lh = lh < head_ ? lh + cap_ : lh;
            rh = rh < head_ ? rh + cap_ : rh;
        }
        return rh - lh;
    }

    template<typename ...Args>
    constexpr void __emplace_back(Args&&... args) {
        __throw_full();
        buffer_[tail_].emplace(std::forward<Args>(args)...);
        tail_ = __next(tail_);
        sz_++;
    }

    std::ptrdiff_t head_ = 0;
    std::ptrdiff_t tail_ = 0;
    std::ptrdiff_t sz_ = 0;
    std::ptrdiff_t cap_ = 0;
    std::vector<std::optional<Ele>> buffer_;
};

template<typename List>
requires concepts::iterable_optional_list<List>
class reorder_queue {
    using iterator_type = typename List::iterator_type;

public:
    using value_type = typename List::value_type;

    constexpr reorder_queue() = default;

    constexpr explicit reorder_queue(const List& list): list_(list) {};

    constexpr explicit reorder_queue(List&& list): list_(std::move(list)) {};

    template<typename ...Args>
    constexpr explicit reorder_queue(Args&&... args): list_(std::forward<Args>(args)...) {};

    constexpr ~reorder_queue() = default;

    template<typename ...Args>
    constexpr void emplace(size_t order, Args&&... args) {
        if (order > max_order_) {
            for (size_t i = max_order_ + 1; i < order; i++) {
                list_.emplace_back(std::nullopt);
            }
            list_.emplace_back(std::forward<Args>(args)...);
            max_order_ = order;
        } else {
            list_.emplace(order, std::forward<Args>(args)...);
        }

        size_t max_order_bound = max_order_ + 1;
        for (; order_bound_ < max_order_bound; ++order_bound_) {
            if (!list_.has_value(order_bound_))
                break;
        }
    }

    constexpr bool has_order_view() const noexcept {
        return order_bound_ > begin_order_;
    }

    constexpr size_t get_max_order_view_size() const noexcept {
        return order_bound_ < begin_order_ ? 0: order_bound_ - begin_order_;
    }

    auto get_order_view(size_t l_ind, size_t r_ind) const {
        auto max_order_view_size = get_max_order_view_size();
        if (l_ind > r_ind || l_ind > max_order_view_size || r_ind > max_order_view_size)
            throw std::out_of_range(
                        std::format("snapflakes::stdplus::execution::utils::reorder_queue get_order_view: [{}, {}) is out of order range [{}, {})", 
                        l_ind + begin_order_, r_ind + begin_order_, begin_order_, order_bound_
                    ));
        
        return std::ranges::subrange(
            list_.begin() + l_ind,
            list_.begin() + r_ind
        );
    }

    auto at(size_t ind) {
        return *(list_.begin() + ind);
    }

    constexpr void pop_front(size_t n) {
        for (size_t i = 0; i < n; i++) {
            list_.pop_front();
        }
        begin_order_ += n;
        order_bound_ = begin_order_ > order_bound_ ? begin_order_ : order_bound_;
    }

private:
    List list_;
    size_t begin_order_ = 0;
    size_t order_bound_ = 0;
    size_t max_order_ = 0;
};

template<typename SetFn, typename Recr, typename Completion>
requires stdexec::receiver<Recr>
void basic_set_any_completion(SetFn&& set_fn, Recr&& recr, Completion&& completion) {
    using completion_t = std::remove_cvref_t<Completion>;

    if constexpr (concepts::variant_type<completion_t>) {
        std::visit(
            [&set_fn, &recr](auto&& v) {
                basic_set_any_completion(
                    std::forward<SetFn>(set_fn),
                    std::forward<Recr>(recr),
                    std::forward<decltype(v)>(v));
            },
            std::forward<Completion>(completion));
    } else if constexpr (std::is_same_v<completion_t, std::monostate>) {
        std::forward<SetFn>(set_fn)(std::forward<Recr>(recr));
    } else if constexpr (concepts::tuple_like<completion_t>) {
        std::apply(
            [&set_fn, &recr](auto&&... args) {
                std::forward<SetFn>(set_fn)(
                    std::forward<Recr>(recr),
                    std::forward<decltype(args)>(args)...);
            },
            std::forward<Completion>(completion));
    } else {
        std::forward<SetFn>(set_fn)(
            std::forward<Recr>(recr),
            std::forward<Completion>(completion));
    }
}

template<typename Recr, typename Value>
requires stdexec::receiver<Recr>
void basic_set_any_value(Recr&& recr, Value&& value) {
    basic_set_any_completion(stdexec::set_value, std::forward<Recr>(recr), std::forward<Value>(value));
}

template<typename Recr, typename Error>
requires stdexec::receiver<Recr>
void basic_set_any_error(Recr&& recr, Error&& error) {
    basic_set_any_completion(stdexec::set_error, std::forward<Recr>(recr), std::forward<Error>(error));
}

template<typename F, typename ValueVariant>
auto basic_inovke_by_any_value(F&& f, ValueVariant&& value) {
    constexpr auto is_rvalue = std::is_rvalue_reference_v<ValueVariant>;
    std::visit([&f](auto&& v) {
        using T = std::remove_cvref_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            // for void
            std::invoke(std::forward<F>(f));
        } else if constexpr (concepts::is_tuple_like_v<T>()) {
            // for multiple values
            std::apply([&f](auto&&... args) {
                if constexpr (is_rvalue)
                    std::invoke(std::forward<F>(f), std::move(args)...);
                else
                    std::invoke(std::forward<F>(f), std::forward<T>(args)...);
            }, v);
        } else {
            // for single value
            if constexpr (is_rvalue)
                std::invoke(std::forward<F>(f), std::move(v));
            else
                std::invoke(std::forward<F>(f), std::forward<T>(v));
        }
    }, std::forward<ValueVariant>(value));
};

template<typename Completions>
struct basic_any_receiver_completion_interface: public immoveable {
    using value_variant_t = stdexec::value_types_of_t<Completions>;
    using error_variant_t = stdexec::error_types_of_t<Completions>;
    using set_value_t = void (*)(basic_any_receiver_completion_interface*, value_variant_t&) noexcept;
    using set_rvalue_t = void (*)(basic_any_receiver_completion_interface*, value_variant_t&&) noexcept;
    using set_error_t = void (*)(basic_any_receiver_completion_interface*, error_variant_t&) noexcept;
    using set_rerror_t = void (*)(basic_any_receiver_completion_interface*, error_variant_t&&) noexcept;
    using set_stopped_t = void (*)(basic_any_receiver_completion_interface* ) noexcept;
    
    constexpr basic_any_receiver_completion_interface() = default;

    constexpr basic_any_receiver_completion_interface(set_value_t set_value, set_rvalue_t set_rvalue, set_error_t set_error, set_rerror_t set_rerror, set_stopped_t set_stopped):
        next_(nullptr),
        prev_(nullptr),
        set_value_(set_value),
        set_rvalue_(set_rvalue),
        set_error_(set_error),
        set_rerror_(set_rerror),
        set_stopped_(set_stopped) 
    {}

    constexpr ~basic_any_receiver_completion_interface() = default;

    template<typename T>
    requires std::constructible_from<value_variant_t, T>
    constexpr void set_value(T&& value) noexcept {
        if constexpr (std::is_rvalue_reference_v<T>) {
            (*set_rvalue_)(this, std::forward<T>(value));
        } else {
            (*set_value_)(this, std::forward<T>(value));
        }
    }

    template<typename T>
    requires std::constructible_from<error_variant_t, T>
    constexpr void set_error(T&& error) noexcept {
        if constexpr (std::is_rvalue_reference_v<T>) {
            (*set_rerror_)(this, std::forward<T>(error));
        } else {
            (*set_error_)(this, std::forward<T>(error));
        }
    }

    constexpr void set_stopped() noexcept {
        (*set_stopped_)(this);
    }

    basic_any_receiver_completion_interface* next_ = nullptr;
    basic_any_receiver_completion_interface* prev_ = nullptr;

private:
    set_value_t set_value_ = nullptr;
    set_rvalue_t set_rvalue_ = nullptr;
    set_error_t set_error_ = nullptr;
    set_rerror_t set_rerror_ = nullptr;
    set_stopped_t set_stopped_ = nullptr;
};

template<typename Recr, typename Completions>
requires stdexec::receiver_of<Recr, Completions>
class basic_any_receiver: public basic_any_receiver_completion_interface<Completions> {
    using base_t = basic_any_receiver_completion_interface<Completions>;
    using value_variant_t = basic_any_receiver_completion_interface<Completions>::value_variant_t;
    using error_variant_t = basic_any_receiver_completion_interface<Completions>::error_variant_t;

public:
    constexpr basic_any_receiver() = delete;

    template<typename T>
    requires std::constructible_from<Recr, T>
    constexpr explicit basic_any_receiver(T&& receiver): base_t(set_value_impl, set_rvalue_impl, set_error_impl, set_rerror_impl, set_stopped_impl), 
        receiver_(std::forward<T>(receiver)) {}

    constexpr ~basic_any_receiver() = default;

    constexpr auto get_env() const noexcept {
        return stdexec::get_env(receiver_);
    }

private:
    static constexpr void set_value_impl(base_t* receiver, value_variant_t& value) noexcept {
        auto self = static_cast<basic_any_receiver*>(receiver);
        basic_set_any_value(self->receiver_, value);
    }

    static constexpr void set_rvalue_impl(base_t* receiver, value_variant_t&& value) noexcept {
        auto self = static_cast<basic_any_receiver*>(receiver);
        basic_set_any_value(self->receiver_, std::move(value));
    }

    static constexpr void set_error_impl(base_t* receiver, error_variant_t& error) noexcept {
        auto self = static_cast<basic_any_receiver*>(receiver);
        basic_set_any_error(self->receiver_, error);
    }

    static constexpr void set_rerror_impl(base_t* receiver, error_variant_t&& error) noexcept {
        auto self = static_cast<basic_any_receiver*>(receiver);
        basic_set_any_error(self->receiver_, std::move(error));
    }

    static constexpr void set_stopped_impl(base_t* receiver) noexcept {
        auto self = static_cast<basic_any_receiver*>(receiver);
        stdexec::set_stopped(self->receiver_);
    }

    Recr receiver_;
};

}
}
}
}
