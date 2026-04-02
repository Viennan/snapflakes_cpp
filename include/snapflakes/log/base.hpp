#ifndef SNAPFLAKES_LOG_IMPL_BASE_HPP
#define SNAPFLAKES_LOG_IMPL_BASE_HPP

#include <snapflakes/log/impl/concepts.hpp>

#ifdef SNAPFLAKES_USE_SPDLOG
#include <snapflakes/log/impl/spdlog_base.hpp>

#define SNPFLK_LOGGER_TRACE_IMPL(logger, ...) SPDLOG_LOGGER_TRACE(logger.get_raw_logger(), __VA_ARGS__)
#define SNPFLK_LOGGER_DEBUG_IMPL(logger, ...) SPDLOG_LOGGER_DEBUG(logger.get_raw_logger(), __VA_ARGS__)
#define SNPFLK_LOGGER_INFO_IMPL(logger, ...) SPDLOG_LOGGER_INFO(logger.get_raw_logger(), __VA_ARGS__)
#define SNPFLK_LOGGER_WARN_IMPL(logger, ...) SPDLOG_LOGGER_WARN(logger.get_raw_logger(), __VA_ARGS__)
#define SNPFLK_LOGGER_ERROR_IMPL(logger, ...) SPDLOG_LOGGER_ERROR(logger.get_raw_logger(), __VA_ARGS__)
#define SNPFLK_LOGGER_CRITICAL_IMPL(logger, ...) SPDLOG_LOGGER_CRITICAL(logger.get_raw_logger(), __VA_ARGS__)

#define SNPFLK_TRACE_IMPL(...) SPDLOG_TRACE(__VA_ARGS__)
#define SNPFLK_DEBUG_IMPL(...) SPDLOG_DEBUG(__VA_ARGS__)
#define SNPFLK_INFO_IMPL(...) SPDLOG_INFO(__VA_ARGS__)
#define SNPFLK_WARN_IMPL(...) SPDLOG_WARN(__VA_ARGS__)
#define SNPFLK_ERROR_IMPL(...) SPDLOG_ERROR(__VA_ARGS__)
#define SNPFLK_CRITICAL_IMPL(...) SPDLOG_CRITICAL(__VA_ARGS__)

namespace snapflakes {
namespace log {

template<typename... Args>
using format_string_t = spd::format_string_t<Args...>;

using Handler = spd::Handler;

using Logger = spd::Logger;

namespace impl {

template<typename OutTag, typename ThreadTag>
using HandlerCreator = spd::HandlerCreator<OutTag, ThreadTag>;

}

}
}
#endif

namespace snapflakes {
namespace log {

using Level = concepts::Level;

}
}

#endif // SNAPFLAKES_LOG_IMPL_BASE_HPP