#ifndef SNAPFLAKES_LOG_FILE_HPP
#define SNAPFLAKES_LOG_FILE_HPP

#include <filesystem>
#include <snapflakes/log/base.hpp>

#ifdef SNAPFLAKES_USE_SPDLOG
#include <snapflakes/log/impl/spdlog_file.hpp>
#endif

namespace snapflakes {
namespace log {

namespace stdfs = std::filesystem;

inline Handler create_file_handler_mt(const stdfs::path& file_path, bool truncate = false) {
    return impl::HandlerCreator<concepts::FileOutTag, concepts::MultiThreadTag>::create(file_path, truncate);
};

inline Handler create_file_handler_st(const std::string &filename, bool truncate = false) {
    return impl::HandlerCreator<concepts::FileOutTag, concepts::SingleThreadTag>::create(filename, truncate);
}

} // namespace log
} // namespace snapflakes

#endif