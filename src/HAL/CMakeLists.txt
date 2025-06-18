# HAL (Hardware Abstraction Layer) CMakeLists.txt

# 添加GPIO子模块
add_subdirectory(Gpio)

# HAL library - 硬件抽象层库（现在主要作为GPIO的别名）
add_library(HAL INTERFACE)

# 链接GPIO模块
target_link_libraries(HAL INTERFACE Gpio)

# 设置包含目录 - 让其他模块可以使用 #include "Gpio.h"
target_include_directories(HAL INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Gpio
) 