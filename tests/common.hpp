#ifndef SNAPFLAKES_TESTS_COMMON_HPP
#define SNAPFLAKES_TESTS_COMMON_HPP

#include <filesystem>

namespace stdfs = std::filesystem;

namespace snapflakes::tests {

inline auto get_tests_root_dir() -> stdfs::path {
    return stdfs::path(".tmp/tests");
}



}

#endif
