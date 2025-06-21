# Platform-specific implementations

# Add subdirectories
add_subdirectory(embedded)
add_subdirectory(windows)

# Create a platform library that includes both implementations
# This allows GpioFactory to choose at runtime
add_library(Platform INTERFACE)

# Link both platform implementations so GpioFactory can access both
target_link_libraries(Platform INTERFACE 
    WindowsGpio
    EmbeddedGpio
)

# Set include directories
target_include_directories(Platform INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/embedded
    ${CMAKE_CURRENT_SOURCE_DIR}/windows
) 