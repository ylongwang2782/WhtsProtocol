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

  public:
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
};

class MasterServer {
  private:
    SOCKET sock;
    sockaddr_in serverAddr;
    ProtocolProcessor processor;
    uint16_t port;
    DeviceManager deviceManager;
    std::unordered_map<uint8_t, std::unique_ptr<IMessageHandler>>
        messageHandlers;

  public:
    MasterServer(uint16_t listenPort = 8888);
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

        // Register slaves in device manager
        for (const auto &slave : configMsg->slaves) {
            server->getDeviceManager().addSlave(slave.id);
        }

        // TODO: Send configuration commands to individual slaves
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

        // TODO: Apply mode configuration to all connected slaves
        Log::i("ModeConfigHandler", "Mode configuration applied: %d",
               static_cast<int>(modeMsg->mode));
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

        // TODO: Send reset commands to specified slaves
        for (const auto &slave : rstMsg->slaves) {
            if (server->getDeviceManager().isSlaveConnected(slave.id)) {
                auto resetCmd = std::make_unique<Master2Slave::RstMessage>();
                resetCmd->lockStatus = slave.lock;
                resetCmd->clipLed = slave.clipStatus;
                // server->sendCommandToSlave(slave.id, std::move(resetCmd),
                // clientAddr);
            }
        }

        Log::i("ResetHandler", "Reset commands sent to %d slaves",
               static_cast<int>(rstMsg->slaveNum));
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

        // TODO: Apply control command to system
        Log::i("ControlHandler", "Control command executed - Status: %d",
               static_cast<int>(ctrlMsg->runningStatus));
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

        // TODO: Execute ping commands to target slave
        Log::i("PingControlHandler", "Ping command executed to target 0x%08X",
               pingMsg->destinationId);
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
        // No additional actions needed for device list request
        Log::i("DeviceListHandler", "Device list request processed");
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

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(sock, (sockaddr *)&serverAddr, sizeof(serverAddr)) ==
        SOCKET_ERROR) {
        closesocket(sock);
        throw std::runtime_error("Bind failed");
    }

    initializeMessageHandlers();
    Log::i("Master", "Master server listening on port %d", port);
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
    Log::i("Master", "Sending Master2Backend response:");

    for (const auto &fragment : responseData) {
        printBytes(fragment, "Master2Backend response data");
        sendto(sock, reinterpret_cast<const char *>(fragment.data()),
               static_cast<int>(fragment.size()), 0, (sockaddr *)&clientAddr,
               sizeof(clientAddr));
    }

    Log::i("Master", "Master2Backend response sent");
}

void MasterServer::sendCommandToSlave(uint32_t slaveId,
                                      std::unique_ptr<Message> command,
                                      const sockaddr_in &clientAddr) {
    if (!command)
        return;

    auto commandData = processor.packMaster2SlaveMessage(slaveId, *command);
    Log::i("Master", "Sending Master2Slave command to 0x%08X:", slaveId);

    for (const auto &fragment : commandData) {
        printBytes(fragment, "Master2Slave command data");
        sendto(sock, reinterpret_cast<const char *>(fragment.data()),
               static_cast<int>(fragment.size()), 0, (sockaddr *)&clientAddr,
               sizeof(clientAddr));
    }

    Log::i("Master", "Master2Slave command sent");
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
            // TODO: Forward response to backend
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
    Log::i("Master", "Press Ctrl+C to exit");

    char buffer[1024];
    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    processor.setMTU(100);

    while (true) {
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
    Log::i("Main", "Handling Backend2Master and Slave2Master packets");

    try {
        MasterServer server(8888);
        server.run();
    } catch (const std::exception &e) {
        Log::e("Main", "Error: %s", e.what());
        return 1;
    }

    return 0;
}