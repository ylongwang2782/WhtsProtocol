#include "ContinuityCollector.h"
#include "Logger.h"
#include "WhtsProtocol.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
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

// 从机设备状态枚举
enum class SlaveDeviceState {
    IDLE,                // 空闲状态
    CONFIGURED,          // 已配置状态
    COLLECTING,          // 采集中状态
    COLLECTION_COMPLETE, // 采集完成状态
    DEV_ERR              // 错误状态
};

class SlaveDevice {
  private:
    SOCKET sock;
    sockaddr_in serverAddr;
    sockaddr_in masterAddr; // Master address (port 8080)
    ProtocolProcessor processor;
    uint16_t port;
    uint32_t deviceId;
    std::unique_ptr<Adapter::ContinuityCollector> continuityCollector;

    // 新增状态管理
    SlaveDeviceState deviceState;
    Adapter::CollectorConfig currentConfig;
    bool isConfigured;
    std::mutex stateMutex;

  public:
    SlaveDevice(uint16_t listenPort = 8081, uint32_t id = 0x3732485B)
        : port(listenPort), deviceId(id), deviceState(SlaveDeviceState::IDLE),
          isConfigured(false) {
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

        // Enable broadcast reception for the socket (to receive wireless
        // broadcasts)
        int broadcast = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&broadcast,
                       sizeof(broadcast)) == SOCKET_ERROR) {
            Log::w("Slave", "Failed to enable broadcast option");
        }

        // Configure server address (Slave listens on port 8081 for broadcasts)
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr =
            INADDR_ANY; // Listen on all interfaces for broadcasts
        serverAddr.sin_port = htons(port);

        // Configure master address (Master uses port 8080)
        masterAddr.sin_family = AF_INET;
        masterAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost
        masterAddr.sin_port = htons(8080);

        if (bind(sock, (sockaddr *)&serverAddr, sizeof(serverAddr)) ==
            SOCKET_ERROR) {
            closesocket(sock);
            throw std::runtime_error("Bind failed");
        }

        // Initialize continuity collector with virtual GPIO
        continuityCollector =
            Adapter::ContinuityCollectorFactory::createWithVirtualGpio();

        Log::i("Slave", "Slave device (ID: 0x%08X) listening on port %d",
               deviceId, port);
        Log::i("Slave", "Master communication port: 8080");
        Log::i("Slave", "Wireless broadcast reception enabled");
    }

    ~SlaveDevice() {
        closesocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
    }

    // Get the current timestamp
    uint32_t getCurrentTimestamp() {
        return static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }

    // Process Master2Slave messages and generate responses
    std::unique_ptr<Message> processAndCreateResponse(const Message &request) {
        switch (request.getMessageId()) {
        case static_cast<uint8_t>(Master2SlaveMessageId::SYNC_MSG): {
            auto syncMsg =
                dynamic_cast<const Master2Slave::SyncMessage *>(&request);
            if (syncMsg) {
                Log::i("MessageProcessor",
                       "Processing sync message - Mode: %d, Timestamp: %u",
                       static_cast<int>(syncMsg->mode), syncMsg->timestamp);

                // 根据TODO要求：收到Sync Message后开始采集
                std::lock_guard<std::mutex> lock(stateMutex);
                if (isConfigured &&
                    deviceState == SlaveDeviceState::CONFIGURED) {
                    Log::i("MessageProcessor",
                           "Starting data collection based on sync message");

                    // 开始采集
                    if (continuityCollector->startCollection()) {
                        deviceState = SlaveDeviceState::COLLECTING;
                        Log::i("MessageProcessor",
                               "Data collection started successfully");
                    } else {
                        Log::e("MessageProcessor",
                               "Failed to start data collection");
                        deviceState = SlaveDeviceState::DEV_ERR;
                    }
                } else {
                    Log::w("MessageProcessor",
                           "Device not configured, cannot start collection");
                }

                return nullptr;
            }
            break;
        }

        case static_cast<uint8_t>(Master2SlaveMessageId::CONDUCTION_CFG_MSG): {
            auto configMsg =
                dynamic_cast<const Master2Slave::ConductionConfigMessage *>(
                    &request);
            if (configMsg) {
                Log::i("MessageProcessor",
                       "Processing conduction configuration - Time slot: %d, "
                       "Interval: %dms",
                       static_cast<int>(configMsg->timeSlot),
                       static_cast<int>(configMsg->interval));

                // 根据TODO要求：收到Conduction Config
                // message后配置ContinuityCollector
                std::lock_guard<std::mutex> lock(stateMutex);

                // 创建采集器配置
                currentConfig = Adapter::CollectorConfig(
                    static_cast<uint8_t>(
                        configMsg->conductionNum), // 导通检测数量
                    static_cast<uint8_t>(
                        configMsg->startConductionNum), // 开始检测数量
                    static_cast<uint8_t>(
                        configMsg->totalConductionNum),        // 总检测数量
                    static_cast<uint32_t>(configMsg->interval) // 检测间隔(ms)
                );

                // 配置采集器
                if (continuityCollector->configure(currentConfig)) {
                    isConfigured = true;
                    deviceState = SlaveDeviceState::CONFIGURED;
                    Log::i("MessageProcessor",
                           "ContinuityCollector configured successfully - "
                           "Pins: %d, Start: %d, Total: %d, Interval: %dms",
                           currentConfig.num, currentConfig.startDetectionNum,
                           currentConfig.totalDetectionNum,
                           currentConfig.interval);
                } else {
                    isConfigured = false;
                    deviceState = SlaveDeviceState::DEV_ERR;
                    Log::e("MessageProcessor",
                           "Failed to configure ContinuityCollector");
                }

                auto response = std::make_unique<
                    Slave2Master::ConductionConfigResponseMessage>();
                response->status = isConfigured ? 0 : 1; // 0=Success, 1=Error
                response->timeSlot = configMsg->timeSlot;
                response->interval = configMsg->interval;
                response->totalConductionNum = configMsg->totalConductionNum;
                response->startConductionNum = configMsg->startConductionNum;
                response->conductionNum = configMsg->conductionNum;
                return std::move(response);
            }
            break;
        }

        case static_cast<uint8_t>(Master2SlaveMessageId::RESISTANCE_CFG_MSG): {
            auto configMsg =
                dynamic_cast<const Master2Slave::ResistanceConfigMessage *>(
                    &request);
            if (configMsg) {
                Log::i("MessageProcessor",
                       "Processing resistance configuration - Time slot: %d, "
                       "Interval: %dms",
                       static_cast<int>(configMsg->timeSlot),
                       static_cast<int>(configMsg->interval));

                auto response = std::make_unique<
                    Slave2Master::ResistanceConfigResponseMessage>();
                response->status = 0; // Success
                response->timeSlot = configMsg->timeSlot;
                response->interval = configMsg->interval;
                response->totalConductionNum = configMsg->totalNum;
                response->startConductionNum = configMsg->startNum;
                response->conductionNum = configMsg->num;
                return std::move(response);
            }
            break;
        }

        case static_cast<uint8_t>(Master2SlaveMessageId::CLIP_CFG_MSG): {
            auto configMsg =
                dynamic_cast<const Master2Slave::ClipConfigMessage *>(&request);
            if (configMsg) {
                Log::i(
                    "MessageProcessor",
                    "Processing clip configuration - Interval: %dms, Mode: %d",
                    static_cast<int>(configMsg->interval),
                    static_cast<int>(configMsg->mode));

                auto response =
                    std::make_unique<Slave2Master::ClipConfigResponseMessage>();
                response->status = 0; // Success
                response->interval = configMsg->interval;
                response->mode = configMsg->mode;
                response->clipPin = configMsg->clipPin;
                return std::move(response);
            }
            break;
        }

        case static_cast<uint8_t>(Master2SlaveMessageId::READ_COND_DATA_MSG): {
            const auto *readCondDataMsg =
                dynamic_cast<const Master2Slave::ReadConductionDataMessage *>(
                    &request);
            if (readCondDataMsg) {
                Log::i("MessageProcessor", "Processing read conduction data");

                auto response =
                    std::make_unique<Slave2Backend::ConductionDataMessage>();

                // 根据TODO要求：从已配置的Collector获得数据并创建response
                std::lock_guard<std::mutex> lock(stateMutex);

                if (isConfigured && continuityCollector) {
                    // 检查采集状态
                    if (deviceState == SlaveDeviceState::COLLECTING) {
                        // 等待完成并获取数据
                        while (!continuityCollector->isCollectionComplete()) {
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(100));
                        }
                        deviceState = SlaveDeviceState::COLLECTION_COMPLETE;
                    }

                    if (deviceState == SlaveDeviceState::COLLECTION_COMPLETE) {
                        // 从采集器获取数据
                        response->conductionData =
                            continuityCollector->getDataVector();
                        response->conductionLength =
                            response->conductionData.size();
                        Log::i("MessageProcessor",
                               "Retrieved %zu bytes of conduction data",
                               response->conductionLength);
                    } else {
                        Log::w("MessageProcessor",
                               "No collection data available, device state: %d",
                               static_cast<int>(deviceState));
                        response->conductionLength = 0;
                        response->conductionData.clear();
                    }
                } else {
                    Log::w("MessageProcessor",
                           "Device not configured or collector not available");
                    response->conductionLength = 0;
                    response->conductionData.clear();
                }

                return std::move(response);
            }

            break;
        }

        case static_cast<uint8_t>(Master2SlaveMessageId::READ_RES_DATA_MSG): {
            const auto *readCondDataMsg =
                dynamic_cast<const Master2Slave::ReadResistanceDataMessage *>(
                    &request);
            if (readCondDataMsg) {
                Log::i("MessageProcessor", "Processing read conduction data");

                auto response =
                    std::make_unique<Slave2Backend::ResistanceDataMessage>();
                response->resistanceLength = 1;
                response->resistanceData = {0x90};
                return std::move(response);
            }
            break;
        }

        case static_cast<uint8_t>(Master2SlaveMessageId::READ_CLIP_DATA_MSG): {
            const auto *readCondDataMsg =
                dynamic_cast<const Master2Slave::ReadClipDataMessage *>(
                    &request);
            if (readCondDataMsg) {
                Log::i("MessageProcessor", "Processing read conduction data");

                auto response =
                    std::make_unique<Slave2Backend::ClipDataMessage>();
                response->clipData = 0xFF;
                return std::move(response);
            }
            break;
        }

        case static_cast<uint8_t>(Master2SlaveMessageId::PING_REQ_MSG): {
            const auto *pingMsg =
                dynamic_cast<const Master2Slave::PingReqMessage *>(&request);
            if (pingMsg) {
                Log::i("MessageProcessor",
                       "Processing Ping request - Sequence number: %d, "
                       "Timestamp: %u",
                       pingMsg->sequenceNumber, pingMsg->timestamp);

                auto response =
                    std::make_unique<Slave2Master::PingRspMessage>();
                response->sequenceNumber = pingMsg->sequenceNumber;
                response->timestamp = getCurrentTimestamp();
                return std::move(response);
            }
            break;
        }

        case static_cast<uint8_t>(Master2SlaveMessageId::RST_MSG): {
            const auto *rstMsg =
                dynamic_cast<const Master2Slave::RstMessage *>(&request);
            if (rstMsg) {
                Log::i("MessageProcessor",
                       "Processing reset message - Lock status: %d",
                       static_cast<int>(rstMsg->lockStatus));

                auto response =
                    std::make_unique<Slave2Master::RstResponseMessage>();
                response->status = 0; // Success
                response->lockStatus = rstMsg->lockStatus;
                response->clipLed = rstMsg->clipLed;
                return std::move(response);
            }
            break;
        }

        case static_cast<uint8_t>(Master2SlaveMessageId::SHORT_ID_ASSIGN_MSG): {
            const auto *assignMsg =
                dynamic_cast<const Master2Slave::ShortIdAssignMessage *>(
                    &request);
            if (assignMsg) {
                Log::i("MessageProcessor",
                       "Processing short ID assignment - Short ID: %d",
                       static_cast<int>(assignMsg->shortId));

                auto response =
                    std::make_unique<Slave2Master::ShortIdConfirmMessage>();
                response->status = 0; // Success
                response->shortId = assignMsg->shortId;
                return std::move(response);
            }
            break;
        }

        default:
            Log::w("MessageProcessor", "Unknown message type: 0x%02X",
                   static_cast<int>(request.getMessageId()));
            break;
        }

        return nullptr;
    }

    // Process received frame
    void processFrame(Frame &frame, const sockaddr_in &masterAddr) {
        Log::i("Slave",
               "Processing frame - PacketId: 0x%02X, payload size: %zu",
               static_cast<int>(frame.packetId), frame.payload.size());

        if (frame.packetId == static_cast<uint8_t>(PacketId::MASTER_TO_SLAVE)) {
            uint32_t targetSlaveId;
            std::unique_ptr<Message> masterMessage;

            if (processor.parseMaster2SlavePacket(frame.payload, targetSlaveId,
                                                  masterMessage)) {
                // Check if this message is for us (or broadcast)
                if (targetSlaveId == deviceId ||
                    targetSlaveId == BROADCAST_ID) {
                    Log::i("Slave",
                           "Processing Master2Slave message for device 0x%08X, "
                           "Message ID: 0x%02X",
                           targetSlaveId,
                           static_cast<int>(masterMessage->getMessageId()));

                    // Process message and create response
                    auto response = processAndCreateResponse(*masterMessage);

                    if (response) {
                        Log::i("Slave", "Generated response message");

                        std::vector<std::vector<uint8_t>> responseData;
                        DeviceStatus deviceStatus = {};
                        if (response->getMessageId() ==
                                static_cast<uint8_t>(Slave2BackendMessageId::
                                                         CONDUCTION_DATA_MSG) ||
                            response->getMessageId() ==
                                static_cast<uint8_t>(Slave2BackendMessageId::
                                                         RESISTANCE_DATA_MSG) ||
                            response->getMessageId() ==
                                static_cast<uint8_t>(
                                    Slave2BackendMessageId::CLIP_DATA_MSG)) {
                            Log::i("ResponseSender",
                                   "Packing Slave2Backend message");
                            responseData = processor.packSlave2BackendMessage(
                                deviceId, deviceStatus, *response);
                        } else {
                            responseData = processor.packSlave2MasterMessage(
                                deviceId, *response);
                        }

                        Log::i("ResponseSender", "Sending response:");

                        // Send all fragments
                        for (const auto &fragment : responseData) {
                            // printBytes(fragment, "Response data");
                            // Send response
                            // Send response to master on port 8080
                            sendto(
                                sock,
                                reinterpret_cast<const char *>(fragment.data()),
                                static_cast<int>(fragment.size()), 0,
                                (sockaddr *)&this->masterAddr,
                                sizeof(this->masterAddr));
                        }
                    }
                } else {
                    Log::d("Slave",
                           "Message not for this device (target: 0x%08X, our "
                           "ID: 0x%08X)",
                           targetSlaveId, deviceId);
                }
            } else {
                Log::e("Slave", "Failed to parse Master2Slave packet");
            }
        } else {
            Log::w("Slave", "Unsupported packet type for Slave: 0x%02X",
                   static_cast<int>(frame.packetId));
        }
    }

    // Main loop
    void run() {
        Log::i("Slave", "Slave device started");
        Log::i("Slave", "Device ID: 0x%08X, listening on port %d", deviceId,
               port);
        Log::i("Slave", "Handling Master2Slave broadcast packets");
        Log::i("Slave", "Sending responses to Master on port 8080");

        char buffer[1024];
        sockaddr_in
            senderAddr; // Renamed to avoid confusion with member masterAddr
        socklen_t senderAddrLen = sizeof(senderAddr);

        processor.setMTU(100);

        while (true) {
            int bytesReceived =
                recvfrom(sock, buffer, sizeof(buffer), 0,
                         (sockaddr *)&senderAddr, &senderAddrLen);

            if (bytesReceived > 0) {
                std::vector<uint8_t> data(buffer, buffer + bytesReceived);

                processor.processReceivedData(data);
                Frame receivedFrame;
                while (processor.getNextCompleteFrame(receivedFrame)) {
                    processFrame(receivedFrame, senderAddr);
                }
            }
        }
    }
};

// Helper function to get Device ID from user input
uint32_t getDeviceIdFromUser() {
    std::string input;
    uint32_t deviceId = 0x00000001; // Default value

    std::cout << "\n=== WhtsProtocol Slave Device Configuration ==="
              << std::endl;
    std::cout << "Please enter Device ID (hex format, e.g., 0x3732485B)"
              << std::endl;
    std::cout << "Press Enter for default (0x00000001): ";

    std::getline(std::cin, input);

    // Trim whitespace
    input.erase(input.find_last_not_of(" \t\r\n") + 1);
    input.erase(0, input.find_first_not_of(" \t\r\n"));

    if (!input.empty()) {
        try {
            // Support both 0x prefix and without prefix
            if (input.substr(0, 2) == "0x" || input.substr(0, 2) == "0X") {
                deviceId = std::stoul(input, nullptr, 16);
            } else {
                deviceId = std::stoul(input, nullptr, 16);
            }
            std::cout << "Using Device ID: 0x" << std::hex << std::uppercase
                      << deviceId << std::endl;
        } catch (const std::exception &e) {
            std::cout << "Invalid input, using default Device ID: 0x00000001"
                      << std::endl;
            deviceId = 0x00000001;
        }
    } else {
        std::cout << "Using default Device ID: 0x00000001" << std::endl;
    }

    return deviceId;
}

int main() {
    Log::i("Main", "WhtsProtocol Slave Device");
    Log::i("Main", "=========================");

    try {
        // Get Device ID from user input
        uint32_t deviceId = getDeviceIdFromUser();

        // Display configuration after getting Device ID
        Log::i("Main", "\nPort Configuration (Wireless Broadcast Simulation):");
        Log::i("Main", "  Backend: 8079");
        Log::i("Main", "  Master:  8080 (receives responses from Slaves)");
        Log::i("Main",
               "  Slaves:  8081 (listen for Master broadcast commands)");
        Log::i("Main", "Wireless Communication Simulation:");
        Log::i("Main", "  Receives: Broadcast commands from Master");
        Log::i("Main", "  Sends: Unicast responses to Master");
        Log::i("Main", "Device Configuration:");
        Log::i("Main", "  Device ID: 0x%08X", deviceId);

        std::cout << "\nStarting slave device..." << std::endl;

        SlaveDevice device(8081, deviceId);
        device.run();
    } catch (const std::exception &e) {
        Log::e("Main", "Error: %s", e.what());
        return 1;
    }

    return 0;
}