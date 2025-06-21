# Adapter Layer CMakeLists.txt

# Adapter library - 适配器层库
add_library(Adapter STATIC
    ContinuityCollector.cpp
    ContinuityCollector.h
    LoggerFactory.cpp
    LoggerFactory.h
    GpioFactory.cpp      # 新添加的GPIO工厂实现
    GpioFactory.h        # 新添加的GPIO工厂头文件
    NetworkFactory.cpp   # 网络工厂实现
    NetworkFactory.h     # 网络工厂头文件
)

# 设置目标属性
target_include_directories(Adapter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../interface
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/embedded
)

# 链接依赖库
find_package(Threads REQUIRED)

# 根据平台链接不同的实现
if(WIN32)
    target_sources(Adapter PRIVATE
        ../platform/windows/SpdlogAdapter.cpp
        ../platform/windows/SpdlogAdapter.h
    )
    target_link_libraries(Adapter PRIVATE spdlog)
elseif(EMBEDDED_PLATFORM)
    target_sources(Adapter PRIVATE
        ../platform/embedded/EmbeddedLogger.cpp
        ../platform/embedded/EmbeddedLogger.h
    )
else()
    # 默认使用spdlog
    target_sources(Adapter PRIVATE
        ../platform/windows/SpdlogAdapter.cpp
        ../platform/windows/SpdlogAdapter.h
    )
    target_link_libraries(Adapter PRIVATE spdlog)
endif()

target_link_libraries(Adapter 
    PUBLIC 
        WindowsPlatform  # 链接平台特定的实现
        EmbeddedPlatform # 链接嵌入式平台实现
    PRIVATE
        Threads::Threads
)

# 设置编译选项
target_compile_features(Adapter PUBLIC cxx_std_17)

# 设置编译器警告
if(MSVC)
    target_compile_options(Adapter PRIVATE /W4)
else()
    target_compile_options(Adapter PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 为调试版本添加调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(Adapter PRIVATE DEBUG_MODE=1)
endif()

# 根据配置添加编译定义
if(GPIO_USE_HARDWARE)
    target_compile_definitions(Adapter PUBLIC GPIO_USE_HARDWARE=1)
endif()

if(GPIO_USE_VIRTUAL)
    target_compile_definitions(Adapter PUBLIC GPIO_USE_VIRTUAL=1)
endif()

# 设置目标属性
set_target_properties(Adapter PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
) 