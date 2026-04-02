#ifndef SNAPFLAKES_LOG_CONCEPT_HPP
#define SNAPFLAKES_LOG_CONCEPT_HPP

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace snapflakes {
namespace log {
namespace concepts {

enum class Level : uint8_t { Trace, Debug, Info, Warning, Error, Critical };

class SingleThreadTag;
class MultiThreadTag;

template<typename T>
concept should_be_threaded_tag = std::same_as<T, SingleThreadTag> || std::same_as<T, MultiThreadTag>;

class StdOutTag;
class StderrOutTag;
class ColoredStdOutTag;
class ColoredStderrOutTag;
class FileOutTag;

template<typename T>
concept should_be_out_tag = std::same_as<T, StdOutTag> || std::same_as<T, StderrOutTag> || std::same_as<T, ColoredStdOutTag> || std::same_as<T, ColoredStderrOutTag> ||
                            std::same_as<T, FileOutTag>;

template<typename T, typename... Args>
concept trace = requires(T t, Args... args) {
    { t.trace("", args...) } -> std::same_as<void>;
};

template<typename T, typename... Args>
concept debug = requires(T t, Args... args) {
    { t.debug("", args...) } -> std::same_as<void>;
};

template<typename T, typename... Args>
concept info = requires(T t, Args... args) {
    { t.info("", args...) } -> std::same_as<void>;
};

template<typename T, typename... Args>
concept warn = requires(T t, Args... args) {
    { t.warn("", args...) } -> std::same_as<void>;
};

template<typename T, typename... Args>
concept error = requires(T t, Args... args) {
    { t.error("", args...) } -> std::same_as<void>;
};

template<typename T, typename... Args>
concept critical = requires(T t, Args... args) {
    { t.critical("", args...) } -> std::same_as<void>;
};

template<typename T, typename... Args>
concept has_default_logger = requires(T t) {
    { t.get_default_logger() } -> std::same_as<T>;
};

template<typename T, typename... Args>
concept default_trace_loggable = has_default_logger<T> && trace<T, Args...>;

template<typename T, typename... Args>
concept default_debug_loggable = has_default_logger<T> && debug<T, Args...>;

template<typename T, typename... Args>
concept default_info_loggable = has_default_logger<T> && info<T, Args...>;

template<typename T, typename... Args>
concept default_warn_loggable = has_default_logger<T> && warn<T, Args...>;

template<typename T, typename... Args>
concept default_error_loggable = has_default_logger<T> && error<T, Args...>;

template<typename T, typename... Args>
concept default_critical_loggable = has_default_logger<T> && critical<T, Args...>;

template<typename T>
using ctx_logger_t = std::invoke_result_t<decltype(&T::get_logger), T>;

template<typename T>
concept has_ctx_logger = requires(T t) {
    { t.get_logger() };
    requires std::is_reference_v<ctx_logger_t<T>> && !std::is_const_v<ctx_logger_t<T>>;
};

template<typename T, typename... Args>
concept ctx_trace_loggable = has_ctx_logger<T> && trace<ctx_logger_t<T>, Args...>;

template<typename T, typename... Args>
concept ctx_debug_loggable = has_ctx_logger<T> && debug<ctx_logger_t<T>, Args...>;

template<typename T, typename... Args>
concept ctx_info_loggable = has_ctx_logger<T> && info<ctx_logger_t<T>, Args...>;

template<typename T, typename... Args>
concept ctx_warn_loggable = has_ctx_logger<T> && warn<ctx_logger_t<T>, Args...>;

template<typename T, typename... Args>
concept ctx_error_loggable = has_ctx_logger<T> && error<ctx_logger_t<T>, Args...>;

template<typename T, typename... Args>
concept ctx_critical_loggable = has_ctx_logger<T> && critical<ctx_logger_t<T>, Args...>;

} // namespace concepts
} // namespace log
} // namespace snapflakes

#endif // SNAPFLAKES_LOG_CONCEPT_HPP