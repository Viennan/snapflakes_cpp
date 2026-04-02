#pragma once

namespace snapflakes {
namespace stdplus {
namespace utils {

class non_mutex {
    constexpr non_mutex() = default;
    constexpr ~non_mutex() = default;

    constexpr void lock() noexcept {}

    constexpr void unlock() noexcept {}
};

}
}
}
