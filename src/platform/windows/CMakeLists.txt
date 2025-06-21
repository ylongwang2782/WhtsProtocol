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