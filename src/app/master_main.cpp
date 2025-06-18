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
    uint8_t currentMode;
    uint8_t systemRunningStatus;

  public:
    DeviceManager() : currentMode(0), systemRunningStatus(0) {}

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
};

class MasterServer {
  private:
    SOCKET sock;
    sockaddr_in serverAddr;
    sockaddr_in backendAddr; // Backend address (port 8079)
    sockaddr_in slaveAddr;   // Slave address (port 8081)
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
        const auto *ctrlMsg =
            dynamic_cast<const Backend2Master::CtrlMessage *>(&message);
        if (!ctrlMsg)
            return nullptr;

        Log::i("ControlHandler",
               "Processing control message - Running status: %d",
               static_cast<int>(ctrlMsg->runningStatus));

        auto response = std::make_unique<Master2Backend::CtrlResponseMessage>();
        response->status = 0; // Success
        response->runningStatus = ctrlMsg->runningStatus;

        return std::move(response);
    }

    void executeActions(const Message &message, MasterServer *server) override {
        const auto *ctrlMsg =
            dynamic_cast<const Backend2Master::CtrlMessage *>(&message);
        if (!ctrlMsg)
            return;

        // Store system running status
        server->getDeviceManager().setSystemRunningStatus(
            ctrlMsg->runningStatus);

        // Apply control command based on running status
        auto connectedSlaves = server->getDeviceManager().getConnectedSlaves();
        int commandsSent = 0;

        switch (ctrlMsg->runningStatus) {
        case 0: // Stop/Pause system
        {
            Log::i("ControlHandler", "Stopping system operations");
            // Send stop commands to all connected slaves if needed
            for (uint32_t slaveId : connectedSlaves) {
                // For now, we could send a sync message to coordinate stopping
                auto syncCmd = std::make_unique<Master2Slave::SyncMessage>();
                syncCmd->mode = 0; // Stop mode
                syncCmd->timestamp = MasterServer::getCurrentTimestamp();

                server->sendCommandToSlaveWithRetry(slaveId, std::move(syncCmd),
                                                    sockaddr_in{}, 2);
                commandsSent++;
            }
        } break;

        case 1: // Start/Resume system
        {
            Log::i("ControlHandler", "Starting system operations");
            // Send start commands and apply current mode configuration
            uint8_t currentMode = server->getDeviceManager().getCurrentMode();
            for (uint32_t slaveId : connectedSlaves) {
                // Send sync message to start operations
                auto syncCmd = std::make_unique<Master2Slave::SyncMessage>();
                syncCmd->mode = currentMode;
                syncCmd->timestamp = MasterServer::getCurrentTimestamp();

                server->sendCommandToSlaveWithRetry(slaveId, std::move(syncCmd),
                                                    sockaddr_in{}, 2);
                commandsSent++;
            }
        } break;

        case 2: // Reset system
        {
            Log::i("ControlHandler", "Resetting system");
            // Send reset to all connected slaves
            for (uint32_t slaveId : connectedSlaves) {
                auto resetCmd = std::make_unique<Master2Slave::RstMessage>();
                resetCmd->lockStatus = 0; // Unlock
                resetCmd->clipLed = 0;    // Turn off LEDs

                server->sendCommandToSlaveWithRetry(
                    slaveId, std::move(resetCmd), sockaddr_in{}, 3);
                commandsSent++;
            }
        } break;

        default:
            Log::w("ControlHandler", "Unknown running status: %d",
                   static_cast<int>(ctrlMsg->runningStatus));
            break;
        }

        Log::i(
            "ControlHandler",
            "Control command executed - Status: %d, Commands sent to %d slaves",
            static_cast<int>(ctrlMsg->runningStatus), commandsSent);
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

    // Configure server address (Master listens on port 8080)
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    // Configure backend address (Backend uses port 8079)
    backendAddr.sin_family = AF_INET;
    backendAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost
    backendAddr.sin_port = htons(8079);

    // Configure slave address (Slaves use port 8081)
    slaveAddr.sin_family = AF_INET;
    slaveAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost
    slaveAddr.sin_port = htons(8081);

    if (bind(sock, (sockaddr *)&serverAddr, sizeof(serverAddr)) ==
        SOCKET_ERROR) {
        closesocket(sock);
        throw std::runtime_error("Bind failed");
    }

    initializeMessageHandlers();
    Log::i("Master", "Master server listening on port %d", port);
    Log::i("Master", "Backend communication port: 8079");
    Log::i("Master", "Slave communication port: 8081");
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
    Log::i("Master",
           "Sending Master2Slave command to 0x%08X via port 8081:", slaveId);

    for (const auto &fragment : commandData) {
        printBytes(fragment, "Master2Slave command data");
        // Send to slaves on port 8081
        sendto(sock, reinterpret_cast<const char *>(fragment.data()),
               static_cast<int>(fragment.size()), 0, (sockaddr *)&slaveAddr,
               sizeof(slaveAddr));
    }

    Log::i("Master", "Master2Slave command sent to slaves (port 8081)");
}

void MasterServer::sendCommandToSlaveWithRetry(uint32_t slaveId,
                                               std::unique_ptr<Message> command,
                                               const sockaddr_in &clientAddr,
                                               uint8_t maxRetries) {
    // Create a pending command for retry management
    PendingCommand pendingCmd(slaveId, std::move(command), clientAddr,
                              maxRetries);
    pendingCmd.timestamp = getCurrentTimestamp();

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
    uint32_t currentTime = getCurrentTimestamp();
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
    session.lastPingTime = getCurrentTimestamp();

    activePingSessions.push_back(std::move(session));

    Log::i("Master",
           "Added ping session for target 0x%08X (mode=%d, count=%d, "
           "interval=%dms)",
           targetId, pingMode, totalCount, interval);
}

void MasterServer::processPingSessions() {
    uint32_t currentTime = getCurrentTimestamp();

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
    Log::i("Master",
           "Processing Slave2Master message from slave 0x%08X, ID: 0x%02X",
           slaveId, static_cast<int>(message.getMessageId()));

    switch (message.getMessageId()) {
    case static_cast<uint8_t>(Slave2MasterMessageId::CONDUCTION_CFG_RSP_MSG): {
        const auto *configRspMsg =
            dynamic_cast<const Slave2Master::ConductionConfigResponseMessage *>(
                &message);
        if (configRspMsg) {
            Log::i("Master",
                   "Received conduction config response - Status: %d, Time "
                   "slot: %d",
                   static_cast<int>(configRspMsg->status),
                   static_cast<int>(configRspMsg->timeSlot));
            // TODO: Forward response to backend
        }
        break;
    }
    case static_cast<uint8_t>(Slave2MasterMessageId::RESISTANCE_CFG_RSP_MSG): {
        const auto *configRspMsg =
            dynamic_cast<const Slave2Master::ResistanceConfigResponseMessage *>(
                &message);
        if (configRspMsg) {
            Log::i("Master",
                   "Received resistance config response - Status: %d, Time "
                   "slot: %d",
                   static_cast<int>(configRspMsg->status),
                   static_cast<int>(configRspMsg->timeSlot));
            // TODO: Forward response to backend
        }
        break;
    }
    case static_cast<uint8_t>(Slave2MasterMessageId::CLIP_CFG_RSP_MSG): {
        const auto *configRspMsg =
            dynamic_cast<const Slave2Master::ClipConfigResponseMessage *>(
                &message);
        if (configRspMsg) {
            Log::i("Master", "Received clip config response - Status: %d",
                   static_cast<int>(configRspMsg->status));
            // TODO: Forward response to backend
        }
        break;
    }
    case static_cast<uint8_t>(Slave2MasterMessageId::RST_RSP_MSG): {
        const auto *resetRspMsg =
            dynamic_cast<const Slave2Master::RstResponseMessage *>(&message);
        if (resetRspMsg) {
            Log::i("Master", "Received reset response - Status: %d",
                   static_cast<int>(resetRspMsg->status));
            // TODO: Forward response to backend
        }
        break;
    }
    case static_cast<uint8_t>(Slave2MasterMessageId::PING_RSP_MSG): {
        const auto *pingRspMsg =
            dynamic_cast<const Slave2Master::PingRspMessage *>(&message);
        if (pingRspMsg) {
            Log::i("Master", "Received ping response - Sequence: %d",
                   static_cast<int>(pingRspMsg->sequenceNumber));

            // Update ping session success count
            for (auto &session : activePingSessions) {
                if (session.targetId == slaveId) {
                    session.successCount++;
                    Log::i("Master",
                           "Updated ping session for 0x%08X: %d/%d successful",
                           slaveId, session.successCount, session.totalCount);
                    break;
                }
            }
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
    Log::i("Master", "Sending commands to Slaves on port 8081");
    Log::i("Master", "Press Ctrl+C to exit");

    char buffer[1024];
    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    processor.setMTU(100);

    while (true) {
        // Process pending commands and ping sessions
        processPendingCommands();
        processPingSessions();

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

int main() {
    Log::i("Main", "WhtsProtocol Master Server");
    Log::i("Main", "==========================");
    Log::i("Main", "Port Configuration:");
    Log::i("Main", "  Backend: 8079 (receives responses from Master)");
    Log::i("Main", "  Master:  8080 (listens for Backend & Slave messages)");
    Log::i("Main", "  Slave:   8081 (receives commands from Master)");
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