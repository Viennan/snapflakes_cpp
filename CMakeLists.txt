cmake_minimum_required(VERSION 3.28)

project(snapflakes LANGUAGES CXX VERSION 0.0.1)

# C++ language settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(NOTICE "snapflakes: disable CMAKE_CXX_SCAN_FOR_MODULES for Clang compiler due to the issue: https://github.com/llvm/llvm-project/pull/184235")
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

# snapflakes options
option(SNAPFLAKES_ENABLE_LOGGING "Enable logging support in snapflakes" ON)
set(SNAPFLAKES_ACTIVE_LOG_LEVEL "info" CACHE STRING "Set the active log level for snapflakes (e.g., trace, debug, info, warn, error, critical, off)")
option(SNAPFLAKES_ENABLE_STDPLUS "Enable stdplus module in snapflakes" ON)
option(SNAPFLAKES_BUILD_TESTS "Build snapflakes unit tests" OFF)

# 3rd-party dependencies options

# SPDLOG Options
option(SNAPFLAKES_USE_SPDLOG "Enable SPDLOG logging library support" ON)
set(SNAPFLAKES_SPDLOG_VERSION "1.17.0" CACHE STRING "Specify the version of SPDLOG to use (e.g., 1.17.0)")

# Process 3rd-party dependencies

# CPM - C++ Package Manager
include(cmake/CPM.cmake)

# SPDLOG library
if(SNAPFLAKES_USE_SPDLOG)
    CPMAddPackage(
        NAME spdlog
        VERSION ${SNAPFLAKES_SPDLOG_VERSION}
        GITHUB_REPOSITORY gabime/spdlog
    )
endif()

# STDEXEC library
if(SNAPFLAKES_ENABLE_STDPLUS)
    # generate install targets of STDEXEC
    set(STDEXEC_INSTALL ON)
    CPMAddPackage(
        NAME STDEXEC
        GITHUB_REPOSITORY NVIDIA/stdexec
        GIT_TAG main
    )
endif()

# Project compile settings
set(SNAPFLAKES_PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(SNAPFLAKES_INSTALL_INCLUDE_DIR_PREFIX "include/snapflakes")
set(SNAPFLAKES_EXPORT_TARGETS "") # for collecting exported targets in subdirectories

set(SNAPFLAKES_ENABLED_COMPONENTS "")
set(SNAPFLAKES_COMPONENT_INSTALL_SRC_DIRS "")
set(SNAPFLAKES_COMPONENT_INSTALL_PREFIXES "")
set(SNPAFLAKES_GLOBAL_INTERFACE_FILES "")

if(SNAPFLAKES_ENABLE_LOGGING)
    add_subdirectory("src/log")
    list(APPEND SNPAFLAKES_GLOBAL_INTERFACE_FILES "log.hpp")
endif()

if(SNAPFLAKES_ENABLE_STDPLUS)
    add_subdirectory("src/stdplus")
    list(APPEND SNPAFLAKES_GLOBAL_INTERFACE_FILES "stdplus.hpp")
endif()

# snapflakes interface aggregation target
add_library(snapflakes INTERFACE)
add_library(snapflakes::snapflakes ALIAS snapflakes)

target_include_directories(snapflakes
    INTERFACE
        $<BUILD_INTERFACE:${SNAPFLAKES_PROJECT_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(
    snapflakes 
    INTERFACE
        ${SNAPFLAKES_ENABLED_COMPONENTS}
)

# Unit tests

if(SNAPFLAKES_BUILD_TESTS)
    CPMAddPackage(
        NAME Catch2
        GITHUB_REPOSITORY catchorg/Catch2
        VERSION 3.13.0
    )
    enable_testing()
    add_subdirectory("tests")
endif()

# Install commands

# install include headers of all components
foreach(component_include_src_dir component_install_prefix ZIP_LISTS SNAPFLAKES_COMPONENT_INSTALL_SRC_DIRS SNAPFLAKES_COMPONENT_INSTALL_PREFIXES)
    install(DIRECTORY ${component_include_src_dir}
        DESTINATION ${SNAPFLAKES_INSTALL_INCLUDE_DIR_PREFIX}/${component_install_prefix}
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
    )
endforeach()
# install global interface (snapflakes::snapflakes) headers
install(FILES ${SNAPFLAKES_GLOBAL_INTERFACE_FILES}
    DESTINATION ${SNAPFLAKES_INSTALL_INCLUDE_DIR_PREFIX}
)

# install all exported targets
list(APPEND SNAPFLAKES_EXPORT_TARGETS snapflakes)
install(TARGETS ${SNAPFLAKES_EXPORT_TARGETS}
    EXPORT snapflakesTargets
    # LIBRARY DESTINATION lib
    # ARCHIVE DESTINATION lib
)

include(GNUInstallDirs)

set(config_install_dir ${CMAKE_INSTALL_LIBDIR}/cmake/snapflakes)
set(config_file_name snapflakesConfig.cmake)
set(config_version_file_name snapflakesConfigVersion.cmake)

configure_file(
    "cmake/snapflakesConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${config_file_name}"
    @ONLY
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${config_version_file_name}"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${config_file_name}"
        "${CMAKE_CURRENT_BINARY_DIR}/${config_version_file_name}"
    DESTINATION ${config_install_dir}
)

install(
    EXPORT snapflakesTargets
    NAMESPACE snapflakes::
    DESTINATION ${config_install_dir}
)
