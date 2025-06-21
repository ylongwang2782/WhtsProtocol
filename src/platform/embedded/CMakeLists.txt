# Embedded Platform GPIO Implementation

add_library(EmbeddedGpio STATIC
    HardwareGpio.cpp
    HardwareGpio.h
)

# 设置包含目录
target_include_directories(EmbeddedGpio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../interface  # IGpio.h
)

# 设置编译选项
target_compile_features(EmbeddedGpio PUBLIC cxx_std_17)

# 设置编译器警告
if(MSVC)
    target_compile_options(EmbeddedGpio PRIVATE /W4)
else()
    target_compile_options(EmbeddedGpio PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 设置目标属性
set_target_properties(EmbeddedGpio PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
) 