#include <catch2/catch_test_macros.hpp>
#include <snapflakes/stdplus.hpp>

#include <chrono>
#include <condition_variable>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

enum class scripted_sender_kind {
    value,
    error,
    stopped,
};

struct broadcast_test_error final : std::exception {
    const char *what() const noexcept override {
        return "broadcast test error";
    }
};

struct scripted_step {
    scripted_sender_kind kind_ = scripted_sender_kind::stopped;
    int value_ = 0;
};

struct scripted_sender {
    using sender_concept = stdexec::sender_t;

    template<typename Receiver>
    struct operation_state {
        constexpr void start() noexcept {
            switch (step_.kind_) {
            case scripted_sender_kind::value:
                stdexec::set_value(std::move(receiver_), step_.value_);
                return;
            case scripted_sender_kind::error:
                stdexec::set_error(std::move(receiver_), std::make_exception_ptr(broadcast_test_error{}));
                return;
            case scripted_sender_kind::stopped:
                stdexec::set_stopped(std::move(receiver_));
                return;
            }
        }

        Receiver receiver_;
        scripted_step step_{};
    };

    template<typename Receiver>
    constexpr auto connect(Receiver receiver) const noexcept {
        return operation_state<std::remove_cvref_t<Receiver>>{
            std::move(receiver),
            step_,
        };
    }

    constexpr auto get_env() const noexcept {
        return stdexec::env<>{};
    }

    static consteval auto get_completion_signatures() noexcept {
        return stdexec::completion_signatures<stdexec::set_value_t(int), stdexec::set_error_t(std::exception_ptr), stdexec::set_stopped_t()>{};
    }

    template<typename Env>
    static consteval auto get_completion_signatures() noexcept {
        return get_completion_signatures();
    }

    scripted_step step_{};
};

struct counting_stream {
    using stream_concept = snapflakes::stdplus::streamifex::stream_t;

    constexpr auto next() noexcept {
        if (current_ < limit_) {
            return scripted_sender{{scripted_sender_kind::value, current_++}};
        }

        return scripted_sender{{scripted_sender_kind::stopped, 0}};
    }

    int current_ = 0;
    int limit_ = 0;
};

struct scripted_stream {
    using stream_concept = snapflakes::stdplus::streamifex::stream_t;

    constexpr auto next() noexcept {
        if (index_ < steps_.size()) {
            return scripted_sender{steps_[index_++]};
        }

        return scripted_sender{{scripted_sender_kind::stopped, 0}};
    }

    std::vector<scripted_step> steps_;
    std::size_t index_ = 0;
};

enum class observed_completion_kind {
    value,
    error,
    stopped,
};

struct observed_completion {
    observed_completion_kind kind_ = observed_completion_kind::stopped;
    std::optional<int> value_;
    std::exception_ptr error_;
};

struct observed_state {
    std::mutex mutex_;
    std::condition_variable cv_;
    bool ready_ = false;
    observed_completion completion_{};
};

struct observed_receiver {
    using receiver_concept = stdexec::receiver_t;

    constexpr auto get_env() const noexcept {
        return stdexec::env<>{};
    }

    void set_value(int value) noexcept {
        {
            std::lock_guard lock(state_->mutex_);
            state_->completion_ = observed_completion{
                observed_completion_kind::value,
                value,
                {},
            };
            state_->ready_ = true;
        }
        state_->cv_.notify_one();
    }

    void set_error(std::exception_ptr error) noexcept {
        {
            std::lock_guard lock(state_->mutex_);
            state_->completion_ = observed_completion{
                observed_completion_kind::error,
                std::nullopt,
                std::move(error),
            };
            state_->ready_ = true;
        }
        state_->cv_.notify_one();
    }

    void set_stopped() noexcept {
        {
            std::lock_guard lock(state_->mutex_);
            state_->completion_ = observed_completion{
                observed_completion_kind::stopped,
                std::nullopt,
                {},
            };
            state_->ready_ = true;
        }
        state_->cv_.notify_one();
    }

    std::shared_ptr<observed_state> state_;
};

inline constexpr auto k_completion_timeout = std::chrono::seconds(2);
inline constexpr auto k_blocked_probe_timeout = std::chrono::milliseconds(50);

template<typename Sender>
auto wait_for_completion(Sender &&sender) {
    auto state = std::make_shared<observed_state>();
    auto op = stdexec::connect(std::forward<Sender>(sender), observed_receiver{state});
    stdexec::start(op);

    std::unique_lock lock(state->mutex_);
    if (!state->cv_.wait_for(lock, k_completion_timeout, [&state]() { return state->ready_; })) {
        throw std::runtime_error("timed out waiting for sender completion");
    }
    return std::move(state->completion_);
}

template<typename Fn>
auto spawn_async_result(Fn &&fn) {
    using result_t = std::invoke_result_t<std::decay_t<Fn>>;

    std::promise<result_t> promise;
    auto future = promise.get_future();
    std::thread([promise = std::move(promise), fn = std::forward<Fn>(fn)]() mutable noexcept {
        try {
            promise.set_value(fn());
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }).detach();
    return future;
}

template<typename Branch>
auto spawn_next_completion(Branch branch) {
    return spawn_async_result([branch = std::move(branch)]() mutable { return wait_for_completion(branch.next()); });
}

template<typename T>
void require_still_blocked(std::future<T> &future) {
    REQUIRE(future.wait_for(k_blocked_probe_timeout) == std::future_status::timeout);
}

template<typename T>
T wait_for_future_value(std::future<T> &future) {
    REQUIRE(future.wait_for(k_completion_timeout) == std::future_status::ready);
    return future.get();
}

void require_value(const observed_completion &completion, int value) {
    REQUIRE(completion.kind_ == observed_completion_kind::value);
    REQUIRE(completion.value_.has_value());
    REQUIRE(*completion.value_ == value);
}

void require_stopped(const observed_completion &completion) {
    REQUIRE(completion.kind_ == observed_completion_kind::stopped);
    REQUIRE_FALSE(completion.value_.has_value());
    REQUIRE_FALSE(static_cast<bool>(completion.error_));
}

void require_error(const observed_completion &completion) {
    REQUIRE(completion.kind_ == observed_completion_kind::error);
    REQUIRE_FALSE(completion.value_.has_value());
    REQUIRE(static_cast<bool>(completion.error_));
    REQUIRE_THROWS_AS(std::rethrow_exception(completion.error_), broadcast_test_error);
}

} // namespace

TEST_CASE("broadcast fans out cached values", "[streamifex][broadcast]") {
    namespace sfstream = snapflakes::stdplus::streamifex;

    auto hub = sfstream::broadcast_from(counting_stream{0, 2}, 2);
    auto left = sfstream::broadcast_to(hub);
    auto right = sfstream::broadcast_to(hub);

    require_value(wait_for_completion(left.next()), 0);
    require_value(wait_for_completion(right.next()), 0);
    require_value(wait_for_completion(left.next()), 1);
    require_value(wait_for_completion(right.next()), 1);
    require_stopped(wait_for_completion(left.next()));
    require_stopped(wait_for_completion(right.next()));
}

TEST_CASE("broadcast works with a single branch", "[streamifex][broadcast]") {
    namespace sfstream = snapflakes::stdplus::streamifex;

    auto hub = sfstream::broadcast_from(counting_stream{0, 2}, 1);
    auto only = sfstream::broadcast_to(hub);

    require_value(wait_for_completion(only.next()), 0);
    require_value(wait_for_completion(only.next()), 1);
    require_stopped(wait_for_completion(only.next()));
}

TEST_CASE("broadcast blocks a leading branch at the configured gap", "[streamifex][broadcast]") {
    namespace sfstream = snapflakes::stdplus::streamifex;

    auto hub = sfstream::broadcast_from(counting_stream{0, 3}, 1);
    auto fast = sfstream::broadcast_to(hub);
    auto slow = sfstream::broadcast_to(hub);

    require_value(wait_for_completion(fast.next()), 0);

    auto blocked_fast = spawn_next_completion(fast);
    require_still_blocked(blocked_fast);

    require_value(wait_for_completion(slow.next()), 0);
    require_value(wait_for_future_value(blocked_fast), 1);
}

TEST_CASE("broadcast normalizes zero max_gap to one", "[streamifex][broadcast]") {
    namespace sfstream = snapflakes::stdplus::streamifex;

    auto hub = sfstream::broadcast_from(counting_stream{0, 3}, 0);
    auto fast = sfstream::broadcast_to(hub);
    auto slow = sfstream::broadcast_to(hub);

    require_value(wait_for_completion(fast.next()), 0);

    auto blocked_fast = spawn_next_completion(fast);
    require_still_blocked(blocked_fast);

    require_value(wait_for_completion(slow.next()), 0);
    require_value(wait_for_future_value(blocked_fast), 1);
}

TEST_CASE("broadcast destroying a lagging branch releases blocked waiters", "[streamifex][broadcast]") {
    namespace sfstream = snapflakes::stdplus::streamifex;

    auto hub = sfstream::broadcast_from(counting_stream{0, 3}, 1);
    auto fast = sfstream::broadcast_to(hub);

    using branch_t = decltype(sfstream::broadcast_to(hub));
    auto slow = std::optional<branch_t>(sfstream::broadcast_to(hub));

    require_value(wait_for_completion(fast.next()), 0);

    auto blocked_fast = spawn_next_completion(fast);
    require_still_blocked(blocked_fast);

    slow.reset();
    require_value(wait_for_future_value(blocked_fast), 1);
    require_value(wait_for_completion(fast.next()), 2);
    require_stopped(wait_for_completion(fast.next()));
}

TEST_CASE("broadcast replays stopped terminal to lagging and late subscribers", "[streamifex][broadcast]") {
    namespace sfstream = snapflakes::stdplus::streamifex;

    auto hub = sfstream::broadcast_from(counting_stream{0, 1}, 2);
    auto left = sfstream::broadcast_to(hub);
    auto right = sfstream::broadcast_to(hub);

    require_value(wait_for_completion(left.next()), 0);
    require_stopped(wait_for_completion(left.next()));

    require_value(wait_for_completion(right.next()), 0);
    require_stopped(wait_for_completion(right.next()));

    auto late = sfstream::broadcast_to(hub);
    require_stopped(wait_for_completion(late.next()));
}

TEST_CASE("broadcast replays error terminal to lagging and late subscribers", "[streamifex][broadcast]") {
    namespace sfstream = snapflakes::stdplus::streamifex;

    auto hub = sfstream::broadcast_from(scripted_stream{{{
                                            {scripted_sender_kind::value, 7},
                                            {scripted_sender_kind::error, 0},
                                        }}},
                                        2);
    auto left = sfstream::broadcast_to(hub);
    auto right = sfstream::broadcast_to(hub);

    require_value(wait_for_completion(left.next()), 7);
    require_error(wait_for_completion(left.next()));

    require_value(wait_for_completion(right.next()), 7);
    require_error(wait_for_completion(right.next()));

    auto late = sfstream::broadcast_to(hub);
    require_error(wait_for_completion(late.next()));
}

TEST_CASE("broadcast_from closure creates a shareable hub", "[streamifex][broadcast]") {
    namespace sfstream = snapflakes::stdplus::streamifex;

    auto closure = sfstream::broadcast_from(2);
    auto hub = closure(counting_stream{0, 2});
    auto left = sfstream::broadcast_to(hub);
    auto right = sfstream::broadcast_to(hub);

    require_value(wait_for_completion(left.next()), 0);
    require_value(wait_for_completion(right.next()), 0);
}
