# Application executables
cmake_minimum_required(VERSION 3.10)

# Add Logger library for app
add_library(AppLogger STATIC
    Logger.cpp
    Logger.h
    NetworkManager.cpp
    NetworkManager.h
)

target_include_directories(AppLogger PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../interface
    ${CMAKE_CURRENT_SOURCE_DIR}/../Adapter
)

target_link_libraries(AppLogger PUBLIC
    Adapter
)

# Add subdirectories
add_subdirectory(master_main)
add_subdirectory(slave_main)