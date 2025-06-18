# WhtsProtocol Build Guide

## Cross-Platform Build with Clang

This project supports cross-platform compilation using Clang compiler to ensure compatibility across different operating systems.

## Prerequisites

- **CMake** (version 3.10 or higher)
- **Clang/Clang++** compiler
- **Ninja** build system (recommended)

### Windows
```powershell
# Install via Chocolatey (if available)
choco install llvm cmake ninja

# Or download manually:
# - LLVM/Clang: https://releases.llvm.org/
# - CMake: https://cmake.org/download/
# - Ninja: https://ninja-build.org/
```

### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install clang cmake ninja-build
```

### macOS
```bash
# Using Homebrew
brew install llvm cmake ninja
```

## Building

### Option 1: Using Build Scripts (Recommended)

**Windows (PowerShell):**
```powershell
./build-clang.ps1
```

**Linux/macOS (Bash):**
```bash
chmod +x build-clang.sh
./build-clang.sh
```

### Option 2: Manual Build

```bash
# Create build directory
mkdir -p build/clang-release
cd build/clang-release

# Configure with CMake
cmake -G "Ninja" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release \
    ../..

# Build
cmake --build .
```

## Running

After successful build:

**Windows:**
```powershell
./build/clang-release/main.exe
```

**Linux/macOS:**
```bash
./build/clang-release/main
```

The program will start a UDP server on port 8888 for protocol testing.

## Features

- ✅ Cross-platform compatibility (Windows, Linux, macOS)
- ✅ Clang compiler for consistent behavior
- ✅ Full protocol support (5 packet types)
- ✅ Auto-fragmentation and reassembly
- ✅ Comprehensive testing framework

## Protocol Support

- Master2Slave packets
- Slave2Master packets  
- Backend2Master packets
- Master2Backend packets
- Slave2Backend packets 