# Network Module CMakeLists.txt

# Network library - 网络适配器模块
add_library(AdapterNetwork STATIC
    NetworkFactory.cpp
    NetworkFactory.h
)

# 设置目标属性
target_include_directories(AdapterNetwork PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../interface
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/embedded
)

# 链接依赖库
target_link_libraries(AdapterNetwork 
    PUBLIC 
        WindowsPlatform  # 链接平台特定的实现
        EmbeddedPlatform # 链接嵌入式平台实现
)

# 设置编译选项
target_compile_features(AdapterNetwork PUBLIC cxx_std_17)

# 设置编译器警告
if(MSVC)
    target_compile_options(AdapterNetwork PRIVATE /W4)
else()
    target_compile_options(AdapterNetwork PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 设置目标属性
set_target_properties(AdapterNetwork PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
) 