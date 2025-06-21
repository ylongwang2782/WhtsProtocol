#include "SlaveDevice.h"
#include "../Logger.h"
#include <chrono>
#include <iostream>
#include <thread>

using namespace WhtsProtocol;
using namespace HAL::Network;

namespace SlaveApp {

SlaveDevice::SlaveDevice(uint16_t listenPort, uint32_t id)
    : port(listenPort), deviceId(id), deviceState(SlaveDeviceState::IDLE),
      isConfigured(false) {

    // 创建网络管理器
    networkManager = NetworkFactory::createNetworkManager();
    if (!networkManager) {
        throw std::runtime_error("Failed to create network manager");
    }

    // Initialize continuity collector with virtual GPIO
    continuityCollector =
        Adapter::ContinuityCollectorFactory::createWithVirtualGpio();

    // Create message processor with references to our state
    messageProcessor = std::make_unique<MessageProcessor>(
        deviceId, deviceState, currentConfig, isConfigured, stateMutex,
        continuityCollector);
}

bool SlaveDevice::initialize() {
    // 创建主套接字
    mainSocketId = networkManager->createUdpSocket("slave_main");
    if (mainSocketId.empty()) {
        Log::e("SlaveDevice", "Failed to create main UDP socket");
        return false;
    }

    // 启用广播功能
    if (!networkManager->setSocketBroadcast(mainSocketId, true)) {
        Log::w("SlaveDevice", "Failed to enable broadcast option");
    }

    // 配置服务器地址 (Slave监听端口8081接收广播)
    serverAddr = NetworkAddress("0.0.0.0", port);

    // 配置主机地址 (Master使用端口8080)
    masterAddr = NetworkAddress("127.0.0.1", 8080);

    // 绑定套接字
    if (!networkManager->bindSocket(mainSocketId, serverAddr.ip,
                                    serverAddr.port)) {
        Log::e("SlaveDevice", "Failed to bind socket");
        return false;
    }

    // 设置为非阻塞模式
    networkManager->setSocketNonBlocking(mainSocketId, true);

    processor.setMTU(100);

    Log::i("SlaveDevice", "Slave device (ID: 0x%08X) initialized successfully",
           deviceId);
    Log::i("SlaveDevice", "Network initialized - listening on port %d", port);
    Log::i("SlaveDevice", "Master communication port: 8080");
    Log::i("SlaveDevice", "Wireless broadcast reception enabled");

    return true;
}

void SlaveDevice::processFrame(Frame &frame, const NetworkAddress &senderAddr) {
    Log::i("SlaveDevice",
           "Processing frame - PacketId: 0x%02X, payload size: %zu",
           static_cast<int>(frame.packetId), frame.payload.size());

    if (frame.packetId == static_cast<uint8_t>(PacketId::MASTER_TO_SLAVE)) {
        uint32_t targetSlaveId;
        std::unique_ptr<Message> masterMessage;

        if (processor.parseMaster2SlavePacket(frame.payload, targetSlaveId,
                                              masterMessage)) {
            // Check if this message is for us (or broadcast)
            if (targetSlaveId == deviceId || targetSlaveId == BROADCAST_ID) {
                Log::i("SlaveDevice",
                       "Processing Master2Slave message for device 0x%08X, "
                       "Message ID: 0x%02X",
                       targetSlaveId,
                       static_cast<int>(masterMessage->getMessageId()));

                // Process message and create response
                auto response =
                    messageProcessor->processAndCreateResponse(*masterMessage);

                if (response) {
                    Log::i("SlaveDevice", "Generated response message");

                    std::vector<std::vector<uint8_t>> responseData;
                    DeviceStatus deviceStatus = {};

                    if (response->getMessageId() ==
                            static_cast<uint8_t>(
                                Slave2BackendMessageId::CONDUCTION_DATA_MSG) ||
                        response->getMessageId() ==
                            static_cast<uint8_t>(
                                Slave2BackendMessageId::RESISTANCE_DATA_MSG) ||
                        response->getMessageId() ==
                            static_cast<uint8_t>(
                                Slave2BackendMessageId::CLIP_DATA_MSG)) {
                        Log::i("SlaveDevice", "Packing Slave2Backend message");
                        responseData = processor.packSlave2BackendMessage(
                            deviceId, deviceStatus, *response);
                    } else {
                        responseData = processor.packSlave2MasterMessage(
                            deviceId, *response);
                    }

                    Log::i("SlaveDevice", "Sending response:");

                    // Send all fragments to master
                    for (const auto &fragment : responseData) {
                        networkManager->sendTo(mainSocketId, fragment,
                                               masterAddr);
                    }
                }
            } else {
                Log::d("SlaveDevice",
                       "Message not for this device (target: 0x%08X, our ID: "
                       "0x%08X)",
                       targetSlaveId, deviceId);
            }
        } else {
            Log::e("SlaveDevice", "Failed to parse Master2Slave packet");
        }
    } else {
        Log::w("SlaveDevice", "Unsupported packet type for Slave: 0x%02X",
               static_cast<int>(frame.packetId));
    }
}

void SlaveDevice::run() {
    Log::i("SlaveDevice", "Slave device started");
    Log::i("SlaveDevice", "Device ID: 0x%08X", deviceId);
    Log::i("SlaveDevice", "Handling Master2Slave broadcast packets");
    Log::i("SlaveDevice", "Sending responses to Master on port 8080");

    uint8_t buffer[1024];
    NetworkAddress senderAddr;

    while (true) {
        // 处理采集状态（状态机）
        if (isConfigured && deviceState == SlaveDeviceState::COLLECTING) {
            continuityCollector->processCollection();

            // 检查是否完成采集
            if (continuityCollector->isCollectionComplete()) {
                Log::i("SlaveDevice",
                       "Data collection completed automatically");
                deviceState = SlaveDeviceState::COLLECTION_COMPLETE;
            }
        }

        // 接收数据（非阻塞）
        int bytesReceived = networkManager->receiveFrom(
            mainSocketId, buffer, sizeof(buffer), senderAddr);

        if (bytesReceived > 0) {
            std::vector<uint8_t> data(buffer, buffer + bytesReceived);

            processor.processReceivedData(data);
            Frame receivedFrame;
            while (processor.getNextCompleteFrame(receivedFrame)) {
                processFrame(receivedFrame, senderAddr);
            }
        } else {
            // 如果没有数据，短暂休眠以避免CPU占用过高
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

} // namespace SlaveApp