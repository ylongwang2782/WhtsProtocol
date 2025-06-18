# HAL (Hardware Abstraction Layer) CMakeLists.txt

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