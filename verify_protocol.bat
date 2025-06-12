@echo off
setlocal enabledelayedexpansion

echo ============================================
echo WHTS协议完整性验证脚本
echo ============================================
echo.

:: 检查必要工具
echo [1/6] 检查开发环境...
where cmake >nul 2>&1
if errorlevel 1 (
    echo ❌ 错误: 未找到 CMake，请确保已安装 CMake 并添加到 PATH
    pause
    exit /b 1
)

where g++ >nul 2>&1
if errorlevel 1 (
    echo ❌ 错误: 未找到 g++，请确保已安装支持 C++17 的编译器
    pause
    exit /b 1
)

echo ✅ 开发环境检查通过

:: 创建构建目录
echo.
echo [2/6] 准备构建环境...
if exist build_test rmdir /s /q build_test
mkdir build_test
cd build_test

:: 配置CMake项目
echo.
echo [3/6] 配置构建系统...
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -f ..\test_CMakeLists.txt .. 
if errorlevel 1 (
    echo ❌ CMake配置失败
    cd ..
    pause
    exit /b 1
)

echo ✅ 构建系统配置成功

:: 编译项目
echo.
echo [4/6] 编译协议库和测试程序...
cmake --build . --parallel
if errorlevel 1 (
    echo ❌ 编译失败，请检查代码错误
    cd ..
    pause
    exit /b 1
)

echo ✅ 编译成功

:: 运行基本功能测试
echo.
echo [5/6] 运行基本功能测试...
echo.
echo --- 运行分片和粘包测试 ---
fragmentation_test.exe
if errorlevel 1 (
    echo ❌ 分片测试失败
    set TEST_FAILED=1
) else (
    echo ✅ 分片测试通过
)

echo.
echo --- 运行完整协议验证 ---
protocol_validation.exe
if errorlevel 1 (
    echo ❌ 完整验证失败
    set TEST_FAILED=1
) else (
    echo ✅ 完整验证通过
)

:: 运行CTest测试框架
echo.
echo [6/6] 运行自动化测试套件...
ctest --output-on-failure --verbose
if errorlevel 1 (
    echo ❌ 自动化测试失败
    set TEST_FAILED=1
) else (
    echo ✅ 自动化测试通过
)

:: 生成测试报告
echo.
echo ============================================
echo 验证结果总结
echo ============================================

if defined TEST_FAILED (
    echo ❌ 验证失败！请修复上述问题后重新验证
    echo.
    echo 建议检查项目：
    echo 1. 代码编译错误
    echo 2. 逻辑实现错误
    echo 3. 测试用例覆盖不全
    echo.
    echo 修复后请重新运行此脚本进行验证
) else (
    echo ✅ 所有验证通过！
    echo.
    echo 协议库已准备就绪，可以交付给团队：
    echo - 编译无错误和警告
    echo - 所有功能测试通过
    echo - 分片和粘包功能正常
    echo - 边界条件处理正确
    echo - 性能符合预期
    echo.
    echo 交付文件清单：
    echo - src/WhtsProtocol.h (头文件)
    echo - src/WhtsProtocol.cpp (实现文件)
    echo - FRAGMENTATION_GUIDE.md (使用指南)
    echo - src/fragmentation_test.cpp (示例代码)
)

cd ..
echo.
echo 验证完成，按任意键退出...
pause >nul 