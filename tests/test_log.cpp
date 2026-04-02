#include <fstream>
#include <string>
#include "common.hpp"
#include "snapflakes/log/base.hpp"
#include "snapflakes/log/console.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <snapflakes/log.hpp>

namespace sf = snapflakes;

static inline auto get_test_log_root_dir() -> stdfs::path {
    auto p = sf::tests::get_tests_root_dir() / "log";
    if (!stdfs::exists(p)) {
        stdfs::create_directories(p);
    }
    return p;
}

static std::string read_file_to_string(const stdfs::path& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("failed open file: " + file_path.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static void write_string_to_file(const stdfs::path& file_path, const std::string& content) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("failed open file: " + file_path.string());
    }
    file << content;
    file.close();
}

TEST_CASE("log levels", "[log]") {
    sf::log::set_level(sf::log::Level::Trace);

    sf::log::trace("This is a trace message: {}", 1);
    sf::log::debug("This is a debug message: {}", 2);
    sf::log::info("This is an info message: {}", 3);
    sf::log::warn("This is a warning message: {}", 4);
    sf::log::error("This is an error message: {}", 5);
    sf::log::critical("This is a critical message: {}", 6);
    sf::log::flush();
    // No assertions here, just ensure that the logging functions compile and run without errors
};

TEST_CASE("console handlers", "[log]") {
    // Test creating console handlers
    auto stdout_handler_mt = sf::log::create_stdout_handler_mt();
    auto stdout_handler_st = sf::log::create_stdout_handler_st();
    auto stderr_handler_mt = sf::log::create_stderr_handler_mt();
    auto stderr_handler_st = sf::log::create_stderr_handler_st();

    // Test creating colored console handlers
    auto colored_stdout_handler_mt = sf::log::create_colored_stdout_handler_mt();
    auto colored_stdout_handler_st = sf::log::create_colored_stdout_handler_st();
    auto colored_stderr_handler_mt = sf::log::create_colored_stderr_handler_mt();
    auto colored_stderr_handler_st = sf::log::create_colored_stderr_handler_st();

    // Test creating console handler pairs
    auto console_handlers_mt = sf::log::create_console_handlers_mt();
    auto console_handlers_st = sf::log::create_console_handlers_st();
    auto colored_console_handlers_mt = sf::log::create_colored_console_handlers_mt();
    auto colored_console_handlers_st = sf::log::create_colored_console_handlers_st();

    // No assertions here, just ensure that the handler creation functions compile and run without errors
};

static void _test_file_handler(const stdfs::path& log_file, const std::string& logger_name, const std::string& test_msg) {
    auto file_handler_mt = sf::log::create_file_handler_mt(log_file);
    auto logger_mt = sf::log::create_logger_with_handlers(logger_name, file_handler_mt);
    logger_mt.info("{}", test_msg);
    logger_mt.flush();
}

TEST_CASE("file handlers", "[log]") {
    auto root_dir = get_test_log_root_dir();

    // Test mt file handlers
    auto log_file_path_mt = root_dir / "test_file_handlers_mt.log";
    stdfs::remove(log_file_path_mt);
    std::string test_msg_mt = "test message for file handler mt";
    _test_file_handler(log_file_path_mt, "file_mt", test_msg_mt);
    REQUIRE(stdfs::exists(log_file_path_mt));
    auto log_content_mt = read_file_to_string(log_file_path_mt);
    REQUIRE(log_content_mt.find(test_msg_mt) != std::string::npos);

    // Test st file handlers
    auto log_file_path_st = root_dir / "test_file_handlers_st.log";
    stdfs::remove(log_file_path_st);
    std::string test_msg_st = "test message for file handler st";
    _test_file_handler(log_file_path_st, "file_st", test_msg_st);
    REQUIRE(stdfs::exists(log_file_path_st));
    auto log_content_st = read_file_to_string(log_file_path_st);
    REQUIRE(log_content_st.find(test_msg_st) != std::string::npos);

    // Test creating file handlers with appending mode
    auto log_file_path_append = root_dir / "test_file_append.log";
    stdfs::remove(log_file_path_append);
    std::string test_msg_append_prefix = "test message for file handler append: ";
    std::string test_msg_append_suffix = "i am the suffix";
    write_string_to_file(log_file_path_append, test_msg_append_prefix);
    auto file_handler_append = sf::log::create_file_handler_mt(log_file_path_append, false);
    _test_file_handler(log_file_path_append, "file_append", test_msg_append_suffix);
    REQUIRE(stdfs::exists(log_file_path_append));
    auto log_content_append = read_file_to_string(log_file_path_append);
    auto prefix_pos = log_content_append.find(test_msg_append_prefix);
    REQUIRE(prefix_pos != std::string::npos);
    auto suffix_pos = log_content_append.find(test_msg_append_suffix);
    REQUIRE(suffix_pos != std::string::npos);
    REQUIRE(suffix_pos > prefix_pos);
};

TEST_CASE("custom logger", "[log]") {
    auto root_dir = get_test_log_root_dir();

    // Create handlers
    auto console_handler = sf::log::create_colored_stdout_handler_mt();
    auto log_file_path = root_dir / "test_custom_log.log";
    stdfs::remove(log_file_path);
    auto file_handler = sf::log::create_file_handler_mt(log_file_path);

    // Create custom logger with multiple handlers
    auto logger = sf::log::create_logger_with_handlers("custom", {console_handler, file_handler});

    // Test logging with custom logger
    logger.set_level(sf::log::Level::Trace);
    logger.trace("Custom trace message");
    logger.info("Custom info message");
    logger.flush();

    // Test setting default logger
    auto original_default_logger = sf::log::get_default_logger();
    sf::log::set_default_logger(logger);
    sf::log::info("Default logger info message");
    sf::log::flush();

    // reset default logger
    sf::log::set_default_logger(original_default_logger);

    // Check log file content
    REQUIRE(stdfs::exists(log_file_path));
    auto log_content = read_file_to_string(log_file_path);
    REQUIRE(log_content.find("Custom trace message") != std::string::npos);
    REQUIRE(log_content.find("Custom info message") != std::string::npos);
    REQUIRE(log_content.find("Default logger info message") != std::string::npos);
};

TEST_CASE("Log format", "[log]") {
    // Test setting log pattern
    sf::log::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");

    // Test logging with custom pattern
    sf::log::info("Message with custom pattern");

    sf::log::flush();
    // No assertions here, just ensure that the pattern setting function compiles and runs without errors
};

TEST_CASE("Context logging", "[log]") {
    // Create a simple context class
    class TestContext {
    public:
        sf::log::Logger& get_logger() const {
            return logger;
        }

    private:
        mutable sf::log::Logger logger = sf::log::create_logger_with_handlers("test_context_logger", sf::log::create_colored_stdout_handler_mt());
    };

    TestContext ctx;

    // Test context logging functions
    sf::log::ctx_info(ctx, "Context info message");
    sf::log::ctx_warn(ctx, "Context warn message");
    ctx.get_logger().flush();

    // No assertions here, just ensure that the context logging functions compile and run without errors
};

TEST_CASE("Log macros", "[log]") {
    // Test basic log macros
    SNPFLK_TRACE("Macro trace message: {}", 1);
    SNPFLK_DEBUG("Macro debug message: {}", 2);
    SNPFLK_INFO("Macro info message: {}", 3);
    SNPFLK_WARN("Macro warning message: {}", 4);
    SNPFLK_ERROR("Macro error message: {}", 5);
    SNPFLK_CRITICAL("Macro critical message: {}", 6);

    // Test logger-specific macros
    auto logger = sf::log::create_logger_with_handlers("test_macro_logger", 
        sf::log::create_colored_stdout_handler_mt(), sf::log::create_colored_stderr_handler_mt());
    logger.set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v"); // [%@] means src file name and line number
    logger.set_level(sf::log::Level::Trace);
    SNPFLK_LOGGER_TRACE(logger, "Logger macro trace message");
    SNPFLK_LOGGER_INFO(logger, "Logger macro info message");
    SNPFLK_LOGGER_ERROR_IMPL(logger, "Logger macro error message");
    logger.flush();

    // Test context logger macros
    class TestContext {
    public:
        sf::log::Logger &get_logger() const {
            return logger;
        }

    private:
        mutable sf::log::Logger logger = sf::log::create_logger_with_handlers("test_ctx_macro_logger", sf::log::create_colored_stdout_handler_mt());
    };

    TestContext ctx;
    SNPFLK_CTX_LOGGER_INFO(ctx, "Context macro info message");
    ctx.get_logger().flush();

    sf::log::flush();
    // No assertions here, just ensure that the macros compile and run without errors
};