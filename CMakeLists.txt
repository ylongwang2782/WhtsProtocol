cmake_minimum_required(VERSION 3.10)
project(WhtsProtocol)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置源文件目录
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(PROTOCOL_DIR ${SRC_DIR}/protocol)

# 协议库源文件
set(PROTOCOL_SOURCES
    # 核心模块
    ${PROTOCOL_DIR}/DeviceStatus.cpp
    ${PROTOCOL_DIR}/Frame.cpp
    
    # 工具模块
    ${PROTOCOL_DIR}/utils/ByteUtils.cpp
    
    # 消息模块
    ${PROTOCOL_DIR}/messages/Master2Slave.cpp
    ${PROTOCOL_DIR}/messages/Slave2Master.cpp
    ${PROTOCOL_DIR}/messages/Backend2Master.cpp
    ${PROTOCOL_DIR}/messages/Master2Backend.cpp
    ${PROTOCOL_DIR}/messages/Slave2Backend.cpp
    
    # 协议处理器
    ${PROTOCOL_DIR}/ProtocolProcessor.cpp
    
    # 兼容性：保留原始文件
    ${SRC_DIR}/WhtsProtocol.cpp
)

# 协议库头文件
set(PROTOCOL_HEADERS
    ${PROTOCOL_DIR}/Common.h
    ${PROTOCOL_DIR}/DeviceStatus.h
    ${PROTOCOL_DIR}/Frame.h
    ${PROTOCOL_DIR}/utils/ByteUtils.h
    ${PROTOCOL_DIR}/messages/Message.h
    ${PROTOCOL_DIR}/messages/Master2Slave.h
    ${PROTOCOL_DIR}/messages/Slave2Master.h
    ${PROTOCOL_DIR}/messages/Slave2Backend.h
    ${PROTOCOL_DIR}/messages/Backend2Master.h
    ${PROTOCOL_DIR}/messages/Master2Backend.h
    ${PROTOCOL_DIR}/ProtocolProcessor.h
    ${PROTOCOL_DIR}/WhtsProtocol.h
)

# 创建协议库
add_library(WhtsProtocol STATIC ${PROTOCOL_SOURCES})

# 设置包含目录
target_include_directories(WhtsProtocol PUBLIC 
    ${SRC_DIR}
    ${PROTOCOL_DIR}
)

# Logger库
add_library(Logger STATIC 
    ${SRC_DIR}/Logger.cpp
)

target_include_directories(Logger PUBLIC ${SRC_DIR})

# 主程序
add_executable(main 
    ${SRC_DIR}/main.cpp
)

target_link_libraries(main 
    WhtsProtocol 
    Logger
)

# Windows特定设置
if(WIN32)
    target_link_libraries(main ws2_32)
endif()

# 编译选项
if(MSVC)
    target_compile_options(WhtsProtocol PRIVATE /W4)
    target_compile_options(main PRIVATE /W4)
else()
    target_compile_options(WhtsProtocol PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(main PRIVATE -Wall -Wextra -pedantic)
endif()

# 安装规则
install(TARGETS WhtsProtocol Logger main
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${PROTOCOL_HEADERS}
    DESTINATION include/WhtsProtocol
)

# 测试目标 (可选)
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    # 添加测试...
endif()
