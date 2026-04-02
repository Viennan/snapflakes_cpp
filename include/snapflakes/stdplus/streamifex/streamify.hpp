#pragma once

#include <snapflakes/stdplus/utilities/type_traits.hpp>
#include <snapflakes/stdplus/utilities/functional.hpp>
#include <snapflakes/stdplus/streamifex/stream_fwd.hpp>

namespace snapflakes {
namespace stdplus {
namespace streamifex {

namespace __streamify {

/// streamify: Adapt stdexec sender algorithms into stream elements
///
/// Bridges stdexec's sender/receiver model and the stream processing model by
/// wrapping sender factories and sender adaptors so that each `next()` call
/// produces a fresh sender representing one stream element.
///
/// The invocation mode is selected at compile time from six concept branches,
/// controlled by whether `UpStream` and `F` are `std::monostate` (absent):
///
///   - up_mono + f_mono:     snd_clsur(args...)
///     Bare sender factory with bound args. No upstream, no arg transform.
///     e.g.  just_s(42)  →  next() produces  stdexec::just(42)
///
///   - up_mono + f_ret_tuple: std::apply(snd_clsur, f(args...))
///     Sender factory; F returns a tuple that is unpacked as the factory args.
///
///   - up_mono:              snd_clsur(f(args...))
///     Sender factory; F maps the bound args to a single factory argument.
///
///   - f_mono:               up.next() | snd_clsur(args...)
///     Sender adaptor with fixed args piped over each upstream sender.
///     e.g.  continues_on_s(schd)
///
///   - f_ret_tuple:          up.next() | std::apply(snd_clsur, f(args...))
///     Adaptor; F returns a tuple that is unpacked as the adaptor closure args.
///
///   - no_mono:              up.next() | snd_clsur(f(args...))
///     Adaptor; F maps the bound args to a single adaptor closure argument.
///
/// Pre-built CPOs expose the most common stdexec algorithms as streams:
///   schedule_s, just_s, just_error_s, just_stopped_s, read_env_s,
///   then_s, upon_error_s, upon_stopped_s,
///   let_value_s, let_error_s, let_stopped_s,
///   continues_on_s, starts_on_s,
///   into_variant_s, stopped_as_optional_s, stopped_as_error_s, sync_wait_s
///
/// Usage:
///   // Source stream: every next() produces stdexec::just(42)
///   auto s = just_s(42);
///
///   // Adaptor closure: applies then(f) to every element of an upstream stream
///   auto s = some_stream | then_s([](int x) { return x * 2; });
///
///   // Low-level: custom sender closure with an argument-producing functor
///   auto s = streamify(some_stream, my_closure, my_arg_fn, arg);

// Concepts

template<typename UpStream, typename SndClsur, typename F, typename ...Args>
concept streamify_up_mono_f_mono =
    std::is_same_v<std::monostate, UpStream> &&
    std::is_same_v<std::monostate, F> &&
    requires {
        requires stdexec::sender<std::remove_cvref_t<decltype(
            std::declval<SndClsur>()(std::declval<Args>()...)
        )>>;
    };

template<typename UpStream, typename SndClsur, typename F, typename ...Args>
concept streamify_up_mono_f_ret_tuple =
    std::is_same_v<std::monostate, UpStream> &&
    !std::is_same_v<std::monostate, F> &&
    stdplus::utils::is_tuple_like_v<std::invoke_result_t<F, Args...>> &&
    requires {
        requires stdexec::sender<std::remove_cvref_t<decltype(
            std::apply(std::declval<SndClsur>(), std::declval<std::invoke_result_t<F, Args...>>())
        )>>;
    };

template<typename UpStream, typename SndClsur, typename F, typename ...Args>
concept streamify_up_mono =
    std::is_same_v<std::monostate, UpStream> &&
    !std::is_same_v<std::monostate, F> &&
    requires {
        requires stdexec::sender<std::remove_cvref_t<decltype(
            std::declval<SndClsur>()(std::declval<std::invoke_result_t<F, Args...>>())
        )>>;
    };

template<typename UpStream, typename SndClsur, typename F, typename ...Args>
concept streamify_f_mono =
    !std::is_same_v<std::monostate, UpStream> &&
    std::is_same_v<std::monostate, F> &&
    requires {
        requires stdexec::sender<std::remove_cvref_t<decltype(
            std::declval<stream_sender_t<UpStream>>() | std::declval<SndClsur>()(std::declval<Args>()...)
        )>>;
    };

template<typename UpStream, typename SndClsur, typename F, typename ...Args>
concept streamify_f_ret_tuple =
    !std::is_same_v<std::monostate, UpStream> &&
    !std::is_same_v<std::monostate, F> &&
    stdplus::utils::is_tuple_like_v<std::invoke_result_t<F, Args...>> &&
    requires {
        requires stdexec::sender<std::remove_cvref_t<decltype(
            std::declval<stream_sender_t<UpStream>>() |
            std::apply(std::declval<SndClsur>(), std::declval<std::invoke_result_t<F, Args...>>())
        )>>;
    };

template<typename UpStream, typename SndClsur, typename F, typename ...Args>
concept streamify_no_mono =
    !std::is_same_v<std::monostate, UpStream> &&
    !std::is_same_v<std::monostate, F> &&
    requires {
        requires stdexec::sender<std::remove_cvref_t<decltype(
            std::declval<stream_sender_t<UpStream>>() |
            std::declval<SndClsur>()(std::declval<std::invoke_result_t<F, Args...>>())
        )>>;
    };

template<typename UpStream, typename SndClsur, typename F, typename ...Args>
concept streamifyable =
    (std::is_same_v<std::monostate, UpStream> || streamable<UpStream>) &&
    (
        streamify_up_mono_f_mono<UpStream, SndClsur, F, Args...> ||
        streamify_up_mono_f_ret_tuple<UpStream, SndClsur, F, Args...> ||
        streamify_up_mono<UpStream, SndClsur, F, Args...> ||
        streamify_f_mono<UpStream, SndClsur, F, Args...> ||
        streamify_f_ret_tuple<UpStream, SndClsur, F, Args...> ||
        streamify_no_mono<UpStream, SndClsur, F, Args...>
    );

// Stream

template<typename UpStream, typename SndClsur, typename F, typename ...Args>
requires streamifyable<UpStream, SndClsur, F, Args...>
struct streamify_stream {

    using stream_concept = stream_t;

    constexpr streamify_stream(UpStream t, SndClsur snd_clsur, F f, Args... args) :
        up_s_(std::move(t)), snd_clsur_(std::move(snd_clsur)), f_(std::move(f)), args_(std::move(args)...) {}

    constexpr auto next() noexcept {
        if constexpr (std::is_same_v<std::monostate, UpStream>) {
            if constexpr (std::is_same_v<F, std::monostate>) {
                return std::apply(snd_clsur_, args_);
            }
            else {
                if constexpr (stdplus::utils::is_tuple_like_v<std::invoke_result_t<F, Args...>>()) {
                    return std::apply(snd_clsur_, std::apply(f_, args_));
                } else {
                    return snd_clsur_(std::apply(f_, args_));
                }
            }
        }
        else {
            if constexpr (std::is_same_v<F, std::monostate>) {
                return up_s_.next() | std::apply(snd_clsur_, args_);
            }
            else {
                if constexpr (stdplus::utils::is_tuple_like_v<std::invoke_result_t<F, Args...>>()) {
                    return up_s_.next() | std::apply(snd_clsur_, std::apply(f_, args_));
                } else {
                    return up_s_.next() | snd_clsur_(std::apply(f_, args_));
                }
            }
        }
    }

private:
    UpStream up_s_;
    SndClsur snd_clsur_;
    F f_;
    std::tuple<Args...> args_;
};

// Stream closure

template<typename SndClsur, typename F, typename ...Args>
struct streamify_closure_t {
    using stream_closure_concept = stream_closure_t;

    constexpr streamify_closure_t(SndClsur snd_clsur, F f, Args... args) :
        snd_clsur_(std::move(snd_clsur)), f_(std::move(f)), args_(std::move(args)...) {}

    template<typename UpStream>
    requires streamifyable<UpStream, SndClsur, F, Args...> && streamable<UpStream>
    constexpr auto operator()(UpStream&& up_stream) const & noexcept {
        return std::apply([this, &up_stream](auto&& ...args){
            return streamify_stream<UpStream, SndClsur, F>(std::forward<UpStream>(up_stream), snd_clsur_, f_, std::forward<decltype(args)>(args)...);
        }, args_);
    }

    template<typename UpStream>
    requires streamifyable<UpStream, SndClsur, F, Args...> && streamable<UpStream>
    constexpr auto operator()(UpStream&& up_stream) && noexcept {
        return std::apply([this, &up_stream](auto&& ...args){
            return streamify_stream<UpStream, SndClsur, F>(std::forward<UpStream>(up_stream), std::move(snd_clsur_), std::move(f_), std::move(args)...);
        }, args_);
    }

private:
    SndClsur snd_clsur_;
    F f_;
    std::tuple<Args...> args_;
};

// CPOs

struct streamify_t {

    template<typename UpStream, typename SndClsur, typename F, typename ...Args>
    requires streamifyable<UpStream, std::remove_cvref_t<SndClsur>, F, Args...> && streamable<UpStream>
    constexpr auto operator()(UpStream&& t, SndClsur&& snd_clsur, F&& f, Args&&... args) const noexcept {
        return streamify_stream(std::forward<UpStream>(t), std::forward<SndClsur>(snd_clsur), std::forward<F>(f), std::forward<Args>(args)...);
    }

    template<typename SndClsur, typename F, typename ...Args>
    constexpr auto operator()(SndClsur&& snd_clsur, F&& f, Args&&... args) const noexcept {
        return streamify_closure_t<SndClsur, F, Args...>(std::forward<SndClsur>(snd_clsur), std::forward<F>(f), std::forward<Args>(args)...);
    }

};

struct streamify_args_t {
    template<typename UpStream, typename SchClusr, typename ...Args>
    constexpr auto operator()(UpStream&& t, SchClusr&& snd_clsur, Args&&... args) const noexcept {
        return streamify_t()(
            t,
            std::forward<SchClusr>(snd_clsur),
            std::monostate{},
            std::forward<Args>(args)...
        );
    }

    template<typename SchClusr, typename ...Args>
    constexpr auto operator()(SchClusr&& snd_clsur, Args&&... args) const noexcept {
        return streamify_t()(
            std::forward<SchClusr>(snd_clsur),
            std::monostate{},
            std::forward<Args>(args)...
        );
    }
};

struct schedule_s_t {

    template<typename Schd>
    constexpr auto operator()(Schd&& schd) const noexcept {
        return streamify_stream(std::monostate{}, stdexec::schedule, std::monostate{}, std::forward<Schd>(schd));
    }

};

struct just_s_t {

    template<typename ...Args>
    constexpr auto operator()(Args&&... args) const noexcept {
        return streamify_stream(std::monostate{}, stdexec::just, std::monostate{}, std::forward<Args>(args)...);
    }

};

struct just_error_s_t {

    template<typename T>
    constexpr auto operator()(T&& t) const noexcept {
        return streamify_stream(std::monostate{}, stdexec::just_error, std::monostate{}, std::forward<T>(t));
    }

};

struct just_stopped_s_t {

    template<typename T>
    constexpr auto operator()(T&& t) const noexcept {
        return streamify_stream(std::monostate{}, stdexec::just_stopped, std::monostate{});
    }

};

struct read_env_s_t {

    template<typename Tag>
    constexpr auto operator()(Tag&& tag) const noexcept {
        return streamify_stream(std::monostate{}, stdexec::read_env, std::monostate{}, std::forward<Tag>(tag));
    }

};

struct then_s_t {

    template<typename UpStream, typename F>
    constexpr auto operator()(UpStream&& t, F&& f) const noexcept {
        return streamify_args_t()(std::forward<UpStream>(t), stdexec::then, std::forward<F>(f));
    }

    template<typename F>
    constexpr auto operator()(F&& f) const noexcept {
        return streamify_args_t()(stdexec::then, std::forward<F>(f));
    }

};

struct upon_error_s_t {

    template<typename UpStream, typename F>
    constexpr auto operator()(UpStream&& t, F&& f) const noexcept {
        return streamify_args_t()(std::forward<UpStream>(t), stdexec::upon_error, std::forward<F>(f));
    }

    template<typename F>
    constexpr auto operator()(F&& f) const noexcept {
        return streamify_args_t()(stdexec::upon_error, std::forward<F>(f));
    }

};

struct upon_stopped_s_t {

    template<typename UpStream, typename F>
    constexpr auto operator()(UpStream&& t, F&& f) const noexcept {
        return streamify_args_t()(std::forward<UpStream>(t), stdexec::upon_stopped, std::forward<F>(f));
    }

    template<typename F>
    constexpr auto operator()(F&& f) const noexcept {
        return streamify_args_t()(stdexec::upon_stopped, std::forward<F>(f));
    }

};

struct let_value_s_t {

    template<typename UpStream, typename F>
    constexpr auto operator()(UpStream&& t, F&& f) const noexcept {
        return streamify_args_t()(std::forward<UpStream>(t), stdexec::let_value, std::forward<F>(f));
    }

    template<typename F>
    constexpr auto operator()(F&& f) const noexcept {
        return streamify_args_t()(stdexec::let_value, std::forward<F>(f));
    }

};

struct let_error_s_t {

    template<typename UpStream, typename F>
    constexpr auto operator()(UpStream&& t, F&& f) const noexcept {
        return streamify_args_t()(std::forward<UpStream>(t), stdexec::let_error, std::forward<F>(f));
    }

    template<typename F>
    constexpr auto operator()(F&& f) const noexcept {
        return streamify_args_t()(stdexec::let_error, std::forward<F>(f));
    }

};

struct let_stopped_s_t {

    template<typename UpStream, typename F>
    constexpr auto operator()(UpStream&& t, F&& f) const noexcept {
        return streamify_args_t()(std::forward<UpStream>(t), stdexec::let_stopped, std::forward<F>(f));
    }

    template<typename F>
    constexpr auto operator()(F&& f) const noexcept {
        return streamify_args_t()(stdexec::let_stopped, std::forward<F>(f));
    }

};

struct continues_on_s_t {

    template<typename UpStream, typename Schd>
    constexpr auto operator()(UpStream&& t, Schd&& schd) const noexcept {
        return streamify_t()(t, stdexec::continues_on, std::monostate{}, std::forward<Schd>(schd));
    }

    template<typename Schd>
    constexpr auto operator()(Schd&& schd) const noexcept {
        return streamify_t()(stdexec::continues_on, std::monostate{}, std::forward<Schd>(schd));
    }

};

struct starts_on_s_t {

    template<typename UpStream, typename Schd>
    constexpr auto operator()(UpStream&& t, Schd&& schd) const noexcept {
        return streamify_t()(t, stdexec::starts_on, std::monostate{}, std::forward<Schd>(schd));
    }

    template<typename Schd>
    constexpr auto operator()(Schd&& schd) const noexcept {
        return streamify_t()(stdexec::starts_on, std::monostate{}, std::forward<Schd>(schd));
    }

};

struct into_variant_s_t {

    template<typename UpStream>
    constexpr auto operator()(UpStream&& t) const noexcept {
        return streamify_t()(std::forward<UpStream>(t), stdexec::into_variant, std::monostate{});
    }

    constexpr auto operator()() const noexcept {
        return streamify_t()(stdexec::into_variant, std::monostate{});
    }

};

struct stopped_as_optional_s_t {

    template<typename UpStream>
    constexpr auto operator()(UpStream&& t) const noexcept {
        return streamify_t()(std::forward<UpStream>(t), stdexec::stopped_as_optional, std::monostate{});
    }

    constexpr auto operator()() const noexcept {
        return streamify_t()(stdexec::stopped_as_optional, std::monostate{});
    }

};

struct stopped_as_error_s_t {

    template<typename UpStream, typename Error>
    constexpr auto operator()(UpStream&& t, Error&& e) const noexcept {
        return streamify_t()(std::forward<UpStream>(t), stdexec::stopped_as_error, std::monostate{}, std::forward<Error>(e));
    }

    template<typename Error>
    constexpr auto operator()(Error&& e) const noexcept {
        return streamify_t()(stdexec::stopped_as_error, std::monostate{}, std::forward<Error>(e));
    }

};

struct sync_wait_s_t {

    template<typename UpStream>
    constexpr auto operator()(UpStream&& t) const noexcept {
        return streamify_t()(std::forward<UpStream>(t), stdexec::sync_wait, std::monostate{});
    }

    constexpr auto operator()() const noexcept {
        return streamify_t()(stdexec::sync_wait, std::monostate{});
    }

};

inline constexpr streamify_t streamify{};
inline constexpr streamify_args_t streamify_args{};
inline constexpr schedule_s_t schedule_s{};
inline constexpr just_s_t just_s{};
inline constexpr just_error_s_t just_error_s{};
inline constexpr just_stopped_s_t just_stopped_s{};
inline constexpr read_env_s_t read_env_s{};
inline constexpr then_s_t then_s{};
inline constexpr upon_error_s_t upon_error_s{};
inline constexpr upon_stopped_s_t upon_stopped_s{};
inline constexpr let_value_s_t let_value_s{};
inline constexpr let_error_s_t let_error_s{};
inline constexpr let_stopped_s_t let_stopped_s{};
inline constexpr continues_on_s_t continues_on_s{};
inline constexpr starts_on_s_t starts_on_s{};
inline constexpr into_variant_s_t into_variant_s{};
inline constexpr stopped_as_optional_s_t stopped_as_optional_s{};
inline constexpr stopped_as_error_s_t stopped_as_error_s{};
inline constexpr sync_wait_s_t sync_wait_s{};

} // namespace __streamify

using __streamify::streamify;
using __streamify::streamify_args;
using __streamify::schedule_s;
using __streamify::just_s;
using __streamify::just_error_s;
using __streamify::just_stopped_s;
using __streamify::read_env_s;
using __streamify::then_s;
using __streamify::upon_error_s;
using __streamify::upon_stopped_s;
using __streamify::let_value_s;
using __streamify::let_error_s;
using __streamify::let_stopped_s;
using __streamify::continues_on_s;
using __streamify::starts_on_s;
using __streamify::into_variant_s;
using __streamify::stopped_as_optional_s;
using __streamify::stopped_as_error_s;
using __streamify::sync_wait_s;

} // namespace streamifex
} // namespace stdplus
} // namespace snapflakes
