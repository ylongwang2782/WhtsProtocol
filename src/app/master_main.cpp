#include "ContinuityCollector.h"
#include "Logger.h"
#include "WhtsProtocol.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

using namespace WhtsProtocol;

// Forward declarations
class MasterServer;

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

// 采集周期状态枚举
enum class CollectionCycleState {
    IDLE,         // 空闲状态
    COLLECTING,   // 正在采集
    READING_DATA, // 正在读取数据
    COMPLETE      // 完成一个周期
};

// 获取当前时间戳（毫秒）
inline uint32_t getCurrentTimestampMs() {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// Command tracking for timeout and retry management
struct PendingCommand {
    uint32_t slaveId;
    std::unique_ptr<Message> command;
    sockaddr_in clientAddr;
    uint32_t timestamp;
    uint8_t retryCount;
    uint8_t maxRetries;

    PendingCommand(uint32_t id, std::unique_ptr<Message> cmd,
                   const sockaddr_in &addr, uint8_t maxRetry = 3)
        : slaveId(id), command(std::move(cmd)), clientAddr(addr), timestamp(0),
          retryCount(0), maxRetries(maxRetry) {}
};

// Ping session tracking
struct PingSession {
    uint32_t targetId;
    uint8_t pingMode;
    uint16_t totalCount;
    uint16_t currentCount;
    uint16_t successCount;
    uint16_t interval;
    uint32_t lastPingTime;
    sockaddr_in clientAddr;

    PingSession(uint32_t target, uint8_t mode, uint16_t total,
                uint16_t intervalMs, const sockaddr_in &addr)
        : targetId(target), pingMode(mode), totalCount(total), currentCount(0),
          successCount(0), interval(intervalMs), lastPingTime(0),
          clientAddr(addr) {}
};

// Message handler interface for extensible message processing
class IMessageHandler {
  public:
    virtual ~IMessageHandler() = default;
    virtual std::unique_ptr<Message> processMessage(const Message &message,
                                                    MasterServer *server) = 0;
    virtual void executeActions(const Message &message,
                                MasterServer *server) = 0;
};

// Action result structure
struct ActionResult {
    bool success;
    std::string errorMessage;
    std::vector<uint32_t> affectedSlaves;

    ActionResult(bool s = true, const std::string &err = "")
        : success(s), errorMessage(err) {}
};

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
    DeviceManager()
        : currentMode(0), systemRunningStatus(0), dataCollectionActive(false),
          cycleState(CollectionCycleState::IDLE), cycleStartTime(0),
          lastCycleTime(0), cycleInterval(5000), syncSent(false) {}

    void addSlave(uint32_t slaveId, uint8_t shortId = 0) {
        connectedSlaves[slaveId] = true;
        if (shortId > 0) {
            slaveShortIds[slaveId] = shortId;
        }
    }

    void removeSlave(uint32_t slaveId) { connectedSlaves[slaveId] = false; }

    bool isSlaveConnected(uint32_t slaveId) const {
        auto it = connectedSlaves.find(slaveId);
        return it != connectedSlaves.end() && it->second;
    }

    std::vector<uint32_t> getConnectedSlaves() const {
        std::vector<uint32_t> result;
        for (const auto &pair : connectedSlaves) {
            if (pair.second) {
                result.push_back(pair.first);
            }
        }
        return result;
    }

    uint8_t getSlaveShortId(uint32_t slaveId) const {
        auto it = slaveShortIds.find(slaveId);
        return it != slaveShortIds.end() ? it->second : 0;
    }

    // Configuration management
    void setSlaveConfig(
        uint32_t slaveId,
        const Backend2Master::SlaveConfigMessage::SlaveInfo &config) {
        slaveConfigs[slaveId] = config;
    }

    Backend2Master::SlaveConfigMessage::SlaveInfo
    getSlaveConfig(uint32_t slaveId) const {
        auto it = slaveConfigs.find(slaveId);
        return it != slaveConfigs.end()
                   ? it->second
                   : Backend2Master::SlaveConfigMessage::SlaveInfo{};
    }

    bool hasSlaveConfig(uint32_t slaveId) const {
        return slaveConfigs.find(slaveId) != slaveConfigs.end();
    }

    // Mode management
    void setCurrentMode(uint8_t mode) { currentMode = mode; }
    uint8_t getCurrentMode() const { return currentMode; }

    // System status management
    void setSystemRunningStatus(uint8_t status) {
        systemRunningStatus = status;
    }
    uint8_t getSystemRunningStatus() const { return systemRunningStatus; }

    // 数据采集管理
    void startDataCollection() {
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
               "Data collection started, mode: %d, active slaves: %zu",
               currentMode, activeCollections.size());
    }

    void resetDataCollection() {
        activeCollections.clear();
        dataCollectionActive = false;
        cycleState = CollectionCycleState::IDLE;
        syncSent = false;

        Log::i("DeviceManager", "Data collection reset");
    }

    // 开始一个新的采集周期
    void startNewCycle(uint32_t currentTime) {
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
    void markSyncSent(uint32_t timestamp) {
        syncSent = true;
        for (auto &collection : activeCollections) {
            collection.startTimestamp = timestamp;
            collection.dataRequested = false;
            collection.dataReceived = false;
        }
        Log::i("DeviceManager", "Sync message sent at time %u", timestamp);
    }

    // 是否应该进入数据读取阶段
    bool shouldEnterReadingPhase(uint32_t currentTime) {
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
    void enterReadingPhase() {
        cycleState = CollectionCycleState::READING_DATA;
        Log::i("DeviceManager", "Entering data reading phase");
    }

    // 标记开始同步采集 (旧方法，为保持兼容)
    void markCollectionStarted(uint32_t timestamp) { markSyncSent(timestamp); }

    // 检查某个从机的采集是否完成
    bool isSlaveCollectionComplete(uint32_t slaveId, uint32_t currentTime) {
        for (const auto &collection : activeCollections) {
            if (collection.slaveId == slaveId) {
                return collection.isCollectionComplete(currentTime);
            }
        }
        return false;
    }

    // 标记数据已请求
    void markDataRequested(uint32_t slaveId) {
        for (auto &collection : activeCollections) {
            if (collection.slaveId == slaveId) {
                collection.dataRequested = true;
                break;
            }
        }
    }

    // 标记数据已接收
    void markDataReceived(uint32_t slaveId) {
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
    std::vector<uint32_t> getSlavesReadyForDataRequest(uint32_t currentTime) {
        return getSlavesForDataRequest();
    }

    // 获取所有未请求数据的从机列表
    std::vector<uint32_t> getSlavesForDataRequest() {
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
    bool isAllDataReceived() {
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
    bool shouldStartNewCycle(uint32_t currentTime) {
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
        return lastCycleTime == 0 ||
               (currentTime - lastCycleTime >= cycleInterval);
    }

    // 获取当前采集周期状态
    CollectionCycleState getCycleState() const { return cycleState; }

    // 是否已发送同步消息
    bool isSyncSent() const { return syncSent; }

    // 设置采集周期间隔
    void setCycleInterval(uint32_t interval) {
        cycleInterval = interval;
        Log::i("DeviceManager", "Set cycle interval to %u ms", interval);
    }

    // 获取采集周期间隔
    uint32_t getCycleInterval() const { return cycleInterval; }

    // 是否有活跃的数据采集
    bool isDataCollectionActive() const { return dataCollectionActive; }
};

class MasterServer {
  private:
    SOCKET sock;
    sockaddr_in serverAddr;
    sockaddr_in backendAddr;        // Backend address (port 8079)
    sockaddr_in slaveBroadcastAddr; // Slave broadcast address (port 8081)
    ProtocolProcessor processor;
    uint16_t port;
    DeviceManager deviceManager;
    std::unordered_map<uint8_t, std::unique_ptr<IMessageHandler>>
        messageHandlers;
    std::vector<PendingCommand> pendingCommands;
    std::vector<PingSession> activePingSessions;

  public:
    MasterServer(uint16_t listenPort = 8080);
    ~MasterServer();

    // Utility methods
    static std::vector<uint8_t> hexStringToBytes(const std::string &hex);
    static uint32_t getCurrentTimestamp();
    void printBytes(const std::vector<uint8_t> &data,
                    const std::string &description);
    std::string bytesToHexString(const std::vector<uint8_t> &bytes);

    // Core processing methods
    void processBackend2MasterMessage(const Message &message,
                                      const sockaddr_in &clientAddr);
    void processSlave2MasterMessage(uint32_t slaveId, const Message &message,
                                    const sockaddr_in &clientAddr);
    void processFrame(Frame &frame, const sockaddr_in &clientAddr);
    void run();

    // Message sending methods
    void sendResponseToBackend(std::unique_ptr<Message> response,
                               const sockaddr_in &clientAddr);
    void sendCommandToSlave(uint32_t slaveId, std::unique_ptr<Message> command,
                            const sockaddr_in &clientAddr);
    void sendCommandToSlaveWithRetry(uint32_t slaveId,
                                     std::unique_ptr<Message> command,
                                     const sockaddr_in &clientAddr,
                                     uint8_t maxRetries = 3);

    // Command management
    void processPendingCommands();
    void addPingSession(uint32_t targetId, uint8_t pingMode,
                        uint16_t totalCount, uint16_t interval,
                        const sockaddr_in &clientAddr);
    void processPingSessions();

    // 数据采集管理
    void processDataCollection();

    // Device management
    DeviceManager &getDeviceManager() { return deviceManager; }
    ProtocolProcessor &getProcessor() { return processor; }
    SOCKET getSocket() { return sock; }

    // Register message handlers
    void registerMessageHandler(uint8_t messageId,
                                std::unique_ptr<IMessageHandler> handler);

  private:
    void initializeMessageHandlers();
};

// Slave Configuration Message Handler
class SlaveConfigHandler : public IMessageHandler {
  public:
    std::unique_ptr<Message> processMessage(const Message &message,
                                            MasterServer *server) override {
        const auto *configMsg =
            dynamic_cast<const Backend2Master::SlaveConfigMessage *>(&message);
        if (!configMsg)
            return nullptr;

        Log::i("SlaveConfigHandler",
               "Processing slave configuration - Slave count: %d",
               static_cast<int>(configMsg->slaveNum));

        for (const auto &slave : configMsg->slaves) {
            Log::i("SlaveConfigHandler",
                   "  Slave ID: 0x%08X, Conduction: %d, Resistance: %d, Clip "
                   "mode: %d",
                   slave.id, static_cast<int>(slave.conductionNum),
                   static_cast<int>(slave.resistanceNum),
                   static_cast<int>(slave.clipMode));
        }

        auto response =
            std::make_unique<Master2Backend::SlaveConfigResponseMessage>();
        response->status = 0; // Success
        response->slaveNum = configMsg->slaveNum;

        // Copy slaves info
        for (const auto &slave : configMsg->slaves) {
            Master2Backend::SlaveConfigResponseMessage::SlaveInfo slaveInfo;
            slaveInfo.id = slave.id;
            slaveInfo.conductionNum = slave.conductionNum;
            slaveInfo.resistanceNum = slave.resistanceNum;
            slaveInfo.clipMode = slave.clipMode;
            slaveInfo.clipStatus = slave.clipStatus;
            response->slaves.push_back(slaveInfo);
        }

        return std::move(response);
    }

    void executeActions(const Message &message, MasterServer *server) override {
        const auto *configMsg =
            dynamic_cast<const Backend2Master::SlaveConfigMessage *>(&message);
        if (!configMsg)
            return;

        // Store slave configurations in device manager
        for (const auto &slave : configMsg->slaves) {
            server->getDeviceManager().addSlave(slave.id);
            server->getDeviceManager().setSlaveConfig(slave.id, slave);
            Log::i("SlaveConfigHandler",
                   "Stored config for slave 0x%08X: Conduction=%d, "
                   "Resistance=%d, ClipMode=%d",
                   slave.id, static_cast<int>(slave.conductionNum),
                   static_cast<int>(slave.resistanceNum),
                   static_cast<int>(slave.clipMode));
        }

        Log::i("SlaveConfigHandler",
               "Configuration actions executed for %d slaves",
               static_cast<int>(configMsg->slaveNum));
    }
};

// Mode Configuration Message Handler
class ModeConfigHandler : public IMessageHandler {
  public:
    std::unique_ptr<Message> processMessage(const Message &message,
                                            MasterServer *server) override {
        const auto *modeMsg =
            dynamic_cast<const Backend2Master::ModeConfigMessage *>(&message);
        if (!modeMsg)
            return nullptr;

        Log::i("ModeConfigHandler", "Processing mode configuration - Mode: %d",
               static_cast<int>(modeMsg->mode));

        auto response =

            std::make_unique<Master2Backend::ModeConfigResponseMessage>();
        response->status = 0; // Success
        response->mode = modeMsg->mode;

        return std::move(response);
    }

    void executeActions(const Message &message, MasterServer *server) override {
        const auto *modeMsg =
            dynamic_cast<const Backend2Master::ModeConfigMessage *>(&message);
        if (!modeMsg)
            return;

        // Store mode information
        server->getDeviceManager().setCurrentMode(modeMsg->mode);

        // Send configuration commands to slaves based on mode and stored slave
        // configs
        auto connectedSlaves = server->getDeviceManager().getConnectedSlaves();
        for (uint32_t slaveId : connectedSlaves) {
            if (server->getDeviceManager().hasSlaveConfig(slaveId)) {
                auto slaveConfig =
                    server->getDeviceManager().getSlaveConfig(slaveId);

                // Send different configuration messages based on mode
                switch (modeMsg->mode) {
                case 0: // Conduction mode
                    if (slaveConfig.conductionNum > 0) {
                        auto condCmd = std::make_unique<
                            Master2Slave::ConductionConfigMessage>();
                        condCmd->timeSlot = 1;
                        condCmd->interval = 100; // 100ms default
                        condCmd->totalConductionNum = slaveConfig.conductionNum;
                        condCmd->startConductionNum = 0;
                        condCmd->conductionNum = slaveConfig.conductionNum;

                        // Use retry mechanism for important configuration
                        // commands
                        server->sendCommandToSlaveWithRetry(
                            slaveId, std::move(condCmd), sockaddr_in{}, 3);
                        Log::i("ModeConfigHandler",
                               "Sent conduction config to slave 0x%08X",
                               slaveId);
                    }
                    break;

                case 1: // Resistance mode
                    if (slaveConfig.resistanceNum > 0) {
                        auto resCmd = std::make_unique<
                            Master2Slave::ResistanceConfigMessage>();
                        resCmd->timeSlot = 1;
                        resCmd->interval = 100; // 100ms default
                        resCmd->totalNum = slaveConfig.resistanceNum;
                        resCmd->startNum = 0;
                        resCmd->num = slaveConfig.resistanceNum;

                        server->sendCommandToSlaveWithRetry(
                            slaveId, std::move(resCmd), sockaddr_in{}, 3);
                        Log::i("ModeConfigHandler",
                               "Sent resistance config to slave 0x%08X",
                               slaveId);
                    }
                    break;

                case 2: // Clip mode
                {
                    auto clipCmd =
                        std::make_unique<Master2Slave::ClipConfigMessage>();
                    clipCmd->interval = 100; // 100ms default
                    clipCmd->mode = slaveConfig.clipMode;
                    clipCmd->clipPin = slaveConfig.clipStatus;

                    server->sendCommandToSlaveWithRetry(
                        slaveId, std::move(clipCmd), sockaddr_in{}, 3);
                    Log::i("ModeConfigHandler",
                           "Sent clip config to slave 0x%08X", slaveId);
                } break;

                default:
                    Log::w("ModeConfigHandler", "Unknown mode: %d",
                           static_cast<int>(modeMsg->mode));
                    break;
                }
            } else {
                Log::w("ModeConfigHandler",
                       "No configuration found for slave 0x%08X", slaveId);
            }
        }

        Log::i("ModeConfigHandler",
               "Mode configuration applied: %d, sent to %zu slaves",
               static_cast<int>(modeMsg->mode), connectedSlaves.size());
    }
};

// Reset Message Handler
class ResetHandler : public IMessageHandler {
  public:
    std::unique_ptr<Message> processMessage(const Message &message,
                                            MasterServer *server) override {
        const auto *rstMsg =
            dynamic_cast<const Backend2Master::RstMessage *>(&message);
        if (!rstMsg)
            return nullptr;

        Log::i("ResetHandler", "Processing reset message - Slave count: %d",
               static_cast<int>(rstMsg->slaveNum));

        for (const auto &slave : rstMsg->slaves) {
            Log::i("ResetHandler",
                   "  Reset Slave ID: 0x%08X, Lock: %d, Clip status: 0x%04X",
                   slave.id, static_cast<int>(slave.lock), slave.clipStatus);
        }

        auto response = std::make_unique<Master2Backend::RstResponseMessage>();
        response->status = 0; // Success
        response->slaveNum = rstMsg->slaveNum;

        // Copy slaves reset info
        for (const auto &slave : rstMsg->slaves) {
            Master2Backend::RstResponseMessage::SlaveRstInfo slaveRstInfo;
            slaveRstInfo.id = slave.id;
            slaveRstInfo.lock = slave.lock;
            slaveRstInfo.clipStatus = slave.clipStatus;
            response->slaves.push_back(slaveRstInfo);
        }

        return std::move(response);
    }

    void executeActions(const Message &message, MasterServer *server) override {
        const auto *rstMsg =
            dynamic_cast<const Backend2Master::RstMessage *>(&message);
        if (!rstMsg)
            return;

        // Send reset commands to specified slaves with retry mechanism
        int successCount = 0;
        for (const auto &slave : rstMsg->slaves) {
            if (server->getDeviceManager().isSlaveConnected(slave.id)) {
                auto resetCmd = std::make_unique<Master2Slave::RstMessage>();
                resetCmd->lockStatus = slave.lock;
                resetCmd->clipLed = slave.clipStatus;

                server->sendCommandToSlaveWithRetry(
                    slave.id, std::move(resetCmd), sockaddr_in{}, 3);
                successCount++;
                Log::i("ResetHandler",
                       "Sent reset command to slave 0x%08X (lock=%d, "
                       "clipLed=0x%04X)",
                       slave.id, static_cast<int>(slave.lock),
                       slave.clipStatus);
            } else {
                Log::w("ResetHandler",
                       "Slave 0x%08X is not connected, skipping reset",
                       slave.id);
            }
        }

        Log::i("ResetHandler", "Reset commands sent to %d/%d slaves",
               successCount, static_cast<int>(rstMsg->slaveNum));
    }
};

// Control Message Handler
class ControlHandler : public IMessageHandler {
  public:
    std::unique_ptr<Message> processMessage(const Message &message,
                                            MasterServer *server) override {
        const auto *controlMsg =
            dynamic_cast<const Backend2Master::CtrlMessage *>(&message);
        if (!controlMsg)
            return nullptr;

        Log::i("ControlHandler",
               "Processing control message - Running status: %d",
               static_cast<int>(controlMsg->runningStatus));

        auto response = std::make_unique<Master2Backend::CtrlResponseMessage>();
        response->status = 0; // Success
        response->runningStatus = controlMsg->runningStatus;

        return std::move(response);
    }

    void executeActions(const Message &message, MasterServer *server) override {
        const auto *controlMsg =
            dynamic_cast<const Backend2Master::CtrlMessage *>(&message);
        if (!controlMsg)
            return;

        auto &deviceManager = server->getDeviceManager();

        // 保存当前运行状态
        deviceManager.setSystemRunningStatus(controlMsg->runningStatus);

        Log::i("ControlHandler", "Setting system running status to %d",
               static_cast<int>(controlMsg->runningStatus));

        // 根据运行状态执行操作
        switch (controlMsg->runningStatus) {
        case 0: // 停止
            Log::i("ControlHandler", "Stopping all operations");

            // 停止所有数据采集
            deviceManager.resetDataCollection();

            // 向所有从机发送停止信号
            for (uint32_t slaveId : deviceManager.getConnectedSlaves()) {
                if (deviceManager.hasSlaveConfig(slaveId)) {
                    // 发送同步消息但设置模式为0（停止）
                    auto syncCmd =
                        std::make_unique<Master2Slave::SyncMessage>();
                    syncCmd->mode = 0; // 停止模式
                    syncCmd->timestamp = getCurrentTimestampMs();

                    server->sendCommandToSlaveWithRetry(
                        slaveId, std::move(syncCmd), sockaddr_in{}, 1);
                }
            }
            break;

        case 1: // 启动
            Log::i("ControlHandler", "Starting operations in mode %d",
                   static_cast<int>(deviceManager.getCurrentMode()));

            // 根据当前模式启动相应操作
            if (deviceManager.getCurrentMode() <=
                2) { // 0=Conduction, 1=Resistance, 2=Clip
                // 启动数据采集模式
                deviceManager.startDataCollection();

                // 数据采集的循环流程将由processDataCollection处理
                Log::i("ControlHandler",
                       "Started data collection in mode %d, cycle will be "
                       "managed by processDataCollection",
                       deviceManager.getCurrentMode());
            } else {
                Log::w("ControlHandler", "Unsupported mode: %d",
                       static_cast<int>(deviceManager.getCurrentMode()));
            }
            break;

        case 2: // 重置
            Log::i("ControlHandler", "Resetting all devices");

            // 发送重置命令给所有从机
            for (uint32_t slaveId : deviceManager.getConnectedSlaves()) {
                auto rstCmd = std::make_unique<Master2Slave::RstMessage>();
                rstCmd->lockStatus = 0;
                rstCmd->clipLed = 0;

                server->sendCommandToSlaveWithRetry(slaveId, std::move(rstCmd),
                                                    sockaddr_in{}, 3);
            }

            // 重置所有采集状态
            deviceManager.resetDataCollection();
            break;

        default:
            Log::w("ControlHandler", "Unknown running status: %d",
                   static_cast<int>(controlMsg->runningStatus));
            break;
        }
    }
};

// Ping Control Message Handler
class PingControlHandler : public IMessageHandler {
  public:
    std::unique_ptr<Message> processMessage(const Message &message,
                                            MasterServer *server) override {
        const auto *pingMsg =
            dynamic_cast<const Backend2Master::PingCtrlMessage *>(&message);
        if (!pingMsg)
            return nullptr;

        Log::i("PingControlHandler",
               "Processing ping control - Mode: %d, Count: %d, Interval: %dms, "
               "Target: 0x%08X",
               static_cast<int>(pingMsg->pingMode), pingMsg->pingCount,
               pingMsg->interval, pingMsg->destinationId);

        auto response = std::make_unique<Master2Backend::PingResponseMessage>();
        response->pingMode = pingMsg->pingMode;
        response->totalCount = pingMsg->pingCount;
        response->successCount = pingMsg->pingCount - 1; // Simulate 1 failure
        response->destinationId = pingMsg->destinationId;

        return std::move(response);
    }

    void executeActions(const Message &message, MasterServer *server) override {
        const auto *pingMsg =
            dynamic_cast<const Backend2Master::PingCtrlMessage *>(&message);
        if (!pingMsg)
            return;

        // Start ping session for the target slave
        if (server->getDeviceManager().isSlaveConnected(
                pingMsg->destinationId)) {
            server->addPingSession(pingMsg->destinationId, pingMsg->pingMode,
                                   pingMsg->pingCount, pingMsg->interval,
                                   sockaddr_in{});
            Log::i("PingControlHandler",
                   "Started ping session to target 0x%08X (mode=%d, count=%d, "
                   "interval=%dms)",
                   pingMsg->destinationId, static_cast<int>(pingMsg->pingMode),
                   pingMsg->pingCount, pingMsg->interval);
        } else {
            Log::w("PingControlHandler", "Target slave 0x%08X is not connected",
                   pingMsg->destinationId);
        }
    }
};

// Device List Request Handler
class DeviceListHandler : public IMessageHandler {
  public:
    std::unique_ptr<Message> processMessage(const Message &message,
                                            MasterServer *server) override {
        const auto *deviceListMsg =
            dynamic_cast<const Backend2Master::DeviceListReqMessage *>(
                &message);
        if (!deviceListMsg)
            return nullptr;

        Log::i("DeviceListHandler", "Processing device list request");

        auto connectedSlaves = server->getDeviceManager().getConnectedSlaves();
        auto response =
            std::make_unique<Master2Backend::DeviceListResponseMessage>();
        response->deviceCount = static_cast<uint8_t>(connectedSlaves.size());

        // Add connected devices
        for (uint32_t slaveId : connectedSlaves) {
            Master2Backend::DeviceListResponseMessage::DeviceInfo device;
            device.deviceId = slaveId;
            device.shortId =
                server->getDeviceManager().getSlaveShortId(slaveId);
            device.online = 1;
            device.versionMajor = 1;
            device.versionMinor = 2;
            device.versionPatch = 3;
            response->devices.push_back(device);
        }

        Log::i("DeviceListHandler", "Returning device list with %d devices",
               response->deviceCount);
        return std::move(response);
    }

    void executeActions(const Message &message, MasterServer *server) override {
        // Device list request doesn't require additional actions
        // The response generation already provides the current device list
        auto connectedSlaves = server->getDeviceManager().getConnectedSlaves();
        Log::i(
            "DeviceListHandler",
            "Device list request processed - %zu devices currently connected",
            connectedSlaves.size());
    }
};

// MasterServer Implementation
MasterServer::MasterServer(uint16_t listenPort) : port(listenPort) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
#endif

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("Socket creation failed");
    }

    // Enable broadcast for the socket (to simulate wireless broadcast)
    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&broadcast,
                   sizeof(broadcast)) == SOCKET_ERROR) {
        Log::w("Master", "Failed to enable broadcast option");
    }

    // Configure server address (Master listens on port 8080)
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    // Configure backend address (Backend uses port 8079)
    backendAddr.sin_family = AF_INET;
    backendAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost
    backendAddr.sin_port = htons(8079);

    // Configure slave broadcast address (Broadcast to all slaves on port 8081)
    // Using localhost broadcast for simulation
    slaveBroadcastAddr.sin_family = AF_INET;
    slaveBroadcastAddr.sin_addr.s_addr =
        inet_addr("127.255.255.255"); // Local broadcast
    slaveBroadcastAddr.sin_port = htons(8081);

    if (bind(sock, (sockaddr *)&serverAddr, sizeof(serverAddr)) ==
        SOCKET_ERROR) {
        closesocket(sock);
        throw std::runtime_error("Bind failed");
    }

    initializeMessageHandlers();
    Log::i("Master", "Master server listening on port %d", port);
    Log::i("Master", "Backend communication port: 8079");
    Log::i("Master", "Slave broadcast communication port: 8081");
    Log::i("Master", "Wireless broadcast simulation enabled");
}

MasterServer::~MasterServer() {
    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
}

void MasterServer::initializeMessageHandlers() {
    registerMessageHandler(
        static_cast<uint8_t>(Backend2MasterMessageId::SLAVE_CFG_MSG),
        std::make_unique<SlaveConfigHandler>());
    registerMessageHandler(
        static_cast<uint8_t>(Backend2MasterMessageId::MODE_CFG_MSG),
        std::make_unique<ModeConfigHandler>());
    registerMessageHandler(
        static_cast<uint8_t>(Backend2MasterMessageId::SLAVE_RST_MSG),
        std::make_unique<ResetHandler>());
    registerMessageHandler(
        static_cast<uint8_t>(Backend2MasterMessageId::CTRL_MSG),
        std::make_unique<ControlHandler>());
    registerMessageHandler(
        static_cast<uint8_t>(Backend2MasterMessageId::PING_CTRL_MSG),
        std::make_unique<PingControlHandler>());
    registerMessageHandler(
        static_cast<uint8_t>(Backend2MasterMessageId::DEVICE_LIST_REQ_MSG),
        std::make_unique<DeviceListHandler>());
}

void MasterServer::registerMessageHandler(
    uint8_t messageId, std::unique_ptr<IMessageHandler> handler) {
    messageHandlers[messageId] = std::move(handler);
}

std::vector<uint8_t> MasterServer::hexStringToBytes(const std::string &hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        if (i + 1 < hex.length()) {
            std::string byteString = hex.substr(i, 2);
            uint8_t byte =
                static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
            bytes.push_back(byte);
        }
    }
    return bytes;
}

uint32_t MasterServer::getCurrentTimestamp() {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

void MasterServer::printBytes(const std::vector<uint8_t> &data,
                              const std::string &description) {
    std::stringstream ss;
    for (auto byte : data) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(byte) << " ";
    }
    Log::d("Master", "%s (%zu bytes): %s", description.c_str(), data.size(),
           ss.str().c_str());
}

std::string MasterServer::bytesToHexString(const std::vector<uint8_t> &bytes) {
    std::stringstream ss;
    for (uint8_t byte : bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(byte);
    }
    return ss.str();
}

void MasterServer::sendResponseToBackend(std::unique_ptr<Message> response,
                                         const sockaddr_in &clientAddr) {
    if (!response)
        return;

    auto responseData = processor.packMaster2BackendMessage(*response);
    Log::i("Master", "Sending Master2Backend response to port 8079:");

    for (const auto &fragment : responseData) {
        printBytes(fragment, "Master2Backend response data");
        // Send to backend on port 8079
        sendto(sock, reinterpret_cast<const char *>(fragment.data()),
               static_cast<int>(fragment.size()), 0, (sockaddr *)&backendAddr,
               sizeof(backendAddr));
    }

    Log::i("Master", "Master2Backend response sent to backend (port 8079)");
}

void MasterServer::sendCommandToSlave(uint32_t slaveId,
                                      std::unique_ptr<Message> command,
                                      const sockaddr_in &clientAddr) {
    if (!command)
        return;

    auto commandData = processor.packMaster2SlaveMessage(slaveId, *command);
    Log::i(
        "Master",
        "Broadcasting Master2Slave command to 0x%08X via port 8081:", slaveId);

    for (const auto &fragment : commandData) {
        printBytes(fragment, "Master2Slave command data");
        // Send to slaves on port 8081
        sendto(sock, reinterpret_cast<const char *>(fragment.data()),
               static_cast<int>(fragment.size()), 0,
               (sockaddr *)&slaveBroadcastAddr, sizeof(slaveBroadcastAddr));
    }

    Log::i("Master", "Master2Slave command broadcasted to slaves (port 8081)");
}

void MasterServer::sendCommandToSlaveWithRetry(uint32_t slaveId,
                                               std::unique_ptr<Message> command,
                                               const sockaddr_in &clientAddr,
                                               uint8_t maxRetries) {
    // Create a pending command for retry management
    PendingCommand pendingCmd(slaveId, std::move(command), clientAddr,
                              maxRetries);
    pendingCmd.timestamp = getCurrentTimestampMs();

    // Send the command immediately
    sendCommandToSlave(slaveId,
                       std::unique_ptr<Message>(pendingCmd.command.get()),
                       clientAddr);

    // Add to pending commands list for retry management
    pendingCommands.push_back(std::move(pendingCmd));

    Log::i("Master",
           "Command sent to slave 0x%08X with retry support (max retries: %d)",
           slaveId, maxRetries);
}

void MasterServer::processPendingCommands() {
    uint32_t currentTime = getCurrentTimestampMs();
    const uint32_t RETRY_TIMEOUT = 5000; // 5 seconds timeout

    auto it = pendingCommands.begin();
    while (it != pendingCommands.end()) {
        if (currentTime - it->timestamp > RETRY_TIMEOUT) {
            if (it->retryCount < it->maxRetries) {
                // Retry the command
                it->retryCount++;
                it->timestamp = currentTime;

                // Create a copy of the command for retry
                // Note: This is a simplified approach, in practice you'd need
                // proper message cloning
                sendCommandToSlave(it->slaveId,
                                   std::unique_ptr<Message>(it->command.get()),
                                   it->clientAddr);

                Log::i("Master",
                       "Retrying command to slave 0x%08X (attempt %d/%d)",
                       it->slaveId, it->retryCount, it->maxRetries);
                ++it;
            } else {
                // Max retries reached, remove from pending list
                Log::w("Master",
                       "Command to slave 0x%08X failed after %d retries",
                       it->slaveId, it->maxRetries);
                it = pendingCommands.erase(it);
            }
        } else {
            ++it;
        }
    }
}

void MasterServer::addPingSession(uint32_t targetId, uint8_t pingMode,
                                  uint16_t totalCount, uint16_t interval,
                                  const sockaddr_in &clientAddr) {
    // Create a new ping session
    PingSession session(targetId, pingMode, totalCount, interval, clientAddr);
    session.lastPingTime = getCurrentTimestampMs();

    activePingSessions.push_back(std::move(session));

    Log::i("Master",
           "Added ping session for target 0x%08X (mode=%d, count=%d, "
           "interval=%dms)",
           targetId, pingMode, totalCount, interval);
}

void MasterServer::processPingSessions() {
    uint32_t currentTime = getCurrentTimestampMs();

    auto it = activePingSessions.begin();
    while (it != activePingSessions.end()) {
        if (currentTime - it->lastPingTime >= it->interval) {
            if (it->currentCount < it->totalCount) {
                // Send ping command
                auto pingCmd = std::make_unique<Master2Slave::PingReqMessage>();
                pingCmd->sequenceNumber = it->currentCount + 1;
                pingCmd->timestamp = currentTime;

                sendCommandToSlave(it->targetId, std::move(pingCmd),
                                   it->clientAddr);

                it->currentCount++;
                it->lastPingTime = currentTime;

                Log::i("Master", "Sent ping %d/%d to target 0x%08X",
                       it->currentCount, it->totalCount, it->targetId);
                ++it;
            } else {
                // Ping session completed
                Log::i("Master",
                       "Ping session completed for target 0x%08X (%d/%d "
                       "successful)",
                       it->targetId, it->successCount, it->totalCount);
                it = activePingSessions.erase(it);
            }
        } else {
            ++it;
        }
    }
}

void MasterServer::processBackend2MasterMessage(const Message &message,
                                                const sockaddr_in &clientAddr) {
    uint8_t messageId = message.getMessageId();
    Log::i("Master", "Processing Backend2Master message, ID: 0x%02X",
           static_cast<int>(messageId));

    auto handlerIt = messageHandlers.find(messageId);
    if (handlerIt != messageHandlers.end()) {
        // Process message and generate response
        auto response = handlerIt->second->processMessage(message, this);

        // Execute associated actions
        handlerIt->second->executeActions(message, this);

        // Send response if generated
        if (response) {
            sendResponseToBackend(std::move(response), clientAddr);
        } else {
            Log::i("Master",
                   "No response needed for this Backend2Master message");
        }
    } else {
        Log::w("Master", "Unknown Backend2Master message type: 0x%02X",
               static_cast<int>(messageId));
    }
}

void MasterServer::processSlave2MasterMessage(uint32_t slaveId,
                                              const Message &message,
                                              const sockaddr_in &clientAddr) {
    Log::i("Master", "Processing Slave2Master message from slave 0x%08X",
           slaveId);

    switch (message.getMessageId()) {
    case static_cast<uint8_t>(Slave2MasterMessageId::CONDUCTION_CFG_RSP_MSG): {
        const auto *rspMsg =
            dynamic_cast<const Slave2Master::ConductionConfigResponseMessage *>(
                &message);
        if (rspMsg) {
            Log::i("Master", "Received conduction config response - Status: %d",
                   static_cast<int>(rspMsg->status));

            // 配置成功，加入连接状态
            if (rspMsg->status == 0) {
                deviceManager.addSlave(slaveId);
            }
        }
        break;
    }

    case static_cast<uint8_t>(Slave2MasterMessageId::RESISTANCE_CFG_RSP_MSG): {
        const auto *rspMsg =
            dynamic_cast<const Slave2Master::ResistanceConfigResponseMessage *>(
                &message);
        if (rspMsg) {
            Log::i("Master", "Received resistance config response - Status: %d",
                   static_cast<int>(rspMsg->status));

            // 配置成功，加入连接状态
            if (rspMsg->status == 0) {
                deviceManager.addSlave(slaveId);
            }
        }
        break;
    }

    case static_cast<uint8_t>(Slave2MasterMessageId::CLIP_CFG_RSP_MSG): {
        const auto *rspMsg =
            dynamic_cast<const Slave2Master::ClipConfigResponseMessage *>(
                &message);
        if (rspMsg) {
            Log::i("Master", "Received clip config response - Status: %d",
                   static_cast<int>(rspMsg->status));

            // 配置成功，加入连接状态
            if (rspMsg->status == 0) {
                deviceManager.addSlave(slaveId);
            }
        }
        break;
    }

    case static_cast<uint8_t>(Slave2MasterMessageId::PING_RSP_MSG): {
        const auto *pingRsp =
            dynamic_cast<const Slave2Master::PingRspMessage *>(&message);
        if (pingRsp) {
            uint32_t currentTime = getCurrentTimestampMs();
            uint32_t roundTripTime = currentTime - pingRsp->timestamp;

            Log::i("Master",
                   "Received ping response - Sequence: %d, RTT: %d ms",
                   static_cast<int>(pingRsp->sequenceNumber), roundTripTime);

            // Mark ping as successful in active ping sessions
            for (auto &session : activePingSessions) {
                if (session.targetId == slaveId) {
                    session.successCount++;
                    break;
                }
            }

            // Add or update slave in connected devices
            deviceManager.addSlave(slaveId);
        }
        break;
    }

    case static_cast<uint8_t>(Slave2MasterMessageId::RST_RSP_MSG): {
        const auto *rstRsp =
            dynamic_cast<const Slave2Master::RstResponseMessage *>(&message);
        if (rstRsp) {
            Log::i("Master", "Received reset response - Status: %d",
                   static_cast<int>(rstRsp->status));
        }
        break;
    }

    case static_cast<uint8_t>(Slave2MasterMessageId::ANNOUNCE_MSG): {
        const auto *announceMsg =
            dynamic_cast<const Slave2Master::AnnounceMessage *>(&message);
        if (announceMsg) {
            Log::i("Master", "Received announce message - Version: %d.%d.%d",
                   static_cast<int>(announceMsg->versionMajor),
                   static_cast<int>(announceMsg->versionMinor),
                   announceMsg->versionPatch);

            // Register the slave as connected
            deviceManager.addSlave(slaveId);
        }
        break;
    }

    case static_cast<uint8_t>(Slave2MasterMessageId::SHORT_ID_CONFIRM_MSG): {
        const auto *confirmMsg =
            dynamic_cast<const Slave2Master::ShortIdConfirmMessage *>(&message);
        if (confirmMsg) {
            Log::i("Master", "Received short ID confirm message - Short ID: %d",
                   static_cast<int>(confirmMsg->shortId));

            // Update slave short ID
            deviceManager.addSlave(slaveId, confirmMsg->shortId);
        }
        break;
    }

    // 处理从机数据类型消息 - 这些是 Slave2Backend 消息，需要转发给后端
    case static_cast<uint8_t>(Slave2BackendMessageId::CONDUCTION_DATA_MSG): {
        const auto *dataMsg =
            dynamic_cast<const Slave2Backend::ConductionDataMessage *>(
                &message);
        if (dataMsg) {
            Log::i("Master",
                   "Received conduction data from slave 0x%08X - %zu bytes",
                   slaveId, dataMsg->conductionData.size());

            // 标记从机的数据已接收
            deviceManager.markDataReceived(slaveId);

            // 将数据转发给后端
            DeviceStatus status = {};
            std::vector<std::vector<uint8_t>> packets =
                processor.packSlave2BackendMessage(slaveId, status, *dataMsg);

            for (const auto &packet : packets) {
                sendto(sock, reinterpret_cast<const char *>(packet.data()),
                       static_cast<int>(packet.size()), 0,
                       (sockaddr *)&backendAddr, sizeof(backendAddr));

                Log::i("Master",
                       "Forwarded conduction data to backend - %zu bytes",
                       packet.size());
            }
        }
        break;
    }

    case static_cast<uint8_t>(Slave2BackendMessageId::RESISTANCE_DATA_MSG): {
        const auto *dataMsg =
            dynamic_cast<const Slave2Backend::ResistanceDataMessage *>(
                &message);
        if (dataMsg) {
            Log::i("Master",
                   "Received resistance data from slave 0x%08X - %zu bytes",
                   slaveId, dataMsg->resistanceData.size());

            // 标记从机的数据已接收
            deviceManager.markDataReceived(slaveId);

            // 将数据转发给后端
            DeviceStatus status = {};
            std::vector<std::vector<uint8_t>> packets =
                processor.packSlave2BackendMessage(slaveId, status, *dataMsg);

            for (const auto &packet : packets) {
                sendto(sock, reinterpret_cast<const char *>(packet.data()),
                       static_cast<int>(packet.size()), 0,
                       (sockaddr *)&backendAddr, sizeof(backendAddr));

                Log::i("Master",
                       "Forwarded resistance data to backend - %zu bytes",
                       packet.size());
            }
        }
        break;
    }

    case static_cast<uint8_t>(Slave2BackendMessageId::CLIP_DATA_MSG): {
        const auto *dataMsg =
            dynamic_cast<const Slave2Backend::ClipDataMessage *>(&message);
        if (dataMsg) {
            Log::i("Master",
                   "Received clip data from slave 0x%08X - value: 0x%02X",
                   slaveId, dataMsg->clipData);

            // 标记从机的数据已接收
            deviceManager.markDataReceived(slaveId);

            // 将数据转发给后端
            DeviceStatus status = {};
            std::vector<std::vector<uint8_t>> packets =
                processor.packSlave2BackendMessage(slaveId, status, *dataMsg);

            for (const auto &packet : packets) {
                sendto(sock, reinterpret_cast<const char *>(packet.data()),
                       static_cast<int>(packet.size()), 0,
                       (sockaddr *)&backendAddr, sizeof(backendAddr));

                Log::i("Master", "Forwarded clip data to backend - %zu bytes",
                       packet.size());
            }
        }
        break;
    }

    default:
        Log::w("Master", "Unknown Slave2Master message type: 0x%02X",
               static_cast<int>(message.getMessageId()));
        break;
    }
}

void MasterServer::processFrame(Frame &frame, const sockaddr_in &clientAddr) {
    Log::i("Master", "Processing frame - PacketId: 0x%02X, payload size: %zu",
           static_cast<int>(frame.packetId), frame.payload.size());

    if (frame.packetId == static_cast<uint8_t>(PacketId::BACKEND_TO_MASTER)) {
        std::unique_ptr<Message> backendMessage;
        if (processor.parseBackend2MasterPacket(frame.payload,
                                                backendMessage)) {
            processBackend2MasterMessage(*backendMessage, clientAddr);
        } else {
            Log::e("Master", "Failed to parse Backend2Master packet");
        }
    } else if (frame.packetId ==
               static_cast<uint8_t>(PacketId::SLAVE_TO_MASTER)) {
        uint32_t slaveId;
        std::unique_ptr<Message> slaveMessage;
        if (processor.parseSlave2MasterPacket(frame.payload, slaveId,
                                              slaveMessage)) {
            processSlave2MasterMessage(slaveId, *slaveMessage, clientAddr);
        } else {
            Log::e("Master", "Failed to parse Slave2Master packet");
        }
    } else {
        Log::w("Master", "Unsupported packet type for Master: 0x%02X",
               static_cast<int>(frame.packetId));
    }
}

void MasterServer::run() {
    Log::i("Master", "Master server started, waiting for UDP messages...");
    Log::i("Master",
           "Listening on port %d for Backend2Master and Slave2Master packets",
           port);
    Log::i("Master", "Sending responses to Backend on port 8079");
    Log::i("Master", "Broadcasting commands to Slaves on port 8081");
    Log::i("Master", "Press Ctrl+C to exit");

    char buffer[1024];
    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    processor.setMTU(100);

    while (true) {
        // Process pending commands, ping sessions, and data collection
        processPendingCommands();
        processPingSessions();
        processDataCollection();

        int bytesReceived = recvfrom(sock, buffer, sizeof(buffer), 0,
                                     (sockaddr *)&clientAddr, &clientAddrLen);

        if (bytesReceived > 0) {
            std::string receivedStr(buffer, bytesReceived);

            // Remove spaces and line breaks
            receivedStr.erase(
                std::remove_if(receivedStr.begin(), receivedStr.end(),
                               [](char c) { return std::isspace(c); }),
                receivedStr.end());

            std::vector<uint8_t> data;

            // If it's a hexadecimal string, convert it to bytes
            if (receivedStr.length() > 0 &&
                std::all_of(receivedStr.begin(), receivedStr.end(),
                            [](char c) { return std::isxdigit(c); })) {
                data = hexStringToBytes(receivedStr);
                Log::i("Master", "Received hexadecimal string: %s",
                       receivedStr.c_str());
            } else {
                // Otherwise, treat it as binary data directly
                data.assign(buffer, buffer + bytesReceived);
                Log::i("Master", "Received binary data");
            }

            if (!data.empty()) {
                processor.processReceivedData(data);
                Frame receivedFrame;
                int frameCount = 0;
                while (processor.getNextCompleteFrame(receivedFrame)) {
                    frameCount++;
                    Log::i("Master",
                           "Parsed frame %d: PacketId=%d, payload size=%zu",
                           frameCount, (int)receivedFrame.packetId,
                           receivedFrame.payload.size());
                    processFrame(receivedFrame, clientAddr);
                }
            }
        }
    }
}

// 数据采集管理
void MasterServer::processDataCollection() {
    DeviceManager &dm = getDeviceManager();

    // 如果没有活跃的数据采集，则不处理
    if (!dm.isDataCollectionActive()) {
        return;
    }

    uint32_t currentTime = getCurrentTimestampMs();

    // 检查当前采集周期状态并根据状态执行相应操作
    switch (dm.getCycleState()) {
    case CollectionCycleState::IDLE:
        // 处于空闲状态，检查是否应该开始新的采集周期
        if (dm.shouldStartNewCycle(currentTime)) {
            dm.startNewCycle(currentTime);
            Log::i("MasterServer", "Started new data collection cycle");
        }
        break;

    case CollectionCycleState::COLLECTING:
        // 处于采集状态
        if (!dm.isSyncSent()) {
            // 发送Sync消息给所有从机
            for (uint32_t slaveId : dm.getConnectedSlaves()) {
                if (dm.hasSlaveConfig(slaveId)) {
                    auto syncCmd =
                        std::make_unique<Master2Slave::SyncMessage>();

                    // 根据当前模式设置同步消息的模式
                    syncCmd->mode = dm.getCurrentMode();
                    syncCmd->timestamp = currentTime;

                    // 发送同步消息
                    sendCommandToSlave(slaveId, std::move(syncCmd),
                                       slaveBroadcastAddr);

                    Log::i("MasterServer",
                           "Sent Sync message to slave 0x%08X with mode %d",
                           slaveId, dm.getCurrentMode());
                }
            }

            // 标记同步消息已发送
            dm.markSyncSent(currentTime);

        } else if (dm.shouldEnterReadingPhase(currentTime)) {
            // 所有从机都已完成采集，可以进入读取数据阶段
            dm.enterReadingPhase();
            Log::i(
                "MasterServer",
                "All slaves completed data collection, entering reading phase");
        }
        break;

    case CollectionCycleState::READING_DATA:
        // 处于读取数据阶段，向未请求数据的从机发送读取数据请求
        for (uint32_t slaveId : dm.getSlavesForDataRequest()) {
            // 根据当前模式创建相应的读取数据消息
            std::unique_ptr<Message> readDataCmd;

            switch (dm.getCurrentMode()) {
            case 0: // Conduction模式
                readDataCmd =
                    std::make_unique<Master2Slave::ReadConductionDataMessage>();
                break;

            case 1: // Resistance模式
                readDataCmd =
                    std::make_unique<Master2Slave::ReadResistanceDataMessage>();
                break;

            case 2: // Clip模式
                readDataCmd =
                    std::make_unique<Master2Slave::ReadClipDataMessage>();
                break;
            }

            if (readDataCmd) {
                // 发送读取数据命令
                sendCommandToSlaveWithRetry(slaveId, std::move(readDataCmd),
                                            slaveBroadcastAddr, 3);

                // 标记数据已请求
                dm.markDataRequested(slaveId);

                Log::i("MasterServer",
                       "Sent Read Data command to slave 0x%08X for mode %d",
                       slaveId, dm.getCurrentMode());
            }
        }
        break;

    case CollectionCycleState::COMPLETE:
        // 采集周期已完成，检查是否应该开始新的采集周期
        if (dm.shouldStartNewCycle(currentTime)) {
            dm.startNewCycle(currentTime);
            Log::i("MasterServer",
                   "Started new data collection cycle after completion");
        }
        break;
    }
}

int main() {
    Log::i("Main", "WhtsProtocol Master Server");
    Log::i("Main", "==========================");
    Log::i("Main", "Port Configuration (Wireless Broadcast Simulation):");
    Log::i("Main", "  Backend: 8079 (receives responses from Master)");
    Log::i("Main", "  Master:  8080 (listens for Backend & Slave messages)");
    Log::i("Main", "  Slaves:  8081 (receive broadcast commands from Master)");
    Log::i("Main", "Wireless Communication Simulation:");
    Log::i("Main",
           "  Master -> Slaves: UDP Broadcast (simulates wireless broadcast)");
    Log::i("Main",
           "  Slaves -> Master: UDP Unicast (simulates wireless response)");
    Log::i("Main", "Handling Backend2Master and Slave2Master packets");

    try {
        MasterServer server(8080);
        server.run();
    } catch (const std::exception &e) {
        Log::e("Main", "Error: %s", e.what());
        return 1;
    }

    return 0;
}