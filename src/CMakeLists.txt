# Source directory CMakeLists.txt

# Interface module (must be first as others depend on it)
add_subdirectory(interface)

# HAL module
add_subdirectory(HAL)

# Adapter module
add_subdirectory(Adapter)

# Protocol module
add_subdirectory(protocol)

# Application executables
add_subdirectory(app) 