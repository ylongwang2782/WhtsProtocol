# HAL (Hardware Abstraction Layer) module
cmake_minimum_required(VERSION 3.10)

# HAL module is now mostly migrated to platform-specific implementations
# This file is kept for compatibility and future HAL components

# Create empty HAL library for compatibility
add_library(HAL INTERFACE)

# Set include directories for HAL (if needed in the future)
target_include_directories(HAL INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Note: Network components have been moved to:
# - Interface: src/interface/IUdpSocket.h
# - Adapter: src/Adapter/NetworkFactory.h/.cpp
# - Platform implementations: src/platform/windows/ and src/platform/embedded/
# - App-level NetworkManager: src/app/NetworkManager.h/.cpp 