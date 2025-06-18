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

class MasterServer {
  private:
    SOCKET sock;
    sockaddr_in serverAddr;
    ProtocolProcessor processor;
    uint16_t port;

  public:
    MasterServer(uint16_t listenPort = 8888) : port(listenPort) {
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

        Log::i("Master", "Master server listening on port %d", port);
    }

    ~MasterServer() {
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

    // Convert a byte array to a hexadecimal string
    std::string bytesToHexString(const std::vector<uint8_t> &bytes) {
        std::stringstream ss;
        for (uint8_t byte : bytes) {
            ss << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(byte);
        }
        return ss.str();
    }

    // Print the byte array
    void printBytes(const std::vector<uint8_t> &data,
                    const std::string &description) {
        std::stringstream ss;
        for (auto byte : data) {
            ss << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(byte) << " ";
        }
        Log::d("Master", "%s (%zu bytes): %s", description.c_str(), data.size(),
               ss.str().c_str());
    }

    // Get the current timestamp
    uint32_t getCurrentTimestamp() {
        return static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }

    // Process Backend2Master messages
    void processBackend2MasterMessage(const Message &message,
                                      const sockaddr_in & /* clientAddr */) {
        Log::i("Master", "Processing Backend2Master message, ID: 0x%02X",
               static_cast<int>(message.getMessageId()));

        switch (message.getMessageId()) {
        case static_cast<uint8_t>(Backend2MasterMessageId::SLAVE_CFG_MSG): {
            auto configMsg =
                dynamic_cast<const Backend2Master::SlaveConfigMessage *>(
                    &message);
            if (configMsg) {
                Log::i("Master",
                       "Received slave config message - Slave count: %d",
                       static_cast<int>(configMsg->slaveNum));
                for (const auto &slave : configMsg->slaves) {
                    Log::i("Master", "  Slave ID: 0x%08X", slave.id);
                }
                // TODO: Forward to slaves and generate response
            }
            break;
        }
        case static_cast<uint8_t>(Backend2MasterMessageId::MODE_CFG_MSG): {
            auto modeMsg =
                dynamic_cast<const Backend2Master::ModeConfigMessage *>(
                    &message);
            if (modeMsg) {
                Log::i("Master", "Received mode config message - Mode: %d",
                       static_cast<int>(modeMsg->mode));
                // TODO: Process mode configuration
            }
            break;
        }
        case static_cast<uint8_t>(Backend2MasterMessageId::SLAVE_RST_MSG): {
            auto resetMsg =
                dynamic_cast<const Backend2Master::RstMessage *>(&message);
            if (resetMsg) {
                Log::i("Master",
                       "Received slave reset message - Slave count: %d",
                       static_cast<int>(resetMsg->slaveNum));
                // TODO: Forward reset command to slaves
            }
            break;
        }
        case static_cast<uint8_t>(Backend2MasterMessageId::CTRL_MSG): {
            auto ctrlMsg =
                dynamic_cast<const Backend2Master::CtrlMessage *>(&message);
            if (ctrlMsg) {
                Log::i("Master", "Received control message - Status: %d",
                       static_cast<int>(ctrlMsg->runningStatus));
                // TODO: Process control command
            }
            break;
        }
        case static_cast<uint8_t>(Backend2MasterMessageId::PING_CTRL_MSG): {
            auto pingMsg =
                dynamic_cast<const Backend2Master::PingCtrlMessage *>(&message);
            if (pingMsg) {
                Log::i(
                    "Master",
                    "Received ping control message - Mode: %d, Target: 0x%08X",
                    static_cast<int>(pingMsg->pingMode),
                    pingMsg->destinationId);
                // TODO: Execute ping command
            }
            break;
        }
        case static_cast<uint8_t>(
            Backend2MasterMessageId::DEVICE_LIST_REQ_MSG): {
            auto deviceListMsg =
                dynamic_cast<const Backend2Master::DeviceListReqMessage *>(
                    &message);
            if (deviceListMsg) {
                Log::i("Master", "Received device list request message");
                // TODO: Generate device list response
            }
            break;
        }
        default:
            Log::w("Master", "Unknown Backend2Master message type: 0x%02X",
                   static_cast<int>(message.getMessageId()));
            break;
        }
    }

    // Process Slave2Master messages
    void processSlave2MasterMessage(uint32_t slaveId, const Message &message,
                                    const sockaddr_in & /* clientAddr */) {
        Log::i("Master",
               "Processing Slave2Master message from slave 0x%08X, ID: 0x%02X",
               slaveId, static_cast<int>(message.getMessageId()));

        switch (message.getMessageId()) {
        case static_cast<uint8_t>(
            Slave2MasterMessageId::CONDUCTION_CFG_RSP_MSG): {
            auto configRspMsg = dynamic_cast<
                const Slave2Master::ConductionConfigResponseMessage *>(
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
        case static_cast<uint8_t>(
            Slave2MasterMessageId::RESISTANCE_CFG_RSP_MSG): {
            auto configRspMsg = dynamic_cast<
                const Slave2Master::ResistanceConfigResponseMessage *>(
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
            auto configRspMsg =
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
            auto resetRspMsg =
                dynamic_cast<const Slave2Master::RstResponseMessage *>(
                    &message);
            if (resetRspMsg) {
                Log::i("Master", "Received reset response - Status: %d",
                       static_cast<int>(resetRspMsg->status));
                // TODO: Forward response to backend
            }
            break;
        }
        case static_cast<uint8_t>(Slave2MasterMessageId::PING_RSP_MSG): {
            auto pingRspMsg =
                dynamic_cast<const Slave2Master::PingRspMessage *>(&message);
            if (pingRspMsg) {
                Log::i("Master", "Received ping response - Sequence: %d",
                       static_cast<int>(pingRspMsg->sequenceNumber));
                // TODO: Forward response to backend
            }
            break;
        }
        case static_cast<uint8_t>(Slave2MasterMessageId::ANNOUNCE_MSG): {
            auto announceMsg =
                dynamic_cast<const Slave2Master::AnnounceMessage *>(&message);
            if (announceMsg) {
                Log::i("Master",
                       "Received announce message - Version: %d.%d.%d",
                       static_cast<int>(announceMsg->versionMajor),
                       static_cast<int>(announceMsg->versionMinor),
                       announceMsg->versionPatch);
                // TODO: Process device announcement
            }
            break;
        }
        case static_cast<uint8_t>(
            Slave2MasterMessageId::SHORT_ID_CONFIRM_MSG): {
            auto confirmMsg =
                dynamic_cast<const Slave2Master::ShortIdConfirmMessage *>(
                    &message);
            if (confirmMsg) {
                Log::i("Master",
                       "Received short ID confirm message - Short ID: %d",
                       static_cast<int>(confirmMsg->shortId));
                // TODO: Process short ID confirmation
            }
            break;
        }
        default:
            Log::w("Master", "Unknown Slave2Master message type: 0x%02X",
                   static_cast<int>(message.getMessageId()));
            break;
        }
    }

    // Process received frame
    void processFrame(Frame &frame, const sockaddr_in &clientAddr) {
        Log::i("Master",
               "Processing frame - PacketId: 0x%02X, payload size: %zu",
               static_cast<int>(frame.packetId), frame.payload.size());

        if (frame.packetId ==
            static_cast<uint8_t>(PacketId::BACKEND_TO_MASTER)) {
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

    // Main loop
    void run() {
        Log::i("Master", "Master server started, waiting for UDP messages...");
        Log::i(
            "Master",
            "Listening on port %d for Backend2Master and Slave2Master packets",
            port);
        Log::i("Master", "Press Ctrl+C to exit");

        char buffer[1024];
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        processor.setMTU(100);

        while (true) {
            int bytesReceived =
                recvfrom(sock, buffer, sizeof(buffer), 0,
                         (sockaddr *)&clientAddr, &clientAddrLen);

            if (bytesReceived > 0) {
                // Check if it's in hexadecimal string format
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
};

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