cmake_minimum_required(VERSION 3.10)

# Create master server library
add_library(MasterCore
    DeviceManager.cpp
    MessageHandlers.cpp
    MasterServer.cpp
)

# Set include directories for the library
target_include_directories(MasterCore 
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../HAL/Gpio
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../../logger
    ${CMAKE_CURRENT_SOURCE_DIR}/../../protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../../protocol/messages
    ${CMAKE_CURRENT_SOURCE_DIR}/../../protocol/utils
)

# Link libraries
target_link_libraries(MasterCore 
    WhtsProtocol
    Logger
    HAL
    Adapter
)

# Set C++ standard
set_target_properties(MasterCore PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Platform-specific settings
if(WIN32)
    target_link_libraries(MasterCore ws2_32)
endif()

# Create Master executable
add_executable(Master main.cpp)

# Link with MasterCore library
target_link_libraries(Master MasterCore)

# Set include directories for the executable
target_include_directories(Master
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set C++ standard for the executable
set_target_properties(Master PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
) 