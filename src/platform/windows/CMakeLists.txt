# Windows Platform GPIO Implementation

add_library(WindowsGpio STATIC
    VirtualGpio.cpp
    VirtualGpio.h
)

# 设置包含目录
target_include_directories(WindowsGpio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../interface  # IGpio.h
)

# 设置编译选项
target_compile_features(WindowsGpio PUBLIC cxx_std_17)

# 设置编译器警告
if(MSVC)
    target_compile_options(WindowsGpio PRIVATE /W4)
else()
    target_compile_options(WindowsGpio PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 链接线程库（用于虚拟GPIO的模拟功能）
find_package(Threads REQUIRED)
target_link_libraries(WindowsGpio 
    PUBLIC 
        Threads::Threads
)

# 设置目标属性
set_target_properties(WindowsGpio PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Windows Platform Implementation

add_library(WindowsPlatform STATIC
    VirtualGpio.cpp
    VirtualGpio.h
    SpdlogAdapter.cpp
    SpdlogAdapter.h
    WindowsUdpSocket.cpp
    WindowsUdpSocket.h
    AsioUdpSocket.cpp
    AsioUdpSocket.h
)

# 设置包含目录
target_include_directories(WindowsPlatform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../interface  # IGpio.h and IUdpSocket.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party/asio/asio/include  # ASIO headers
)

# 设置编译选项
target_compile_features(WindowsPlatform PUBLIC cxx_std_17)

# 设置编译器警告
if(MSVC)
    target_compile_options(WindowsPlatform PRIVATE /W4)
else()
    target_compile_options(WindowsPlatform PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 链接库
find_package(Threads REQUIRED)
target_link_libraries(WindowsPlatform 
    PUBLIC 
        Threads::Threads
        spdlog
)

# Windows特定的网络库
if(WIN32)
    target_link_libraries(WindowsPlatform PUBLIC ws2_32)
endif()

# 根据配置添加编译定义
if(USE_ASIO)
    target_compile_definitions(WindowsPlatform PUBLIC USE_ASIO=1)
endif()

# 设置目标属性
set_target_properties(WindowsPlatform PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 为了兼容，创建WindowsGpio别名
add_library(WindowsGpio ALIAS WindowsPlatform) 