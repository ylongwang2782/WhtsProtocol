# Adapter Layer CMakeLists.txt - 模块化重构版本

# 添加各个子模块
add_subdirectory(Logger)
add_subdirectory(Gpio)
add_subdirectory(Network)
add_subdirectory(Collector)

# 创建统一的Adapter库，汇总所有子模块
add_library(Adapter INTERFACE)

# 链接所有子模块
target_link_libraries(Adapter 
    INTERFACE
        AdapterLogger
        AdapterGpio
        AdapterNetwork
        AdapterCollector
)

# 设置包含目录
target_include_directories(Adapter INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Logger
    ${CMAKE_CURRENT_SOURCE_DIR}/Gpio
    ${CMAKE_CURRENT_SOURCE_DIR}/Network
    ${CMAKE_CURRENT_SOURCE_DIR}/Collector
)

# 设置编译特性
target_compile_features(Adapter INTERFACE cxx_std_17)