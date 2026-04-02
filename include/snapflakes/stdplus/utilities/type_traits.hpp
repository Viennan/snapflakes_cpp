#pragma once

#include <type_traits>
#include <tuple>

namespace snapflakes {
namespace stdplus {
namespace utils {

template<typename T>
struct __is_tuple_like_t : std::false_type {};

template<typename... Ts>
struct __is_tuple_like_t<std::tuple<Ts...>> : std::true_type {};

template<typename T>
constexpr inline auto __is_tuple_like_v() {
    return __is_tuple_like_t<T>{};
}

template<typename T>
inline constexpr bool is_tuple_like_v = __is_tuple_like_v<T>();

}
}
}