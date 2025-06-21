# Interface Layer CMakeLists.txt

# Create Interface library for logger
add_library(LoggerInterface INTERFACE)

# Set include directories for interface
target_include_directories(LoggerInterface 
    INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Interface layer only contains headers
target_sources(LoggerInterface 
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/ILogger.h
)

# Interface Library CMakeLists.txt

add_library(Interface INTERFACE)

# 设置包含目录
target_include_directories(Interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 设置编译选项
target_compile_features(Interface INTERFACE cxx_std_17)

# 设置目标属性
set_target_properties(Interface PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
) 