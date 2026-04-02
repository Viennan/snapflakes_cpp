#pragma once

#include <utility>
#include <snapflakes/stdplus/streamifex/concepts.hpp>

namespace snapflakes {
namespace stdplus {
namespace streamifex {

template<typename Stream, typename Closure>
requires streamable<Stream> && stream_closureable<Closure>
constexpr inline auto operator|(Stream&& stream, Closure&& clsur) noexcept {
    return std::forward<Closure>(clsur)(std::forward<Stream>(stream));
}

namespace __closure {

    template<typename C1, typename C2>
    requires stream_closureable<C1> && stream_closureable<C2>
    struct compose_closure {
        using stream_closure_concept = stream_closure_t;

        constexpr compose_closure(C1 c1, C2 c2) : c1_(std::move(c1)), c2_(std::move(c2)) {}

        template<typename Stream>
        constexpr auto operator()(Stream&& stream) const & noexcept {
            return c2_(c1_(std::forward<Stream>(stream)));
        }

        template<typename Stream>
        constexpr auto operator()(Stream&& stream) && noexcept {
            return std::move(c2_)(std::move(c1_)(std::forward<Stream>(stream)));
        }

    private:
        C1 c1_;
        C2 c2_;
    };

}

template<typename C1, typename C2>
requires stream_closureable<C1> && stream_closureable<C2>
constexpr inline auto operator| (C1&& c1, C2&& c2) noexcept {
    return ::snapflakes::stdplus::streamifex::__closure::compose_closure<C1, C2>(std::forward<C1>(c1), std::forward<C2>(c2));
}

}
}
}
