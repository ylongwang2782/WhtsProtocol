# HAL (Hardware Abstraction Layer) CMakeLists.txt

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

# HAL library - 硬件抽象层库
add_library(HAL STATIC
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

# 设置目标属性
target_include_directories(HAL PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接线程库（用于虚拟GPIO的模拟功能）
find_package(Threads REQUIRED)
target_link_libraries(HAL 
    PUBLIC 
        Threads::Threads
)

# 设置编译选项
target_compile_features(HAL PUBLIC cxx_std_17)

# 根据配置添加编译定义
if(GPIO_USE_HARDWARE)
    target_compile_definitions(HAL PUBLIC GPIO_USE_HARDWARE=1)
endif()

if(GPIO_USE_VIRTUAL)
    target_compile_definitions(HAL PUBLIC GPIO_USE_VIRTUAL=1)
endif()

# 设置编译器警告
if(MSVC)
    target_compile_options(HAL PRIVATE /W4)
else()
    target_compile_options(HAL PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 为调试版本添加调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(HAL PRIVATE DEBUG_MODE=1)
endif() 