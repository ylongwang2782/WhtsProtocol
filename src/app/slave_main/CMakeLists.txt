cmake_minimum_required(VERSION 3.10)

# Set the target name
set(SLAVE_TARGET "Slave")

# Collect all source files
set(SLAVE_SOURCES
    main.cpp
    SlaveDevice.cpp
    MessageProcessor.cpp
    NetworkManager.cpp
    Gpio.cpp
)

# Collect all header files
set(SLAVE_HEADERS
    SlaveDeviceState.h
    SlaveDevice.h
    MessageProcessor.h
    NetworkManager.h
    Gpio.h
)

# Create the executable
add_executable(${SLAVE_TARGET} ${SLAVE_SOURCES} ${SLAVE_HEADERS})

# Set include directories
target_include_directories(${SLAVE_TARGET} 
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../interface
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/embedded
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../../protocol/messages
    ${CMAKE_CURRENT_SOURCE_DIR}/../../protocol/utils
)

# Link libraries
target_link_libraries(${SLAVE_TARGET} 
    WhtsProtocol
    AppLogger
    HAL
    Adapter
    Interface
    Platform
)

# Set C++ standard
set_target_properties(${SLAVE_TARGET} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Platform-specific settings
if(WIN32)
    target_link_libraries(${SLAVE_TARGET} ws2_32)
endif()

# Set output directory (optional)
set_target_properties(${SLAVE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
) 