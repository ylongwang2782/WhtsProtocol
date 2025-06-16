cmake_minimum_required(VERSION 3.16)
project(WhtsProtocol LANGUAGES CXX)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(Slave src/main.cpp src/WhtsProtocol.cpp src/Logger.cpp)

# Windows 平台需要链接 ws2_32 库
if(WIN32)
    target_link_libraries(Slave ws2_32)
endif()
