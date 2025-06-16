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

        case static_cast<uint8_t>(Master2SlaveMessageId::READ_COND_DATA_MSG): {
            auto readCondDataMsg =
                dynamic_cast<const Master2Slave::ReadConductionDataMessage *>(
                    &request);
            if (readCondDataMsg) {
                Log::i("MessageProcessor", "Processing read conduction data");

                auto response =
                    std::make_unique<Slave2Backend::ConductionDataMessage>();
                response->conductionLength = 1;
                response->conductionData = {0x90};
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

    // Process Backend2Master messages and generate Master2Backend responses
    std::unique_ptr<Message>
    processBackend2MasterAndCreateResponse(const Message &request) {
        switch (request.getMessageId()) {
        case static_cast<uint8_t>(Backend2MasterMessageId::SLAVE_CFG_MSG): {
            auto configMsg =
                dynamic_cast<const Backend2Master::SlaveConfigMessage *>(
                    &request);
            if (configMsg) {
                Log::i("MessageProcessor",
                       "Processing slave configuration - Slave count: %d",
                       static_cast<int>(configMsg->slaveNum));

                for (const auto &slave : configMsg->slaves) {
                    Log::i("MessageProcessor",
                           "  Slave ID: 0x%08X, Conduction: %d, Resistance: "
                           "%d, Clip "
                           "mode: %d",
                           slave.id, static_cast<int>(slave.conductionNum),
                           static_cast<int>(slave.resistanceNum),
                           static_cast<int>(slave.clipMode));
                }

                auto response = std::make_unique<
                    Master2Backend::SlaveConfigResponseMessage>();
                response->status = 0; // Success
                response->slaveNum = configMsg->slaveNum;
                // Copy slaves info manually
                for (const auto &slave : configMsg->slaves) {
                    Master2Backend::SlaveConfigResponseMessage::SlaveInfo
                        slaveInfo;
                    slaveInfo.id = slave.id;
                    slaveInfo.conductionNum = slave.conductionNum;
                    slaveInfo.resistanceNum = slave.resistanceNum;
                    slaveInfo.clipMode = slave.clipMode;
                    slaveInfo.clipStatus = slave.clipStatus;
                    response->slaves.push_back(slaveInfo);
                }
                return std::move(response);
            }
            break;
        }

        case static_cast<uint8_t>(Backend2MasterMessageId::MODE_CFG_MSG): {
            auto modeMsg =
                dynamic_cast<const Backend2Master::ModeConfigMessage *>(
                    &request);
            if (modeMsg) {
                Log::i("MessageProcessor",
                       "Processing mode configuration - Mode: %d",
                       static_cast<int>(modeMsg->mode));

                auto response = std::make_unique<
                    Master2Backend::ModeConfigResponseMessage>();
                response->status = 0; // Success
                response->mode = modeMsg->mode;
                return std::move(response);
            }
            break;
        }

        case static_cast<uint8_t>(Backend2MasterMessageId::RST_MSG): {
            auto rstMsg =
                dynamic_cast<const Backend2Master::RstMessage *>(&request);
            if (rstMsg) {
                Log::i("MessageProcessor",
                       "Processing reset message - Slave count: %d",
                       static_cast<int>(rstMsg->slaveNum));

                for (const auto &slave : rstMsg->slaves) {
                    Log::i("MessageProcessor",
                           "  Reset Slave ID: 0x%08X, Lock: %d, Clip status: "
                           "0x%04X",
                           slave.id, static_cast<int>(slave.lock),
                           slave.clipStatus);
                }

                auto response =
                    std::make_unique<Master2Backend::RstResponseMessage>();
                response->status = 0; // Success
                response->slaveNum = rstMsg->slaveNum;
                // Copy slaves reset info manually
                for (const auto &slave : rstMsg->slaves) {
                    Master2Backend::RstResponseMessage::SlaveRstInfo
                        slaveRstInfo;
                    slaveRstInfo.id = slave.id;
                    slaveRstInfo.lock = slave.lock;
                    slaveRstInfo.clipStatus = slave.clipStatus;
                    response->slaves.push_back(slaveRstInfo);
                }
                return std::move(response);
            }
            break;
        }

        case static_cast<uint8_t>(Backend2MasterMessageId::CTRL_MSG): {
            auto ctrlMsg =
                dynamic_cast<const Backend2Master::CtrlMessage *>(&request);
            if (ctrlMsg) {
                Log::i("MessageProcessor",
                       "Processing control message - Running status: %d",
                       static_cast<int>(ctrlMsg->runningStatus));

                auto response =
                    std::make_unique<Master2Backend::CtrlResponseMessage>();
                response->status = 0; // Success
                response->runningStatus = ctrlMsg->runningStatus;
                return std::move(response);
            }
            break;
        }

        case static_cast<uint8_t>(Backend2MasterMessageId::PING_CTRL_MSG): {
            auto pingCtrlMsg =
                dynamic_cast<const Backend2Master::PingCtrlMessage *>(&request);
            if (pingCtrlMsg) {
                Log::i("MessageProcessor",
                       "Processing ping control - Mode: %d, Count: %d, "
                       "Interval: %dms, "
                       "Target: 0x%08X",
                       static_cast<int>(pingCtrlMsg->pingMode),
                       pingCtrlMsg->pingCount, pingCtrlMsg->interval,
                       pingCtrlMsg->destinationId);

                auto response =
                    std::make_unique<Master2Backend::PingResponseMessage>();
                response->pingMode = pingCtrlMsg->pingMode;
                response->totalCount = pingCtrlMsg->pingCount;
                response->successCount =
                    pingCtrlMsg->pingCount - 1; // Simulate 1 failure
                response->destinationId = pingCtrlMsg->destinationId;
                return std::move(response);
            }
            break;
        }

        case static_cast<uint8_t>(
            Backend2MasterMessageId::DEVICE_LIST_REQ_MSG): {
            auto deviceListReqMsg =
                dynamic_cast<const Backend2Master::DeviceListReqMessage *>(
                    &request);
            if (deviceListReqMsg) {
                Log::i("MessageProcessor", "Processing device list request");

                auto response = std::make_unique<
                    Master2Backend::DeviceListResponseMessage>();
                response->deviceCount = 2; // Simulate 2 devices

                // Add first device
                Master2Backend::DeviceListResponseMessage::DeviceInfo device1;
                device1.deviceId = 0x12345678;
                device1.shortId = 1;
                device1.online = 1;
                device1.versionMajor = 1;
                device1.versionMinor = 2;
                device1.versionPatch = 3;
                response->devices.push_back(device1);

                // Add second device
                Master2Backend::DeviceListResponseMessage::DeviceInfo device2;
                device2.deviceId = 0x87654321;
                device2.shortId = 2;
                device2.online = 0; // Offline
                device2.versionMajor = 1;
                device2.versionMinor = 1;
                device2.versionPatch = 0;
                response->devices.push_back(device2);

                Log::i("MessageProcessor",
                       "Returning device list with %d devices",
                       response->deviceCount);
                return std::move(response);
            }
            break;
        }

        default:
            Log::w("MessageProcessor",
                   "Unknown Backend2Master message type: 0x%02X",
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
                DeviceStatus deviceStatus = {0};

                // Process message and generate response
                auto response = processAndCreateResponse(slaveId, *message);

                if (response) {
                    // Pack response message
                    std::vector<std::vector<uint8_t>> responseData;
                    if (response->getMessageId() ==
                            static_cast<uint8_t>(
                                Slave2BackendMessageId::CONDUCTION_DATA_MSG) ||
                        response->getMessageId() ==
                            static_cast<uint8_t>(
                                Slave2BackendMessageId::RESISTANCE_DATA_MSG) ||
                        response->getMessageId() ==
                            static_cast<uint8_t>(
                                Slave2BackendMessageId::CLIP_DATA_MSG)) {
                        Log::i("ResponseSender",
                               "Packing Slave2Backend message");
                        responseData = processor.packSlave2BackendMessage(
                            slaveId, deviceStatus, *response);
                    } else {
                        responseData = processor.packSlave2MasterMessage(
                            slaveId, *response);
                    }

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
        } else if (frame.packetId ==
                   static_cast<uint8_t>(PacketId::BACKEND_TO_MASTER)) {
            std::unique_ptr<Message> backendMessage;

            if (processor.parseBackend2MasterPacket(frame.payload,
                                                    backendMessage)) {
                Log::i("PacketProcessor",
                       "Backend2Master packet parsed successfully:");
                Log::i("PacketProcessor", "Message ID: 0x%02X",
                       static_cast<int>(backendMessage->getMessageId()));

                // Process message and generate response
                auto backendResponse =
                    processBackend2MasterAndCreateResponse(*backendMessage);

                if (backendResponse) {
                    // Pack response message
                    auto responseData =
                        processor.packMaster2BackendMessage(*backendResponse);

                    Log::i("ResponseSender",
                           "Sending Master2Backend response:");

                    // Send all fragments
                    for (const auto &fragment : responseData) {
                        printBytes(fragment, "Master2Backend response data");
                        // Send response
                        sendto(sock,
                               reinterpret_cast<const char *>(fragment.data()),
                               static_cast<int>(fragment.size()), 0,
                               (sockaddr *)&clientAddr, sizeof(clientAddr));
                    }

                    Log::i("ResponseSender", "Master2Backend response sent");
                } else {
                    Log::i("ResponseSender", "No response needed for this "
                                             "Backend2Master message");
                }
            } else {
                Log::e("PacketProcessor",
                       "Failed to parse Backend2Master packet");
            }
        } else if (frame.packetId ==
                   static_cast<uint8_t>(PacketId::SLAVE_TO_BACKEND)) {
            uint32_t slaveId;
            DeviceStatus deviceStatus;
            std::unique_ptr<Message> slaveMessage;
            // FIXME 导通数据消息接受失败
            if (processor.parseSlave2BackendPacket(
                    frame.payload, slaveId, deviceStatus, slaveMessage)) {
                Log::i("PacketProcessor",
                       "Slave2Backend packet parsed successfully:");
                Log::i("PacketProcessor", "Slave ID: 0x%08X", slaveId);
                Log::i("PacketProcessor", "Device Status: 0x%04X",
                       deviceStatus.toUint16());
                Log::i("PacketProcessor", "Message ID: 0x%02X",
                       static_cast<int>(slaveMessage->getMessageId()));

                // Process different types of slave data messages
                switch (slaveMessage->getMessageId()) {
                case static_cast<uint8_t>(
                    Slave2BackendMessageId::CONDUCTION_DATA_MSG): {
                    auto condDataMsg = dynamic_cast<
                        const Slave2Backend::ConductionDataMessage *>(
                        slaveMessage.get());
                    if (condDataMsg) {
                        Log::i("DataProcessor",
                               "Received conduction data from slave 0x%08X, "
                               "length: %d",
                               slaveId, condDataMsg->conductionLength);
                        Log::i("DataProcessor", "Conduction data: %s",
                               bytesToHexString(condDataMsg->conductionData)
                                   .c_str());
                    }
                    break;
                }
                case static_cast<uint8_t>(
                    Slave2BackendMessageId::RESISTANCE_DATA_MSG): {
                    auto resDataMsg = dynamic_cast<
                        const Slave2Backend::ResistanceDataMessage *>(
                        slaveMessage.get());
                    if (resDataMsg) {
                        Log::i("DataProcessor",
                               "Received resistance data from slave 0x%08X, "
                               "length: %d",
                               slaveId, resDataMsg->resistanceLength);
                        Log::i("DataProcessor", "Resistance data: %s",
                               bytesToHexString(resDataMsg->resistanceData)
                                   .c_str());
                    }
                    break;
                }
                case static_cast<uint8_t>(
                    Slave2BackendMessageId::CLIP_DATA_MSG): {
                    auto clipDataMsg =
                        dynamic_cast<const Slave2Backend::ClipDataMessage *>(
                            slaveMessage.get());
                    if (clipDataMsg) {
                        Log::i("DataProcessor",
                               "Received clip data from slave 0x%08X, data: "
                               "0x%04X",
                               slaveId, clipDataMsg->clipData);
                    }
                    break;
                }
                default:
                    Log::w("DataProcessor",
                           "Unknown Slave2Backend message type: 0x%02X",
                           static_cast<int>(slaveMessage->getMessageId()));
                    break;
                }
            } else {
                Log::e("PacketProcessor",
                       "Failed to parse Slave2Backend packet");
            }
        } else if (frame.packetId ==
                   static_cast<uint8_t>(PacketId::MASTER_TO_BACKEND)) {
            std::unique_ptr<Message> masterMessage;

            if (processor.parseMaster2BackendPacket(frame.payload,
                                                    masterMessage)) {
                Log::i("PacketProcessor",
                       "Master2Backend packet parsed successfully:");
                Log::i("PacketProcessor", "Message ID: 0x%02X",
                       static_cast<int>(masterMessage->getMessageId()));

                // Process different types of Master2Backend messages
                switch (masterMessage->getMessageId()) {
                case static_cast<uint8_t>(
                    Master2BackendMessageId::SLAVE_CFG_RSP_MSG): {
                    auto configRspMsg = dynamic_cast<
                        const Master2Backend::SlaveConfigResponseMessage *>(
                        masterMessage.get());
                    if (configRspMsg) {
                        Log::i("ResponseProcessor",
                               "Received slave config response - Status: %d, "
                               "Slave count: %d",
                               static_cast<int>(configRspMsg->status),
                               static_cast<int>(configRspMsg->slaveNum));
                        for (const auto &slave : configRspMsg->slaves) {
                            Log::i("ResponseProcessor",
                                   "  Slave ID: 0x%08X, Status confirmed",
                                   slave.id);
                        }
                    }
                    break;
                }
                case static_cast<uint8_t>(
                    Master2BackendMessageId::MODE_CFG_RSP_MSG): {
                    auto modeRspMsg = dynamic_cast<
                        const Master2Backend::ModeConfigResponseMessage *>(
                        masterMessage.get());
                    if (modeRspMsg) {
                        Log::i("ResponseProcessor",
                               "Received mode config response - Status: %d, "
                               "Mode: %d",
                               static_cast<int>(modeRspMsg->status),
                               static_cast<int>(modeRspMsg->mode));
                    }
                    break;
                }
                case static_cast<uint8_t>(
                    Master2BackendMessageId::PING_RES_MSG): {
                    auto pingRspMsg = dynamic_cast<
                        const Master2Backend::PingResponseMessage *>(
                        masterMessage.get());
                    if (pingRspMsg) {
                        Log::i("ResponseProcessor",
                               "Received ping response - Mode: %d, Success: "
                               "%d/%d, Target: "
                               "0x%08X",
                               static_cast<int>(pingRspMsg->pingMode),
                               pingRspMsg->successCount, pingRspMsg->totalCount,
                               pingRspMsg->destinationId);
                    }
                    break;
                }
                case static_cast<uint8_t>(
                    Master2BackendMessageId::DEVICE_LIST_RSP_MSG): {
                    auto deviceListRspMsg = dynamic_cast<
                        const Master2Backend::DeviceListResponseMessage *>(
                        masterMessage.get());
                    if (deviceListRspMsg) {
                        Log::i("ResponseProcessor",
                               "Received device list response - Device "
                               "count: %d",
                               static_cast<int>(deviceListRspMsg->deviceCount));
                        for (const auto &device : deviceListRspMsg->devices) {
                            Log::i("ResponseProcessor",
                                   "  Device ID: 0x%08X, Short ID: %d, "
                                   "Online: "
                                   "%s, Version: "
                                   "%d.%d.%d",
                                   device.deviceId,
                                   static_cast<int>(device.shortId),
                                   device.online ? "Yes" : "No",
                                   static_cast<int>(device.versionMajor),
                                   static_cast<int>(device.versionMinor),
                                   device.versionPatch);
                        }
                    }
                    break;
                }
                default:
                    Log::i("ResponseProcessor",
                           "Received Master2Backend message type: 0x%02X",
                           static_cast<int>(masterMessage->getMessageId()));
                    break;
                }
            } else {
                Log::e("PacketProcessor",
                       "Failed to parse Master2Backend packet");
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
                               "Parsed frame %d: PacketId=%d, payload "
                               "size=%zu",
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