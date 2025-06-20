# 日志系统架构说明

## 概述

本项目实现了一个可跨平台的抽象日志系统，支持在不同平台间轻松切换日志实现。

## 架构设计

```
应用层代码
    ↓
Logger/Log 类 (统一接口)
    ↓
interface/ (接口层)
├── ILogger.h (抽象接口)
├── LoggerFactory.h/cpp (工厂模式)
    ↓
adapter/ (适配器层)          hal/ (硬件抽象层)
├── SpdlogAdapter.h/cpp  ←→  ├── EmbeddedLogger.h/cpp
└── (其他平台适配器)          └── (其他嵌入式实现)
```

## 目录结构

```
src/logger/
├── Logger.h/cpp           # 应用层统一接口
├── README.md             # 文档说明
├── interface/            # 接口层
│   ├── ILogger.h         # 抽象日志接口
│   ├── LoggerFactory.h   # 工厂类头文件
│   └── LoggerFactory.cpp # 工厂类实现
├── adapter/              # 适配器层
│   ├── SpdlogAdapter.h   # spdlog适配器头文件
│   └── SpdlogAdapter.cpp # spdlog适配器实现
└── hal/                  # 硬件抽象层
    ├── EmbeddedLogger.h  # 嵌入式日志头文件
    └── EmbeddedLogger.cpp# 嵌入式日志实现
```

## 核心组件

### 接口层 (interface/)
- **ILogger.h**: 抽象日志接口，定义了所有日志功能的纯虚函数
- **LoggerFactory**: 工厂类，根据编译时的平台宏定义自动选择合适的日志实现
  - Windows/Linux/Mac: 使用 SpdlogAdapter
  - 嵌入式平台: 使用 EmbeddedLogger (需要定义 `EMBEDDED_PLATFORM` 宏)

### 适配器层 (adapter/)
- **SpdlogAdapter**: Windows/Linux平台实现，基于成熟的spdlog库
  - 彩色控制台输出
  - 文件日志支持
  - 高性能异步日志
  - 丰富的格式化选项

### 硬件抽象层 (hal/)
- **EmbeddedLogger**: 嵌入式平台实现示例
  - 简化的串口输出模拟
  - Flash存储接口示例
  - 环形缓冲区管理示例
  - 可根据具体硬件定制

### 应用接口层
- **Logger/Log 类**: 提供与原有代码兼容的API，支持单例模式和静态方法调用

## 使用方法

### 基本用法（保持原有API兼容）

```cpp
#include "Logger.h"

// 字符串消息
Log::i("TAG", "This is an info message");
Log::w("TAG", "This is a warning message");
Log::e("TAG", "This is an error message");

// 格式化消息
Log::i("TAG", "User %s logged in with ID %d", username.c_str(), userId);
Log::e("TAG", "Error code: %d, message: %s", errorCode, errorMsg.c_str());
```

### 高级配置

```cpp
// 设置日志级别
Log::setLogLevel(LogLevel::INFO);

// 启用文件日志
Log::enableFileLogging("application.log");

// 禁用文件日志
Log::disableFileLogging();
```

### 实例方法使用

```cpp
Logger& logger = Logger::getInstance();
logger.i("TAG", "Instance method call");
logger.e("TAG", "Error: %s", errorMessage.c_str());
```

## 平台切换

### Windows平台（当前默认）
使用spdlog实现，提供丰富的功能和高性能。

### 嵌入式平台
1. 在CMakeLists.txt中定义宏：
   ```cmake
   target_compile_definitions(your_target PRIVATE EMBEDDED_PLATFORM)
   ```

2. 根据具体硬件修改 `EmbeddedLogger.cpp`：
   - 替换 `printf` 为串口输出函数
   - 实现Flash存储逻辑
   - 添加环形缓冲区管理
   - 优化内存使用

## 日志级别

- `VERBOSE`: 详细调试信息
- `DEBUG`: 调试信息
- `INFO`: 一般信息
- `WARN`: 警告信息
- `ERR`: 错误信息

## 输出格式

### Windows平台 (spdlog)
```
[2025-06-20 15:02:17.651] [info] [Main] WhtsProtocol Master Server
[2025-06-20 15:02:17.652] [info] [Main] ==========================
```

### 嵌入式平台 (示例)
```
[000001][I][Main] WhtsProtocol Master Server
[000002][I][Main] ==========================
```

## 扩展新平台

要为新平台添加日志支持：

1. 创建新的实现类继承 `ILogger`
2. 在 `LoggerFactory.cpp` 中添加平台检测逻辑
3. 实现所有虚函数
4. 根据平台特性优化实现

## 性能考虑

- 日志级别过滤在最早阶段进行，避免不必要的字符串格式化
- spdlog提供异步日志支持，适合高频日志场景
- 嵌入式实现可以使用环形缓冲区和批量输出来优化性能

## 注意事项

1. 格式化字符串使用C风格的printf格式（`%s`, `%d`等）
2. 确保在嵌入式平台上字符串参数的生命周期
3. 大量日志输出时考虑使用异步模式
4. 文件日志在嵌入式平台上需要考虑存储空间限制 