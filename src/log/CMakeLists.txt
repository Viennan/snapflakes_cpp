add_library(snapflakes_log INTERFACE)
add_library(snapflakes::log ALIAS snapflakes_log)
set_target_properties(snapflakes_log
    PROPERTIES 
        EXPORT_NAME "log"
)

set(VALID_LOG_LEVELS "trace" "debug" "info" "warn" "error" "critical" "off")
list(FIND VALID_LOG_LEVELS ${SNAPFLAKES_ACTIVE_LOG_LEVEL} LEVEL_INDEX)
if(LEVEL_INDEX EQUAL -1)
    message(FATAL_ERROR 
        "Invalid spdlog level '${SNAPFLAKES_ACTIVE_LOG_LEVEL}'. "
        "Please set SNAPFLAKES_ACTIVE_LOG_LEVEL to one of: trace, debug, info, warn, error, critical, off")
endif()

if(SNAPFLAKES_USE_SPDLOG)
    # Set the active log level for spdlog
    target_compile_definitions(snapflakes_log INTERFACE SPDLOG_ACTIVE_LEVEL=${LEVEL_INDEX})
    target_compile_definitions(snapflakes_log INTERFACE SNAPFLAKES_USE_SPDLOG)
endif()

target_include_directories(snapflakes_log INTERFACE
    $<BUILD_INTERFACE:${SNAPFLAKES_PROJECT_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(snapflakes_log INTERFACE
    $<$<BOOL:${SNAPFLAKES_USE_SPDLOG}>:spdlog::spdlog>
)

list(APPEND SNAPFLAKES_EXPORT_TARGETS snapflakes_log)
set(SNAPFLAKES_EXPORT_TARGETS "${SNAPFLAKES_EXPORT_TARGETS}" PARENT_SCOPE)

list(APPEND SNAPFLAKES_ENABLED_COMPONENTS snapflakes::log)
set(SNAPFLAKES_ENABLED_COMPONENTS "${SNAPFLAKES_ENABLED_COMPONENTS}" PARENT_SCOPE)
list(APPEND SNAPFLAKES_COMPONENT_INSTALL_SRC_DIRS ${SNAPFLAKES_PROJECT_INCLUDE_DIR}/log)
set(SNAPFLAKES_COMPONENT_INSTALL_SRC_DIRS "${SNAPFLAKES_COMPONENT_INSTALL_SRC_DIRS}" PARENT_SCOPE)
list(APPEND SNAPFLAKES_COMPONENT_INSTALL_PREFIXES "log")
set(SNAPFLAKES_COMPONENT_INSTALL_PREFIXES "${SNAPFLAKES_COMPONENT_INSTALL_PREFIXES}" PARENT_SCOPE)
