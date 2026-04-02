#pragma once

#include <tuple>
#include <variant>

namespace snapflakes {
namespace stdplus {
namespace execution {

namespace concepts {

template<typename T>
struct is_tuple_like_v : std::false_type {};

template<typename... Ts>
struct is_tuple_like_v<std::tuple<Ts...>> : std::true_type {};

template<typename T>
concept tuple_like = is_tuple_like_v<std::remove_cvref_t<T>>::value;

template<typename T>
concept variant_type = requires(T t) {
    typename std::variant_size<T>::type;
    requires std::variant_size_v<T> >= 0;
    typename std::variant_alternative<0, T>::type;
};

template<typename Mutex>
concept mutex = requires(Mutex m) {
    { m.lock() };
    { m.unlock() };
};

}
}
}
}
