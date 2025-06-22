# Logger Module CMakeLists.txt

# Logger library - 日志适配器模块
add_library(AdapterLogger STATIC
    LoggerFactory.cpp
    LoggerFactory.h
)

# 设置目标属性
target_include_directories(AdapterLogger PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../interface
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/embedded
)

# 根据平台链接不同的实现
if(WIN32)
    target_sources(AdapterLogger PRIVATE
        ../../platform/windows/SpdlogAdapter.cpp
        ../../platform/windows/SpdlogAdapter.h
    )
    target_link_libraries(AdapterLogger PRIVATE spdlog)
elseif(EMBEDDED_PLATFORM)
    target_sources(AdapterLogger PRIVATE
        ../../platform/embedded/EmbeddedLogger.cpp
        ../../platform/embedded/EmbeddedLogger.h
    )
else()
    # 默认使用spdlog
    target_sources(AdapterLogger PRIVATE
        ../../platform/windows/SpdlogAdapter.cpp
        ../../platform/windows/SpdlogAdapter.h
    )
    target_link_libraries(AdapterLogger PRIVATE spdlog)
endif()

# 设置编译选项
target_compile_features(AdapterLogger PUBLIC cxx_std_17)

# 设置编译器警告
if(MSVC)
    target_compile_options(AdapterLogger PRIVATE /W4)
else()
    target_compile_options(AdapterLogger PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 设置目标属性
set_target_properties(AdapterLogger PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
) 