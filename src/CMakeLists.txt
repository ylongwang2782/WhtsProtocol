# Source directory CMakeLists.txt

# Logger module
add_subdirectory(logger)

# HAL module
add_subdirectory(HAL)

# Adapter module
add_subdirectory(Adapter)

# Protocol module
add_subdirectory(protocol)

# Original main executable (for backward compatibility)
add_executable(main main.cpp)

# Master executable
add_executable(Master master_main.cpp)

# Slave executable  
add_executable(Slave slave_main.cpp)

# Link libraries for all executables
foreach(target main Master Slave)
    target_link_libraries(${target} 
        PRIVATE
        WhtsProtocol 
        Logger
        HAL
        Adapter
    )

    # Set include directories
    target_include_directories(${target} 
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # Windows特定设置
    if(WIN32)
        target_link_libraries(${target} PRIVATE ws2_32)
    endif()

    # 编译选项
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
    endif()
endforeach() 