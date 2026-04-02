#pragma once

#include <snapflakes/stdplus/exec/std.hpp>

namespace snapflakes {
namespace stdplus {
namespace streamifex {

struct stream_t {};

template<typename T>
concept streamable = requires(T&& t) {
    requires stdexec::sender<std::remove_cvref_t<decltype(t.next())>>;
    requires std::is_same_v<stream_t, typename T::stream_concept>;
};

struct stream_closure_t {};

template<typename T>
concept stream_closureable = requires(T&& t) {
    requires std::is_same_v<stream_closure_t, typename T::stream_closure_concept>;
};

}
}
}
