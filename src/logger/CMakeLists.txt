# Logger Module CMakeLists.txt

# Create Logger library
add_library(Logger STATIC 
    Logger.cpp
    Logger.h
)

# Set include directories
target_include_directories(Logger 
    PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
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