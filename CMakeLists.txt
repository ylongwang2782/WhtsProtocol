cmake_minimum_required(VERSION 3.10)
project(WhtsProtocol)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 导出编译命令数据库，用于clangd等语言服务器
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加源代码目录
add_subdirectory(src)

# 测试目标 (可选)
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    # 添加测试...
endif()

# 示例目标 (可选)
option(BUILD_EXAMPLES "Build examples" OFF)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
