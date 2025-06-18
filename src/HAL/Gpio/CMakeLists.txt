# GPIO模块 CMakeLists.txt

# GPIO平台配置选项
option(GPIO_USE_HARDWARE "Use hardware GPIO implementation" OFF)
option(GPIO_USE_VIRTUAL "Use virtual GPIO implementation" OFF)

# 如果没有明确指定，默认使用虚拟GPIO
if(NOT GPIO_USE_HARDWARE AND NOT GPIO_USE_VIRTUAL)
    set(GPIO_USE_VIRTUAL ON)
    message(STATUS "GPIO: Using Virtual GPIO (default)")
elseif(GPIO_USE_HARDWARE)
    message(STATUS "GPIO: Using Hardware GPIO")
elseif(GPIO_USE_VIRTUAL)
    message(STATUS "GPIO: Using Virtual GPIO")
endif()

# GPIO库 - 包含所有GPIO相关实现
add_library(Gpio STATIC
    # GPIO模块文件
    Gpio.cpp              # 统一包含文件（向后兼容）
    Gpio.h                # 统一头文件
    IGpio.h               # GPIO接口定义
    VirtualGpio.cpp       # 虚拟GPIO实现
    VirtualGpio.h         # 虚拟GPIO头文件
    HardwareGpio.cpp      # 硬件GPIO实现模板
    HardwareGpio.h        # 硬件GPIO头文件
    GpioFactory.cpp       # GPIO工厂实现
    GpioFactory.h         # GPIO工厂头文件
)

# 设置包含目录 - 让其他模块可以使用 #include "IGpio.h" 等
target_include_directories(Gpio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 根据配置添加编译定义
if(GPIO_USE_HARDWARE)
    target_compile_definitions(Gpio PUBLIC GPIO_USE_HARDWARE=1)
endif()

if(GPIO_USE_VIRTUAL)
    target_compile_definitions(Gpio PUBLIC GPIO_USE_VIRTUAL=1)
endif()

# 设置编译选项
target_compile_features(Gpio PUBLIC cxx_std_17)

# 设置编译器警告
if(MSVC)
    target_compile_options(Gpio PRIVATE /W4)
else()
    target_compile_options(Gpio PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 链接线程库（用于虚拟GPIO的模拟功能）
find_package(Threads REQUIRED)
target_link_libraries(Gpio 
    PUBLIC 
        Threads::Threads
)

# 为调试版本添加调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(Gpio PRIVATE DEBUG_MODE=1)
endif()

# 设置目标属性
set_target_properties(Gpio PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
) 