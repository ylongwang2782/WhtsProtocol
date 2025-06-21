#include "MasterServer.h"
#include "../Logger.h"
#include "MessageHandlers.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

MasterServer::MasterServer(uint16_t listenPort) : port(listenPort) {
    // 创建网络管理器
    networkManager = NetworkFactory::createNetworkManager();
    if (!networkManager) {
        throw std::runtime_error("Failed to create network manager");
    }

    // 创建主套接字
    mainSocketId = networkManager->createUdpSocket("master_main");
    if (mainSocketId.empty()) {
        throw std::runtime_error("Failed to create main UDP socket");
    }

    // 设置网络事件回调
    networkManager->setEventCallback(
        [this](const NetworkEvent &event) { onNetworkEvent(event); });

    // 启用广播功能
    if (!networkManager->setSocketBroadcast(mainSocketId, true)) {
        Log::w("Master", "Failed to enable broadcast option");
    }

    // 配置服务器地址 (Master监听端口8080)
    serverAddr = NetworkAddress("0.0.0.0", port);

    // 配置后端地址 (Backend使用端口8079)
    backendAddr = NetworkAddress("127.0.0.1", 8079);

    // 配置从机广播地址 (广播到所有从机端口8081)
    // 使用本地广播进行模拟
    slaveBroadcastAddr = NetworkAddress("127.255.255.255", 8081);

    if (!networkManager->bindSocket(mainSocketId, serverAddr.ip,
                                    serverAddr.port)) {
        throw std::runtime_error("Failed to bind socket");
    }

    initializeMessageHandlers();
    Log::i("Master", "Master server listening on port %d", port);
    Log::i("Master", "Backend communication port: 8079");
    Log::i("Master", "Slave broadcast communication port: 8081");
    Log::i("Master", "Wireless broadcast simulation enabled");
}

MasterServer::~MasterServer() {
    if (networkManager) {
        networkManager->cleanup();
    }
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
                                         const NetworkAddress &clientAddr) {
    if (!response)
        return;

    auto responseData = processor.packMaster2BackendMessage(*response);
    Log::i("Master", "Sending Master2Backend response to port 8079:");

    for (const auto &fragment : responseData) {
        printBytes(fragment, "Master2Backend response data");
        // Send to backend on port 8079
        networkManager->sendTo(mainSocketId, fragment, backendAddr);
    }

    Log::i("Master", "Master2Backend response sent to backend (port 8079)");
}

void MasterServer::sendCommandToSlave(uint32_t slaveId,
                                      std::unique_ptr<Message> command,
                                      const NetworkAddress &clientAddr) {
    if (!command)
        return;

    auto commandData = processor.packMaster2SlaveMessage(slaveId, *command);
    Log::i(
        "Master",
        "Broadcasting Master2Slave command to 0x%08X via port 8081:", slaveId);

    for (const auto &fragment : commandData) {
        printBytes(fragment, "Master2Slave command data");
        // Send to slaves on port 8081
        networkManager->broadcast(mainSocketId, fragment,
                                  slaveBroadcastAddr.port);
    }

    Log::i("Master", "Master2Slave command broadcasted to slaves (port 8081)");
}

void MasterServer::sendCommandToSlaveWithRetry(uint32_t slaveId,
                                               std::unique_ptr<Message> command,
                                               const NetworkAddress &clientAddr,
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
                                  const NetworkAddress &clientAddr) {
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

void MasterServer::processBackend2MasterMessage(
    const Message &message, const NetworkAddress &clientAddr) {
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

void MasterServer::processSlave2MasterMessage(
    uint32_t slaveId, const Message &message,
    const NetworkAddress &clientAddr) {
    Log::i("Master", "Processing Slave2Master message from slave 0x%08X",
           slaveId);

    switch (message.getMessageId()) {
    case static_cast<uint8_t>(Slave2MasterMessageId::CONDUCTION_CFG_RSP_MSG): {
        const auto *rspMsg =
            dynamic_cast<const Slave2Master::ConductionConfigResponseMessage *>(
                &message);
        if (rspMsg) {
            Log::i("Master",
                   "Received conduction config response from slave 0x%08X",
                   slaveId);
            deviceManager.addSlave(slaveId);
        }
        break;
    }

    case static_cast<uint8_t>(Slave2MasterMessageId::RESISTANCE_CFG_RSP_MSG): {
        const auto *rspMsg =
            dynamic_cast<const Slave2Master::ResistanceConfigResponseMessage *>(
                &message);
        if (rspMsg) {
            Log::i("Master",
                   "Received resistance config response from slave 0x%08X",
                   slaveId);
            deviceManager.addSlave(slaveId);
        }
        break;
    }

    case static_cast<uint8_t>(Slave2MasterMessageId::PING_RSP_MSG): {
        const auto *pingRsp =
            dynamic_cast<const Slave2Master::PingRspMessage *>(&message);
        if (pingRsp) {
            Log::i("Master",
                   "Received ping response from slave 0x%08X (seq=%d)", slaveId,
                   pingRsp->sequenceNumber);

            // Update ping session success count
            for (auto &session : activePingSessions) {
                if (session.targetId == slaveId) {
                    session.successCount++;
                    break;
                }
            }
        }
        break;
    }

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
                networkManager->sendTo(mainSocketId, packet, backendAddr);

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
                networkManager->sendTo(mainSocketId, packet, backendAddr);

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
                networkManager->sendTo(mainSocketId, packet, backendAddr);

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

void MasterServer::processFrame(Frame &frame,
                                const NetworkAddress &clientAddr) {
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

void MasterServer::onNetworkEvent(const NetworkEvent &event) {
    switch (event.type) {
    case NetworkEventType::DATA_RECEIVED: {
        Log::i("Master", "Received %zu bytes from %s:%d", event.data.size(),
               event.remoteAddr.ip.c_str(), event.remoteAddr.port);

        // Convert to hex string for processing (maintaining compatibility)
        std::string receivedStr = bytesToHexString(event.data);

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
            data = event.data;
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
                processFrame(receivedFrame, event.remoteAddr);
            }
        }
        break;
    }
    case NetworkEventType::DATA_SENT:
        Log::d("Master", "Data sent successfully");
        break;
    case NetworkEventType::CONNECTION_ERROR:
        Log::e("Master", "Network error: %s", event.errorMessage.c_str());
        break;
    case NetworkEventType::SOCKET_CLOSED:
        Log::i("Master", "Socket closed: %s", event.socketId.c_str());
        break;
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

    processor.setMTU(100);
    networkManager->start();

    while (true) {
        // Process pending commands, ping sessions, and data collection
        processPendingCommands();
        processPingSessions();
        processDataCollection();

        // Process network events
        networkManager->processEvents();

        // Small delay to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
        // 处于采集状态，检查是否应该进入读取数据阶段
        if (dm.shouldEnterReadingPhase(currentTime)) {
            dm.enterReadingPhase();
            Log::i("MasterServer", "Entered reading data phase");
        }
        break;

    case CollectionCycleState::READING_DATA:
        // 处于读取数据阶段，检查是否完成
        if (dm.isAllDataReceived()) {
            // 数据采集周期完成，可以开始新的周期
            if (dm.shouldStartNewCycle(currentTime)) {
                dm.startNewCycle(currentTime);
                Log::i("MasterServer", "Started new data collection cycle");
            }
        }
        break;

    case CollectionCycleState::COMPLETE:
        // 完成状态，检查是否应该开始新的周期
        if (dm.shouldStartNewCycle(currentTime)) {
            dm.startNewCycle(currentTime);
            Log::i("MasterServer", "Started new data collection cycle");
        }
        break;
    }
}

uint32_t MasterServer::getCurrentTimestampMs() {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}
