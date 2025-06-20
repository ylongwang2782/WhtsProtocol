# Network HAL Module
cmake_minimum_required(VERSION 3.16)

# 定义网络模块库
set(NETWORK_SOURCES
    IUdpSocket.h
    NetworkManager.h
    NetworkManager.cpp
    NetworkFactory.h
    NetworkFactory.cpp
    WindowsUdpSocket.h
    WindowsUdpSocket.cpp
    LwipUdpSocket.h
    LwipUdpSocket.cpp
)

# 创建网络HAL库
add_library(NetworkHAL ${NETWORK_SOURCES})

# 设置包含目录
target_include_directories(NetworkHAL
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

# 平台特定的链接库
if(WIN32)
    target_link_libraries(NetworkHAL PRIVATE ws2_32)
endif()

# lwip支持（可选）
option(USE_LWIP "Enable lwip support for embedded platforms" OFF)
if(USE_LWIP)
    target_compile_definitions(NetworkHAL PUBLIC USE_LWIP)
    # 这里需要根据实际的lwip库配置进行调整
    # find_package(lwip REQUIRED)
    # target_link_libraries(NetworkHAL PRIVATE lwip)
    message(STATUS "Network HAL: lwip support enabled")
else()
    message(STATUS "Network HAL: Using native socket implementation")
endif()

# 编译选项
target_compile_features(NetworkHAL PUBLIC cxx_std_14)

# 设置目标属性
set_target_properties(NetworkHAL PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 安装规则（可选）
install(TARGETS NetworkHAL
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    IUdpSocket.h
    NetworkManager.h
    NetworkFactory.h
    DESTINATION include/HAL/Network
) 