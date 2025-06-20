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