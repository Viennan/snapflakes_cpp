#ifndef SNAPFLAKES_LOG_CONSOLE_HPP
#define SNAPFLAKES_LOG_CONSOLE_HPP

#include <tuple>
#include <snapflakes/log/base.hpp>

#ifdef SNAPFLAKES_USE_SPDLOG
#include <snapflakes/log/impl/spdlog_console.hpp>
#endif

namespace snapflakes {
namespace log {

// Handler

inline Handler create_stdout_handler_mt() {
    return impl::HandlerCreator<concepts::StdOutTag, concepts::MultiThreadTag>::create();
}

inline Handler create_stdout_handler_st() {
    return impl::HandlerCreator<concepts::StdOutTag, concepts::SingleThreadTag>::create();
}

inline Handler create_stderr_handler_mt() {
    return impl::HandlerCreator<concepts::StderrOutTag, concepts::MultiThreadTag>::create();
}

inline Handler create_stderr_handler_st() {
    return impl::HandlerCreator<concepts::StderrOutTag, concepts::SingleThreadTag>::create();
}

inline std::tuple<Handler, Handler> create_console_handlers_mt() {
    return std::make_tuple(create_stdout_handler_mt(), create_stderr_handler_mt());
}

inline std::tuple<Handler, Handler> create_console_handlers_st() {
    return std::make_tuple(create_stdout_handler_st(), create_stderr_handler_st());
}

inline Handler create_colored_stdout_handler_mt() {
    return impl::HandlerCreator<concepts::ColoredStdOutTag, concepts::MultiThreadTag>::create();
}

inline Handler create_colored_stdout_handler_st() {
    return impl::HandlerCreator<concepts::ColoredStdOutTag, concepts::SingleThreadTag>::create();
}

inline Handler create_colored_stderr_handler_mt() {
    return impl::HandlerCreator<concepts::ColoredStderrOutTag, concepts::MultiThreadTag>::create();
}

inline Handler create_colored_stderr_handler_st() {
    return impl::HandlerCreator<concepts::ColoredStderrOutTag, concepts::SingleThreadTag>::create();
}

inline std::tuple<Handler, Handler> create_colored_console_handlers_mt() {
    return std::make_tuple(create_colored_stdout_handler_mt(), create_colored_stderr_handler_mt());
}

inline std::tuple<Handler, Handler> create_colored_console_handlers_st() {
    return std::make_tuple(create_colored_stdout_handler_st(), create_colored_stderr_handler_st());
}

} // namespace log
} // namespace snapflakes

#endif // SNAPFLAKES_LOG_CONSOLE_HPP