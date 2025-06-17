# Source directory CMakeLists.txt

# Logger module
add_subdirectory(logger)

# HAL module
add_subdirectory(HAL)

# Adapter module
add_subdirectory(Adapter)

# Protocol module
add_subdirectory(protocol)

# Main executable
add_executable(main main.cpp)

# Link main with required libraries
target_link_libraries(main 
    PRIVATE
    WhtsProtocol 
    Logger
    HAL
    Adapter
)

# Set include directories for main
target_include_directories(main 
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Windows特定设置
if(WIN32)
    target_link_libraries(main PRIVATE ws2_32)
endif()

# 编译选项
if(MSVC)
    target_compile_options(main PRIVATE /W4)
else()
    target_compile_options(main PRIVATE -Wall -Wextra -pedantic)
endif() 