#ifndef SNAPFLAKES_LOGGER_HPP
#define SNAPFLAKES_LOGGER_HPP

#include <snapflakes/log/base.hpp>

namespace snapflakes {
namespace log {

template<typename... Args>
requires concepts::trace<Logger, Args...>
inline void trace(format_string_t<Args...> fmt, Args &&...args) {
    Logger::get_default_logger().trace(fmt, std::forward<Args>(args)...);
};

template<typename... Args>
requires concepts::debug<Logger, Args...>
inline void debug(format_string_t<Args...> fmt, Args &&...args) {
    Logger::get_default_logger().debug(fmt, std::forward<Args>(args)...);
};

template<typename... Args>
requires concepts::info<Logger, Args...>
inline void info(format_string_t<Args...> fmt, Args &&...args) {
    Logger::get_default_logger().info(fmt, std::forward<Args>(args)...);
};

template<typename... Args>
requires concepts::warn<Logger, Args...>
inline void warn(format_string_t<Args...> fmt, Args &&...args) {
    Logger::get_default_logger().warn(fmt, std::forward<Args>(args)...);
};

template<typename... Args>
requires concepts::error<Logger, Args...>
inline void error(format_string_t<Args...> fmt, Args &&...args) {
    Logger::get_default_logger().error(fmt, std::forward<Args>(args)...);
};

template<typename... Args>
requires concepts::critical<Logger, Args...>
inline void critical(format_string_t<Args...> fmt, Args &&...args) {
    Logger::get_default_logger().critical(fmt, std::forward<Args>(args)...);
};

inline void set_level(Level level) {
    Logger::get_default_logger().set_level(level);
};

inline void set_pattern(const std::string &pattern) {
    Logger::get_default_logger().set_pattern(pattern);
};

inline Logger get_default_logger() {
    return Logger::get_default_logger();
};

inline void set_default_logger(const Logger &logger) {
    Logger::set_default_logger(logger);
};

inline void set_default_logger(Logger &&logger) {
    Logger::set_default_logger(std::move(logger));
};

inline void flush() {
    Logger::get_default_logger().flush();
};

template<typename Ctx, typename... Args>
requires concepts::ctx_trace_loggable<Ctx, Args...>
inline void ctx_trace(Ctx &ctx, format_string_t<Args...> fmt, Args &&...args) {
    ctx.get_logger().trace(fmt, std::forward<Args>(args)...);
};

template<typename Ctx, typename... Args>
requires concepts::ctx_debug_loggable<Ctx, Args...>
inline void ctx_debug(Ctx &ctx, format_string_t<Args...> fmt, Args &&...args) {
    ctx.get_logger().debug(fmt, std::forward<Args>(args)...);
};

template<typename Ctx, typename... Args>
requires concepts::ctx_info_loggable<Ctx, Args...>
inline void ctx_info(Ctx &ctx, format_string_t<Args...> fmt, Args &&...args) {
    ctx.get_logger().info(fmt, std::forward<Args>(args)...);
};

template<typename Ctx, typename... Args>
requires concepts::ctx_warn_loggable<Ctx, Args...>
inline void ctx_warn(Ctx &ctx, format_string_t<Args...> fmt, Args &&...args) {
    ctx.get_logger().warn(fmt, std::forward<Args>(args)...);
};

template<typename Ctx, typename... Args>
requires concepts::ctx_error_loggable<Ctx, Args...>
inline void ctx_error(Ctx &ctx, format_string_t<Args...> fmt, Args &&...args) {
    ctx.get_logger().error(fmt, std::forward<Args>(args)...);
};

template<typename Ctx, typename... Args>
requires concepts::ctx_critical_loggable<Ctx, Args...>
inline void ctx_critical(Ctx &ctx, format_string_t<Args...> fmt, Args &&...args) {
    ctx.get_logger().critical(fmt, std::forward<Args>(args)...);
};

inline Logger create_logger_with_handlers(const std::string &name, const Handler &handler) {
    return Logger(name, handler);
};

inline Logger create_logger_with_handlers(const std::string &name, Handler &&handler) {
    return Logger(name, std::move(handler));
};

template<typename... Args>
requires (std::is_same_v<std::decay_t<Args>, Handler> && ...)
inline Logger create_logger_with_handlers(const std::string &name, Args &&...handlers) {
    return Logger(name, std::forward<Args>(handlers)...);
}

inline Logger create_logger_with_handlers(const std::string &name, std::initializer_list<Handler> list) {
    return Logger(name, list);
}

} // namespace log
} // namespace snapflakes

#define SNPFLK_TRACE(...) SNPFLK_TRACE_IMPL(__VA_ARGS__)
#define SNPFLK_DEBUG(...) SNPFLK_DEBUG_IMPL(__VA_ARGS__)
#define SNPFLK_INFO(...) SNPFLK_INFO_IMPL(__VA_ARGS__)  
#define SNPFLK_WARN(...) SNPFLK_WARN_IMPL(__VA_ARGS__)  
#define SNPFLK_ERROR(...) SNPFLK_ERROR_IMPL(__VA_ARGS__)  
#define SNPFLK_CRITICAL(...) SNPFLK_CRITICAL_IMPL(__VA_ARGS__)

#define SNPFLK_LOGGER_TRACE(logger, ...) SNPFLK_LOGGER_TRACE_IMPL(logger, __VA_ARGS__)
#define SNPFLK_LOGGER_DEBUG(logger, ...) SNPFLK_LOGGER_DEBUG_IMPL(logger, __VA_ARGS__)
#define SNPFLK_LOGGER_INFO(logger, ...) SNPFLK_LOGGER_INFO_IMPL(logger, __VA_ARGS__)
#define SNPFLK_LOGGER_WARN(logger, ...) SNPFLK_LOGGER_WARN_IMPL(logger, __VA_ARGS__)
#define SNPFLK_LOGGER_ERROR(logger, ...) SNPFLK_LOGGER_ERROR_IMPL(logger, __VA_ARGS__)
#define SNPFLK_LOGGER_CRITICAL(logger, ...) SNPFLK_LOGGER_CRITICAL_IMPL(logger, __VA_ARGS__)

#define SNPFLK_CTX_LOGGER_TRACE(ctx, ...) SNPFLK_LOGGER_TRACE_IMPL(ctx.get_logger(), __VA_ARGS__)
#define SNPFLK_CTX_LOGGER_DEBUG(ctx, ...) SNPFLK_LOGGER_DEBUG_IMPL(ctx.get_logger(), __VA_ARGS__)
#define SNPFLK_CTX_LOGGER_INFO(ctx, ...) SNPFLK_LOGGER_INFO_IMPL(ctx.get_logger(), __VA_ARGS__)
#define SNPFLK_CTX_LOGGER_WARN(ctx, ...) SNPFLK_LOGGER_WARN_IMPL(ctx.get_logger(), __VA_ARGS__)
#define SNPFLK_CTX_LOGGER_ERROR(ctx, ...) SNPFLK_LOGGER_ERROR_IMPL(ctx.get_logger(), __VA_ARGS__)
#define SNPFLK_CTX_LOGGER_CRITICAL(ctx, ...) SNPFLK_LOGGER_CRITICAL_IMPL(ctx.get_logger(), __VA_ARGS__)

#endif