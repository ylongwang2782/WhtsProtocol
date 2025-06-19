#pragma once

#include "WhtsProtocol.h"
#include <chrono>
#include <unordered_map>
#include <vector>

using namespace WhtsProtocol;

// 采集周期状态枚举
enum class CollectionCycleState {
    IDLE,         // 空闲状态
    COLLECTING,   // 正在采集
    READING_DATA, // 正在读取数据
    COMPLETE      // 完成一个周期
};

// Data Collection Management structure
struct DataCollectionInfo {
    uint32_t slaveId;           // 从机ID
    uint32_t startTimestamp;    // 开始采集时间戳
    uint32_t estimatedDuration; // 估计采集时长(毫秒)
    bool dataRequested;         // 是否已发送数据请求
    bool dataReceived;          // 是否已接收数据

    DataCollectionInfo(uint32_t id, uint32_t duration)
        : slaveId(id), startTimestamp(0), estimatedDuration(duration),
          dataRequested(false), dataReceived(false) {}

    // 计算是否采集完成
    bool isCollectionComplete(uint32_t currentTime) const {
        return startTimestamp > 0 &&
               (currentTime - startTimestamp >= estimatedDuration);
    }
};

// 获取当前时间戳（毫秒）
inline uint32_t getCurrentTimestampMs() {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// Device management for tracking connected slaves
class DeviceManager {
  private:
    std::unordered_map<uint32_t, bool> connectedSlaves;
    std::unordered_map<uint32_t, uint8_t> slaveShortIds;
    std::unordered_map<uint32_t, Backend2Master::SlaveConfigMessage::SlaveInfo>
        slaveConfigs;
    uint8_t currentMode;         // 0=Conduction, 1=Resistance, 2=Clip
    uint8_t systemRunningStatus; // 0=Stop, 1=Run, 2=Reset

    // 数据采集管理
    std::vector<DataCollectionInfo> activeCollections;
    bool dataCollectionActive;
    CollectionCycleState cycleState; // 当前采集周期状态
    uint32_t cycleStartTime;         // 周期开始时间
    uint32_t lastCycleTime;          // 上次完成周期的时间
    uint32_t cycleInterval;          // 采集周期间隔(毫秒)
    bool syncSent;                   // 是否已发送同步消息

  public:
    DeviceManager();

    void addSlave(uint32_t slaveId, uint8_t shortId = 0);
    void removeSlave(uint32_t slaveId);
    bool isSlaveConnected(uint32_t slaveId) const;
    std::vector<uint32_t> getConnectedSlaves() const;
    uint8_t getSlaveShortId(uint32_t slaveId) const;

    // Configuration management
    void
    setSlaveConfig(uint32_t slaveId,
                   const Backend2Master::SlaveConfigMessage::SlaveInfo &config);
    Backend2Master::SlaveConfigMessage::SlaveInfo
    getSlaveConfig(uint32_t slaveId) const;
    bool hasSlaveConfig(uint32_t slaveId) const;

    // Mode management
    void setCurrentMode(uint8_t mode);
    uint8_t getCurrentMode() const;

    // System status management
    void setSystemRunningStatus(uint8_t status);
    uint8_t getSystemRunningStatus() const;

    // 数据采集管理
    void startDataCollection();
    void resetDataCollection();
    void startNewCycle(uint32_t currentTime);
    void markSyncSent(uint32_t timestamp);
    bool shouldEnterReadingPhase(uint32_t currentTime);
    void enterReadingPhase();
    void markCollectionStarted(uint32_t timestamp);
    bool isSlaveCollectionComplete(uint32_t slaveId, uint32_t currentTime);
    void markDataRequested(uint32_t slaveId);
    void markDataReceived(uint32_t slaveId);
    std::vector<uint32_t> getSlavesReadyForDataRequest(uint32_t currentTime);
    std::vector<uint32_t> getSlavesForDataRequest();
    bool isAllDataReceived();
    bool shouldStartNewCycle(uint32_t currentTime);
    CollectionCycleState getCycleState() const;
    bool isSyncSent() const;
    void setCycleInterval(uint32_t interval);
    uint32_t getCycleInterval() const;
    bool isDataCollectionActive() const;
};