# Application executables
cmake_minimum_required(VERSION 3.10)

# Create executables
add_executable(main main.cpp)
add_executable(Master master_main.cpp)
add_executable(Slave slave_main.cpp)
add_executable(MultiSlave multi_slave_main.cpp)

# Set of all executables for common configuration
set(ALL_EXECUTABLES main Master Slave MultiSlave)

# Apply common settings to all executables
foreach(target ${ALL_EXECUTABLES})
    # Set include directories for each target
    target_include_directories(${target} 
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../HAL/Gpio
        ${CMAKE_CURRENT_SOURCE_DIR}/../Adapter
        ${CMAKE_CURRENT_SOURCE_DIR}/../logger
        ${CMAKE_CURRENT_SOURCE_DIR}/../protocol
        ${CMAKE_CURRENT_SOURCE_DIR}/../protocol/messages
        ${CMAKE_CURRENT_SOURCE_DIR}/../protocol/utils
    )
    
    # Link libraries
    target_link_libraries(${target} 
        WhtsProtocol
        Logger
        HAL
        Adapter
    )
    
    # Set C++ standard
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    
    # Platform-specific settings
    if(WIN32)
        target_link_libraries(${target} ws2_32)
    endif()
endforeach() 