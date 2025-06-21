# HAL (Hardware Abstraction Layer) CMakeLists.txt

# 添加Network子目录
add_subdirectory(Network)

# HAL library - 硬件抽象层库
add_library(HAL INTERFACE)

# 设置包含目录 - 让其他模块可以使用 #include "Gpio.h"
target_include_directories(HAL INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Network
) 

# 链接NetworkHAL库
target_link_libraries(HAL INTERFACE NetworkHAL) 