#include "MasterServer.h"
#include "../Logger.h"
#include "MessageHandlers.h"

// Define this to suppress deprecated warnings for inet_addr
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

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

    // Send the command immediately - create a copy by serializing and
    // deserializing
    auto serialized = pendingCmd.command->serialize();
    auto messageCopy = processor.createMessage(
        PacketId::MASTER_TO_SLAVE, pendingCmd.command->getMessageId());
    if (messageCopy && messageCopy->deserialize(serialized)) {
        sendCommandToSlave(slaveId, std::move(messageCopy), clientAddr);
    }

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

                // Create a copy of the command by serializing and deserializing
                auto serialized = it->command->serialize();
                auto messageCopy = processor.createMessage(
                    PacketId::MASTER_TO_SLAVE, it->command->getMessageId());
                if (messageCopy && messageCopy->deserialize(serialized)) {
                    sendCommandToSlave(it->slaveId, std::move(messageCopy),
                                       it->clientAddr);
                }

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
