#ifndef CONTINUITY_COLLECTOR_H
#define CONTINUITY_COLLECTOR_H

#include "IGpio.h"
#include "VirtualGpio.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Adapter {

// 导通状态枚举
enum class ContinuityState : uint8_t {
    DISCONNECTED = 0, // 断开
    CONNECTED = 1     // 导通
};

// 导通数据采集配置
struct CollectorConfig {
    uint8_t num;               // 导通检测数量 a (< 64)
    uint8_t startDetectionNum; // 开始检测数量 b
    uint8_t totalDetectionNum; // 总检测数量 k
    uint32_t interval;         // 检测间隔 (毫秒)
    bool autoStart;            // 是否自动开始采集

    CollectorConfig(uint8_t n = 8, uint8_t startDetNum = 0,
                    uint8_t totalDetNum = 16, uint32_t i = 100,
                    bool autoS = false)
        : num(n), startDetectionNum(startDetNum),
          totalDetectionNum(totalDetNum), interval(i), autoStart(autoS) {
        if (num > 64)
            num = 64;
        if (totalDetectionNum == 0 || totalDetectionNum > 64)
            totalDetectionNum = 64;
        if (startDetectionNum >= totalDetectionNum)
            startDetectionNum = 0;
    }
};

// 导通数据矩阵类型
using ContinuityMatrix = std::vector<std::vector<ContinuityState>>;

// 采集状态枚举
enum class CollectionStatus : uint8_t {
    IDLE = 0,      // 空闲状态
    RUNNING = 1,   // 正在采集
    COMPLETED = 2, // 采集完成
    ERROR = 3      // 错误状态
};

// 采集进度回调函数类型
using ProgressCallback =
    std::function<void(uint8_t cycle, uint8_t totalCycles)>;

// 导通数据采集器类
class ContinuityCollector {
  private:
    static constexpr uint8_t MAX_GPIO_PINS = 64;

    std::unique_ptr<HAL::IGpio> gpio_; // GPIO接口
    CollectorConfig config_;           // 采集配置
    ContinuityMatrix dataMatrix_;      // 数据矩阵

    std::atomic<CollectionStatus> status_; // 采集状态
    std::atomic<uint8_t> currentCycle_;    // 当前周期
    std::atomic<bool> stopRequested_;      // 停止请求标志

    std::thread collectionThread_;      // 采集线程
    mutable std::mutex dataMutex_;      // 数据保护互斥锁
    ProgressCallback progressCallback_; // 进度回调

    // 私有方法
    void collectionWorker();                          // 采集工作线程
    void initializeGpioPins();                        // 初始化GPIO引脚
    void deinitializeGpioPins();                      // 反初始化GPIO引脚
    ContinuityState readPinContinuity(uint8_t pin);   // 读取单个引脚导通状态
    void configurePinsForCycle(uint8_t currentCycle); // 为当前周期配置引脚模式

  public:
    ContinuityCollector(std::unique_ptr<HAL::IGpio> gpio);
    ~ContinuityCollector();

    // 删除拷贝构造函数和赋值操作符
    ContinuityCollector(const ContinuityCollector &) = delete;
    ContinuityCollector &operator=(const ContinuityCollector &) = delete;

    // 配置采集参数
    bool configure(const CollectorConfig &config);

    // 开始采集
    bool startCollection();

    // 停止采集
    void stopCollection();

    // 获取采集状态
    CollectionStatus getStatus() const;

    // 获取当前周期
    uint8_t getCurrentCycle() const;

    // 获取总周期数
    uint8_t getTotalCycles() const;

    // 获取采集进度 (0-100)
    uint8_t getProgress() const;

    // 是否采集完成
    bool isCollectionComplete() const;

    // 获取数据矩阵（线程安全）
    ContinuityMatrix getDataMatrix() const;

    // 获取压缩数据向量（按位压缩，小端模式）
    std::vector<uint8_t> getDataVector() const;

    // 获取指定周期的数据行
    std::vector<ContinuityState> getCycleData(uint8_t cycle) const;

    // 获取指定引脚的所有周期数据
    std::vector<ContinuityState> getPinData(uint8_t pin) const;

    // 清空数据矩阵
    void clearData();

    // 设置进度回调函数
    void setProgressCallback(ProgressCallback callback);

    // 获取配置信息
    const CollectorConfig &getConfig() const;

    // 导出数据为字符串格式（用于调试和显示）
    std::string exportDataAsString() const;

    // 统计功能
    struct Statistics {
        uint32_t totalConnections;    // 总导通次数
        uint32_t totalDisconnections; // 总断开次数
        double connectionRate;        // 导通率
        uint8_t mostActivePins[5];    // 最活跃的5个引脚
    };

    Statistics calculateStatistics() const;

    // 测试功能 - 模拟特定模式的数据
    void simulateTestPattern(uint32_t pattern);
};

// 工厂类
class ContinuityCollectorFactory {
  public:
    // 创建带虚拟GPIO的采集器
    static std::unique_ptr<ContinuityCollector> createWithVirtualGpio();

    // 创建带自定义GPIO的采集器
    static std::unique_ptr<ContinuityCollector>
    createWithCustomGpio(std::unique_ptr<HAL::IGpio> gpio);
};

} // namespace Adapter

#endif // CONTINUITY_COLLECTOR_H
