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

template<template<typename> typename Buffer, typename T>
concept able_to_cache_as_list = requires(Buffer<T>&& buffer, T&& value) {
    buffer.push_back(std::forward<T>(value));
    buffer.emplace_back(std::forward<T>(value));
    std::is_same_v<std::remove_cvref_t<T>, std::remove_cvref_t<decltype(buffer.front())>>;
    { buffer.pop_front() };
    { buffer.has_value_front() };
    { buffer.is_full()} -> std::same_as<bool>;
};

}
}
}
}
