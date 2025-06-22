#include "ContinuityCollector.h"
#include "GpioFactory.h"
#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>

namespace Adapter {

ContinuityCollector::ContinuityCollector(std::unique_ptr<IGpio> gpio)
    : gpio_(std::move(gpio)), status_(CollectionStatus::IDLE), currentCycle_(0),
      lastProcessTime_(0) {

    if (!gpio_) {
        status_ = CollectionStatus::ERROR;
    }
}

ContinuityCollector::~ContinuityCollector() {
    stopCollection();
    deinitializeGpioPins();
}

bool ContinuityCollector::configure(const CollectorConfig &config) {
    if (status_ == CollectionStatus::RUNNING) {
        return false; // 不能在运行时重新配置
    }

    if (config.num == 0 || config.num > MAX_GPIO_PINS || config.interval == 0) {
        return false;
    }

    if (config.totalDetectionNum == 0 ||
        config.totalDetectionNum > MAX_GPIO_PINS) {
        return false;
    }

    if (config.startDetectionNum >= config.totalDetectionNum) {
        return false;
    }

    config_ = config;

    // 重新初始化数据矩阵 - 基于总检测数量
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        dataMatrix_.clear();
        dataMatrix_.resize(config_.totalDetectionNum,
                           std::vector<ContinuityState>(
                               config_.num, ContinuityState::DISCONNECTED));
    }

    currentCycle_ = 0;
    status_ = CollectionStatus::IDLE;

    return true;
}

bool ContinuityCollector::startCollection() {
    if (!gpio_ || status_ == CollectionStatus::RUNNING) {
        return false;
    }

    if (config_.num == 0) {
        return false;
    }

    // 停止之前的采集
    stopCollection();

    // 初始化GPIO引脚
    initializeGpioPins();

    // 重置状态
    currentCycle_ = 0;
    status_ = CollectionStatus::RUNNING;
    lastProcessTime_ = getCurrentTimeMs();

    return true;
}

void ContinuityCollector::stopCollection() {
    if (status_ == CollectionStatus::RUNNING) {
        status_ = CollectionStatus::IDLE;
    }
}

uint32_t ContinuityCollector::getCurrentTimeMs() {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// 状态机处理方法
void ContinuityCollector::processCollection() {
    // 只处理RUNNING状态
    if (status_ != CollectionStatus::RUNNING) {
        return;
    }

    // 检查是否已完成所有周期
    if (currentCycle_ >= config_.totalDetectionNum) {
        status_ = CollectionStatus::COMPLETED;
        return;
    }

    // 检查是否到了下一个采集周期的时间
    uint32_t currentTime = getCurrentTimeMs();
    uint32_t elapsedTime = currentTime - lastProcessTime_;

    if (elapsedTime >= config_.interval || currentCycle_ == 0) {
        // 为当前周期配置GPIO引脚模式
        configurePinsForCycle(currentCycle_);

        // 读取当前周期的所有引脚状态
        std::vector<ContinuityState> cycleData;
        cycleData.reserve(config_.num);

        for (uint8_t pin = 0; pin < config_.num; pin++) {
            cycleData.push_back(readPinContinuity(pin));
        }

        // 保存数据到矩阵
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            if (currentCycle_ < dataMatrix_.size()) {
                dataMatrix_[currentCycle_] = std::move(cycleData);
            }
        }

        // 调用进度回调
        if (progressCallback_) {
            progressCallback_(currentCycle_ + 1, config_.totalDetectionNum);
        }

        // 更新时间和周期
        lastProcessTime_ = currentTime;
        currentCycle_++;

        // 检查是否完成
        if (currentCycle_ >= config_.totalDetectionNum) {
            status_ = CollectionStatus::COMPLETED;
        }
    }
}

CollectionStatus ContinuityCollector::getStatus() const { return status_; }

uint8_t ContinuityCollector::getCurrentCycle() const { return currentCycle_; }

uint8_t ContinuityCollector::getTotalCycles() const {
    return config_.totalDetectionNum;
}

float ContinuityCollector::getProgress() const {
    uint8_t current = getCurrentCycle();
    uint8_t total = getTotalCycles();
    if (total == 0)
        return 0.0f;
    return static_cast<float>(current * 100) / total;
}

bool ContinuityCollector::hasNewData() const {
    return status_ == CollectionStatus::COMPLETED;
}

bool ContinuityCollector::isCollectionComplete() const {
    return status_ == CollectionStatus::COMPLETED;
}

ContinuityMatrix ContinuityCollector::getDataMatrix() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return dataMatrix_;
}

std::vector<uint8_t> ContinuityCollector::getDataVector() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    std::vector<uint8_t> compressedData;

    // 计算总位数
    size_t totalBits = dataMatrix_.size() * config_.num;
    size_t totalBytes = (totalBits + 7) / 8; // 向上取整
    compressedData.reserve(totalBytes);

    uint8_t currentByte = 0;
    uint8_t bitPosition = 0;

    // 按行遍历矩阵，将每个状态转换为位
    for (const auto &row : dataMatrix_) {
        for (size_t pin = 0; pin < config_.num && pin < row.size(); pin++) {
            // 将导通状态转换为位值
            uint8_t bitValue = (row[pin] == ContinuityState::CONNECTED) ? 1 : 0;

            // 按小端模式设置位（低位在前）
            currentByte |= (bitValue << bitPosition);
            bitPosition++;

            // 如果累积了8位，保存字节并重置
            if (bitPosition == 8) {
                compressedData.push_back(currentByte);
                currentByte = 0;
                bitPosition = 0;
            }
        }
    }

    // 如果还有未满8位的数据，补零并保存
    if (bitPosition > 0) {
        compressedData.push_back(currentByte);
    }

    return compressedData;
}

std::vector<ContinuityState>
ContinuityCollector::getCycleData(uint8_t cycle) const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    if (cycle < dataMatrix_.size()) {
        return dataMatrix_[cycle];
    }
    return {};
}

std::vector<ContinuityState>
ContinuityCollector::getPinData(uint8_t pin) const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    std::vector<ContinuityState> result;

    if (pin < config_.num) {
        result.reserve(dataMatrix_.size());
        for (const auto &row : dataMatrix_) {
            if (pin < row.size()) {
                result.push_back(row[pin]);
            }
        }
    }

    return result;
}

void ContinuityCollector::clearData() {
    std::lock_guard<std::mutex> lock(dataMutex_);
    for (auto &row : dataMatrix_) {
        std::fill(row.begin(), row.end(), ContinuityState::DISCONNECTED);
    }
    currentCycle_ = 0;
}

void ContinuityCollector::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = callback;
}

std::string ContinuityCollector::exportDataAsString() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    std::ostringstream oss;

    oss << "Continuity Data Matrix (" << config_.totalDetectionNum << "x"
        << config_.num << "):\n";
    oss << "Detection Pins: " << static_cast<int>(config_.num) << "\n";
    oss << "Start Detection: " << static_cast<int>(config_.startDetectionNum)
        << "\n";
    oss << "Total Detection: " << static_cast<int>(config_.totalDetectionNum)
        << "\n";
    oss << "Interval: " << config_.interval << "ms\n\n";

    // 表头
    oss << "Cycle\\Pin ";
    for (uint8_t i = 0; i < config_.num; i++) {
        oss << std::setw(3) << static_cast<int>(i) << " ";
    }
    oss << "\n";

    // 数据行
    for (uint8_t cycle = 0; cycle < dataMatrix_.size(); cycle++) {
        oss << std::setw(9) << static_cast<int>(cycle) << " ";
        for (uint8_t pin = 0; pin < dataMatrix_[cycle].size(); pin++) {
            char symbol =
                (dataMatrix_[cycle][pin] == ContinuityState::CONNECTED) ? '1'
                                                                        : '0';
            oss << std::setw(3) << symbol << " ";
        }
        oss << "\n";
    }

    return oss.str();
}

ContinuityCollector::Statistics
ContinuityCollector::calculateStatistics() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    Statistics stats = {};

    uint32_t totalConnections = 0;
    uint32_t totalReadings = 0;
    std::map<uint8_t, uint32_t> pinActivity;

    // 统计数据
    for (const auto &row : dataMatrix_) {
        for (uint8_t pin = 0; pin < row.size(); pin++) {
            totalReadings++;
            if (row[pin] == ContinuityState::CONNECTED) {
                totalConnections++;
                pinActivity[pin]++;
            }
        }
    }

    stats.totalConnections = totalConnections;
    stats.totalDisconnections = totalReadings - totalConnections;
    stats.connectionRate =
        totalReadings > 0
            ? static_cast<double>(totalConnections) / totalReadings * 100.0
            : 0.0;

    // 找出最活跃的引脚
    std::vector<std::pair<uint8_t, uint32_t>> sortedPins;
    for (const auto &pair : pinActivity) {
        sortedPins.push_back(pair);
    }

    std::sort(sortedPins.begin(), sortedPins.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    for (size_t i = 0; i < 5 && i < sortedPins.size(); i++) {
        stats.mostActivePins[i] = sortedPins[i].first;
    }

    return stats;
}

void ContinuityCollector::simulateTestPattern(uint32_t pattern) {
    if (gpio_) {
        // 将模式传递给虚拟GPIO以模拟特定的测试环境
        auto *virtualGpio =
            dynamic_cast<Platform::Windows::VirtualGpio *>(gpio_.get());
        if (virtualGpio) {
            virtualGpio->simulateContinuityPattern(config_.num, pattern);
        }
    }
}

void ContinuityCollector::initializeGpioPins() {
    if (!gpio_)
        return;

    // 初始化所有需要的GPIO引脚为输入模式
    for (uint8_t pin = 0; pin < config_.num; pin++) {
        GpioConfig gpioConfig(pin, GpioMode::INPUT_PULLDOWN);
        gpio_->init(gpioConfig);
    }
}

void ContinuityCollector::deinitializeGpioPins() {
    if (!gpio_)
        return;

    // 反初始化所有GPIO引脚
    for (uint8_t pin = 0; pin < config_.num; pin++) {
        gpio_->deinit(pin);
    }
}

ContinuityState ContinuityCollector::readPinContinuity(uint8_t pin) {
    if (!gpio_ || pin >= config_.num) {
        return ContinuityState::DISCONNECTED;
    }

    GpioState gpioState = gpio_->read(pin);

    // 高电平表示导通，低电平表示断开
    return (gpioState == GpioState::HIGH) ? ContinuityState::CONNECTED
                                          : ContinuityState::DISCONNECTED;
}

void ContinuityCollector::configurePinsForCycle(uint8_t currentCycle) {
    if (!gpio_) {
        return;
    }

    // 检测逻辑：
    // 当 startDetectionNum <= currentCycle < startDetectionNum + num 时
    // 将对应的引脚设置为高电平输出，其余为输入模式

    if (currentCycle >= config_.startDetectionNum &&
        currentCycle < config_.startDetectionNum + config_.num) {

        // 计算当前应该输出高电平的引脚
        uint8_t activePin = currentCycle - config_.startDetectionNum;

        // 配置所有引脚
        for (uint8_t pin = 0; pin < config_.num; pin++) {
            if (pin == activePin) {
                // 设置为输出模式并输出高电平
                GpioConfig gpioConfig(pin, GpioMode::OUTPUT, GpioState::HIGH);
                gpio_->init(gpioConfig);
                gpio_->write(pin, GpioState::HIGH);
            } else {
                // 设置为输入下拉模式
                GpioConfig gpioConfig(pin, GpioMode::INPUT_PULLDOWN);
                gpio_->init(gpioConfig);
            }
        }
    } else {
        // 在检测范围外，所有引脚都设置为输入下拉模式
        for (uint8_t pin = 0; pin < config_.num; pin++) {
            GpioConfig gpioConfig(pin, GpioMode::INPUT_PULLDOWN);
            gpio_->init(gpioConfig);
        }
    }
}

// 工厂类实现
std::unique_ptr<ContinuityCollector>
ContinuityCollectorFactory::createWithVirtualGpio() {
    // 使用统一接口，平台由CMake配置决定
    auto gpio = Adapter::GpioFactory::createGpio();
    return std::make_unique<ContinuityCollector>(std::move(gpio));
}

std::unique_ptr<ContinuityCollector>
ContinuityCollectorFactory::createWithCustomGpio(std::unique_ptr<IGpio> gpio) {
    return std::make_unique<ContinuityCollector>(std::move(gpio));
}

} // namespace Adapter