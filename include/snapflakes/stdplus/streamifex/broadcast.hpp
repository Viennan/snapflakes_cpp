#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include <snapflakes/stdplus/exec/std.hpp>
#include <snapflakes/stdplus/exec/trampoline_scheduler.hpp>
#include <snapflakes/stdplus/exec/utils/basic_completions.hpp>
#include <snapflakes/stdplus/streamifex/stream_fwd.hpp>
#include <snapflakes/stdplus/utilities/initialization.hpp>
#include <snapflakes/stdplus/utilities/list.hpp>

namespace snapflakes {
namespace stdplus {
namespace streamifex {

namespace __broadcast_stream {

template<typename Completion>
constexpr bool completion_is_terminal(const Completion& completion) noexcept {
    return completion.has_error() || completion.has_stopped();
}

template<typename UpStream, typename Env = stdexec::env<>>
class broadcast_shared_state;

template<typename UpStream, typename Env = stdexec::env<>>
class broadcast_branch_stream;

// Shared broadcast hub state.
//
// This object owns:
// - the single upstream stream instance
// - the bounded cache of upstream completions
// - the active downstream branches
// - the list of blocked downstream operations waiting for progress
//
// The core invariants are:
// - only one upstream `next()` operation is in flight at a time
// - each branch has at most one active downstream operation at a time
// - no `start()` or receiver completion is performed while holding `mutex_`
// - `Env` fixes the cached completion shape, while the live upstream receiver
//   still forwards the actual downstream env at connect time
template<typename UpStream, typename Env>
class broadcast_shared_state:
    public stdplus::utils::immoveable_and_noncopyable,
    public std::enable_shared_from_this<broadcast_shared_state<UpStream, Env>> {
public:
    using upstream_stream_t = UpStream;
    using upstream_sender_t = stream_sender_t<upstream_stream_t>;
    using completion_t = execution::utils::basic_any_completions<upstream_sender_t, Env>;
    using branch_list_element_t = stdplus::utils::static_double_linked_list_element;
    using waiter_base_t = stdplus::utils::invokeable_list_element_base<void>;

    struct branch_state;
    struct waiter_operation_base: waiter_base_t {
        constexpr waiter_operation_base() = default;

        constexpr explicit waiter_operation_base(typename waiter_base_t::f f) noexcept:
            waiter_base_t(f) {}

        bool waiting_ = false;
        std::atomic_bool upstream_completed_{false};
        std::atomic_bool starting_upstream_{false};
        std::atomic_bool deferred_resume_{false};
    };

    using branch_list_t = stdplus::utils::static_double_linked_list<branch_list_element_t>;
    using waiter_list_t = stdplus::utils::static_double_linked_list<waiter_operation_base>;

    struct slot_t {
        // `ref_count_` tracks how many currently registered branches still need
        // to consume this cached completion before the slot can be dropped.
        constexpr slot_t(std::size_t seq, std::size_t ref_count, const completion_t& completion) noexcept:
            seq_(seq), ref_count_(ref_count), completion_(completion) {}

        constexpr slot_t(std::size_t seq, std::size_t ref_count, completion_t&& completion) noexcept:
            seq_(seq), ref_count_(ref_count), completion_(std::move(completion)) {}

        std::size_t seq_ = 0;
        std::size_t ref_count_ = 0;
        completion_t completion_;
    };

    struct branch_state: branch_list_element_t {
        constexpr branch_state(
            std::shared_ptr<broadcast_shared_state> hub,
            std::size_t next_seq,
            bool registered,
            bool late_terminal) noexcept:
            hub_(std::move(hub)),
            next_seq_(next_seq),
            registered_(registered),
            late_terminal_(late_terminal) {}

        ~branch_state() {
            if (hub_) {
                hub_->detach_branch(this);
            }
        }

        std::shared_ptr<broadcast_shared_state> hub_;
        std::size_t next_seq_ = 0;
        // Tracks the single downstream op that is currently allowed to drive
        // this branch. This matches the user's rule that a branch will not call
        // `next()` again before the previous result is observed.
        waiter_operation_base* active_op_ = nullptr;
        bool inflight_ = false;
        bool closed_ = false;
        bool registered_ = false;
        bool late_terminal_ = false;
    };

    constexpr broadcast_shared_state(upstream_stream_t upstream, std::size_t max_gap) noexcept:
        upstream_(std::move(upstream)),
        max_gap_(max_gap),
        branches_(&branch_sentinel_),
        waiters_(&waiter_sentinel_) {
        branches_.set_sentinel(&branch_sentinel_);
        waiters_.set_sentinel(&waiter_sentinel_);
    }

    constexpr auto make_branch() {
        std::scoped_lock lock(mutex_);

        const bool late_terminal = terminated_ && final_completion_.has_value();
        const std::size_t next_seq =
            late_terminal && tail_seq_ > 0 ? tail_seq_ - 1 : tail_seq_;

        // Late subscribers only observe the terminal completion. Live
        // subscribers are registered into the normal branch list.
        auto branch = std::make_shared<branch_state>(
            this->shared_from_this(), next_seq, !late_terminal, late_terminal);

        if (!late_terminal) {
            branches_.push_back(branch.get());
        }

        return branch;
    }

    constexpr void detach_branch(branch_state* branch) noexcept {
        std::vector<waiter_operation_base*> waiters;
        {
            std::scoped_lock lock(mutex_);
            if (!branch->registered_) {
                branch->closed_ = true;
                return;
            }

            branches_.erase(branch);
            branch->registered_ = false;
            branch->closed_ = true;

            // A detached branch relinquishes every unread cached completion so
            // those slots can be reclaimed once all remaining branches catch up.
            for (std::size_t seq = std::max(branch->next_seq_, head_seq_);
                 seq < tail_seq_; ++seq) {
                auto& slot = cache_.at(seq - head_seq_);
                if (slot.ref_count_ > 0) {
                    --slot.ref_count_;
                }
            }

            pop_exhausted_slots_locked();
            collect_waiters_locked(waiters);
        }

        invoke_waiters(waiters);
    }

    constexpr bool gap_blocked_locked(const branch_state& branch) const noexcept {
        if (!branch.registered_ || branches_.empty()) {
            return false;
        }

        auto slowest = tail_seq_;
        for (auto* node = branches_.front(); node != &branch_sentinel_; node = node->get_next()) {
            slowest = std::min(slowest, static_cast<const branch_state*>(node)->next_seq_);
        }

        return branch.next_seq_ - slowest >= max_gap_;
    }

    constexpr bool has_cached_locked(std::size_t seq) const noexcept {
        return seq >= head_seq_ && seq < tail_seq_;
    }

    constexpr completion_t reserve_locked(branch_state& branch) noexcept {
        if (branch.late_terminal_ && final_completion_.has_value()) {
            branch.late_terminal_ = false;
            branch.closed_ = true;
            branch.next_seq_ = tail_seq_;
            return *final_completion_;
        }

        auto& slot = cache_.at(branch.next_seq_ - head_seq_);
        auto completion = slot.ref_count_ > 1 ? completion_t(slot.completion_) : std::move(slot.completion_);
        --slot.ref_count_;
        ++branch.next_seq_;

        if (completion_is_terminal(completion)) {
            if (branch.registered_) {
                branches_.erase(&branch);
                branch.registered_ = false;
            }
            branch.closed_ = true;
        }

        pop_exhausted_slots_locked();
        return completion;
    }

    constexpr void enqueue_waiter_locked(waiter_operation_base* waiter) noexcept {
        if (!waiter->waiting_) {
            waiters_.push_back(waiter);
            waiter->waiting_ = true;
        }
    }

    constexpr void collect_waiters_locked(std::vector<waiter_operation_base*>& waiters) noexcept {
        while (!waiters_.empty()) {
            auto* waiter = waiters_.pop_front_value();
            waiter->waiting_ = false;
            waiters.push_back(waiter);
        }
    }

    static constexpr void invoke_waiters(std::vector<waiter_operation_base*>& waiters) noexcept {
        for (auto* waiter : waiters) {
            waiter->invoke();
        }
    }

    constexpr void finish_upstream(
        waiter_operation_base* owner,
        branch_state& winner_branch,
        completion_t&& completion) noexcept {
        std::vector<waiter_operation_base*> waiters;
        {
            std::scoped_lock lock(mutex_);
            upstream_inflight_ = false;
            if (owner != nullptr) {
                owner->upstream_completed_.store(true, std::memory_order_release);
            }

            std::size_t follower_ref_count = branches_.size();
            if (winner_branch.registered_ && follower_ref_count > 0) {
                --follower_ref_count;
            }

            ++winner_branch.next_seq_;

            if (completion_is_terminal(completion)) {
                terminated_ = true;
                final_completion_ = completion;

                if (winner_branch.registered_) {
                    branches_.erase(&winner_branch);
                    winner_branch.registered_ = false;
                }
                winner_branch.closed_ = true;
            }

            cache_.emplace_back(tail_seq_, follower_ref_count, std::move(completion));
            ++tail_seq_;
            pop_exhausted_slots_locked();
            collect_waiters_locked(waiters);
        }

        // Resume blocked downstream work only after leaving the critical
        // section so downstream connect/start/completion paths cannot deadlock
        // with the broadcast mutex.
        invoke_waiters(waiters);
    }

    constexpr auto next_upstream() noexcept {
        return upstream_.next();
    }

    constexpr void pop_exhausted_slots_locked() noexcept {
        while (!cache_.empty() && cache_.front().ref_count_ == 0) {
            cache_.pop_front();
            ++head_seq_;
        }
    }

    upstream_stream_t upstream_;
    std::size_t max_gap_ = 0;
    std::mutex mutex_;
    std::deque<slot_t> cache_;
    std::size_t head_seq_ = 0;
    std::size_t tail_seq_ = 0;
    bool terminated_ = false;
    bool upstream_inflight_ = false;
    std::optional<completion_t> final_completion_;
    branch_list_element_t branch_sentinel_{};
    branch_list_t branches_;
    waiter_operation_base waiter_sentinel_{};
    waiter_list_t waiters_;
};

/// broadcast_branch_stream: one downstream-facing view into a broadcast hub
///
/// Each branch behaves like a normal stream. Calling `next()` returns a sender
/// whose operation state is owned by the downstream caller. If the branch is
/// too far ahead of the slowest consumer, that sender does not complete until
/// the lag shrinks below `max_gap`.
template<typename UpStream, typename Env>
class broadcast_branch_stream {
    using shared_state_t = broadcast_shared_state<UpStream, Env>;
    using branch_state_t = typename shared_state_t::branch_state;
    using completion_t = typename shared_state_t::completion_t;

    template<typename Receiver>
    struct next_operation_state;

    struct next_sender_t {
        using sender_concept = stdexec::sender_t;
        using completion_signatures_t = typename completion_t::completion_signatures_t;

        constexpr explicit next_sender_t(const std::shared_ptr<branch_state_t>& branch) noexcept:
            branch_(branch) {}

        constexpr explicit next_sender_t(std::shared_ptr<branch_state_t>&& branch) noexcept:
            branch_(std::move(branch)) {}

        template<typename Receiver>
        constexpr auto connect(Receiver receiver) const & noexcept {
            return next_operation_state<std::remove_cvref_t<Receiver>>(branch_, std::move(receiver));
        }

        template<typename Receiver>
        constexpr auto connect(Receiver receiver) && noexcept {
            return next_operation_state<std::remove_cvref_t<Receiver>>(std::move(branch_), std::move(receiver));
        }

        static consteval completion_signatures_t get_completion_signatures() noexcept {
            return {};
        }

        template<typename AnyEnv>
        static consteval completion_signatures_t get_completion_signatures() noexcept {
            return {};
        }

        std::shared_ptr<branch_state_t> branch_;
    };

    template<typename Receiver>
    struct next_operation_state:
        shared_state_t::waiter_operation_base {
        using receiver_t = Receiver;
        using env_t = std::remove_cvref_t<decltype(stdexec::get_env(std::declval<receiver_t&>()))>;
        using scheduler_t = execution::trampoline_scheduler;
        using schedule_sender_t = decltype(stdexec::schedule(std::declval<scheduler_t>()));
        using actual_completion_t = execution::utils::basic_any_completions<typename shared_state_t::upstream_sender_t, env_t>;
        using value_t = typename completion_t::value_t;
        using error_t = typename completion_t::error_t;

        struct completion_cleanup {
            constexpr void operator()() const noexcept {
                self_->finish_delivery();
            }

            next_operation_state* self_ = nullptr;
        };

        using guarded_receiver_t = execution::utils::basic_completion_guard_receiver<receiver_t, completion_cleanup>;

        struct scheduled_receiver {
            using receiver_concept = stdexec::receiver_t;

            constexpr auto get_env() const noexcept {
                return stdexec::env<>{};
            }

            template<typename ...Args>
            constexpr void set_value(Args&&...) noexcept {
                self_->replay_ready_result();
            }

            template<typename ...Args>
            constexpr void set_error(Args&& ...args) noexcept {
                auto receiver = guarded_receiver_t(std::move(self_->receiver_), completion_cleanup{self_});
                stdexec::set_error(std::move(receiver), std::forward<Args>(args)...);
            }

            constexpr void set_stopped() noexcept {
                auto receiver = guarded_receiver_t(std::move(self_->receiver_), completion_cleanup{self_});
                stdexec::set_stopped(std::move(receiver));
            }

            next_operation_state* self_ = nullptr;
        };

        struct upstream_receiver {
            using receiver_concept = stdexec::receiver_t;

            constexpr env_t get_env() const noexcept {
                return self_->get_downstream_env();
            }

            template<typename ...Args>
            constexpr void set_value(Args&& ...args) noexcept {
                self_->finish_upstream_value(std::forward<Args>(args)...);
            }

            template<typename ...Args>
            constexpr void set_error(Args&& ...args) noexcept {
                self_->finish_upstream_error(std::forward<Args>(args)...);
            }

            constexpr void set_stopped() noexcept {
                self_->finish_upstream_stopped();
            }

            next_operation_state* self_ = nullptr;
        };

        using delivery_op_t = stdexec::connect_result_t<schedule_sender_t, scheduled_receiver>;
        using upstream_sender_t = typename shared_state_t::upstream_sender_t;
        using upstream_op_t = stdexec::connect_result_t<upstream_sender_t, upstream_receiver>;

        static_assert(
            std::same_as<typename actual_completion_t::value_t, typename completion_t::value_t> &&
            std::same_as<typename actual_completion_t::error_t, typename completion_t::error_t> &&
            std::same_as<typename actual_completion_t::completion_signatures_t, typename completion_t::completion_signatures_t>,
            "broadcast downstream receiver env is not compatible with the Env selected for this broadcast hub");

        struct delivery_state {
            // Broadcast only needs a recursion barrier. If downstream wants to
            // migrate onto a real scheduler, it can compose that explicitly
            // after the branch stream.
            constexpr explicit delivery_state(next_operation_state* self) noexcept:
                op_(stdexec::connect(
                    stdexec::schedule(scheduler_t{}),
                    scheduled_receiver{self})) {}

            constexpr void start() noexcept {
                stdexec::start(op_);
            }

            delivery_op_t op_;
        };

        struct upstream_state {
            constexpr upstream_state(upstream_sender_t sender, next_operation_state* self) noexcept:
                op_(stdexec::connect(std::move(sender), upstream_receiver{self})) {}

            constexpr void start() noexcept {
                stdexec::start(op_);
            }

            upstream_op_t op_;
        };

        constexpr next_operation_state(const std::shared_ptr<branch_state_t>& branch, receiver_t receiver) noexcept:
            shared_state_t::waiter_operation_base(&next_operation_state::resume),
            branch_(branch),
            receiver_(std::move(receiver)) {}

        constexpr next_operation_state(std::shared_ptr<branch_state_t>&& branch, receiver_t receiver) noexcept:
            shared_state_t::waiter_operation_base(&next_operation_state::resume),
            branch_(std::move(branch)),
            receiver_(std::move(receiver)) {}

        constexpr void start() noexcept {
            try_progress();
        }

        static constexpr void resume(typename shared_state_t::waiter_base_t* base) noexcept {
            static_cast<next_operation_state*>(base)->try_progress();
        }

        constexpr env_t get_downstream_env() const noexcept {
            return stdexec::get_env(receiver_);
        }

        constexpr void try_progress() noexcept {
            if (this->starting_upstream_.load(std::memory_order_acquire)) {
                this->deferred_resume_.store(true, std::memory_order_release);
                return;
            }

            auto* state = branch_->hub_.get();
            std::optional<completion_t> completion;
            bool should_start_upstream = false;
            bool should_reset_upstream = this->upstream_completed_.exchange(false, std::memory_order_acq_rel);

            if (should_reset_upstream) {
                upstream_state_.reset();
            }

            if (has_ready_result()) {
                start_delivery();
                return;
            }

            {
                std::scoped_lock lock(state->mutex_);

                if (branch_->active_op_ == nullptr) {
                    branch_->active_op_ = this;
                    branch_->inflight_ = true;
                } else if (branch_->active_op_ != this) {
                    return;
                }

                // Priority:
                // 1. consume already cached completions
                // 2. observe the terminal completion for late/closed branches
                // 3. block if this branch is already too far ahead
                // 4. otherwise request one more upstream item
                if (branch_->late_terminal_ && state->final_completion_.has_value()) {
                    completion.emplace(state->reserve_locked(*branch_));
                } else if (branch_->closed_ && state->final_completion_.has_value()) {
                    completion.emplace(*state->final_completion_);
                } else if (state->has_cached_locked(branch_->next_seq_)) {
                    completion.emplace(state->reserve_locked(*branch_));
                } else if (state->terminated_ && state->final_completion_.has_value()) {
                    completion.emplace(*state->final_completion_);
                    branch_->closed_ = true;
                } else if (state->gap_blocked_locked(*branch_)) {
                    state->enqueue_waiter_locked(this);
                } else {
                    if (!state->upstream_inflight_) {
                        state->enqueue_waiter_locked(this);
                        state->upstream_inflight_ = true;
                        this->starting_upstream_.store(true, std::memory_order_release);
                        should_start_upstream = true;
                    } else {
                        state->enqueue_waiter_locked(this);
                    }
                }
            }

            if (should_start_upstream) {
                auto sender = state->next_upstream();
                upstream_state_.emplace(std::move(sender), this);
                upstream_state_->start();
                this->starting_upstream_.store(false, std::memory_order_release);

                if (this->deferred_resume_.exchange(false, std::memory_order_acq_rel)) {
                    try_progress();
                    return;
                }
            }

            if (completion.has_value()) {
                ready_completion_.emplace(std::move(*completion));
                start_delivery();
            }
        }

        constexpr bool has_ready_result() const noexcept {
            return ready_completion_.has_value() || ready_value_.has_value() ||
                ready_error_.has_value() || ready_stopped_;
        }

        constexpr void start_delivery() noexcept {
            // Always break the inline upstream->downstream chain with a
            // trampoline hop so broadcast cannot recurse without bound.
            if (!delivery_state_.has_value()) {
                delivery_state_.emplace(this);
                delivery_state_->start();
            }
        }

        constexpr void replay_ready_result() noexcept {
            if (ready_completion_.has_value()) {
                auto op = stdexec::connect(
                    std::move(*ready_completion_),
                    guarded_receiver_t(std::move(receiver_), completion_cleanup{this}));
                stdexec::start(op);
                return;
            }

            auto receiver = guarded_receiver_t(std::move(receiver_), completion_cleanup{this});
            if (ready_error_.has_value()) {
                execution::utils::basic_set_any_error(std::move(receiver), std::move(*ready_error_));
                return;
            }

            if (ready_stopped_) {
                stdexec::set_stopped(std::move(receiver));
                return;
            }

            execution::utils::basic_set_any_value(std::move(receiver), std::move(*ready_value_));
        }

        template<typename ...Args>
        constexpr void finish_upstream_value(Args&& ...args) noexcept {
            ready_value_.emplace(execution::utils::to_variant<value_t>(std::forward<Args>(args)...));

            completion_t completion(get_downstream_env());
            completion.store_value(*ready_value_);
            branch_->hub_->finish_upstream(this, *branch_, std::move(completion));
        }

        template<typename ...Args>
        constexpr void finish_upstream_error(Args&& ...args) noexcept {
            ready_error_.emplace(execution::utils::to_variant<error_t>(std::forward<Args>(args)...));

            completion_t completion(get_downstream_env());
            completion.store_error(*ready_error_);
            branch_->hub_->finish_upstream(this, *branch_, std::move(completion));
        }

        constexpr void finish_upstream_stopped() noexcept {
            ready_stopped_ = true;

            completion_t completion(get_downstream_env());
            completion.store_stopped();
            branch_->hub_->finish_upstream(this, *branch_, std::move(completion));
        }

        constexpr void finish_delivery() noexcept {
            std::vector<typename shared_state_t::waiter_operation_base*> waiters;
            {
                std::scoped_lock lock(branch_->hub_->mutex_);
                if (branch_->active_op_ == this) {
                    branch_->active_op_ = nullptr;
                    branch_->inflight_ = false;
                }
                branch_->hub_->collect_waiters_locked(waiters);
            }

            shared_state_t::invoke_waiters(waiters);
        }

        std::shared_ptr<branch_state_t> branch_;
        receiver_t receiver_;
        std::optional<completion_t> ready_completion_;
        std::optional<value_t> ready_value_;
        std::optional<error_t> ready_error_;
        bool ready_stopped_ = false;
        std::optional<upstream_state> upstream_state_;
        // The scheduler hop op-state stays inside the downstream-owned
        // operation state so we keep concrete types without type erasure.
        std::optional<delivery_state> delivery_state_;
    };

public:
    using stream_concept = stream_t;

    constexpr explicit broadcast_branch_stream(std::shared_ptr<branch_state_t> branch) noexcept:
        branch_(std::move(branch)) {}

    constexpr auto next() noexcept {
        return next_sender_t(branch_);
    }

private:
    std::shared_ptr<branch_state_t> branch_;
};

/// broadcast_hub: stage-one broadcast object
///
/// Constructed by `broadcast_from`. Each call to `subscribe()` or
/// `broadcast_to(hub)` creates a new branch stream that consumes from the same
/// upstream source and shared cache.
template<typename UpStream, typename Env>
class broadcast_hub {
public:
    using shared_state_t = broadcast_shared_state<UpStream, Env>;

    constexpr explicit broadcast_hub(std::shared_ptr<shared_state_t> state) noexcept:
        state_(std::move(state)) {}

    constexpr auto subscribe() const noexcept {
        return broadcast_branch_stream<UpStream, Env>(state_->make_branch());
    }

private:
    std::shared_ptr<shared_state_t> state_;
};

/// broadcast_from: convert one upstream stream into a shareable broadcast hub
///
/// The returned hub can be subscribed multiple times. Each branch sees the same
/// sequence of upstream completions, while the hub enforces that no branch can
/// get more than `max_gap` items ahead of the slowest live branch.
///
/// Usage:
///   auto hub = broadcast_from(upstream_stream, 8);
///   auto left = broadcast_to(hub);
///   auto right = broadcast_to(hub);
///
///   auto hub2 = some_stream | broadcast_from(8);
struct broadcast_from_t {
    template<typename Env = stdexec::env<>, typename UpStream>
    requires streamable<UpStream>
    constexpr auto operator()(UpStream&& upstream, std::size_t max_gap) const noexcept {
        using stream_t = std::remove_cvref_t<UpStream>;
        using hub_t = broadcast_hub<stream_t, Env>;
        using state_t = typename hub_t::shared_state_t;

        return hub_t(std::make_shared<state_t>(std::forward<UpStream>(upstream), max_gap));
    }

    template<typename Env = stdexec::env<>>
    struct closure_t {
        using stream_closure_concept = stream_closure_t;

        constexpr explicit closure_t(std::size_t max_gap) noexcept:
            max_gap_(max_gap) {}

        template<typename UpStream>
        requires streamable<UpStream>
        constexpr auto operator()(UpStream&& upstream) const noexcept {
            return broadcast_from_t{}.template operator()<Env>(std::forward<UpStream>(upstream), max_gap_);
        }

        std::size_t max_gap_ = 0;
    };

    template<typename Env = stdexec::env<>>
    constexpr auto operator()(std::size_t max_gap) const noexcept {
        return closure_t<Env>(max_gap);
    }
};

/// broadcast_to: subscribe a new downstream branch to a broadcast hub
///
/// The returned branch is itself a stream and can be consumed with repeated
/// `next()` calls just like any other stream in this library.
struct broadcast_to_t {
    template<typename Hub>
    requires requires(Hub&& hub) { std::forward<Hub>(hub).subscribe(); }
    constexpr auto operator()(Hub&& hub) const noexcept {
        return std::forward<Hub>(hub).subscribe();
    }
};

inline constexpr broadcast_from_t broadcast_from{};
inline constexpr broadcast_to_t broadcast_to{};

} // namespace __broadcast_stream

using __broadcast_stream::broadcast_from;
using __broadcast_stream::broadcast_to;
using __broadcast_stream::broadcast_hub;
using __broadcast_stream::broadcast_branch_stream;

} // namespace streamifex
} // namespace stdplus
} // namespace snapflakes
