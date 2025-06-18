#include "HAL/Gpio.h"
#include <iostream>
#include <memory>


void testUnifiedGpioInterface() {
    std::cout << "=== Test Unified GPIO Interface ===" << std::endl;

    // Create GPIO instance using unified interface
    // The actual implementation (Virtual or Hardware GPIO) is determined by
    // CMake configuration
    auto gpio = HAL::GpioFactory::createGpio();

    if (!gpio) {
        std::cout << "Error: Failed to create GPIO instance" << std::endl;
        return;
    }

    std::cout << "Successfully created GPIO instance" << std::endl;

    // Configure GPIO
    HAL::GpioConfig config(0, HAL::GpioMode::OUTPUT);
    if (!gpio->init(config)) {
        std::cout << "Error: GPIO initialization failed" << std::endl;
        return;
    }

    std::cout << "GPIO initialization successful" << std::endl;

    // Test GPIO write operations
    std::cout << "Testing GPIO write operations..." << std::endl;
    gpio->write(0, HAL::GpioState::HIGH);
    std::cout << "Set Pin 0 to HIGH" << std::endl;

    gpio->write(0, HAL::GpioState::LOW);
    std::cout << "Set Pin 0 to LOW" << std::endl;

    // Test GPIO read operations
    std::cout << "Testing GPIO read operations..." << std::endl;
    auto state = gpio->read(0);
    std::cout << "Pin 0 current state: "
              << (state == HAL::GpioState::HIGH ? "HIGH" : "LOW") << std::endl;

    // Cleanup
    gpio->deinit(0);
    std::cout << "GPIO cleanup completed" << std::endl;

    std::cout << "=== Unified GPIO Interface Test Complete ===" << std::endl;
}

void showPlatformInfo() {
    std::cout << "=== Platform Configuration Info ===" << std::endl;

#if defined(GPIO_USE_HARDWARE)
    std::cout << "Currently using: Hardware GPIO implementation" << std::endl;
#elif defined(GPIO_USE_VIRTUAL)
    std::cout << "Currently using: Virtual GPIO implementation" << std::endl;
#else
    std::cout << "Currently using: Default Virtual GPIO implementation"
              << std::endl;
#endif

    std::cout << "Platform switching methods:" << std::endl;
    std::cout << "  Development/Testing: cmake -B build -DGPIO_USE_VIRTUAL=ON"
              << std::endl;
    std::cout << "  Production: cmake -B build -DGPIO_USE_HARDWARE=ON"
              << std::endl;
    std::cout << "====================================" << std::endl;
}

int main() {
    std::cout << "Unified GPIO Interface Test Program" << std::endl;
    std::cout << "===================================" << std::endl;

    showPlatformInfo();
    std::cout << std::endl;

    try {
        testUnifiedGpioInterface();
    } catch (const std::exception &e) {
        std::cout << "Exception occurred during testing: " << e.what()
                  << std::endl;
        return 1;
    }

    std::cout << std::endl;
    std::cout << "Test completed!" << std::endl;

    return 0;
}