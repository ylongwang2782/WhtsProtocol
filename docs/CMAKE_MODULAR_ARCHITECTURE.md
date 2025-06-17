# CMake 模块化架构

## 概述

本项目已成功重构为模块化的 CMakeLists.txt 架构，每个模块都拥有自己独立的 CMakeLists.txt 文件，使用 `add_subdirectory()` 进行管理。这种架构提供了更好的模块化、可维护性和可重用性。

## 目录结构

```
WhtsProtocol/
├── CMakeLists.txt                          # 根级CMakeLists.txt
├── src/
│   ├── CMakeLists.txt                      # 源代码目录管理
│   ├── main.cpp                            # 应用程序入口点
│   ├── logger/                             # Logger模块
│   │   ├── CMakeLists.txt                  # Logger模块配置
│   │   ├── Logger.cpp
│   │   └── Logger.h
│   └── protocol/                           # 协议模块
│       ├── CMakeLists.txt                  # 协议模块配置
│       ├── WhtsProtocol.h                  # 主协议头文件（已移至protocol目录）
│       ├── ProtocolProcessor.cpp
│       ├── ProtocolProcessor.h
│       ├── DeviceStatus.cpp/h
│       ├── Frame.cpp/h
│       ├── Common.h
│       ├── utils/                          # 工具子模块
│       │   ├── CMakeLists.txt              # 工具模块配置
│       │   ├── ByteUtils.cpp
│       │   └── ByteUtils.h
│       └── messages/                       # 消息子模块
│           ├── CMakeLists.txt              # 消息模块配置
│           ├── Message.h
│           ├── Master2Slave.cpp/h
│           ├── Slave2Master.cpp/h
│           ├── Backend2Master.cpp/h
│           ├── Master2Backend.cpp/h
│           └── Slave2Backend.cpp/h
└── examples/                               # 示例代码（可选）
    ├── CMakeLists.txt                      # 示例代码配置
    ├── modular_usage_example.cpp
    └── test_all_messages.cpp
```

## 模块库结构

### 1. Logger 模块
- **库名**: `Logger`
- **类型**: STATIC 库
- **文件**: `src/logger/`
- **功能**: 日志记录功能

### 2. Protocol Utils 模块
- **库名**: `ProtocolUtils`
- **类型**: STATIC 库
- **文件**: `src/protocol/utils/`
- **功能**: 字节操作工具

### 3. Protocol Messages 模块
- **库名**: `ProtocolMessages`
- **类型**: STATIC 库
- **文件**: `src/protocol/messages/`
- **功能**: 协议消息定义和实现

### 4. Protocol Core 模块
- **库名**: `ProtocolCore`
- **类型**: STATIC 库
- **文件**: `src/protocol/`
- **功能**: 核心协议处理逻辑
- **依赖**: ProtocolUtils, ProtocolMessages, Logger

### 5. WhtsProtocol 主库
- **库名**: `WhtsProtocol`
- **类型**: INTERFACE 库
- **功能**: 统一的协议库接口
- **依赖**: ProtocolCore, ProtocolMessages, ProtocolUtils

## 构建选项

### 基本构建
```bash
cmake ..
cmake --build .
```

### 带示例的构建
```bash
cmake -DBUILD_EXAMPLES=ON ..
cmake --build .
```

### 带测试的构建
```bash
cmake -DBUILD_TESTS=ON ..
cmake --build .
```

## CMakeLists.txt 文件详解

### 根级 CMakeLists.txt
- 设置项目基本配置（C++17标准）
- 使用 `add_subdirectory(src)` 添加源代码目录
- 提供可选的 BUILD_EXAMPLES 和 BUILD_TESTS 选项

### src/CMakeLists.txt
- 管理源代码下的子模块
- 创建主可执行文件 `main`
- 配置主程序与库的链接关系

### 模块级 CMakeLists.txt
每个模块的 CMakeLists.txt 都包含：
- 库的创建和配置
- 依赖关系管理
- 包含目录设置
- 编译选项配置
- 安装规则

## 优势

1. **模块化设计**: 每个模块独立管理自己的构建配置
2. **清晰的依赖关系**: 明确定义模块间的依赖关系
3. **可重用性**: 模块可以独立编译和使用
4. **可维护性**: 模块化结构便于维护和扩展
5. **并行构建**: CMake 可以并行构建独立的模块
6. **灵活的配置**: 支持可选的示例和测试构建

## 依赖关系图

```
main.exe
├── WhtsProtocol (INTERFACE)
│   ├── ProtocolCore
│   │   ├── ProtocolUtils
│   │   ├── ProtocolMessages
│   │   └── Logger
│   ├── ProtocolMessages
│   └── ProtocolUtils
└── Logger
```

## 最新变更

### 2024年12月更新

1. **头文件重组**: `WhtsProtocol.h` 已移动到 `src/protocol/` 目录下
2. **移除安装规则**: 根据需求，所有 CMakeLists.txt 文件中的安装指令已被移除  
3. **修复重复定义**: 解决了 ProtocolProcessor 在两个头文件中重复定义的问题
   - `src/protocol/ProtocolProcessor.h` - 完整的类定义
   - `src/protocol/WhtsProtocol.h` - 简洁的包含头文件（仅包含 #include 指令）
4. **包含路径更新**: main.cpp 中的包含路径已更新为：
   - `#include "logger/Logger.h"`
   - `#include "protocol/WhtsProtocol.h"`

## 迁移说明

从单一 CMakeLists.txt 迁移到模块化架构的主要变化：

1. **文件重组**: 
   - Logger 文件移动到 `src/logger/` 目录
   - WhtsProtocol.h 移动到 `src/protocol/` 目录
2. **库分离**: 将原来的单一库分解为多个专门的库
3. **依赖管理**: 使用 `target_link_libraries()` 管理依赖关系
4. **包含路径**: 使用 `target_include_directories()` 管理包含路径
5. **模块化配置**: 每个模块独立配置编译选项
6. **简化构建**: 移除了所有安装规则，专注于开发构建

这种模块化架构为项目的长期维护和扩展提供了坚实的基础。 