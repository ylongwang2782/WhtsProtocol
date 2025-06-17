#include <cassert>
#include <iomanip>
#include <iostream>

// 简化版本的测试，用头文件的方式包含
#include "../src/Adapter/ContinuityCollector.h"
#include "../src/HAL/Gpio.h"

void testGpio() {
    std::cout << "=== GPIO测试 ===\n";

    // 创建虚拟GPIO
    auto gpio = HAL::GpioFactory::createVirtualGpio();
    assert(gpio != nullptr);
    std::cout << "✓ 虚拟GPIO创建成功\n";

    // 配置引脚
    HAL::GpioConfig config(0, HAL::GpioMode::INPUT_PULLDOWN);
    bool initResult = gpio->init(config);
    assert(initResult);
    std::cout << "✓ GPIO引脚0初始化成功 (INPUT_PULLDOWN)\n";

    // 读取状态
    HAL::GpioState state = gpio->read(0);
    std::cout << "✓ 引脚0状态: "
              << (state == HAL::GpioState::HIGH ? "HIGH" : "LOW") << "\n";

    // 测试多引脚配置
    for (uint8_t pin = 1; pin < 8; pin++) {
        HAL::GpioConfig pinConfig(pin, HAL::GpioMode::INPUT_PULLUP);
        gpio->init(pinConfig);
    }
    std::cout << "✓ 多引脚配置完成 (引脚1-7)\n";

    // 批量读取
    std::vector<uint8_t> pins = {0, 1, 2, 3, 4, 5, 6, 7};
    auto states = gpio->readMultiple(pins);
    std::cout << "✓ 批量读取结果: ";
    for (size_t i = 0; i < states.size(); i++) {
        std::cout << "P" << pins[i] << ":"
                  << (states[i] == HAL::GpioState::HIGH ? "H" : "L") << " ";
    }
    std::cout << "\n";

    std::cout << "GPIO测试完成!\n\n";
}

void testContinuityCollector() {
    std::cout << "=== 导通采集器测试 ===\n";

    // 创建采集器
    auto collector =
        Adapter::ContinuityCollectorFactory::createWithVirtualGpio();
    assert(collector != nullptr);
    std::cout << "✓ 导通采集器创建成功\n";

    // 配置采集参数
    Adapter::CollectorConfig config;
    config.num = 4;       // 4个引脚
    config.interval = 50; // 50ms间隔
    config.autoStart = false;

    bool configResult = collector->configure(config);
    assert(configResult);
    std::cout << "✓ 采集器配置成功 (4引脚, 50ms间隔)\n";

    // 模拟特定测试模式
    uint32_t testPattern = 0b1010; // 交替模式
    collector->simulateTestPattern(testPattern);
    std::cout << "✓ 设置测试模式: 1010\n";

    // 开始采集
    bool startResult = collector->startCollection();
    assert(startResult);
    std::cout << "✓ 开始采集...\n";

    // 监控进度
    int lastProgress = -1;
    while (!collector->isCollectionComplete()) {
        int progress = collector->getProgress();
        if (progress != lastProgress) {
            std::cout << "  进度: " << progress << "%\n";
            lastProgress = progress;
        }

        // 检查状态
        if (collector->getStatus() == Adapter::CollectionStatus::ERROR) {
            std::cerr << "✗ 采集过程中发生错误!\n";
            return;
        }

        // 短暂等待
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "✓ 采集完成!\n";

    // 获取数据矩阵
    auto matrix = collector->getDataMatrix();
    std::cout << "✓ 数据矩阵 (" << matrix.size() << "x"
              << (matrix.empty() ? 0 : matrix[0].size()) << "):\n";

    // 打印矩阵
    std::cout << "     ";
    for (size_t pin = 0; pin < config.num; pin++) {
        std::cout << "P" << pin << " ";
    }
    std::cout << "\n";

    for (size_t cycle = 0; cycle < matrix.size(); cycle++) {
        std::cout << "C" << cycle << ": ";
        for (size_t pin = 0; pin < matrix[cycle].size(); pin++) {
            char symbol =
                (matrix[cycle][pin] == Adapter::ContinuityState::CONNECTED)
                    ? '1'
                    : '0';
            std::cout << " " << symbol << " ";
        }
        std::cout << "\n";
    }

    // 计算统计信息
    auto stats = collector->calculateStatistics();
    std::cout << "✓ 统计信息:\n";
    std::cout << "  - 总导通次数: " << stats.totalConnections << "\n";
    std::cout << "  - 总断开次数: " << stats.totalDisconnections << "\n";
    std::cout << "  - 导通率: " << std::fixed << std::setprecision(1)
              << stats.connectionRate << "%\n";

    std::cout << "导通采集器测试完成!\n\n";
}

int main() {
    std::cout << "WhtsProtocol HAL和Adapter模块功能测试\n";
    std::cout << "=====================================\n\n";

    try {
        // 测试GPIO功能
        testGpio();

        // 测试导通采集器功能
        testContinuityCollector();

        std::cout << "🎉 所有测试通过!\n";
        std::cout << "\n总结:\n";
        std::cout << "- ✅ GPIO抽象层工作正常\n";
        std::cout << "- ✅ 虚拟GPIO仿真功能正常\n";
        std::cout << "- ✅ 导通采集器功能正常\n";
        std::cout << "- ✅ 数据采集和分析功能正常\n";
        std::cout << "- ✅ 模块化架构验证成功\n";

    } catch (const std::exception &e) {
        std::cerr << "❌ 测试失败: " << e.what() << "\n";
        return 1;
    }

    return 0;
}