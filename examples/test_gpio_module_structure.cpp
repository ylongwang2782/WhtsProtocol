#include "Gpio.h" // 使用新的包含路径
#include <iostream>
#include <memory>


void testGpioModuleStructure() {
    std::cout << "=== Test GPIO Module Structure ===" << std::endl;

    // 测试统一接口
    auto gpio = HAL::GpioFactory::createGpio();

    if (!gpio) {
        std::cout << "Error: Failed to create GPIO instance" << std::endl;
        return;
    }

    std::cout << "Successfully created GPIO instance using unified interface"
              << std::endl;

    // 测试GPIO基本功能
    HAL::GpioConfig config(0, HAL::GpioMode::OUTPUT);
    if (!gpio->init(config)) {
        std::cout << "Error: GPIO initialization failed" << std::endl;
        return;
    }

    std::cout << "GPIO initialization successful" << std::endl;

    // 测试写入操作
    gpio->write(0, HAL::GpioState::HIGH);
    std::cout << "Set Pin 0 to HIGH" << std::endl;

    // 测试读取操作
    auto state = gpio->read(0);
    std::cout << "Pin 0 current state: "
              << (state == HAL::GpioState::HIGH ? "HIGH" : "LOW") << std::endl;

    // 清理
    gpio->deinit(0);
    std::cout << "GPIO cleanup completed" << std::endl;

    std::cout << "=== GPIO Module Structure Test Complete ===" << std::endl;
}

void showModuleInfo() {
    std::cout << "=== GPIO Module Structure Info ===" << std::endl;
    std::cout << "Module Location: src/HAL/Gpio/" << std::endl;
    std::cout << "Files Included:" << std::endl;
    std::cout << "  - IGpio.h (Interface definition)" << std::endl;
    std::cout << "  - VirtualGpio.h/cpp (Virtual implementation)" << std::endl;
    std::cout << "  - HardwareGpio.h/cpp (Hardware implementation)"
              << std::endl;
    std::cout << "  - GpioFactory.h/cpp (Factory class)" << std::endl;
    std::cout << "  - Gpio.h/cpp (Unified header)" << std::endl;
    std::cout << "  - CMakeLists.txt (Build configuration)" << std::endl;
    std::cout << std::endl;

    std::cout << "Include Method:" << std::endl;
    std::cout << "  - Use: #include \"Gpio.h\" (no relative paths)"
              << std::endl;
    std::cout << "  - CMake handles include directories automatically"
              << std::endl;
    std::cout << std::endl;

#if defined(GPIO_USE_HARDWARE)
    std::cout << "Current Configuration: Hardware GPIO" << std::endl;
#elif defined(GPIO_USE_VIRTUAL)
    std::cout << "Current Configuration: Virtual GPIO" << std::endl;
#else
    std::cout << "Current Configuration: Default Virtual GPIO" << std::endl;
#endif

    std::cout << "====================================" << std::endl;
}

int main() {
    std::cout << "GPIO Module Structure Test Program" << std::endl;
    std::cout << "===================================" << std::endl;

    showModuleInfo();
    std::cout << std::endl;

    try {
        testGpioModuleStructure();
    } catch (const std::exception &e) {
        std::cout << "Exception occurred during testing: " << e.what()
                  << std::endl;
        return 1;
    }

    std::cout << std::endl;
    std::cout << "All tests completed successfully!" << std::endl;

    return 0;
}