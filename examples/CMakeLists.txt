# Examples CMakeLists.txt

# Create modular usage example
add_executable(modular_usage_example 
    modular_usage_example.cpp
)

target_link_libraries(modular_usage_example 
    PRIVATE
    WhtsProtocol
    Logger
)

target_include_directories(modular_usage_example 
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Create test all messages example
add_executable(test_all_messages 
    test_all_messages.cpp
)

target_link_libraries(test_all_messages 
    PRIVATE
    WhtsProtocol
    Logger
)

target_include_directories(test_all_messages 
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Create continuity collector example
add_executable(continuity_collector_example 
    continuity_collector_example.cpp
)

target_link_libraries(continuity_collector_example 
    PRIVATE
    WhtsProtocol
    Logger
    HAL
    Adapter
)

target_include_directories(continuity_collector_example 
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Create test enhanced collector example
add_executable(test_enhanced_collector 
    test_enhanced_collector.cpp
)

target_link_libraries(test_enhanced_collector 
    PRIVATE
    Adapter
    HAL
    Logger
)

target_include_directories(test_enhanced_collector 
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Create test refactored GPIO example
add_executable(test_refactored_gpio 
    test_refactored_gpio.cpp
)

target_link_libraries(test_refactored_gpio 
    PRIVATE
    HAL
)

target_include_directories(test_refactored_gpio 
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Set target properties
set_target_properties(modular_usage_example test_all_messages continuity_collector_example test_enhanced_collector test_refactored_gpio PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Windows特定设置
if(WIN32)
    target_link_libraries(modular_usage_example PRIVATE ws2_32)
    target_link_libraries(test_all_messages PRIVATE ws2_32)
    target_link_libraries(continuity_collector_example PRIVATE ws2_32)
    target_link_libraries(test_enhanced_collector PRIVATE ws2_32)
    target_link_libraries(test_refactored_gpio PRIVATE ws2_32)
endif()

# 编译选项
if(MSVC)
    target_compile_options(modular_usage_example PRIVATE /W4)
    target_compile_options(test_all_messages PRIVATE /W4)
    target_compile_options(continuity_collector_example PRIVATE /W4)
    target_compile_options(test_enhanced_collector PRIVATE /W4)
    target_compile_options(test_refactored_gpio PRIVATE /W4)
else()
    target_compile_options(modular_usage_example PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(test_all_messages PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(continuity_collector_example PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(test_enhanced_collector PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(test_refactored_gpio PRIVATE -Wall -Wextra -pedantic)
endif()

# Install rules removed as requested 