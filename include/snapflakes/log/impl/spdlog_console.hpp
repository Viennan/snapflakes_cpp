#ifndef SNAPFLAKES_LOG_SPDLOG_CONSOLE_HPP
#define SNAPFLAKES_LOG_SPDLOG_CONSOLE_HPP

#include <memory>
#include "snapflakes/log/impl/concepts.hpp"
#include <snapflakes/log/impl/spdlog_base.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>

namespace snapflakes {
namespace log {
namespace spd {

template<typename ThreadTag>
class HandlerCreator<concepts::StdOutTag, ThreadTag> {
public:
    static Handler create() {
        if constexpr (std::is_same_v<ThreadTag, concepts::MultiThreadTag>) {
            return Handler(std::make_shared<spdlog::sinks::stdout_sink_mt>());
        }
        return Handler(std::make_shared<spdlog::sinks::stdout_sink_st>());
    }
};

template<typename ThreadTag>
class HandlerCreator<concepts::StderrOutTag, ThreadTag> {
public:
    static Handler create() {
        if constexpr (std::is_same_v<ThreadTag, concepts::MultiThreadTag>) {
            return Handler(std::make_shared<spdlog::sinks::stderr_sink_mt>());
        }
        return Handler(std::make_shared<spdlog::sinks::stderr_sink_st>());
    }
};

template<typename ThreadTag>
class HandlerCreator<concepts::ColoredStdOutTag, ThreadTag> {
public:
    static Handler create() {
        if constexpr (std::is_same_v<ThreadTag, concepts::MultiThreadTag>) {
            return Handler(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        }
        return Handler(std::make_shared<spdlog::sinks::stdout_color_sink_st>());
    }
};

template<typename ThreadTag>
class HandlerCreator<concepts::ColoredStderrOutTag, ThreadTag> {
public:
    static Handler create() {
        if constexpr (std::is_same_v<ThreadTag, concepts::MultiThreadTag>) {
            return Handler(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
        }
        return Handler(std::make_shared<spdlog::sinks::stderr_color_sink_st>());
    }
};

} // namespace spd
} // namespace log
} // namespace snapflakes

#endif // SNAPFLAKES_LOG_SPDLOG_CONSOLE_HPP