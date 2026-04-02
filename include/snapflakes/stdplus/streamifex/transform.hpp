#pragma once

#include <snapflakes/stdplus/streamifex/stream_fwd.hpp>
#include <snapflakes/stdplus/exec/std.hpp>

namespace snapflakes {
namespace stdplus {
namespace streamifex {

namespace __transform {

/// transform: Apply a transformation to each element emitted by a stream
///
/// Adapts a stream by applying functor `F` to each element produced by `next()`.
/// The transformation is applied via a sender adaptor `Snd` (defaults to
/// `stdexec::then`), allowing the scheduling behaviour of the transform step
/// to be customised by the caller.
///
/// Behaviour:
/// - On each `next()` call: pipes the upstream sender through `snd_(f_)`, so
///   `F` is invoked with the upstream values and its result forwarded downstream
/// - Error and stopped completions pass through unchanged
///
/// Usage:
///   auto s = some_stream | transform([](int x) { return x * 2; });
///   auto s = transform(some_stream, [](int x) { return x * 2; });
///
///   // Custom sender adaptor (e.g. then_rsh for rescheduling):
///   auto s = some_stream | transform([](int x) { return x * 2; }, then_rsh);

// Concept
template<typename UpStream, typename Snd, typename F>
concept transformable =
    streamable<UpStream> &&
    requires(UpStream&& us, Snd&& snd, F&& f) {
        requires stdexec::sender<std::remove_cvref_t<decltype(
            std::forward<UpStream>(us).next() | std::forward<Snd>(snd)(std::forward<F>(f))
        )>>;
    };

// Stream
template<typename UpStream, typename F, typename Snd = std::remove_cvref_t<decltype(stdexec::then)>>
requires transformable<UpStream, Snd, F>
struct transform_stream {

    using stream_concept = stream_t;

    constexpr transform_stream(UpStream up_stream, F f, Snd snd = Snd{}) noexcept
        : up_stream_(std::move(up_stream)), f_(std::move(f)), snd_(std::move(snd)) {}

    constexpr auto next() noexcept {
        return up_stream_.next() | snd_([this](auto&&... args) { return std::invoke(f_, std::forward<decltype(args)>(args)...); });
    }

private:
    UpStream up_stream_;
    F f_;
    Snd snd_;
};

// Stream closure
template<typename F, typename Snd = std::remove_cvref_t<decltype(stdexec::then)>>
struct transform_closure {
    using stream_closure_concept = stream_closure_t;

    constexpr transform_closure(F f, Snd snd = Snd{}) noexcept : f_(std::move(f)), snd_(std::move(snd)) {}

    template<typename UpStream>
    requires streamable<UpStream> && transformable<UpStream, Snd, F>
    constexpr auto operator()(UpStream&& up_stream) const & noexcept {
        return transform_stream(std::forward<UpStream>(up_stream), f_, snd_);
    }

    template<typename UpStream>
    requires streamable<UpStream> && transformable<UpStream, Snd, F>
    constexpr auto operator()(UpStream&& up_stream) && noexcept {
        return transform_stream(std::forward<UpStream>(up_stream), std::move(f_), std::move(snd_));
    }

private:
    F f_;
    Snd snd_;
};

// CPO
struct transform_t {

    template<typename UpStream, typename F, typename Snd = std::remove_cvref_t<decltype(stdexec::then)>>
    requires streamable<UpStream> && transformable<UpStream, Snd, F>
    constexpr auto operator()(UpStream&& up_stream, F f, Snd snd = Snd{}) const noexcept {
        return transform_stream(std::forward<UpStream>(up_stream), std::move(f), std::move(snd));
    }

    template<typename F, typename Snd = std::remove_cvref_t<decltype(stdexec::then)>>
    constexpr auto operator()(F f, Snd snd = Snd{}) const noexcept {
        return transform_closure(std::move(f), std::move(snd));
    }

};

inline constexpr transform_t transform{};

} // namespace __transform

using __transform::transform;

} // namespace streamifex
} // namespace stdplus
} // namespace snapflakes
