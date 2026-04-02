#pragma once

#include <functional>
#include <utility>

namespace snapflakes {
namespace stdplus {
namespace utilities {

template<typename F>
struct ptr_as_function {
    constexpr explicit ptr_as_function(F* fptr) : fptr_(fptr) {}

    template<typename... Args>
    constexpr auto operator()(Args&&... args) const {
        return std::invoke(*fptr_, std::forward<Args>(args)...);
    }

private:
    F* fptr_;
};

}
}
}