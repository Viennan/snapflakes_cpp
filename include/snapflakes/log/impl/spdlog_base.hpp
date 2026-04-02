#ifndef SNAPFLAKES_LOG_SPDLOG_HPP
#define SNAPFLAKES_LOG_SPDLOG_HPP

#include <array>
#include <initializer_list>
#include <memory>
#include <ranges>
#include <snapflakes/log/impl/concepts.hpp>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>
#include <type_traits>

namespace snapflakes {
namespace log {
namespace spd {

template<typename... Args>
using format_string_t = spdlog::format_string_t<Args...>;

using Level = concepts::Level;

inline spdlog::level::level_enum sf_level_to_spd_level(Level level) {
    switch (level) {
    case snapflakes::log::concepts::Level::Trace:
        return spdlog::level::trace;
    case snapflakes::log::concepts::Level::Debug:
        return spdlog::level::debug;
    case snapflakes::log::concepts::Level::Info:
        return spdlog::level::info;
    case snapflakes::log::concepts::Level::Warning:
        return spdlog::level::warn;
    case snapflakes::log::concepts::Level::Error:
        return spdlog::level::err;
    case snapflakes::log::concepts::Level::Critical:
        return spdlog::level::critical;
    default:
        return spdlog::level::info; // default to info if unknown
    }
};

class Logger;

class Handler final {
    friend class Logger;

public:
    Handler() = delete;
    Handler(const Handler &) = default;
    Handler(Handler &&) = default;
    Handler &operator=(const Handler &) = default;
    Handler &operator=(Handler &&) = default;

    explicit Handler(const spdlog::sink_ptr &sink) : m_sink(sink) {};

    explicit Handler(spdlog::sink_ptr &&sink) : m_sink(std::move(sink)) {};

    void set_level(Level level) {
        m_sink->set_level(sf_level_to_spd_level(level));
    }

    void set_pattern(const std::string &pattern) {
        m_sink->set_pattern(pattern);
    }

    void flush() {
        m_sink->flush();
    }

private:
    spdlog::sink_ptr m_sink;
};

template<typename T, typename ThreadedTag>
requires concepts::should_be_out_tag<T> && concepts::should_be_threaded_tag<ThreadedTag>
class HandlerCreator {
public:
    static Handler create();
};

class Logger final {
public:
    Logger() = delete;
    Logger(const Logger &) = default;
    Logger(Logger &&) = default;
    Logger &operator=(const Logger &) = default;
    Logger &operator=(Logger &&) = default;

    explicit Logger(const std::shared_ptr<spdlog::logger> &logger) : m_logger(logger) {};

    explicit Logger(std::shared_ptr<spdlog::logger> &&logger) : m_logger(std::move(logger)) {};

    Logger(const std::string &name) : m_logger(std::make_shared<spdlog::logger>(name)) {};

    Logger(const std::string &name, const Handler &handler) : m_logger(std::make_shared<spdlog::logger>(name, handler.m_sink)) {};

    Logger(const std::string &name, Handler &&handler) : m_logger(std::make_shared<spdlog::logger>(name, std::move(handler.m_sink))) {};

    template<typename... Args>
    requires (std::is_same_v<std::decay_t<Args>, Handler> && ...)
    Logger(const std::string &name, Args &&...handlers) : m_logger(nullptr) {
        std::array<Handler, sizeof...(handlers)> handler_array{std::forward<Args>(handlers)...};
        if constexpr ( (std::is_rvalue_reference_v<Args> && ...) ) {
            auto sinks_view = std::views::transform(handler_array, [](Handler &handler) -> spdlog::sink_ptr && { return std::move(handler.m_sink); });
            m_logger = std::make_shared<spdlog::logger>(name, sinks_view.begin(), sinks_view.end());
        } else {
            auto sinks_view = std::views::transform(handler_array, [](const Handler &handler) -> const spdlog::sink_ptr & { return handler.m_sink; });
            m_logger = std::make_shared<spdlog::logger>(name, sinks_view.cbegin(), sinks_view.cend());
        }
    };

    Logger(const std::string &name, std::initializer_list<Handler> list) : m_logger(nullptr) {
        auto sinks_view = std::views::transform(list, [](const Handler &handler) -> const spdlog::sink_ptr & { return handler.m_sink; });
        m_logger = std::make_shared<spdlog::logger>(name, sinks_view.cbegin(), sinks_view.cend());
    };

    template<typename... Args>
    void trace(spdlog::format_string_t<Args...> fmt, Args &&...args) {
        m_logger->trace(fmt, std::forward<Args>(args)...);
    };

    template<typename... Args>
    void debug(spdlog::format_string_t<Args...> fmt, Args &&...args) {
        m_logger->debug(fmt, std::forward<Args>(args)...);
    };

    template<typename... Args>
    void info(spdlog::format_string_t<Args...> fmt, Args &&...args) {
        m_logger->info(fmt, std::forward<Args>(args)...);
    };

    template<typename... Args>
    void warn(spdlog::format_string_t<Args...> fmt, Args &&...args) {
        m_logger->warn(fmt, std::forward<Args>(args)...);
    };

    template<typename... Args>
    void error(spdlog::format_string_t<Args...> fmt, Args &&...args) {
        m_logger->error(fmt, std::forward<Args>(args)...);
    };

    template<typename... Args>
    void critical(spdlog::format_string_t<Args...> fmt, Args &&...args) {
        m_logger->critical(fmt, std::forward<Args>(args)...);
    };

    void set_level(Level level) {
        m_logger->set_level(sf_level_to_spd_level(level));
    };

    template<typename P>
    void set_pattern(P &&pattern) {
        m_logger->set_pattern(std::forward<P>(pattern), spdlog::pattern_time_type::local);
    };

    void flush() {
        m_logger->flush();
    };

    std::shared_ptr<spdlog::logger>& get_raw_logger() {
        return m_logger;
    }

    const std::string &name() const {
        return m_logger->name();
    };

    static Logger get_default_logger() {
        return Logger(spdlog::default_logger());
    }

    static void set_default_logger(const Logger &logger) {
        spdlog::set_default_logger(logger.m_logger);
    }

    static void set_default_logger(Logger &&logger) {
        spdlog::set_default_logger(std::move(logger.m_logger));
    }

private:
    std::shared_ptr<spdlog::logger> m_logger;
};

} // namespace spd
} // namespace log
} // namespace snapflakes

#endif // SNAPFLAKES_LOG_SPDLOG_HPP