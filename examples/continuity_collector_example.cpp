#include "../src/Adapter/ContinuityCollector.h"
#include "../src/logger/Logger.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace Adapter;
using namespace HAL;

void printMatrix(const ContinuityMatrix &matrix, const std::string &title) {
    std::cout << "\n=== " << title << " ===\n";

    if (matrix.empty()) {
        std::cout << "矩阵为空\n";
        return;
    }

    // 打印表头
    std::cout << "Cycle\\Pin ";
    for (size_t i = 0; i < matrix[0].size(); i++) {
        std::cout << std::setw(3) << i << " ";
    }
    std::cout << "\n";

    // 打印数据行
    for (size_t cycle = 0; cycle < matrix.size(); cycle++) {
        std::cout << std::setw(9) << cycle << " ";
        for (size_t pin = 0; pin < matrix[cycle].size(); pin++) {
            char symbol =
                (matrix[cycle][pin] == ContinuityState::CONNECTED) ? '1' : '0';
            std::cout << std::setw(3) << symbol << " ";
        }
        std::cout << "\n";
    }
}

void progressCallback(uint8_t cycle, uint8_t totalCycles) {
    double percentage = (static_cast<double>(cycle) / totalCycles) * 100.0;
    std::cout << "采集进度: " << std::setprecision(1) << std::fixed
              << percentage << "% (" << static_cast<int>(cycle) << "/"
              << static_cast<int>(totalCycles) << ")\n";
}

int main() {
    try {
        // 初始化日志系统
        Logger::getInstance().setLogLevel(LogLevel::INFO);
        Logger::getInstance().log(LogLevel::INFO, "MAIN",
                                  "导通数据采集器示例开始运行");

        // 创建导通数据采集器
        auto collector = ContinuityCollectorFactory::createWithVirtualGpio();

        // 配置采集参数
        CollectorConfig config;
        config.num = 8;        // 检测8个引脚
        config.interval = 200; // 200ms间隔
        config.autoStart = false;

        if (!collector->configure(config)) {
            std::cerr << "配置采集器失败！\n";
            return 1;
        }

        std::cout << "导通数据采集器配置完成:\n";
        std::cout << "- 检测引脚数量: " << static_cast<int>(config.num) << "\n";
        std::cout << "- 采集间隔: " << config.interval << "ms\n";
        std::cout << "- 总共周期数: " << static_cast<int>(config.num) << "\n\n";

        // 设置进度回调
        collector->setProgressCallback(progressCallback);

        // 示例1：基本采集测试
        std::cout << "=== 示例1: 基本采集测试 ===\n";

        if (!collector->startCollection()) {
            std::cerr << "启动采集失败！\n";
            return 1;
        }

        // 等待采集完成
        while (!collector->isCollectionComplete()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (collector->getStatus() == CollectionStatus::ERROR) {
                std::cerr << "采集过程中发生错误！\n";
                return 1;
            }
        }

        auto dataMatrix = collector->getDataMatrix();
        printMatrix(dataMatrix, "基本采集结果");

        // 打印统计信息
        auto stats = collector->calculateStatistics();
        std::cout << "\n统计信息:\n";
        std::cout << "- 总导通次数: " << stats.totalConnections << "\n";
        std::cout << "- 总断开次数: " << stats.totalDisconnections << "\n";
        std::cout << "- 导通率: " << std::setprecision(2) << std::fixed
                  << stats.connectionRate << "%\n";
        std::cout << "- 最活跃的引脚: ";
        for (int i = 0; i < 5; i++) {
            if (stats.mostActivePins[i] < config.num) {
                std::cout << static_cast<int>(stats.mostActivePins[i]) << " ";
            }
        }
        std::cout << "\n";

        // 示例2：模拟特定模式测试
        std::cout << "\n=== 示例2: 模拟棋盘模式测试 ===\n";

        // 重新配置
        config.num = 6;
        config.interval = 100;
        collector->configure(config);

        // 模拟棋盘模式（交替的1和0）
        uint32_t checkerboardPattern = 0xAAAAAAAA; // 二进制: 10101010...
        collector->simulateTestPattern(checkerboardPattern);

        if (!collector->startCollection()) {
            std::cerr << "启动模拟采集失败！\n";
            return 1;
        }

        // 等待采集完成
        while (!collector->isCollectionComplete()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        auto checkerboardData = collector->getDataMatrix();
        printMatrix(checkerboardData, "棋盘模式采集结果");

        // 示例3：获取特定数据
        std::cout << "\n=== 示例3: 数据查询测试 ===\n";

        // 获取第一个周期的数据
        auto firstCycle = collector->getCycleData(0);
        std::cout << "第0周期数据: ";
        for (auto state : firstCycle) {
            std::cout << (state == ContinuityState::CONNECTED ? '1' : '0')
                      << " ";
        }
        std::cout << "\n";

        // 获取第一个引脚的所有周期数据
        auto firstPin = collector->getPinData(0);
        std::cout << "引脚0的所有周期数据: ";
        for (auto state : firstPin) {
            std::cout << (state == ContinuityState::CONNECTED ? '1' : '0')
                      << " ";
        }
        std::cout << "\n";

        // 导出完整数据
        std::cout << "\n=== 完整数据导出 ===\n";
        std::cout << collector->exportDataAsString();

        Logger::getInstance().log(LogLevel::INFO, "MAIN",
                                  "导通数据采集器示例运行完成");

    } catch (const std::exception &e) {
        std::cerr << "异常: " << e.what() << "\n";
        Logger::getInstance().log(LogLevel::ERROR, "MAIN",
                                  std::string("异常: ") + e.what());
        return 1;
    }

    return 0;
}