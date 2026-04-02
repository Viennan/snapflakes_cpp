#pragma once

#include <exception>
#include <functional>
#include <type_traits>
#include <utility>
#include <snapflakes/stdplus/utilities/initialization.hpp>
#include <snapflakes/stdplus/exec/std.hpp>

namespace snapflakes {
namespace stdplus {
namespace execution {

namespace __then_rsh {

/// then_rsh: Reschedule-then-transform sender adaptor
///
/// Like `stdexec::then`, but always reschedules on the downstream receiver's
/// scheduler before invoking the transformation function. This ensures the
/// functor executes in the downstream's execution context.
///
/// Behavior:
/// - On set_value: reschedules to downstream's scheduler, then invokes F with
///   the upstream values, forwarding the result downstream
/// - On set_error/set_stopped: passes through without rescheduling
///
/// The scheduler is obtained from the downstream receiver's environment via
/// `stdexec::get_scheduler(stdexec::get_env(receiver))` at connect time.
///
/// Usage:
///   auto s = some_sender | then_rsh([](int x) { return x * 2; });
///   auto s = then_rsh(some_sender, [](int x) { return x * 2; });
///
/// Note: Scheduler customizations of `stdexec::then` do not affect `then_rsh`.
/// Only direct customizations of `then_rsh` itself will be applied.

// Helper: compute value completion signature after applying F to Args...
template<typename F, typename... Args>
using then_rsh_value_sig_t = std::conditional_t<
    std::is_void_v<std::invoke_result_t<F, Args...>>,
    stdexec::completion_signatures<stdexec::set_value_t()>,
    stdexec::completion_signatures<stdexec::set_value_t(std::invoke_result_t<F, Args...>)>
>;

template<typename F>
struct value_transform {
    template<typename... Args>
    using __f = then_rsh_value_sig_t<F, Args...>;
};

// Intermediate receiver: invokes F on set_value, passes through error/stopped
template<typename F, typename Downstream>
struct then_rsh_recr {
    using receiver_concept = stdexec::receiver_t;

    F f_;
    Downstream downstream_;

    constexpr auto get_env() const noexcept {
        return stdexec::get_env(downstream_);
    }

    template<typename... Args>
    constexpr void set_value(Args&&... args) noexcept {
        try {
            if constexpr (std::is_void_v<std::invoke_result_t<F, Args...>>) {
                std::invoke(std::move(f_), std::forward<Args>(args)...);
                stdexec::set_value(std::move(downstream_));
            } else {
                stdexec::set_value(std::move(downstream_),
                    std::invoke(std::move(f_), std::forward<Args>(args)...));
            }
        } catch (...) {
            stdexec::set_error(std::move(downstream_), std::current_exception());
        }
    }

    template<typename E>
    constexpr void set_error(E&& e) noexcept {
        stdexec::set_error(std::move(downstream_), std::forward<E>(e));
    }

    constexpr void set_stopped() noexcept {
        stdexec::set_stopped(std::move(downstream_));
    }
};

// Operation state
template<typename Upstream, typename F, typename Downstream>
struct then_rsh_op : stdplus::utils::immoveable {
    using env_t = decltype(stdexec::get_env(std::declval<Downstream&>()));
    using sched_t = decltype(stdexec::get_scheduler(std::declval<env_t>()));
    using resched_t = decltype(stdexec::continues_on(std::declval<Upstream>(), std::declval<sched_t>()));
    using recr_t = then_rsh_recr<F, Downstream>;
    using op_t = stdexec::connect_result_t<resched_t, recr_t>;

    op_t op_;

    template<typename U, typename G, typename R>
    static constexpr op_t make_op(U&& upstream, G&& f, R&& downstream) noexcept{
        auto sched = stdexec::get_scheduler(stdexec::get_env(downstream));
        return stdexec::connect(
            stdexec::continues_on(std::forward<U>(upstream), std::move(sched)),
            recr_t{std::forward<G>(f), std::forward<R>(downstream)}
        );
    }

    template<typename U, typename G, typename R>
    constexpr then_rsh_op(U&& upstream, G&& f, R&& downstream)
        : op_(make_op(std::forward<U>(upstream), std::forward<G>(f), std::forward<R>(downstream))) {}

    constexpr void start() noexcept {
        stdexec::start(op_);
    }
};

// Sender
template<stdexec::sender Upstream, typename F>
class then_rsh_sender {
    Upstream upstream_;
    F f_;

public:
    using sender_concept = stdexec::sender_t;

    constexpr then_rsh_sender(Upstream u, F f)
        : upstream_(std::move(u)), f_(std::move(f)) {}

    template<typename Env>
    constexpr static auto get_completion_signatures(Env&&) noexcept {
        using sched_t = decltype(stdexec::get_scheduler(std::declval<const Env&>()));
        using resched_t = decltype(stdexec::continues_on(std::declval<Upstream>(), std::declval<sched_t>()));
        // Transform value completions through F, add std::exception_ptr for functor exceptions
        return execution::transform_completion_signatures<
            resched_t, Env,
            stdexec::completion_signatures<stdexec::set_error_t(std::exception_ptr)>,
            value_transform<F>::template __f
        >();
    }

    template<stdexec::receiver Downstream>
    constexpr auto connect(Downstream&& recv) const & noexcept {
        return then_rsh_op<Upstream, F, std::remove_cvref_t<Downstream>>{
            upstream_, f_, std::forward<Downstream>(recv)
        };
    }

    template<stdexec::receiver Downstream>
    constexpr auto connect(Downstream&& recv) && noexcept {
        return then_rsh_op<Upstream, F, std::remove_cvref_t<Downstream>>{
            std::move(upstream_), std::move(f_), std::forward<Downstream>(recv)
        };
    }
};

// Pipe closure
template<typename F>
struct then_rsh_closure : stdexec::sender_adaptor_closure<then_rsh_closure<F>> {
    F f_;

    constexpr explicit then_rsh_closure(F f) : f_(std::move(f)) {}

    template<stdexec::sender Upstream>
    constexpr auto operator()(Upstream&& up) const & noexcept {
        return then_rsh_sender {
            std::forward<Upstream>(up), f_
        };    
    }

    template<stdexec::sender Upstream>
    constexpr auto operator()(Upstream&& up) && noexcept {
        return then_rsh_sender {
            std::forward<Upstream>(up), std::move(f_)
        };
    }
};

// CPO
struct then_rsh_t {
    template<stdexec::sender Upstream, typename F>
    constexpr auto operator()(Upstream&& up, F&& f) const noexcept {
        return then_rsh_sender {
            std::forward<Upstream>(up), std::forward<F>(f)
        };
    }

    template<typename F>
    constexpr auto operator()(F&& f) const noexcept {
        return then_rsh_closure{ std::forward<F>(f) };
    }
};

inline constexpr then_rsh_t then_rsh{};

} // namespace __then_rsh

using __then_rsh::then_rsh;

} // namespace execution
} // namespace stdplus
} // namespace snapflakes
