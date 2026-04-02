# Snapflakes Log 模块使用指南

## 1. 模块概述

Snapflakes Log 模块是一个灵活、高效的日志封装，提供了多种日志输出方式和配置选项。

## 2. 头文件目录结构

```
snapflakes/
├── log.hpp # 集合头文件，一次性包含log子目录下所有头文件
└── log/
    ├── base.hpp # 基础定义和宏
    ├── logger.hpp # 日志记录接口
    ├── console.hpp # 控制台日志，可按需包含
    └── file.hpp # 文件日志，可按需包含
```

## 3. 快速开始

### 3.1 使用默认日志器

```cpp
#include <snapflakes/log/logger.hpp>

// 设置日志级别
sf::log::set_level(sf::log::Level::Trace);

// 输出不同级别的日志
sf::log::trace("This is a trace message: {}", 1);
sf::log::debug("This is a debug message: {}", 2);
sf::log::info("This is an info message: {}", 3);
sf::log::warn("This is a warning message: {}", 4);
sf::log::error("This is an error message: {}", 5);
sf::log::critical("This is a critical message: {}", 6);

// 刷新日志，建议在程序结束调用一次
sf::log::flush();
```

### 3.2 使用日志宏

```cpp
#include <snapflakes/log/logger.hpp>
// 基本日志宏
SNPFLK_TRACE("Macro trace message: {}", 1);
SNPFLK_DEBUG("Macro debug message: {}", 2);
SNPFLK_INFO("Macro info message: {}", 3);
SNPFLK_WARN("Macro warning message: {}", 4);
SNPFLK_ERROR("Macro error message: {}", 5);
SNPFLK_CRITICAL("Macro critical message: {}", 6);

// 刷新日志，建议在程序结束调用一次
sf::log::flush();
```

## 4. Handler

### 4.1 控制台 Handler
通过创建不同的 Handler 来实现不同的日志输出方式。

#### 标准控制台 Handler

```cpp
// 多线程安全的标准输出处理器
auto stdout_handler_mt = sf::log::create_stdout_handler_mt();

// 单线程的标准输出处理器
auto stdout_handler_st = sf::log::create_stdout_handler_st();

// 多线程安全的标准错误处理器
auto stderr_handler_mt = sf::log::create_stderr_handler_mt();

// 单线程的标准错误处理器
auto stderr_handler_st = sf::log::create_stderr_handler_st();
```

#### 彩色控制台 Handler

```cpp
// 多线程安全的彩色标准输出处理器
auto colored_stdout_handler_mt = sf::log::create_colored_stdout_handler_mt();

// 单线程的彩色标准输出处理器
auto colored_stdout_handler_st = sf::log::create_colored_stdout_handler_st();

// 多线程安全的彩色标准错误处理器
auto colored_stderr_handler_mt = sf::log::create_colored_stderr_handler_mt();

// 单线程的彩色标准错误处理器
auto colored_stderr_handler_st = sf::log::create_colored_stderr_handler_st();
```

#### 控制台 Handler 对

```cpp
// 多线程安全的控制台处理器对（标准输出 + 标准错误）
auto console_handlers_mt = sf::log::create_console_handlers_mt();

// 单线程的控制台处理器对（标准输出 + 标准错误）
auto console_handlers_st = sf::log::create_console_handlers_st();

// 多线程安全的彩色控制台处理器对
auto colored_console_handlers_mt = sf::log::create_colored_console_handlers_mt();

// 单线程的彩色控制台处理器对
auto colored_console_handlers_st = sf::log::create_colored_console_handlers_st();
```

### 4.2 文件 Handler

```cpp
// 多线程安全的文件处理器
auto file_handler_mt = sf::log::create_file_handler_mt(log_file_path);

// 单线程的文件处理器
auto file_handler_st = sf::log::create_file_handler_st(log_file_path);

// 多线程安全的文件处理器（指定是否截断文件）
auto file_handler_append = sf::log::create_file_handler_mt(log_file_path, false); // false 表示追加模式
```

## 5. 自定义 Logger

### 5.1 创建自定义 Logger

```cpp
// 创建处理器
auto console_handler = sf::log::create_colored_stdout_handler_mt();
auto file_handler = sf::log::create_file_handler_mt(log_file_path);

// 创建自定义日志器（支持多个处理器）
auto logger = sf::log::create_logger_with_handlers("custom", console_handler, file_handler);

// 设置日志级别
logger.set_level(sf::log::Level::Trace);

// 使用自定义日志器
logger.trace("Custom trace message");
logger.info("Custom info message");
logger.flush();
```

### 5.2 设置默认 Logger

```cpp
// 获取原始默认日志器
auto original_default_logger = sf::log::get_default_logger();

// 设置新的默认日志器
sf::log::set_default_logger(logger);

// 使用默认日志器
sf::log::info("Default logger info message");

// 恢复原始默认日志器
sf::log::set_default_logger(original_default_logger);
```

## 6. 日志格式设置

```cpp
// 设置日志格式模式
sf::log::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");

// 或者为特定日志器设置格式
logger.set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
```

### 6.1 格式占位符说明

| 分类 | 占位符 | 说明 | 示例输出 |
|------|--------|------|----------|
| 时间日期 | `%Y`、`%m`、`%d` | 年、月、日 | 2024、05、21 |
|  | `%H`、`%M`、`%S` | 时（24小时制）、分、秒 | 15、30、45 |
|  | `%e` | 毫秒，带小数点，例如 .123 | .123 |
|  | `%F` | 等同于 `%Y-%m-%d` | 2024-05-21 |
|  | `%T` | 等同于 `%H:%M:%S` | 15:30:45 |
| 日志级别 | `%l` | 日志级别全名 | info、warning、error |
|  | `%L` | 日志级别短名 | I、W、E |
|  | `%^` 与 `%$` | 将包裹在其中的文本染上对应级别的颜色（终端支持） | `[%^%l%$]` 会让级别名显示颜色 |
| 消息内容 | `%v` | 用户输入的日志正文 | This is an info message |
| 代码位置 | `%@` | 源文件名和行号，格式为 文件:行号 | main.cpp:25 |
|  | `%s`、`%#` | 源文件名、行号 | main.cpp、25 |
|  | `%!` | 函数名 | main |
| 其他信息 | `%n` | Logger 的名称 | console |
|  | `%t` | 线程 ID | 0x7f8a0b0b5700 |
|  | `%P` | 进程 ID | 12345 |
|  | `%%` | 百分号字符 | % |

### 6.2 实用组合示例

你可以通过组合上述占位符，实现各种常见的日志格式需求。

#### 基础格式：包含完整时间戳、带颜色的级别和消息正文

```cpp
set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
```

输出示例：
```
[2024-05-21 15:30:45.123] [info] This is an info message
```

#### 详细调试格式：包含文件名、行号和函数名，方便定位问题

```cpp
set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%@] [%!] [%l] %v");
```

输出示例：
```
[2024-05-21 15:30:45.123] [0x7f8a0b0b5700] [main.cpp:25] [main] [info] User logged in
```

#### 简洁格式：只显示简短级别和消息

```cpp
set_pattern("[%L] %v");
```

输出示例：
```
[I] User logged in
```

## 7. 上下文日志

```cpp
// 定义一个包含日志器的上下文类
class TestContext {
public:
    sf::log::Logger& get_logger() const {
        return logger;
    }

private:
    mutable sf::log::Logger logger = sf::log::create_logger_with_handlers(
        "test_context_logger", 
        sf::log::create_colored_stdout_handler_mt()
    );
};

// 使用上下文日志
TestContext ctx;
sf::log::ctx_info(ctx, "Context info message");
sf::log::ctx_warn(ctx, "Context warn message");

// 使用上下文日志宏
SNPFLK_CTX_LOGGER_INFO(ctx, "Context macro info message");
```

## 8. 日志级别
```cpp
#include <snapflakes/log/base.hpp>

namespace sflog = snapflakes::log;

// 日志级别从低到高排序：
sflog::Level::Trace; // 最详细的日志，通常用于调试
sflog::Level::Debug; // 调试信息
sflog::Level::Info; // 一般信息
sflog::Level::Warn; // 警告信息
sflog::Level::Error; // 错误信息
sflog::Level::Critical; // 严重错误信息
```

## 9. 线程安全
### 9.1 Handler

- `_mt` 后缀的`create_xxxx_handler`：适用于多线程环境
- `_st` 后缀的`create_xxx_handler`：适用于单线程环境

### 9.2 Logger

- 均可用于多线程环境

## 10. 完整示例

```cpp
#include <snapflakes/log.hpp>
#include <filesystem>

namespace sf = snapflakes;
namespace stdfs = std::filesystem;

int main() {
    // 设置默认日志器级别
    sf::log::set_level(sf::log::Level::Info);
    
    // 输出基本日志
    sf::log::info("Application started");
    
    // 创建自定义日志器
    auto log_file = stdfs::current_path() / "app.log";
    auto console_handler = sf::log::create_colored_stdout_handler_mt();
    auto file_handler = sf::log::create_file_handler_mt(log_file);
    
    auto custom_logger = sf::log::create_logger_with_handlers("app", console_handler, file_handler);
    custom_logger.set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
    
    // 使用自定义日志器
    custom_logger.info("Custom logger initialized");
    
    // 使用日志宏
    SNPFLK_INFO("Using log macro");
    
    // 模拟错误
    try {
        throw std::runtime_error("Test error");
    } catch (const std::exception& e) {
        custom_logger.error("Exception caught: {}", e.what());
    }
    
    // 刷新日志
    custom_logger.flush();
    sf::log::flush();
    
    return 0;
}
```