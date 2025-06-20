#include "DeviceManager.h"
#include "../Logger.h"

DeviceManager::DeviceManager()
    : currentMode(0), systemRunningStatus(0), dataCollectionActive(false),
      cycleState(CollectionCycleState::IDLE), cycleStartTime(0),
      lastCycleTime(0), cycleInterval(5000), syncSent(false) {}

void DeviceManager::addSlave(uint32_t slaveId, uint8_t shortId) {
    connectedSlaves[slaveId] = true;
    if (shortId > 0) {
        slaveShortIds[slaveId] = shortId;
    }
}

void DeviceManager::removeSlave(uint32_t slaveId) {
    connectedSlaves[slaveId] = false;
}

bool DeviceManager::isSlaveConnected(uint32_t slaveId) const {
    auto it = connectedSlaves.find(slaveId);
    return it != connectedSlaves.end() && it->second;
}

std::vector<uint32_t> DeviceManager::getConnectedSlaves() const {
    std::vector<uint32_t> result;
    for (const auto &pair : connectedSlaves) {
        if (pair.second) {
            result.push_back(pair.first);
        }
    }
    return result;
}

uint8_t DeviceManager::getSlaveShortId(uint32_t slaveId) const {
    auto it = slaveShortIds.find(slaveId);
    return it != slaveShortIds.end() ? it->second : 0;
}

// Configuration management
void DeviceManager::setSlaveConfig(
    uint32_t slaveId,
    const Backend2Master::SlaveConfigMessage::SlaveInfo &config) {
    slaveConfigs[slaveId] = config;
}

Backend2Master::SlaveConfigMessage::SlaveInfo
DeviceManager::getSlaveConfig(uint32_t slaveId) const {
    auto it = slaveConfigs.find(slaveId);
    return it != slaveConfigs.end()
               ? it->second
               : Backend2Master::SlaveConfigMessage::SlaveInfo{};
}

bool DeviceManager::hasSlaveConfig(uint32_t slaveId) const {
    return slaveConfigs.find(slaveId) != slaveConfigs.end();
}

// Mode management
void DeviceManager::setCurrentMode(uint8_t mode) { currentMode = mode; }
uint8_t DeviceManager::getCurrentMode() const { return currentMode; }

// System status management
void DeviceManager::setSystemRunningStatus(uint8_t status) {
    systemRunningStatus = status;
}
uint8_t DeviceManager::getSystemRunningStatus() const {
    return systemRunningStatus;
}

// 数据采集管理
void DeviceManager::startDataCollection() {
    activeCollections.clear();

    // 为每个已配置的从机创建采集信息
    for (const auto &pair : slaveConfigs) {
        if (isSlaveConnected(pair.first)) {
            uint32_t slaveId = pair.first;
            const auto &config = pair.second;

            uint32_t duration = 0;

            // 根据当前模式选择配置参数和估算采集时间
            switch (currentMode) {
            case 0: // Conduction模式
                duration =
                    config.conductionNum * 100 + 500; // 添加额外的500ms缓冲
                break;
            case 1: // Resistance模式
                duration = config.resistanceNum * 100 + 500;
                break;
            case 2:              // Clip模式
                duration = 1000; // Clip模式估计1秒钟完成
                break;
            }

            activeCollections.emplace_back(slaveId, duration);
        }
    }

    dataCollectionActive = !activeCollections.empty();
    cycleState = CollectionCycleState::IDLE;
    syncSent = false;
    lastCycleTime = 0; // 重置上次周期完成时间

    Log::i("DeviceManager",
           "Data collection started, mode: %d, active slaves: %zu", currentMode,
           activeCollections.size());
}

void DeviceManager::resetDataCollection() {
    activeCollections.clear();
    dataCollectionActive = false;
    cycleState = CollectionCycleState::IDLE;
    syncSent = false;

    Log::i("DeviceManager", "Data collection reset");
}

// 开始一个新的采集周期
void DeviceManager::startNewCycle(uint32_t currentTime) {
    cycleState = CollectionCycleState::COLLECTING;
    cycleStartTime = currentTime;
    syncSent = false;

    // 重置所有从机的采集状态
    for (auto &collection : activeCollections) {
        collection.startTimestamp = 0;
        collection.dataRequested = false;
        collection.dataReceived = false;
    }

    Log::i("DeviceManager", "Starting new collection cycle at time %u",
           currentTime);
}

// 标记同步消息已发送
void DeviceManager::markSyncSent(uint32_t timestamp) {
    syncSent = true;
    for (auto &collection : activeCollections) {
        collection.startTimestamp = timestamp;
        collection.dataRequested = false;
        collection.dataReceived = false;
    }
    Log::i("DeviceManager", "Sync message sent at time %u", timestamp);
}

// 是否应该进入数据读取阶段
bool DeviceManager::shouldEnterReadingPhase(uint32_t currentTime) {
    if (cycleState != CollectionCycleState::COLLECTING || !syncSent) {
        return false;
    }

    // 检查所有从机是否已完成采集
    bool allComplete = true;
    for (const auto &collection : activeCollections) {
        if (!collection.isCollectionComplete(currentTime)) {
            allComplete = false;
            break;
        }
    }

    return allComplete;
}

// 进入数据读取阶段
void DeviceManager::enterReadingPhase() {
    cycleState = CollectionCycleState::READING_DATA;
    Log::i("DeviceManager", "Entering data reading phase");
}

// 标记开始同步采集 (旧方法，为保持兼容)
void DeviceManager::markCollectionStarted(uint32_t timestamp) {
    markSyncSent(timestamp);
}

// 检查某个从机的采集是否完成
bool DeviceManager::isSlaveCollectionComplete(uint32_t slaveId,
                                              uint32_t currentTime) {
    for (const auto &collection : activeCollections) {
        if (collection.slaveId == slaveId) {
            return collection.isCollectionComplete(currentTime);
        }
    }
    return false;
}

// 标记数据已请求
void DeviceManager::markDataRequested(uint32_t slaveId) {
    for (auto &collection : activeCollections) {
        if (collection.slaveId == slaveId) {
            collection.dataRequested = true;
            break;
        }
    }
}

// 标记数据已接收
void DeviceManager::markDataReceived(uint32_t slaveId) {
    for (auto &collection : activeCollections) {
        if (collection.slaveId == slaveId) {
            collection.dataReceived = true;
            break;
        }
    }

    // 检查是否所有数据都已接收，如果是则完成本周期
    if (isAllDataReceived()) {
        cycleState = CollectionCycleState::COMPLETE;
        uint32_t currentTime = getCurrentTimestampMs();
        lastCycleTime = currentTime;
        Log::i("DeviceManager", "Collection cycle completed at time %u",
               lastCycleTime);
    }
}

// 获取所有可以请求数据的从机列表 (旧方法，为保持兼容)
std::vector<uint32_t>
DeviceManager::getSlavesReadyForDataRequest(uint32_t currentTime) {
    return getSlavesForDataRequest();
}

// 获取所有未请求数据的从机列表
std::vector<uint32_t> DeviceManager::getSlavesForDataRequest() {
    std::vector<uint32_t> readySlaves;

    if (cycleState != CollectionCycleState::READING_DATA) {
        return readySlaves;
    }

    for (const auto &collection : activeCollections) {
        if (!collection.dataRequested) {
            readySlaves.push_back(collection.slaveId);
        }
    }

    return readySlaves;
}

// 检查所有从机是否都已接收数据
bool DeviceManager::isAllDataReceived() {
    if (activeCollections.empty()) {
        return false;
    }

    for (const auto &collection : activeCollections) {
        if (!collection.dataReceived) {
            return false;
        }
    }

    return true;
}

// 检查是否应该开始新的采集周期
bool DeviceManager::shouldStartNewCycle(uint32_t currentTime) {
    // 如果没有处于运行状态，则不开始新周期
    if (systemRunningStatus != 1) {
        return false;
    }

    // 如果当前正在采集中，则不开始新周期
    if (cycleState != CollectionCycleState::IDLE &&
        cycleState != CollectionCycleState::COMPLETE) {
        return false;
    }

    // 如果是首次采集或者距离上次采集完成已经超过了周期间隔
    return lastCycleTime == 0 || (currentTime - lastCycleTime >= cycleInterval);
}

// 获取当前采集周期状态
CollectionCycleState DeviceManager::getCycleState() const { return cycleState; }

// 是否已发送同步消息
bool DeviceManager::isSyncSent() const { return syncSent; }

// 设置采集周期间隔
void DeviceManager::setCycleInterval(uint32_t interval) {
    cycleInterval = interval;
    Log::i("DeviceManager", "Set cycle interval to %u ms", interval);
}

// 获取采集周期间隔
uint32_t DeviceManager::getCycleInterval() const { return cycleInterval; }

// 是否有活跃的数据采集
bool DeviceManager::isDataCollectionActive() const {
    return dataCollectionActive;
}