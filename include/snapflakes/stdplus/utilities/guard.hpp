#pragma once

#include <utility>
#include <snapflakes/stdplus/utilities/initialization.hpp>

namespace snapflakes {
namespace stdplus {
namespace utils {

template<typename F>
class guard: public stdplus::utils::immoveable_and_noncopyable {
public:
    guard() = delete;

    constexpr explicit guard(F&& f): f_(std::move(f)) {}

    constexpr ~guard() noexcept {
        std::move(f_)();
    }

private:
    F f_;
};

}
}
}
