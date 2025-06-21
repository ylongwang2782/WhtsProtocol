# Embedded Platform Implementation

add_library(EmbeddedPlatform STATIC
    HardwareGpio.cpp
    HardwareGpio.h
    EmbeddedLogger.cpp
    EmbeddedLogger.h
    LwipUdpSocket.cpp
    LwipUdpSocket.h
)

# 设置包含目录
target_include_directories(EmbeddedPlatform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../interface  # IGpio.h and IUdpSocket.h
)

# 设置编译选项
target_compile_features(EmbeddedPlatform PUBLIC cxx_std_17)

# 设置编译器警告
if(MSVC)
    target_compile_options(EmbeddedPlatform PRIVATE /W4)
else()
    target_compile_options(EmbeddedPlatform PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 根据配置添加编译定义
if(USE_LWIP)
    target_compile_definitions(EmbeddedPlatform PUBLIC USE_LWIP=1)
endif()

# 设置目标属性
set_target_properties(EmbeddedPlatform PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 为了兼容，创建EmbeddedGpio别名
add_library(EmbeddedGpio ALIAS EmbeddedPlatform) 