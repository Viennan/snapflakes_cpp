#ifndef SNAPFLAKES_LOG_SPDLOG_FILE_HPP
#define SNAPFLAKES_LOG_SPDLOG_FILE_HPP

#include <filesystem>
#include "snapflakes/log/impl/concepts.hpp"
#include <snapflakes/log/impl/spdlog_base.hpp>
#include <spdlog/sinks/basic_file_sink.h>

namespace snapflakes {
namespace log {
namespace spd {

namespace stdfs = std::filesystem;

template<typename ThreadTag>
class HandlerCreator<concepts::FileOutTag, ThreadTag> {
public:
    static Handler create(const stdfs::path& file_path, bool truncate = false) {
        if constexpr (std::is_same_v<ThreadTag, concepts::MultiThreadTag>) {
            return Handler(std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path.string(), truncate));
        }
        return Handler(std::make_shared<spdlog::sinks::basic_file_sink_st>(file_path.string(), truncate));
    }
};

} // namespace spd
} // namespace log
} // namespace snapflakes

#endif