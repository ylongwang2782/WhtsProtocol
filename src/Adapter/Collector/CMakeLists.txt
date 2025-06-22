# Collector Module CMakeLists.txt

# Collector library - 数据采集器模块
add_library(AdapterCollector STATIC
    ContinuityCollector.cpp
    ContinuityCollector.h
)

# 设置目标属性
target_include_directories(AdapterCollector PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../interface
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/embedded
    ${CMAKE_CURRENT_SOURCE_DIR}/../Gpio
)

# 查找线程库
find_package(Threads REQUIRED)

# 链接依赖库
target_link_libraries(AdapterCollector 
    PUBLIC 
        AdapterGpio      # 链接GPIO适配器模块
        WindowsPlatform  # 链接平台特定的实现
        EmbeddedPlatform # 链接嵌入式平台实现
    PRIVATE
        Threads::Threads
)

# 设置编译选项
target_compile_features(AdapterCollector PUBLIC cxx_std_17)

# 设置编译器警告
if(MSVC)
    target_compile_options(AdapterCollector PRIVATE /W4)
else()
    target_compile_options(AdapterCollector PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 为调试版本添加调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(AdapterCollector PRIVATE DEBUG_MODE=1)
endif()

# 设置目标属性
set_target_properties(AdapterCollector PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
) 