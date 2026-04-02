#pragma once

namespace snapflakes {
namespace stdplus {
namespace utils {

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

}
}
}