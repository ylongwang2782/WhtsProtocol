# Application executables
cmake_minimum_required(VERSION 3.10)

# Include directories for headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../HAL/Gpio)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../Adapter)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../logger)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../protocol)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../protocol/messages)

# Create executables
add_executable(main main.cpp)
add_executable(Master master_main.cpp)
add_executable(Slave slave_main.cpp)

# Set of all executables for common configuration
set(ALL_EXECUTABLES main Master Slave)

# Apply common settings to all executables
foreach(target ${ALL_EXECUTABLES})
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