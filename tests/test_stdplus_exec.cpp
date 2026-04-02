#include <snapflakes/stdplus.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <future>

namespace {

enum class simple_sender_kind {
    value,
    stopped,
};

struct simple_sender {
    using sender_concept = stdexec::sender_t;

    template<typename Receiver>
    struct operation_state {
        constexpr void start() noexcept {
            if (kind_ == simple_sender_kind::value) {
                stdexec::set_value(std::move(receiver_), value_);
            } else {
                stdexec::set_stopped(std::move(receiver_));
            }
        }

        Receiver receiver_;
        simple_sender_kind kind_ = simple_sender_kind::stopped;
        int value_ = 0;
    };

    template<typename Receiver>
    constexpr auto connect(Receiver receiver) const noexcept {
        return operation_state<std::remove_cvref_t<Receiver>>{
            std::move(receiver),
            kind_,
            value_,
        };
    }

    constexpr auto get_env() const noexcept {
        return stdexec::env<>{};
    }

    static consteval auto get_completion_signatures() noexcept {
        return stdexec::completion_signatures<
            stdexec::set_value_t(int),
            stdexec::set_stopped_t()
        >{};
    }

    template<typename Env>
    static consteval auto get_completion_signatures() noexcept {
        return get_completion_signatures();
    }

    simple_sender_kind kind_ = simple_sender_kind::stopped;
    int value_ = 0;
};

struct counting_stream {
    using stream_concept = snapflakes::stdplus::streamifex::stream_t;

    constexpr auto next() noexcept {
        if (current_ < limit_) {
            return simple_sender{simple_sender_kind::value, current_++};
        }

        return simple_sender{simple_sender_kind::stopped, 0};
    }

    int current_ = 0;
    int limit_ = 0;
};

} // namespace

TEST_CASE("stdexec demo", "[stdexec]") {
    namespace sfexec = snapflakes::stdplus::execution;
    
     // Declare a pool of 3 worker threads:
    sfexec::static_thread_pool pool(3);

    // Get a handle to the thread pool:
    auto sched = pool.get_scheduler();

    // Describe some work:
    // Creates 3 sender pipelines that are executed concurrently by passing to `when_all`
    // Each sender is scheduled on `sched` using `on` and starts with `just(n)` that creates a
    // Sender that just forwards `n` to the next sender.
    // After `just(n)`, we chain `then(fun)` which invokes `fun` using the value provided from `just()`
    // Note: No work actually happens here. Everything is lazy and `work` is just an object that statically
    // represents the work to later be executed
    auto fun = [](int i) { return i*i; };
    auto work = stdexec::when_all(
        stdexec::starts_on(sched, stdexec::just(0) | stdexec::then(fun)),
        stdexec::starts_on(sched, stdexec::just(1) | stdexec::then(fun)),
        stdexec::starts_on(sched, stdexec::just(2) | stdexec::then(fun))
    );

    // Launch the work and wait for the result
    auto [i, j, k] = stdexec::sync_wait(std::move(work)).value();

    // Prints "0 1 4":
    std::printf("%d %d %d\n", i, j, k);
    REQUIRE(i == 0);
    REQUIRE(j == 1);
    REQUIRE(k == 4);
};

TEST_CASE("broadcast fans out cached values", "[streamifex][broadcast]") {
    namespace sfstream = snapflakes::stdplus::streamifex;

    auto hub = sfstream::broadcast_from(counting_stream{0, 2}, 2);
    auto left = sfstream::broadcast_to(hub);
    auto right = sfstream::broadcast_to(hub);

    auto left0 = stdexec::sync_wait(left.next());
    auto right0 = stdexec::sync_wait(right.next());
    auto left1 = stdexec::sync_wait(left.next());
    auto right1 = stdexec::sync_wait(right.next());
    auto left_done = stdexec::sync_wait(left.next());
    auto right_done = stdexec::sync_wait(right.next());

    REQUIRE(left0.has_value());
    REQUIRE(std::get<0>(*left0) == 0);
    REQUIRE(right0.has_value());
    REQUIRE(std::get<0>(*right0) == 0);
    REQUIRE(left1.has_value());
    REQUIRE(std::get<0>(*left1) == 1);
    REQUIRE(right1.has_value());
    REQUIRE(std::get<0>(*right1) == 1);
    REQUIRE_FALSE(left_done.has_value());
    REQUIRE_FALSE(right_done.has_value());
}

TEST_CASE("broadcast works with a single branch", "[streamifex][broadcast]") {
    namespace sfstream = snapflakes::stdplus::streamifex;

    auto hub = sfstream::broadcast_from(counting_stream{0, 2}, 2);
    auto only = sfstream::broadcast_to(hub);

    auto first = stdexec::sync_wait(only.next());
    auto second = stdexec::sync_wait(only.next());
    auto done = stdexec::sync_wait(only.next());

    REQUIRE(first.has_value());
    REQUIRE(std::get<0>(*first) == 0);
    REQUIRE(second.has_value());
    REQUIRE(std::get<0>(*second) == 1);
    REQUIRE_FALSE(done.has_value());
}

TEST_CASE("broadcast blocks a leading branch at the configured gap", "[streamifex][broadcast]") {
    using namespace std::chrono_literals;
    namespace sfstream = snapflakes::stdplus::streamifex;

    auto hub = sfstream::broadcast_from(counting_stream{0, 3}, 1);
    auto fast = sfstream::broadcast_to(hub);
    auto slow = sfstream::broadcast_to(hub);

    auto fast0 = stdexec::sync_wait(fast.next());
    REQUIRE(fast0.has_value());
    REQUIRE(std::get<0>(*fast0) == 0);

    auto blocked_fast = std::async(std::launch::async, [&fast]() {
        return stdexec::sync_wait(fast.next());
    });

    std::this_thread::sleep_for(50ms);
    REQUIRE(blocked_fast.wait_for(0ms) == std::future_status::timeout);

    auto slow0 = stdexec::sync_wait(slow.next());
    REQUIRE(slow0.has_value());
    REQUIRE(std::get<0>(*slow0) == 0);

    auto fast1 = blocked_fast.get();
    REQUIRE(fast1.has_value());
    REQUIRE(std::get<0>(*fast1) == 1);
}
