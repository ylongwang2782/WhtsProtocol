# Gpio Module CMakeLists.txt

# Gpio library - GPIO适配器模块
add_library(AdapterGpio STATIC
    GpioFactory.cpp
    GpioFactory.h
)

# 设置目标属性
target_include_directories(AdapterGpio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../interface
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/embedded
)

# 链接依赖库
target_link_libraries(AdapterGpio 
    PUBLIC 
        WindowsPlatform  # 链接平台特定的实现
        EmbeddedPlatform # 链接嵌入式平台实现
)

# 设置编译选项
target_compile_features(AdapterGpio PUBLIC cxx_std_17)

# 设置编译器警告
if(MSVC)
    target_compile_options(AdapterGpio PRIVATE /W4)
else()
    target_compile_options(AdapterGpio PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 根据配置添加编译定义
if(GPIO_USE_HARDWARE)
    target_compile_definitions(AdapterGpio PUBLIC GPIO_USE_HARDWARE=1)
endif()

if(GPIO_USE_VIRTUAL)
    target_compile_definitions(AdapterGpio PUBLIC GPIO_USE_VIRTUAL=1)
endif()

# 设置目标属性
set_target_properties(AdapterGpio PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
) 