# HAL (Hardware Abstraction Layer) CMakeLists.txt

# HAL library - 硬件抽象层库
add_library(HAL INTERFACE)

# 设置包含目录
target_include_directories(HAL INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
) 