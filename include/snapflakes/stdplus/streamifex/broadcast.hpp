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
constexpr bool completion_is_terminal(const Completion &completion) noexcept {
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
class broadcast_shared_state : public stdplus::utils::immoveable_and_noncopyable, public std::enable_shared_from_this<broadcast_shared_state<UpStream, Env>> {
public:
    using upstream_stream_t = UpStream;
    using upstream_sender_t = stream_sender_t<upstream_stream_t>;
    using completion_t = execution::utils::basic_any_completions<upstream_sender_t, Env>;
    using branch_list_element_t = stdplus::utils::static_double_linked_list_element;
    using waiter_base_t = stdplus::utils::invokeable_list_element_base<void>;

    struct branch_state;
    struct waiter_operation_base : waiter_base_t {
        constexpr waiter_operation_base() = default;

        constexpr explicit waiter_operation_base(typename waiter_base_t::f f) noexcept : waiter_base_t(f) {
        }

        bool waiting_ = false;
        // Coalesces concurrent `resume()` calls so only one thread drains the
        // local progress loop at a time.
        std::atomic<std::size_t> resume_counter_{0};
    };

    using branch_list_t = stdplus::utils::static_double_linked_list<branch_list_element_t>;
    using waiter_list_t = stdplus::utils::static_double_linked_list<waiter_operation_base>;

    struct slot_t {
        // `ref_count_` tracks how many currently registered branches still need
        // to consume this cached completion before the slot can be dropped.
        constexpr slot_t(std::size_t seq, std::size_t ref_count, const completion_t &completion) noexcept : seq_(seq), ref_count_(ref_count), completion_(completion) {
        }

        constexpr slot_t(std::size_t seq, std::size_t ref_count, completion_t &&completion) noexcept : seq_(seq), ref_count_(ref_count), completion_(std::move(completion)) {
        }

        std::size_t seq_ = 0;
        std::size_t ref_count_ = 0;
        completion_t completion_;
    };

    struct branch_state : branch_list_element_t {
        constexpr branch_state(const std::shared_ptr<broadcast_shared_state> &hub, std::size_t next_seq, bool registered, bool late_terminal) noexcept
            : hub_(hub), next_seq_(next_seq), registered_(registered), late_terminal_(late_terminal) {
        }

        ~branch_state() {
            if (hub_) {
                hub_->detach_branch(this);
            }
        }

        constexpr bool try_activate(waiter_operation_base *op) noexcept {
            if (active_op_ == nullptr) {
                active_op_ = op;
            }
            return active_op_ == op;
        }

        constexpr void release(waiter_operation_base *op) noexcept {
            if (active_op_ == op) {
                active_op_ = nullptr;
            }
        }

        constexpr void advance() noexcept {
            ++next_seq_;
        }

        constexpr void mark_closed() noexcept {
            closed_ = true;
        }

        constexpr void consume_late_terminal(std::size_t tail_seq) noexcept {
            late_terminal_ = false;
            closed_ = true;
            next_seq_ = tail_seq;
        }

        constexpr void unregister(branch_list_t &branches) noexcept {
            if (registered_) {
                branches.erase(this);
                registered_ = false;
            }
        }

        constexpr void detach(branch_list_t &branches) noexcept {
            unregister(branches);
            closed_ = true;
        }

        constexpr void close_terminal(branch_list_t &branches) noexcept {
            unregister(branches);
            closed_ = true;
            late_terminal_ = false;
        }

        std::shared_ptr<broadcast_shared_state> hub_;
        std::size_t next_seq_ = 0;
        // Tracks the single downstream op that is currently allowed to drive
        // this branch. This matches the user's rule that a branch will not call
        // `next()` again before the previous result is observed.
        waiter_operation_base *active_op_ = nullptr;
        bool closed_ = false;
        bool registered_ = false;
        bool late_terminal_ = false;
    };

    constexpr broadcast_shared_state(upstream_stream_t upstream, std::size_t max_gap) noexcept
        : upstream_(std::move(upstream)),
          // A zero gap would deadlock the very first request, so broadcast
          // always normalizes it to the smallest meaningful window.
          max_gap_(std::max<std::size_t>(1, max_gap)) {
    }

    constexpr auto make_branch() {
        std::scoped_lock lock(mutex_);

        const bool late_terminal = terminated_ && final_completion_.has_value();
        const std::size_t next_seq = late_terminal && tail_seq_ > 0 ? tail_seq_ - 1 : tail_seq_;

        // Late subscribers only observe the terminal completion. Live
        // subscribers are registered into the normal branch list.
        auto branch = std::make_shared<branch_state>(this->shared_from_this(), next_seq, !late_terminal, late_terminal);

        if (!late_terminal) {
            branches_.push_back(branch.get());
        }

        return branch;
    }

    constexpr void detach_branch(branch_state *branch) noexcept {
        std::vector<waiter_operation_base *> waiters;
        {
            std::scoped_lock lock(mutex_);
            if (!branch->registered_) {
                branch->mark_closed();
                return;
            }

            branch->detach(branches_);

            // A detached branch relinquishes every unread cached completion so
            // those slots can be reclaimed once all remaining branches catch up.
            for (std::size_t seq = std::max(branch->next_seq_, head_seq_); seq < tail_seq_; ++seq) {
                auto &slot = cache_.at(seq - head_seq_);
                if (slot.ref_count_ > 0) {
                    --slot.ref_count_;
                }
            }

            pop_exhausted_slots_locked();
            collect_waiters_locked(waiters);
        }

        invoke_waiters(waiters);
    }

    constexpr bool gap_blocked_locked(const branch_state &branch) const noexcept {
        if (!branch.registered_ || branches_.empty()) {
            return false;
        }

        auto slowest = tail_seq_;
        for (auto *node = branches_.front(); node != &branch_sentinel_; node = node->get_next()) {
            slowest = std::min(slowest, static_cast<const branch_state *>(node)->next_seq_);
        }

        return branch.next_seq_ - slowest >= max_gap_;
    }

    constexpr bool has_cached_locked(std::size_t seq) const noexcept {
        return seq >= head_seq_ && seq < tail_seq_;
    }

    constexpr completion_t reserve_locked(branch_state &branch) noexcept {
        if (branch.late_terminal_ && final_completion_.has_value()) {
            // Branches created after termination skip live branch tracking and
            // consume the remembered terminal completion exactly once.
            branch.consume_late_terminal(tail_seq_);
            return *final_completion_;
        }

        auto &slot = cache_.at(branch.next_seq_ - head_seq_);
        auto completion = slot.ref_count_ > 1 ? completion_t(slot.completion_) : std::move(slot.completion_);
        --slot.ref_count_;
        branch.advance();

        if (completion_is_terminal(completion)) {
            branch.close_terminal(branches_);
        }

        pop_exhausted_slots_locked();
        return completion;
    }

    constexpr void enqueue_waiter_locked(waiter_operation_base *waiter) noexcept {
        if (!waiter->waiting_) {
            waiters_.push_back(waiter);
            waiter->waiting_ = true;
        }
    }

    constexpr void collect_waiters_locked(std::vector<waiter_operation_base *> &waiters) noexcept {
        while (!waiters_.empty()) {
            auto *waiter = waiters_.pop_front_value();
            waiter->waiting_ = false;
            waiters.push_back(waiter);
        }
    }

    static constexpr void invoke_waiters(std::vector<waiter_operation_base *> &waiters) noexcept {
        for (auto *waiter : waiters) {
            waiter->invoke();
        }
    }

    constexpr void finish_upstream(completion_t &&completion) noexcept {
        std::vector<waiter_operation_base *> waiters;
        {
            std::scoped_lock lock(mutex_);
            upstream_inflight_ = false;

            if (completion_is_terminal(completion)) {
                // Remember the terminal completion for late subscribers. Live
                // branches still consume the cached terminal item exactly once.
                terminated_ = true;
                final_completion_ = completion;
            }

            cache_.emplace_back(tail_seq_, branches_.size(), std::move(completion));
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
    branch_list_t branches_{&branch_sentinel_};
    waiter_operation_base waiter_sentinel_{};
    waiter_list_t waiters_{&waiter_sentinel_};
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

        constexpr explicit next_sender_t(const std::shared_ptr<branch_state_t> &branch) noexcept : branch_(branch) {
        }

        constexpr explicit next_sender_t(std::shared_ptr<branch_state_t> &&branch) noexcept : branch_(std::move(branch)) {
        }

        template<typename Receiver>
        constexpr auto connect(Receiver &&receiver) const & noexcept {
            return next_operation_state<std::remove_cvref_t<Receiver>>(branch_, std::forward<Receiver>(receiver));
        }

        template<typename Receiver>
        constexpr auto connect(Receiver &&receiver) && noexcept {
            return next_operation_state<std::remove_cvref_t<Receiver>>(std::move(branch_), std::forward<Receiver>(receiver));
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
    struct next_operation_state : shared_state_t::waiter_operation_base {
        using receiver_t = Receiver;
        using env_t = std::remove_cvref_t<decltype(stdexec::get_env(std::declval<receiver_t &>()))>;
        using scheduler_t = execution::trampoline_scheduler;
        using schedule_sender_t = decltype(stdexec::schedule(std::declval<scheduler_t>()));
        using actual_completion_t = execution::utils::basic_any_completions<typename shared_state_t::upstream_sender_t, env_t>;

        struct scheduled_receiver {
            using receiver_concept = stdexec::receiver_t;

            constexpr auto get_env() const noexcept {
                return stdexec::env<>{};
            }

            template<typename... Args>
            constexpr void set_value(Args &&...) noexcept {
                self_->replay_ready_result();
            }

            template<typename... Args>
            constexpr void set_error(Args &&...args) noexcept {
                self_->deliver_scheduler_error(std::forward<Args>(args)...);
            }

            constexpr void set_stopped() noexcept {
                self_->deliver_scheduler_stopped();
            }

            next_operation_state *self_ = nullptr;
        };

        struct upstream_receiver {
            using receiver_concept = stdexec::receiver_t;

            constexpr env_t get_env() const noexcept {
                return self_->get_downstream_env();
            }

            template<typename... Args>
            constexpr void set_value(Args &&...args) noexcept {
                completion_t completion;
                completion.store_value(std::forward<Args>(args)...);
                self_->finish_upstream(std::move(completion));
            }

            template<typename... Args>
            constexpr void set_error(Args &&...args) noexcept {
                completion_t completion;
                completion.store_error(std::forward<Args>(args)...);
                self_->finish_upstream(std::move(completion));
            }

            constexpr void set_stopped() noexcept {
                completion_t completion;
                completion.store_stopped();
                self_->finish_upstream(std::move(completion));
            }

            next_operation_state *self_ = nullptr;
        };

        using delivery_op_t = stdexec::connect_result_t<schedule_sender_t, scheduled_receiver>;
        using upstream_sender_t = typename shared_state_t::upstream_sender_t;
        using upstream_op_t = stdexec::connect_result_t<upstream_sender_t, upstream_receiver>;
        using replay_op_t = stdexec::connect_result_t<completion_t, receiver_t>;

        static_assert(std::same_as<typename actual_completion_t::value_t, typename completion_t::value_t> && std::same_as<typename actual_completion_t::error_t, typename completion_t::error_t> &&
                          std::same_as<typename actual_completion_t::completion_signatures_t, typename completion_t::completion_signatures_t>,
                      "broadcast downstream receiver env is not compatible with the Env selected for this broadcast hub");

        constexpr next_operation_state(const std::shared_ptr<branch_state_t> &branch, receiver_t receiver) noexcept
            : shared_state_t::waiter_operation_base(&next_operation_state::resume), branch_(branch), receiver_(std::move(receiver)) {
        }

        constexpr next_operation_state(std::shared_ptr<branch_state_t> &&branch, receiver_t receiver) noexcept
            : shared_state_t::waiter_operation_base(&next_operation_state::resume), branch_(std::move(branch)), receiver_(std::move(receiver)) {
        }

        constexpr void start() noexcept {
            run_progress_loop();
        }

        static constexpr void resume(typename shared_state_t::waiter_base_t *base) noexcept {
            static_cast<next_operation_state *>(base)->run_progress_loop();
        }

        constexpr env_t get_downstream_env() const noexcept {
            return stdexec::get_env(receiver_);
        }

        enum class progress_action {
            idle,
            wait,
            deliver_ready,
            start_upstream,
        };

        struct progress_result {
            progress_action action_ = progress_action::idle;
            std::optional<completion_t> completion_;
        };

        constexpr void run_progress_loop() noexcept {
            // Each entry contributes one resume token. Only the thread that
            // observes the 0 -> 1 transition drains the loop; re-entrant or
            // concurrent resumes simply bump the counter and return.
            if (!try_enter_progress_loop()) {
                return;
            }

            do {
                if (has_ready_result()) {
                    // Upstream completion may have arrived inline while
                    // another call stack was still inside `run_progress_loop`.
                    // Drain the pending resumes, then hand off to delivery.
                    finish_progress_before_delivery();
                    start_delivery();
                    return;
                }

                auto progress = collect_progress_action();
                switch (progress.action_) {
                case progress_action::deliver_ready:
                    // Cached or remembered completion is ready; deliver it
                    // after a trampoline hop so broadcast never recurses
                    // without bound.
                    ready_completion_.emplace(std::move(*progress.completion_));
                    finish_progress_before_delivery();
                    start_delivery();
                    return;
                case progress_action::start_upstream:
                    start_upstream_request();
                    break;
                case progress_action::wait:
                case progress_action::idle:
                    break;
                }
            } while (consume_resume_request());
        }

        constexpr bool try_enter_progress_loop() noexcept {
            return this->resume_counter_.fetch_add(1, std::memory_order_acq_rel) == 0;
        }

        constexpr bool consume_resume_request() noexcept {
            return this->resume_counter_.fetch_sub(1, std::memory_order_acq_rel) != 1;
        }

        constexpr void finish_progress_before_delivery() noexcept {
            // Once delivery starts, this op is leaving the hub progress loop
            // and is about to complete or suspend in its own downstream path.
            // Any coalesced resume requests are stale at that point.
            this->resume_counter_.exchange(0, std::memory_order_acq_rel);
        }

        constexpr auto collect_progress_action() noexcept -> progress_result {
            auto &state = branch_->hub_;
            std::scoped_lock lock(state->mutex_);

            if (!branch_->try_activate(this)) {
                // A branch admits only one downstream `next()` operation at a
                // time, matching the stream usage contract.
                return {};
            }

            if (auto completion = try_take_ready_completion_locked(*state); completion.has_value()) {
                return {progress_action::deliver_ready, std::move(completion)};
            }

            if (state->gap_blocked_locked(*branch_)) {
                // Leading branches wait here until the slowest live branch
                // catches up enough to reopen the gap window.
                state->enqueue_waiter_locked(this);
                return {progress_action::wait};
            }

            if (arm_upstream_request_locked(*state)) {
                return {progress_action::start_upstream};
            }

            return {progress_action::wait};
        }

        constexpr auto try_take_ready_completion_locked(shared_state_t &state) noexcept -> std::optional<completion_t> {
            // Late subscribers bypass live branch tracking and only see the
            // remembered terminal completion.
            if (branch_->late_terminal_ && state.final_completion_.has_value()) {
                return state.reserve_locked(*branch_);
            }

            // A closed branch no longer participates in cache/gap tracking,
            // but it may still need the remembered terminal.
            if (branch_->closed_ && state.final_completion_.has_value()) {
                return *state.final_completion_;
            }

            // Followers consume cached completions in sequence order.
            if (state.has_cached_locked(branch_->next_seq_)) {
                return state.reserve_locked(*branch_);
            }

            // The hub terminated before this branch observed the final
            // completion through the normal cached path.
            if (state.terminated_ && state.final_completion_.has_value()) {
                branch_->mark_closed();
                return *state.final_completion_;
            }

            return std::nullopt;
        }

        constexpr bool arm_upstream_request_locked(shared_state_t &state) noexcept {
            state.enqueue_waiter_locked(this);
            if (state.upstream_inflight_) {
                // Another branch is already driving upstream progress, so wait
                // until its completion is published.
                return false;
            }

            // This op wins the race to drive the next upstream request and
            // becomes responsible for publishing it.
            state.upstream_inflight_ = true;
            return true;
        }

        constexpr void start_upstream_request() noexcept {
            // Start the upstream op after leaving the mutex. It may finish
            // inline, so the bookkeeping above must already be in place.
            auto sender = branch_->hub_->next_upstream();
            // We intentionally keep the previous completed upstream op alive
            // until the next request replaces it or this downstream op dies.
            // Resetting it inside `finish_upstream()` would be unsafe because
            // that function runs on the upstream receiver completion path.
            upstream_op_.emplace(stdexec::connect(std::move(sender), upstream_receiver{this}));
            stdexec::start(*upstream_op_);
        }

        constexpr bool has_ready_result() const noexcept {
            return ready_completion_.has_value();
        }

        constexpr void start_delivery() noexcept {
            // Always break the inline upstream->downstream chain with a
            // trampoline hop so broadcast cannot recurse without bound.
            if (!delivery_op_.has_value()) {
                delivery_op_.emplace(stdexec::connect(stdexec::schedule(scheduler_t{}), scheduled_receiver{this}));
                stdexec::start(*delivery_op_);
            }
        }

        constexpr auto prepare_delivery() noexcept {
            std::vector<typename shared_state_t::waiter_operation_base *> waiters;
            {
                std::scoped_lock lock(branch_->hub_->mutex_);
                branch_->release(this);
                branch_->hub_->collect_waiters_locked(waiters);
            }
            return waiters;
        }

        constexpr void replay_ready_result() noexcept {
            auto waiters = prepare_delivery();
            auto completion = std::move(*ready_completion_);
            ready_completion_.reset();
            shared_state_t::invoke_waiters(waiters);
            // Keep the replay op-state inside the downstream-owned operation:
            // `completion_t::start()` is currently inline, but sender/receiver
            // code should not rely on that detail for lifetime safety.
            replay_op_.emplace(stdexec::connect(std::move(completion), std::move(receiver_)));
            stdexec::start(*replay_op_);
        }

        template<typename... Args>
        constexpr void deliver_scheduler_error(Args &&...args) noexcept {
            auto waiters = prepare_delivery();
            auto receiver = std::move(receiver_);
            shared_state_t::invoke_waiters(waiters);
            stdexec::set_error(std::move(receiver), std::forward<Args>(args)...);
        }

        constexpr void deliver_scheduler_stopped() noexcept {
            auto waiters = prepare_delivery();
            auto receiver = std::move(receiver_);
            shared_state_t::invoke_waiters(waiters);
            stdexec::set_stopped(std::move(receiver));
        }

        constexpr void finish_upstream(completion_t &&completion) noexcept {
            branch_->hub_->finish_upstream(std::move(completion));
        }

        std::shared_ptr<branch_state_t> branch_;
        receiver_t receiver_;
        std::optional<completion_t> ready_completion_;
        std::optional<replay_op_t> replay_op_;
        std::optional<upstream_op_t> upstream_op_;
        // The scheduler hop op-state stays inside the downstream-owned
        // operation state so we keep concrete types without type erasure.
        std::optional<delivery_op_t> delivery_op_;
    };

public:
    using stream_concept = stream_t;

    constexpr explicit broadcast_branch_stream(const std::shared_ptr<branch_state_t> &branch) noexcept : branch_(branch) {
    }

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

    constexpr explicit broadcast_hub(std::shared_ptr<shared_state_t> state) noexcept : state_(std::move(state)) {
    }

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
/// get more than `max_gap` items ahead of the slowest live branch. `max_gap`
/// is normalized to at least 1.
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
    constexpr auto operator()(UpStream &&upstream, std::size_t max_gap) const noexcept {
        using stream_t = std::remove_cvref_t<UpStream>;
        using hub_t = broadcast_hub<stream_t, Env>;
        using state_t = typename hub_t::shared_state_t;

        return hub_t(std::make_shared<state_t>(std::forward<UpStream>(upstream), max_gap));
    }

    template<typename Env = stdexec::env<>>
    struct closure_t {
        using stream_closure_concept = stream_closure_t;

        constexpr explicit closure_t(std::size_t max_gap) noexcept : max_gap_(max_gap) {
        }

        template<typename UpStream>
        requires streamable<UpStream>
        constexpr auto operator()(UpStream &&upstream) const noexcept {
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
    requires requires(Hub &&hub) { std::forward<Hub>(hub).subscribe(); }
    constexpr auto operator()(Hub &&hub) const noexcept {
        return std::forward<Hub>(hub).subscribe();
    }
};

inline constexpr broadcast_from_t broadcast_from{};
inline constexpr broadcast_to_t broadcast_to{};

} // namespace __broadcast_stream

using __broadcast_stream::broadcast_branch_stream;
using __broadcast_stream::broadcast_from;
using __broadcast_stream::broadcast_hub;
using __broadcast_stream::broadcast_to;

} // namespace streamifex
} // namespace stdplus
} // namespace snapflakes
