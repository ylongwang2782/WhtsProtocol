# CMake 工具链文件说明

本目录包含了项目使用的CMake工具链文件，用于配置不同的编译器和构建环境。

## 工具链文件

### toolchain-gcc.cmake
- **用途**: GCC编译器工具链配置
- **支持平台**: Windows, Linux, macOS
- **特性**:
  - 配置GCC/G++编译器
  - 设置编译标志和优化选项
  - 平台特定的链接设置
  - 自动检测和配置工具链组件

### toolchain-clang.cmake
- **用途**: Clang编译器工具链配置
- **支持平台**: Windows, Linux, macOS
- **特性**:
  - 配置Clang/Clang++编译器
  - 启用彩色诊断输出
  - 优先使用LLD链接器（如果可用）
  - LLVM工具链集成

## 使用方法

### 通过CMakePresets.json使用（推荐）

项目已配置了多个预设，可以直接使用：

```bash
# 使用GCC工具链构建Debug版本
cmake --preset ninja-gcc-toolchain-debug
cmake --build --preset gcc-debug

# 使用Clang工具链构建Release版本
cmake --preset ninja-clang-toolchain-release
cmake --build --preset clang-release

# 使用工作流预设（配置+构建+测试）
cmake --workflow --preset gcc-toolchain
cmake --workflow --preset clang-toolchain
```

### 手动指定工具链文件

```bash
# 使用GCC工具链
cmake -S . -B build/gcc-debug \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-gcc.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -G Ninja

# 使用Clang工具链
cmake -S . -B build/clang-release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-clang.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -G Ninja
```

## 工具链特性

### 编译器标志
- **Debug**: `-g -O0 -Wall -Wextra -DDEBUG`
- **Release**: `-O3 -DNDEBUG`
- **RelWithDebInfo**: `-O2 -g -DNDEBUG`
- **MinSizeRel**: `-Os -DNDEBUG`

### 平台特定设置

#### Windows
- 定义 `_WIN32_WINNT=0x0601` (Windows 7+)
- 自动链接Winsock库（在主CMakeLists.txt中处理）

#### Unix/Linux
- 启用pthread支持
- 使用LLD链接器（如果可用）

### 自动配置
- C++11标准
- 位置无关代码(-fPIC)
- 编译命令导出(compile_commands.json)
- 工具链组件自动检测

## 扩展工具链

要添加新的工具链，请：

1. 创建新的工具链文件（如`toolchain-msvc.cmake`）
2. 在`CMakePresets.json`中添加相应的预设
3. 更新本文档

## 故障排除

### 编译器未找到
确保编译器在系统PATH中，或在工具链文件中指定完整路径：
```cmake
set(CMAKE_C_COMPILER /usr/bin/gcc-11)
set(CMAKE_CXX_COMPILER /usr/bin/g++-11)
```

### 链接器问题
如果遇到链接器问题，可以在工具链文件中禁用LLD：
```cmake
# 注释掉LLD相关设置
# find_program(LLD_LINKER ld.lld)
```

### Windows特定问题
在Windows上使用GCC时，可能需要安装MinGW-w64或使用MSYS2环境。 