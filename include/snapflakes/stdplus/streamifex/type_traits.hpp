#pragma once

#include <optional>
#include <snapflakes/stdplus/exec/std.hpp>
#include <snapflakes/stdplus/streamifex/concepts.hpp>

namespace snapflakes {
namespace stdplus {
namespace streamifex {

template<typename Stream, typename Env = stdexec::env<>>
requires streamable<Stream>
using stream_sender_t = std::remove_cvref_t<decltype(std::declval<Stream&>().next())>;

template<typename Stream, typename Env = stdexec::env<>>
requires streamable<Stream>
using stream_completion_signatures_t = stdexec::completion_signatures_of_t<stream_sender_t<Stream, Env>, Env>;
        
}
}
}
