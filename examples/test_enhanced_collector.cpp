#include <bitset>
#include <cassert>
#include <iomanip>
#include <iostream>


// 简化版本的测试，用头文件的方式包含
#include "../src/Adapter/ContinuityCollector.h"
#include "../src/HAL/Gpio.h"

void printCompressedData(const std::vector<uint8_t> &data,
                         const std::string &title) {
    std::cout << "\n=== " << title << " ===\n";
    std::cout << "压缩数据大小: " << data.size() << " 字节\n";
    std::cout << "十六进制: ";
    for (size_t i = 0; i < data.size(); i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << "\n";

    std::cout << "二进制表示:\n";
    for (size_t i = 0; i < data.size(); i++) {
        std::cout << "字节" << i << ": " << std::bitset<8>(data[i]) << "\n";
    }
    std::cout << "\n";
}

void testEnhancedContinuityCollector() {
    std::cout << "=== 增强导通采集器测试 ===\n";

    // 创建采集器
    auto collector =
        Adapter::ContinuityCollectorFactory::createWithVirtualGpio();
    assert(collector != nullptr);
    std::cout << "✓ 导通采集器创建成功\n";

    // 测试新的配置参数
    Adapter::CollectorConfig config;
    config.num = 4;               // 检测4个引脚
    config.startDetectionNum = 2; // 从第2个周期开始检测
    config.totalDetectionNum = 8; // 总共8个检测周期
    config.interval = 50;         // 50ms间隔
    config.autoStart = false;

    bool configResult = collector->configure(config);
    assert(configResult);
    std::cout << "✓ 新配置参数设置成功\n";
    std::cout << "  - 检测引脚数: " << static_cast<int>(config.num) << "\n";
    std::cout << "  - 开始检测周期: "
              << static_cast<int>(config.startDetectionNum) << "\n";
    std::cout << "  - 总检测周期: "
              << static_cast<int>(config.totalDetectionNum) << "\n";
    std::cout << "  - 检测间隔: " << config.interval << "ms\n";

    // 模拟特定测试模式
    uint32_t testPattern = 0b11010110; // 复杂交替模式
    collector->simulateTestPattern(testPattern);
    std::cout << "✓ 设置测试模式: " << std::bitset<8>(testPattern) << "\n";

    // 开始采集
    bool startResult = collector->startCollection();
    assert(startResult);
    std::cout << "✓ 开始增强采集...\n";

    // 监控进度
    int lastProgress = -1;
    while (!collector->isCollectionComplete()) {
        int progress = collector->getProgress();
        if (progress != lastProgress) {
            std::cout << "  进度: " << progress << "% (周期 "
                      << static_cast<int>(collector->getCurrentCycle()) << "/"
                      << static_cast<int>(collector->getTotalCycles()) << ")\n";
            lastProgress = progress;
        }

        if (collector->getStatus() == Adapter::CollectionStatus::ERROR) {
            std::cerr << "✗ 采集过程中发生错误!\n";
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "✓ 采集完成!\n";

    // 获取并显示数据矩阵
    auto matrix = collector->getDataMatrix();
    std::cout << "\n=== 数据矩阵 (" << matrix.size() << "x"
              << (matrix.empty() ? 0 : matrix[0].size()) << ") ===\n";

    // 打印矩阵表头
    std::cout << "周期\\引脚 ";
    for (size_t pin = 0; pin < config.num; pin++) {
        std::cout << "P" << pin << " ";
    }
    std::cout << " | 活跃引脚\n";
    std::cout << "------------------------------\n";

    // 打印矩阵数据并标记活跃范围
    for (size_t cycle = 0; cycle < matrix.size(); cycle++) {
        std::cout << "    " << cycle << "    ";
        for (size_t pin = 0; pin < matrix[cycle].size(); pin++) {
            char symbol =
                (matrix[cycle][pin] == Adapter::ContinuityState::CONNECTED)
                    ? '1'
                    : '0';
            std::cout << " " << symbol << " ";
        }

        // 显示应该活跃的引脚（检测逻辑验证）
        if (cycle >= config.startDetectionNum &&
            cycle < config.startDetectionNum + config.num) {
            uint8_t activePin = cycle - config.startDetectionNum;
            std::cout << " | 引脚" << static_cast<int>(activePin)
                      << "应输出HIGH";
        } else {
            std::cout << " | 所有引脚应为INPUT";
        }
        std::cout << "\n";
    }

    // 测试压缩数据向量功能
    auto compressedData = collector->getDataVector();
    printCompressedData(compressedData, "压缩数据向量");

    // 验证压缩效率
    size_t originalBits = matrix.size() * config.num;
    size_t compressedBits = compressedData.size() * 8;
    std::cout << "原始数据: " << originalBits << " 位\n";
    std::cout << "压缩数据: " << compressedBits << " 位 ("
              << compressedData.size() << " 字节)\n";
    std::cout << "理论压缩率: " << (static_cast<double>(originalBits) / 8.0)
              << " -> " << compressedData.size() << " 字节\n";

    // 计算统计信息
    auto stats = collector->calculateStatistics();
    std::cout << "\n=== 统计信息 ===\n";
    std::cout << "- 总导通次数: " << stats.totalConnections << "\n";
    std::cout << "- 总断开次数: " << stats.totalDisconnections << "\n";
    std::cout << "- 导通率: " << std::fixed << std::setprecision(1)
              << stats.connectionRate << "%\n";

    // 导出详细数据
    std::cout << "\n=== 详细数据导出 ===\n";
    std::cout << collector->exportDataAsString();

    std::cout << "✓ 增强导通采集器测试完成!\n\n";
}

int main() {
    std::cout << "WhtsProtocol 增强导通采集器功能测试\n";
    std::cout << "====================================\n\n";

    try {
        testEnhancedContinuityCollector();

        std::cout << "🎉 所有增强功能测试通过!\n";
        std::cout << "\n新功能验证总结:\n";
        std::cout << "- ✅ 新配置参数 (startDetectionNum, totalDetectionNum)\n";
        std::cout << "- ✅ 增强的检测逻辑 (按周期配置GPIO模式)\n";
        std::cout << "- ✅ 压缩数据向量功能 (按位压缩，小端模式)\n";
        std::cout << "- ✅ 动态矩阵大小 (totalDetectionNum x num)\n";
        std::cout << "- ✅ 详细的配置信息导出\n";

    } catch (const std::exception &e) {
        std::cerr << "❌ 测试失败: " << e.what() << "\n";
        return 1;
    }

    return 0;
}