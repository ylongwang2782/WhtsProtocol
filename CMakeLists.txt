cmake_minimum_required(VERSION 3.10)
project(WhtsProtocol)

# 如果没有使用工具链文件，设置默认的C++标准和编译选项
if(NOT CMAKE_TOOLCHAIN_FILE)
  # 设置C++标准
  set(CMAKE_CXX_STANDARD 14)
  set(CMAKE_CXX_STANDARD_REQUIRED True)

  # 设置编译选项
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

  # 生成compile_commands.json以供clangd使用
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 添加源文件
set(COMMON_SRC src/fragmentation_test.cpp src/WhtsProtocol.cpp)

enable_language(C CXX)

# 添加可执行文件
add_executable(whts_protocol ${COMMON_SRC})

# 如果是Windows平台，需要链接winsock库
if(WIN32)
  target_link_libraries(whts_protocol wsock32 ws2_32)
endif()
