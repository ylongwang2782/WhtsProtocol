# Examples CMakeLists.txt

# Create test GPIO module structure example
add_executable(test_gpio_module_structure 
    test_gpio_module_structure.cpp
)

target_link_libraries(test_gpio_module_structure 
    PRIVATE
    HAL
)

target_include_directories(test_gpio_module_structure 
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Set target properties
set_target_properties(test_gpio_module_structure PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Windows特定设置
if(WIN32)
    target_link_libraries(test_gpio_module_structure PRIVATE ws2_32)
endif()

# 编译选项
if(MSVC)
    target_compile_options(test_gpio_module_structure PRIVATE /W4)
else()
    target_compile_options(test_gpio_module_structure PRIVATE -Wall -Wextra -pedantic)
endif() 