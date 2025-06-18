#include "ContinuityCollector.h"
#include "Logger.h"
#include "WhtsProtocol.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
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
    ProtocolProcessor processor;
    uint16_t port;
    uint32_t deviceId;
    std::unique_ptr<Adapter::ContinuityCollector> continuityCollector;

  public:
    SlaveDevice(uint16_t listenPort = 8889, uint32_t id = 0x3732485B)
        : port(listenPort), deviceId(id) {
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

        // Initialize continuity collector with virtual GPIO
        continuityCollector =
            Adapter::ContinuityCollectorFactory::createWithVirtualGpio();

        Log::i("Slave", "Slave device (ID: 0x%08X) listening on port %d",
               deviceId, port);
    }

    ~SlaveDevice() {
        closesocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
    }

    // Convert a hexadecimal string to a byte array
    std::vector<uint8_t> hexStringToBytes(const std::string &hex) {
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < hex.length(); i += 2) {
            if (i + 1 < hex.length()) {
                std::string byteString = hex.substr(i, 2);
                uint8_t byte = static_cast<uint8_t>(
                    strtol(byteString.c_str(), nullptr, 16));
                bytes.push_back(byte);
            }
        }
        return bytes;
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
            auto readCondDataMsg =
                dynamic_cast<const Master2Slave::ReadConductionDataMessage *>(
                    &request);
            if (readCondDataMsg) {
                Log::i("MessageProcessor", "Processing read conduction data");

                auto response =
                    std::make_unique<Slave2Backend::ConductionDataMessage>();
                // 创建采集器
                auto collector = Adapter::ContinuityCollectorFactory::
                    createWithVirtualGpio();

                // 配置采集
                Adapter::CollectorConfig config(
                    2, 0, 4, 20); // 2引脚, 从周期0开始, 总共4周期, 20ms间隔
                collector->configure(config);

                // 开始采集
                collector->startCollection();

                // 等待完成并获取数据
                while (!collector->isCollectionComplete()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                response->conductionData = collector->getDataVector();
                response->conductionLength = response->conductionData.size();
                return std::move(response);
            }

            break;
        }

        case static_cast<uint8_t>(Master2SlaveMessageId::READ_RES_DATA_MSG): {
            auto readCondDataMsg =
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
            auto readCondDataMsg =
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
            auto pingMsg =
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
            auto rstMsg =
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
            auto assignMsg =
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
    void processFrame(Frame &frame, const sockaddr_in & /* masterAddr */) {
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
        Log::i("Slave", "Handling Master2Slave packets only");

        char buffer[1024];
        sockaddr_in masterAddr;
        socklen_t masterAddrLen = sizeof(masterAddr);

        processor.setMTU(100);

        while (true) {
            int bytesReceived =
                recvfrom(sock, buffer, sizeof(buffer), 0,
                         (sockaddr *)&masterAddr, &masterAddrLen);

            if (bytesReceived > 0) {
                std::vector<uint8_t> data(buffer, buffer + bytesReceived);

                processor.processReceivedData(data);
                Frame receivedFrame;
                while (processor.getNextCompleteFrame(receivedFrame)) {
                    processFrame(receivedFrame, masterAddr);
                }
            }
        }
    }
};

int main() {
    Log::i("Main", "WhtsProtocol Slave Device");
    Log::i("Main", "=========================");

    try {
        SlaveDevice device(8889, 0x3732485B);
        device.run();
    } catch (const std::exception &e) {
        Log::e("Main", "Error: %s", e.what());
        return 1;
    }

    return 0;
}