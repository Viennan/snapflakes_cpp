#pragma once

#include <concepts>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <optional>
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
concept tuple_like = is_tuple_like_v<std::remove_cvref_t<T>>();

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

template<typename T>
concept iterable_optional_list = requires(T t) {
    typename T::value_type;
    typename T::iterator_type;
    { t.begin() } -> std::random_access_iterator;
    { t.emplace_back(std::nullopt) };
    { t.emplace(size_t(0), std::nullopt) };
    { t.has_value(size_t(0)) } -> std::same_as<bool>;
    { t.pop_front() };
};

template<typename Ele>
concept static_double_linked_listable = requires(Ele e) {
    !std::is_move_constructible_v<Ele>;
    !std::is_move_assignable_v<Ele>;
    std::is_same_v<Ele*, decltype(e.get_next())> || std::derived_from<Ele, std::remove_pointer_t<decltype(e.get_next())>>;
    std::is_same_v<Ele*, decltype(e.get_prev())> || std::derived_from<Ele, std::remove_pointer_t<decltype(e.get_prev())>>;
    { e.set_next(nullptr) };
    { e.set_prev(nullptr) };
};

}
}
}
}