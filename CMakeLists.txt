cmake_minimum_required(VERSION 3.16)
project(WhtsProtocol VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 生成compile_commands.json用于clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 编译选项
if(MSVC)
    add_compile_options(/W4 /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 创建静态库
add_library(WhtsProtocol STATIC
    WhtsProtocol.h
    WhtsProtocol.cpp
)

# 设置库的包含目录
target_include_directories(WhtsProtocol PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# 创建UDP测试程序
add_executable(udp_whts_protocol
    udp_example.cpp
)

# 链接库
target_link_libraries(udp_whts_protocol PRIVATE WhtsProtocol)

# Windows下链接网络库
if(WIN32)
    target_link_libraries(udp_whts_protocol PRIVATE ws2_32 wsock32)
endif()

# 设置目标属性
set_target_properties(udp_whts_protocol PROPERTIES
    OUTPUT_NAME "udp_whts_protocol"
    DEBUG_POSTFIX "_d"
)

# 安装配置（可选）
install(TARGETS WhtsProtocol udp_whts_protocol
    EXPORT WhtsProtocolTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES WhtsProtocol.h
    DESTINATION include
)

# 输出构建信息
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_GENERATOR: ${CMAKE_GENERATOR}")
