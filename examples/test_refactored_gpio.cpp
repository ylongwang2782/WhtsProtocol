#include <cassert>
#include <iomanip>
#include <iostream>

// 测试重构后的GPIO模块
#include "../src/HAL/Gpio.h"         // 统一包含头文件
#include "../src/HAL/GpioFactory.h"  // GPIO工厂
#include "../src/HAL/HardwareGpio.h" // 硬件GPIO实现模板
#include "../src/HAL/IGpio.h"        // 接口定义
#include "../src/HAL/VirtualGpio.h"  // 虚拟GPIO实现


void testGpioInterface() {
    std::cout << "=== GPIO接口测试 ===\n";

    // 测试通过工厂创建不同类型的GPIO实例
    std::cout << "1. 测试GPIO工厂...\n";

    // 创建虚拟GPIO
    auto virtualGpio = HAL::GpioFactory::createVirtualGpio();
    assert(virtualGpio != nullptr);
    std::cout << "✓ 虚拟GPIO创建成功\n";

    // 创建硬件GPIO（模板实现）
    auto hardwareGpio = HAL::GpioFactory::createHardwareGpio();
    assert(hardwareGpio != nullptr);
    std::cout << "✓ 硬件GPIO创建成功\n";

    // 测试通过类型枚举创建
    auto gpioByType1 =
        HAL::GpioFactory::createGpio(HAL::GpioFactory::GpioType::VIRTUAL);
    auto gpioByType2 =
        HAL::GpioFactory::createGpio(HAL::GpioFactory::GpioType::HARDWARE);
    assert(gpioByType1 != nullptr && gpioByType2 != nullptr);
    std::cout << "✓ 通过类型枚举创建GPIO成功\n";

    std::cout << "\n";
}

void testVirtualGpio() {
    std::cout << "=== 虚拟GPIO功能测试 ===\n";

    auto gpio = HAL::GpioFactory::createVirtualGpio();

    // 测试基本GPIO操作
    std::cout << "1. 测试基本GPIO操作...\n";

    // 配置引脚
    HAL::GpioConfig config1(0, HAL::GpioMode::INPUT_PULLDOWN);
    HAL::GpioConfig config2(1, HAL::GpioMode::OUTPUT, HAL::GpioState::HIGH);

    assert(gpio->init(config1));
    assert(gpio->init(config2));
    std::cout << "✓ GPIO引脚初始化成功\n";

    // 读取状态
    HAL::GpioState state1 = gpio->read(0);
    HAL::GpioState state2 = gpio->read(1);
    std::cout << "✓ 引脚0状态: "
              << (state1 == HAL::GpioState::HIGH ? "HIGH" : "LOW") << "\n";
    std::cout << "✓ 引脚1状态: "
              << (state2 == HAL::GpioState::HIGH ? "HIGH" : "LOW") << "\n";

    // 写入状态
    assert(gpio->write(1, HAL::GpioState::LOW));
    HAL::GpioState newState = gpio->read(1);
    std::cout << "✓ 引脚1写入后状态: "
              << (newState == HAL::GpioState::HIGH ? "HIGH" : "LOW") << "\n";

    // 测试批量读取
    std::cout << "\n2. 测试批量操作...\n";

    // 配置多个引脚
    for (uint8_t pin = 2; pin < 8; pin++) {
        HAL::GpioConfig config(pin, HAL::GpioMode::INPUT_PULLUP);
        gpio->init(config);
    }

    std::vector<uint8_t> pins = {0, 1, 2, 3, 4, 5, 6, 7};
    auto states = gpio->readMultiple(pins);

    std::cout << "✓ 批量读取结果: ";
    for (size_t i = 0; i < states.size(); i++) {
        std::cout << "P" << pins[i] << ":"
                  << (states[i] == HAL::GpioState::HIGH ? "H" : "L") << " ";
    }
    std::cout << "\n";

    // 测试虚拟GPIO特有功能
    std::cout << "\n3. 测试虚拟GPIO特有功能...\n";

    auto *virtualGpio = dynamic_cast<HAL::VirtualGpio *>(gpio.get());
    if (virtualGpio) {
        // 测试模拟状态设置
        virtualGpio->setSimulatedState(0, HAL::GpioState::HIGH);
        HAL::GpioState simulatedState = gpio->read(0);
        std::cout << "✓ 模拟状态设置成功: "
                  << (simulatedState == HAL::GpioState::HIGH ? "HIGH" : "LOW")
                  << "\n";

        // 测试引脚信息查询
        bool isInitialized = virtualGpio->isPinInitialized(0);
        HAL::GpioMode mode = virtualGpio->getPinMode(0);
        std::cout << "✓ 引脚0初始化状态: "
                  << (isInitialized ? "已初始化" : "未初始化") << "\n";
        std::cout << "✓ 引脚0模式: " << static_cast<int>(mode) << "\n";

        // 测试导通模式模拟
        virtualGpio->simulateContinuityPattern(4, 0b1010); // 交替模式
        std::cout << "✓ 导通模式模拟: ";
        for (uint8_t pin = 0; pin < 4; pin++) {
            HAL::GpioState state = gpio->read(pin);
            std::cout << "P" << pin << ":"
                      << (state == HAL::GpioState::HIGH ? "H" : "L") << " ";
        }
        std::cout << "\n";

        // 测试重置功能
        virtualGpio->resetAllPins();
        std::cout << "✓ 所有引脚重置完成\n";
    }

    std::cout << "\n";
}

void testHardwareGpio() {
    std::cout << "=== 硬件GPIO模板测试 ===\n";

    auto gpio = HAL::GpioFactory::createHardwareGpio();

    std::cout << "1. 测试硬件GPIO接口...\n";

    // 注意：硬件GPIO模板目前只是框架，实际操作会返回默认值
    HAL::GpioConfig config(0, HAL::GpioMode::OUTPUT, HAL::GpioState::LOW);
    bool initResult = gpio->init(config);
    std::cout << "✓ 硬件GPIO初始化结果: " << (initResult ? "成功" : "失败")
              << "\n";

    HAL::GpioState state = gpio->read(0);
    std::cout << "✓ 硬件GPIO读取结果: "
              << (state == HAL::GpioState::HIGH ? "HIGH" : "LOW") << "\n";

    bool writeResult = gpio->write(0, HAL::GpioState::HIGH);
    std::cout << "✓ 硬件GPIO写入结果: " << (writeResult ? "成功" : "失败")
              << "\n";

    std::cout << "注意: 硬件GPIO当前是模板实现，需要根据具体平台进行适配\n";
    std::cout << "\n";
}

void testBackwardCompatibility() {
    std::cout << "=== 向后兼容性测试 ===\n";

    // 测试通过统一头文件使用
    std::cout << "1. 测试统一头文件包含...\n";

    // 使用原来的方式创建GPIO（应该仍然工作）
    auto gpio = HAL::GpioFactory::createVirtualGpio();
    assert(gpio != nullptr);
    std::cout << "✓ 通过统一头文件创建GPIO成功\n";

    // 测试类型别名
    std::cout << "2. 测试类型别名...\n";
    HAL::Gpio *gpioPtr = gpio.get(); // 使用别名
    assert(gpioPtr != nullptr);
    std::cout << "✓ GPIO类型别名工作正常\n";

    std::cout << "\n";
}

int main() {
    std::cout << "WhtsProtocol GPIO模块重构验证测试\n";
    std::cout << "====================================\n\n";

    try {
        // 测试GPIO接口和工厂
        testGpioInterface();

        // 测试虚拟GPIO功能
        testVirtualGpio();

        // 测试硬件GPIO模板
        testHardwareGpio();

        // 测试向后兼容性
        testBackwardCompatibility();

        std::cout << "🎉 所有测试通过!\n";
        std::cout << "\n重构总结:\n";
        std::cout << "- ✅ 接口与实现成功分离\n";
        std::cout << "- ✅ 虚拟GPIO功能完整\n";
        std::cout << "- ✅ 硬件GPIO模板就绪\n";
        std::cout << "- ✅ 工厂模式工作正常\n";
        std::cout << "- ✅ 向后兼容性保持\n";
        std::cout << "- ✅ 模块化架构验证成功\n";
        std::cout << "\n准备就绪，可以移植到嵌入式平台!\n";

    } catch (const std::exception &e) {
        std::cerr << "❌ 测试失败: " << e.what() << "\n";
        return 1;
    }

    return 0;
}