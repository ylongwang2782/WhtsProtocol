#include "ContinuityCollector.h"
#include "Logger.h"
#include "WhtsProtocol.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
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

class SlaveDevice {
  private:
    SOCKET sock;
    sockaddr_in serverAddr;
    sockaddr_in masterAddr; // Master address (port 8080)
    ProtocolProcessor processor;
    uint16_t port;
    uint32_t deviceId;
    std::unique_ptr<Adapter::ContinuityCollector> continuityCollector;
    bool running;

  public:
    SlaveDevice(uint16_t listenPort, uint32_t id)
        : port(listenPort), deviceId(id), running(false) {
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
            Log::w("Slave",
                   "Failed to enable broadcast option for device 0x%08X",
                   deviceId);
        }

        // Allow address reuse for multiple slaves on same port
        int reuse = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse,
                       sizeof(reuse)) == SOCKET_ERROR) {
            Log::w("Slave", "Failed to enable address reuse for device 0x%08X",
                   deviceId);
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
            throw std::runtime_error("Bind failed for device " +
                                     std::to_string(deviceId));
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
        running = false;
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
            const auto *syncMsg =
                dynamic_cast<const Master2Slave::SyncMessage *>(&request);
            if (syncMsg) {
                Log::i("MessageProcessor",
                       "[0x%08X] Processing sync message - Mode: %d, "
                       "Timestamp: %u",
                       deviceId, static_cast<int>(syncMsg->mode),
                       syncMsg->timestamp);
                // Sync messages typically don't have responses
            }
            break;
        }

        case static_cast<uint8_t>(Master2SlaveMessageId::CONDUCTION_CFG_MSG): {
            const auto *configMsg =
                dynamic_cast<const Master2Slave::ConductionConfigMessage *>(
                    &request);
            if (configMsg) {
                Log::i("MessageProcessor",
                       "[0x%08X] Processing conduction configuration - Time "
                       "slot: %d, Interval: %dms",
                       deviceId, static_cast<int>(configMsg->timeSlot),
                       static_cast<int>(configMsg->interval));

                auto response = std::make_unique<
                    Slave2Master::ConductionConfigResponseMessage>();
                response->status = 0; // Success
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
            const auto *configMsg =
                dynamic_cast<const Master2Slave::ResistanceConfigMessage *>(
                    &request);
            if (configMsg) {
                Log::i("MessageProcessor",
                       "[0x%08X] Processing resistance configuration - Time "
                       "slot: %d, Interval: %dms",
                       deviceId, static_cast<int>(configMsg->timeSlot),
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
            const auto *configMsg =
                dynamic_cast<const Master2Slave::ClipConfigMessage *>(&request);
            if (configMsg) {
                Log::i("MessageProcessor",
                       "[0x%08X] Processing clip configuration - Interval: "
                       "%dms, Mode: %d",
                       deviceId, static_cast<int>(configMsg->interval),
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

        case static_cast<uint8_t>(Master2SlaveMessageId::PING_REQ_MSG): {
            const auto *pingMsg =
                dynamic_cast<const Master2Slave::PingReqMessage *>(&request);
            if (pingMsg) {
                Log::i("MessageProcessor",
                       "[0x%08X] Processing Ping request - Sequence number: "
                       "%d, Timestamp: %u",
                       deviceId, pingMsg->sequenceNumber, pingMsg->timestamp);

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
                       "[0x%08X] Processing reset message - Lock status: %d",
                       deviceId, static_cast<int>(rstMsg->lockStatus));

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
                       "[0x%08X] Processing short ID assignment - Short ID: %d",
                       deviceId, static_cast<int>(assignMsg->shortId));

                auto response =
                    std::make_unique<Slave2Master::ShortIdConfirmMessage>();
                response->status = 0; // Success
                response->shortId = assignMsg->shortId;
                return std::move(response);
            }
            break;
        }

        default:
            Log::w("MessageProcessor", "[0x%08X] Unknown message type: 0x%02X",
                   deviceId, static_cast<int>(request.getMessageId()));
            break;
        }

        return nullptr;
    }

    // Process received frame
    void processFrame(Frame &frame, const sockaddr_in &senderAddr) {
        Log::i(
            "Slave",
            "[0x%08X] Processing frame - PacketId: 0x%02X, payload size: %zu",
            deviceId, static_cast<int>(frame.packetId), frame.payload.size());

        if (frame.packetId == static_cast<uint8_t>(PacketId::MASTER_TO_SLAVE)) {
            uint32_t targetSlaveId;
            std::unique_ptr<Message> masterMessage;

            if (processor.parseMaster2SlavePacket(frame.payload, targetSlaveId,
                                                  masterMessage)) {
                // Check if this message is for us (or broadcast)
                if (targetSlaveId == deviceId ||
                    targetSlaveId == BROADCAST_ID) {
                    Log::i("Slave",
                           "[0x%08X] Processing Master2Slave message for "
                           "device 0x%08X, Message ID: 0x%02X",
                           deviceId, targetSlaveId,
                           static_cast<int>(masterMessage->getMessageId()));

                    // Process message and create response
                    auto response = processAndCreateResponse(*masterMessage);

                    if (response) {
                        Log::i("Slave", "[0x%08X] Generated response message",
                               deviceId);

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
                                   "[0x%08X] Packing Slave2Backend message",
                                   deviceId);
                            responseData = processor.packSlave2BackendMessage(
                                deviceId, deviceStatus, *response);
                        } else {
                            responseData = processor.packSlave2MasterMessage(
                                deviceId, *response);
                        }

                        Log::i("ResponseSender",
                               "[0x%08X] Sending response:", deviceId);

                        // Send all fragments
                        for (const auto &fragment : responseData) {
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
                           "[0x%08X] Message not for this device (target: "
                           "0x%08X, our ID: 0x%08X)",
                           deviceId, targetSlaveId, deviceId);
                }
            } else {
                Log::e("Slave", "[0x%08X] Failed to parse Master2Slave packet",
                       deviceId);
            }
        } else {
            Log::w("Slave",
                   "[0x%08X] Unsupported packet type for Slave: 0x%02X",
                   deviceId, static_cast<int>(frame.packetId));
        }
    }

    // Main loop
    void run() {
        running = true;
        Log::i("Slave", "[0x%08X] Slave device started", deviceId);
        Log::i("Slave", "[0x%08X] Device ID: 0x%08X, listening on port %d",
               deviceId, deviceId, port);
        Log::i("Slave", "[0x%08X] Handling Master2Slave broadcast packets",
               deviceId);
        Log::i("Slave", "[0x%08X] Sending responses to Master on port 8080",
               deviceId);

        char buffer[1024];
        sockaddr_in senderAddr;
        socklen_t senderAddrLen = sizeof(senderAddr);

        processor.setMTU(100);

        while (running) {
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

    void stop() { running = false; }
};

// Multi-slave launcher
class MultiSlaveManager {
  private:
    std::vector<std::unique_ptr<SlaveDevice>> slaves;
    std::vector<std::thread> slaveThreads;
    bool running;

  public:
    MultiSlaveManager() : running(false) {}

    ~MultiSlaveManager() { stop(); }

    void addSlave(uint32_t deviceId) {
        try {
            auto slave = std::make_unique<SlaveDevice>(8081, deviceId);
            slaves.push_back(std::move(slave));
            Log::i("MultiSlave", "Added slave device 0x%08X", deviceId);
        } catch (const std::exception &e) {
            Log::e("MultiSlave", "Failed to create slave 0x%08X: %s", deviceId,
                   e.what());
        }
    }

    void start() {
        running = true;
        Log::i("MultiSlave", "Starting %zu slave devices...", slaves.size());

        for (auto &slave : slaves) {
            slaveThreads.emplace_back([&slave]() { slave->run(); });
        }

        Log::i("MultiSlave", "All slave devices started");
    }

    void stop() {
        if (!running)
            return;

        running = false;
        Log::i("MultiSlave", "Stopping all slave devices...");

        for (auto &slave : slaves) {
            slave->stop();
        }

        for (auto &thread : slaveThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        Log::i("MultiSlave", "All slave devices stopped");
    }

    void waitForAll() {
        for (auto &thread : slaveThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
};

int main() {
    Log::i("Main", "WhtsProtocol Multi-Slave Device Simulator");
    Log::i("Main", "==========================================");
    Log::i("Main", "Port Configuration (Wireless Broadcast Simulation):");
    Log::i("Main", "  Backend: 8079");
    Log::i("Main", "  Master:  8080 (receives responses from Slaves)");
    Log::i("Main", "  Slaves:  8081 (listen for Master broadcast commands)");
    Log::i("Main", "Wireless Communication Simulation:");
    Log::i("Main", "  Receives: Broadcast commands from Master");
    Log::i("Main", "  Sends: Unicast responses to Master");

    try {
        MultiSlaveManager manager;

        // Get number of slaves from user input
        int numSlaves = 0;
        std::cout << "\nPlease enter the number of slave devices to simulate "
                     "(1-99): ";
        std::cin >> numSlaves;

        // Validate input
        if (numSlaves < 1 || numSlaves > 99) {
            Log::e("Main",
                   "Invalid number of slaves. Must be between 1 and 99.");
            return 1;
        }

        Log::i("Main", "Creating %d slave devices...", numSlaves);

        // Create slaves with sequential device IDs starting from 0x00000001
        std::vector<uint32_t> deviceIds;
        for (int i = 0; i < numSlaves; i++) {
            uint32_t deviceId = 0x00000001 + i;
            manager.addSlave(deviceId);
            deviceIds.push_back(deviceId);
        }

        // Output all device IDs in a list format
        Log::i("Main", "Successfully created %d slave devices:", numSlaves);
        Log::i("Main", "Device ID List:");
        for (size_t i = 0; i < deviceIds.size(); i++) {
            Log::i("Main", "  [%zu] Device ID: 0x%08X", i + 1, deviceIds[i]);
        }

        Log::i("Main", "Starting multi-slave simulation...");
        manager.start();

        Log::i("Main", "Multi-slave simulation running. Press Ctrl+C to exit");
        manager.waitForAll();

    } catch (const std::exception &e) {
        Log::e("Main", "Error: %s", e.what());
        return 1;
    }

    return 0;
}