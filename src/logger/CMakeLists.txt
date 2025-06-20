# Logger Module CMakeLists.txt

# Create main Logger library that provides the unified interface
add_library(Logger STATIC 
    Logger.cpp
    Logger.h
    LoggerFactory.cpp
    LoggerFactory.h
)

# Set include directories
target_include_directories(Logger 
    PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with the adapters and spdlog
target_link_libraries(Logger 
    PUBLIC 
    LoggerInterface
    Adapter  # Contains SpdlogAdapter
    HALEmbeddedLogger  # From HAL
    spdlog
)

# Set target properties
set_target_properties(Logger PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 编译选项
if(MSVC)
    target_compile_options(Logger PRIVATE /W4)
else()
    target_compile_options(Logger PRIVATE -Wall -Wextra -pedantic)
endif()

# Install rules removed as requested 