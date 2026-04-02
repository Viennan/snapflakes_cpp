# These examples use the standard separate compilation
set(SNAPFLAKES_TEST_SOURCES
    test_log.cpp
    test_stdplus_exec.cpp
    test_stdplus_streamifex_broadcast.cpp
)

string(REPLACE ".cpp" "" BASENAMES_SNAPFLAKES_TESTS "${SNAPFLAKES_TEST_SOURCES}")
set(SNAPFLAKES_TESTS_TARGETS ${BASENAMES_SNAPFLAKES_TESTS})

foreach(name ${SNAPFLAKES_TESTS_TARGETS})
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Catch2::Catch2WithMain snapflakes::snapflakes)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
endforeach()
