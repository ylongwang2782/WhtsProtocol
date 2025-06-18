# Adapter Layer CMakeLists.txt

# Adapter library - 适配器层库
add_library(Adapter STATIC
    ContinuityCollector.cpp
    ContinuityCollector.h
)

# 设置目标属性
target_include_directories(Adapter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接依赖库
find_package(Threads REQUIRED)
target_link_libraries(Adapter 
    PUBLIC 
        Gpio  # 直接链接Gpio库而不是HAL接口库
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