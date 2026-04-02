#pragma once

#include <snapflakes/stdplus/exec/utils/concepts.hpp>
#include <snapflakes/stdplus/exec/utils/stdexec_fwd.hpp>
#include <snapflakes/stdplus/utilities/initialization.hpp>

namespace snapflakes {
namespace stdplus {
namespace execution {

namespace utils {

template<typename Variant, typename Visitor>
requires concepts::variant_type<Variant>
constexpr inline void visit_types(Visitor &&visitor) {
    []<std::size_t... Is>(std::index_sequence<Is...>, Visitor &&v) {
        (v.template operator()<std::variant_alternative_t<Is, Variant>>(), ...);
    }(std::make_index_sequence<std::variant_size_v<Variant>>{}, std::forward<Visitor>(visitor));
}

template<typename Variant, typename... Args>
requires concepts::variant_type<Variant>
constexpr inline Variant to_variant(Args &&...args) noexcept {
    if constexpr (sizeof...(Args) == 0) {
        return std::monostate{};
    } else if constexpr (sizeof...(Args) == 1) {
        return std::forward<std::tuple_element_t<0, std::tuple<Args...>>>(std::get<0>(std::tuple<Args...>(std::forward<Args>(args)...)));
    } else {
        return std::make_tuple(std::forward<Args>(args)...);
    }
}

template<typename SetFn, typename Recr, typename Completion>
requires stdexec::receiver<Recr>
constexpr inline void basic_set_any_completion(SetFn &&set_fn, Recr &&recr, Completion &&completion) {
    using completion_t = std::remove_cvref_t<Completion>;

    if constexpr (concepts::variant_type<completion_t>) {
        std::visit([&set_fn, &recr](auto &&v) { basic_set_any_completion(std::forward<SetFn>(set_fn), std::forward<Recr>(recr), std::forward<decltype(v)>(v)); }, std::forward<Completion>(completion));
    } else if constexpr (std::is_same_v<completion_t, std::monostate>) {
        std::forward<SetFn>(set_fn)(std::forward<Recr>(recr));
    } else if constexpr (concepts::tuple_like<completion_t>) {
        std::apply([&set_fn, &recr](auto &&...args) { std::forward<SetFn>(set_fn)(std::forward<Recr>(recr), std::forward<decltype(args)>(args)...); }, std::forward<Completion>(completion));
    } else {
        std::forward<SetFn>(set_fn)(std::forward<Recr>(recr), std::forward<Completion>(completion));
    }
}

template<typename Recr, typename Value>
requires stdexec::receiver<Recr>
constexpr inline void basic_set_any_value(Recr &&recr, Value &&value) {
    basic_set_any_completion(stdexec::set_value, std::forward<Recr>(recr), std::forward<Value>(value));
}

template<typename Recr, typename Error>
requires stdexec::receiver<Recr>
constexpr inline void basic_set_any_error(Recr &&recr, Error &&error) {
    basic_set_any_completion(stdexec::set_error, std::forward<Recr>(recr), std::forward<Error>(error));
}

template<typename Snd, typename Env = stdexec::env<>>
class basic_any_completions {
public:
    using sender_concept = stdexec::sender_t;
    using completion_signatures_t = stdexec::completion_signatures_of_t<Snd, Env>;
    using value_t = stdexec::value_types_of_t<Snd, Env>;
    using error_t = stdexec::error_types_of_t<Snd, Env>;
    // `Env` fixes the cached completion shape. The replay sender itself keeps
    // an inert env because broadcast forwards the real downstream env directly
    // to the live upstream connection instead of reusing it from cached items.
    using env_t = stdexec::env<>;
    using snd_env_t = env_t;

    constexpr basic_any_completions() noexcept = default;

    template<typename T>
    requires (!std::same_as<std::remove_cvref_t<T>, basic_any_completions>)
    constexpr explicit basic_any_completions(T &&) noexcept {
    }

    constexpr basic_any_completions(const basic_any_completions &) = default;

    constexpr basic_any_completions &operator=(const basic_any_completions &) = default;

    constexpr basic_any_completions(basic_any_completions &&other) : value_(std::move(other.value_)), error_(std::move(other.error_)), stopped_(other.stopped_), env_(std::move(other.env_)) {
        other.reset();
    }

    constexpr basic_any_completions &operator=(basic_any_completions &&other) noexcept {
        value_ = std::move(other.value_);
        error_ = std::move(other.error_);
        stopped_ = other.stopped_;
        env_ = std::move(other.env_);
        other.reset();
        return *this;
    }

    ~basic_any_completions() = default;

    template<typename... Args>
    constexpr void store_value(Args &&...args) noexcept {
        value_.emplace(utils::to_variant<value_t>(std::forward<Args>(args)...));
    }

    template<typename... Args>
    constexpr void store_error(Args &&...args) noexcept {
        error_.emplace(utils::to_variant<error_t>(std::forward<Args>(args)...));
    }

    constexpr void store_stopped() noexcept {
        stopped_ = true;
    }

    constexpr bool has_value() const noexcept {
        return value_.has_value();
    }

    constexpr bool has_error() const noexcept {
        return error_.has_value();
    }

    constexpr bool has_stopped() const noexcept {
        return stopped_;
    }

    constexpr value_t &value() noexcept {
        return value_.value();
    }

    constexpr error_t &error() noexcept {
        return error_.value();
    }

    constexpr void reset() noexcept {
        value_.reset();
        error_.reset();
        stopped_ = false;
    }

    template<typename Recr>
    struct operation_state_t {

        template<typename T>
        requires std::constructible_from<Recr, T>
        constexpr explicit operation_state_t(T &&rec, const basic_any_completions &completions) noexcept
            : rec_(std::forward<T>(rec)), value_(completions.value_), error_(completions.error_), stopped_(completions.stopped_) {
        }

        template<typename T>
        requires std::constructible_from<Recr, T>
        constexpr explicit operation_state_t(T &&rec, basic_any_completions &&completions) noexcept
            : rec_(std::forward<T>(rec)), value_(std::move(completions.value_)), error_(std::move(completions.error_)), stopped_(completions.stopped_) {
            completions.reset();
        }

        constexpr ~operation_state_t() = default;

        constexpr void start() noexcept {
            if (error_.has_value()) {
                basic_set_any_error(std::move(rec_), std::move(error_.value()));
                return;
            }

            if (stopped_) {
                stdexec::set_stopped(std::move(rec_));
                return;
            }

            basic_set_any_value(std::move(rec_), std::move(value_.value()));
        }

    private:
        Recr rec_;
        std::optional<value_t> value_;
        std::optional<error_t> error_;
        bool stopped_;
    };

    template<typename Recr>
    constexpr auto connect(Recr rec) & noexcept {
        return operation_state_t<std::remove_cvref_t<Recr>>(std::move(rec), *this);
    }

    template<typename Recr>
    constexpr auto connect(Recr rec) && noexcept {
        return operation_state_t<std::remove_cvref_t<Recr>>(std::move(rec), std::move(*this));
    }

    constexpr env_t get_env() const noexcept {
        return env_;
    }

    static consteval completion_signatures_t get_completion_signatures() noexcept {
        return {};
    }

    template<typename ENV>
    static consteval completion_signatures_t get_completion_signatures() noexcept {
        return {};
    }

private:
    std::optional<value_t> value_ = std::nullopt;
    std::optional<error_t> error_ = std::nullopt;
    bool stopped_ = false;
    env_t env_;
};

template<template<typename> typename Buffer, typename UpSnd, typename Env>
requires concepts::able_to_cache_as_list<Buffer, stdexec::value_types_of_t<UpSnd, Env>>
class basic_completions_cache_list : public stdplus::utils::immoveable {
    using value_t = stdexec::value_types_of_t<UpSnd, Env>;
    using error_t = stdexec::error_types_of_t<UpSnd, Env>;

public:
    using sender_concept = stdexec::sender_t;
    using completion_signatures_t = stdexec::completion_signatures_of_t<UpSnd, Env>;
    using snd_env_t = std::remove_cvref_t<decltype(stdexec::get_env(std::declval<UpSnd>()))>;

    struct cache_item_t {

        template<typename... Args>
        constexpr cache_item_t(size_t ref_cnt, Args &&...args) : ref_cnt_(ref_cnt), value_(to_variant<value_t>(std::forward<Args>(args)...)) {
        }

        size_t ref_cnt_;
        value_t value_;
    };

    template<typename SndEnv>
    requires std::constructible_from<SndEnv, snd_env_t>
    constexpr explicit basic_completions_cache_list(SndEnv &&env) noexcept : env_(std::forward<SndEnv>(env)) {
    }

    ~basic_completions_cache_list() = default;

    template<typename... Args>
    constexpr void emplace_back_value(size_t ref_cnt, Args &&...args) noexcept {
        if (has_error() || has_stopped()) {
            return;
        }
        buffer_.emplace_back(ref_cnt, std::forward<Args>(args)...);
    }

    constexpr bool has_value_front() const noexcept {
        return buffer_.has_value_front() && !has_error() && !has_stopped();
    }

    template<typename... Args>
    constexpr void store_error(Args &&...args) noexcept {
        error_.emplace(utils::to_variant<error_t>(std::forward<Args>(args)...));
    }

    constexpr bool has_error() const noexcept {
        return error_.has_value();
    }

    constexpr void store_stopped() noexcept {
        stopped_ = true;
    }

    constexpr bool has_stopped() const noexcept {
        return stopped_;
    }

    constexpr bool is_full() const noexcept {
        return buffer_.is_full();
    }

    template<typename Recr>
    struct operation_state_t {
        constexpr explicit operation_state_t(Recr &&rec, basic_completions_cache_list *completions) noexcept : rec_(std::move(rec)), completions_(completions) {
        }

        constexpr ~operation_state_t() = default;

        constexpr void start() noexcept {
            if (completions_->error_.has_value()) {
                utils::basic_set_any_error(std::move(rec_), completions_->error_.value());
                return;
            }

            if (completions_->stopped_) {
                stdexec::set_stopped(std::move(rec_));
                return;
            }

            __try_set_value();
        }

    private:
        void __try_set_value() {
            auto &cache_list = completions_->buffer_;
            cache_item_t &fr = cache_list.front();
            --fr.ref_cnt_;
            if (fr.ref_cnt_ > 0) {
                utils::basic_set_any_value(std::move(rec_), fr.value_);
            } else {
                utils::basic_set_any_value(std::move(rec_), std::move(fr.value_));
                // The completions_ ptr may be set to nullptr after set_value done, so we need to use pre binded cache_list to pop_front.
                // In general, we should not make any use of the member inside operation state after completion signals done.
                cache_list.pop_front();
            }
        }

        Recr rec_;
        basic_completions_cache_list *completions_;
    };

    struct front_sender_t {
        using sender_concept = stdexec::sender_t;

        constexpr explicit front_sender_t(basic_completions_cache_list *completions) noexcept : completions_(completions) {
        }

        constexpr ~front_sender_t() = default;

        template<typename Recr>
        constexpr auto connect(Recr rec) const noexcept {
            return operation_state_t<Recr>(std::move(rec), completions_);
        }

        constexpr snd_env_t get_env() const noexcept {
            return completions_->env_;
        }

        static consteval completion_signatures_t get_completion_signatures() noexcept {
            return {};
        }

        template<typename ENV>
        static consteval completion_signatures_t get_completion_signatures() noexcept {
            return {};
        }

    private:
        basic_completions_cache_list *completions_;
    };

    constexpr front_sender_t create_front_sender() noexcept {
        return front_sender_t(this);
    }

private:
    Buffer<cache_item_t> buffer_;
    std::optional<error_t> error_ = std::nullopt;
    bool stopped_ = false;
    snd_env_t env_;
};

template<typename Completions>
struct basic_any_receiver_completion_interface : public stdplus::utils::immoveable {
    using value_variant_t = stdexec::value_types_of_t<Completions>;
    using error_variant_t = stdexec::error_types_of_t<Completions>;
    using set_value_t = void (*)(basic_any_receiver_completion_interface *, value_variant_t &) noexcept;
    using set_rvalue_t = void (*)(basic_any_receiver_completion_interface *, value_variant_t &&) noexcept;
    using set_error_t = void (*)(basic_any_receiver_completion_interface *, error_variant_t &) noexcept;
    using set_rerror_t = void (*)(basic_any_receiver_completion_interface *, error_variant_t &&) noexcept;
    using set_stopped_t = void (*)(basic_any_receiver_completion_interface *) noexcept;

    constexpr basic_any_receiver_completion_interface() = default;

    constexpr basic_any_receiver_completion_interface(set_value_t set_value, set_rvalue_t set_rvalue, set_error_t set_error, set_rerror_t set_rerror, set_stopped_t set_stopped)
        : set_value_(set_value), set_rvalue_(set_rvalue), set_error_(set_error), set_rerror_(set_rerror), set_stopped_(set_stopped) {
    }

    constexpr ~basic_any_receiver_completion_interface() = default;

    template<typename T>
    requires std::constructible_from<value_variant_t, T>
    constexpr void set_value(T &&value) noexcept {
        if constexpr (std::is_rvalue_reference_v<T>) {
            (*set_rvalue_)(this, std::forward<T>(value));
        } else {
            (*set_value_)(this, std::forward<T>(value));
        }
    }

    template<typename T>
    requires std::constructible_from<error_variant_t, T>
    constexpr void set_error(T &&error) noexcept {
        if constexpr (std::is_rvalue_reference_v<T>) {
            (*set_rerror_)(this, std::forward<T>(error));
        } else {
            (*set_error_)(this, std::forward<T>(error));
        }
    }

    constexpr void set_stopped() noexcept {
        (*set_stopped_)(this);
    }

private:
    set_value_t set_value_ = nullptr;
    set_rvalue_t set_rvalue_ = nullptr;
    set_error_t set_error_ = nullptr;
    set_rerror_t set_rerror_ = nullptr;
    set_stopped_t set_stopped_ = nullptr;
};

} // namespace utils
} // namespace execution
} // namespace stdplus
} // namespace snapflakes
