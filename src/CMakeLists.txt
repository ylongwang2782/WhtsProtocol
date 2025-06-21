# Source directory CMakeLists.txt

# Interface module (must be first as others depend on it)
add_subdirectory(interface)

# Platform-specific implementations
add_subdirectory(platform)

# Adapter module
add_subdirectory(Adapter)

# Protocol module
add_subdirectory(protocol)

# Application executables
add_subdirectory(app) 