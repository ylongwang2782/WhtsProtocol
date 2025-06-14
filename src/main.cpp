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

class UdpProtocolTester {
  private:
    SOCKET sock;
    sockaddr_in serverAddr;
    ProtocolProcessor processor;
    uint16_t port;

  public:
    UdpProtocolTester(uint16_t listenPort = 8888) : port(listenPort) {
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

        Log::i("UdpTester", "UDP Protocol Tester listening on port %d", port);
    }

    ~UdpProtocolTester() {
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
        Log::d("UdpTester", "%s (%zu bytes): %s", description.c_str(),
               data.size(), ss.str().c_str());
    }

    // Get the current timestamp
    uint32_t getCurrentTimestamp() {
        return static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }

    // Process Master2Slave messages and generate responses
    std::unique_ptr<Message> processAndCreateResponse(uint32_t slaveId,
                                                      const Message &request) {
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

    // Process the received data
    void processFrame(Frame &frame, const sockaddr_in &clientAddr) {

        Log::i("FrameProcessor", "Frame parsed successfully:");
        Log::i("FrameProcessor", "Packet type: 0x%02X",
               static_cast<int>(frame.packetId));
        Log::i("FrameProcessor", "Fragment sequence: %d",
               static_cast<int>(frame.fragmentsSequence));
        Log::i("FrameProcessor", "More fragments: %d",
               static_cast<int>(frame.moreFragmentsFlag));
        Log::i("FrameProcessor", "Payload length: %d", frame.packetLength);

        // Process by packet type
        if (frame.packetId == static_cast<uint8_t>(PacketId::MASTER_TO_SLAVE)) {
            uint32_t destinationId;
            std::unique_ptr<Message> message;

            if (processor.parseMaster2SlavePacket(frame.payload, destinationId,
                                                  message)) {
                Log::i("PacketProcessor",
                       "Master2Slave packet parsed successfully:");
                Log::i("PacketProcessor", "Destination ID: 0x%08X",
                       destinationId);
                Log::i("PacketProcessor", "Message ID: 0x%02X",
                       static_cast<int>(message->getMessageId()));

                // Simulate slave ID
                uint32_t slaveId = 0x12345678;

                // Process message and generate response
                auto response = processAndCreateResponse(slaveId, *message);

                if (response) {
                    // Pack response message
                    auto responseData =
                        processor.packSlave2MasterMessage(slaveId, *response);

                    Log::i("ResponseSender", "Sending response:");

                    // Send all fragments
                    for (const auto &fragment : responseData) {
                        printBytes(fragment, "Response data");
                        // Send response
                        sendto(sock,
                               reinterpret_cast<const char *>(fragment.data()),
                               static_cast<int>(fragment.size()), 0,
                               (sockaddr *)&clientAddr, sizeof(clientAddr));
                    }

                    Log::i("ResponseSender", "Response sent");
                } else {
                    Log::i("ResponseSender",
                           "No response needed for this message");
                }
            } else {
                Log::e("PacketProcessor",
                       "Failed to parse Master2Slave packet");
            }
        } else {
            Log::w("PacketProcessor", "Unsupported packet type: 0x%02X",
                   static_cast<int>(frame.packetId));
        }
    }

    // Main loop
    void run() {
        Log::i("UdpTester",
               "Protocol tester started, waiting for UDP messages...");
        Log::i("UdpTester", "Tip: Send hexadecimal data to localhost:%d", port);
        Log::i("UdpTester", "Press Ctrl+C to exit");

        char buffer[1024];
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        processor.setMTU(14);

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
                    Log::i("DataReceiver", "Received hexadecimal string: %s",
                           receivedStr.c_str());
                } else {
                    // Otherwise, treat it as binary data directly
                    data.assign(buffer, buffer + bytesReceived);
                    Log::i("DataReceiver", "Received binary data");
                }

                if (!data.empty()) {
                    // processReceivedData(data, clientAddr);
                    processor.processReceivedData(data);
                    Frame receivedFrame;
                    int frameCount = 0;
                    while (processor.getNextCompleteFrame(receivedFrame)) {
                        frameCount++;
                        Log::i("FrameParser",
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
    Log::i("Main", "WhtsProtocol UDP tester");
    Log::i("Main", "======================");

    try {
        UdpProtocolTester tester(8888);
        tester.run();
    } catch (const std::exception &e) {
        Log::e("Main", "Error: %s", e.what());
        return 1;
    }

    return 0;
}