#include "SlaveDevice.h"
#include <chrono>
#include <iostream>
#include <thread>

using namespace WhtsProtocol;

namespace SlaveApp {

SlaveDevice::SlaveDevice(uint16_t listenPort, uint32_t id)
    : deviceId(id), deviceState(SlaveDeviceState::IDLE), isConfigured(false) {

    networkManager = std::make_unique<NetworkManager>(listenPort, deviceId);

    // Initialize continuity collector with virtual GPIO
    continuityCollector =
        Adapter::ContinuityCollectorFactory::createWithVirtualGpio();

    // Create message processor with references to our state
    messageProcessor = std::make_unique<MessageProcessor>(
        deviceId, deviceState, currentConfig, isConfigured, stateMutex,
        continuityCollector);
}

bool SlaveDevice::initialize() {
    if (!networkManager->initialize()) {
        std::cout << "[ERROR] SlaveDevice: Failed to initialize network manager"
                  << std::endl;
        return false;
    }

    // Set non-blocking mode for the socket
    networkManager->setNonBlocking();

    std::cout << "[INFO] SlaveDevice: Slave device (ID: 0x" << std::hex
              << deviceId << ") initialized successfully" << std::dec
              << std::endl;
    return true;
}

void SlaveDevice::processFrame(Frame &frame, const sockaddr_in &senderAddr) {
    std::cout << "[INFO] SlaveDevice: Processing frame - PacketId: 0x"
              << std::hex << static_cast<int>(frame.packetId)
              << ", payload size: " << std::dec << frame.payload.size()
              << std::endl;

    if (frame.packetId == static_cast<uint8_t>(PacketId::MASTER_TO_SLAVE)) {
        uint32_t targetSlaveId;
        std::unique_ptr<Message> masterMessage;

        auto &processor = networkManager->getProcessor();
        if (processor.parseMaster2SlavePacket(frame.payload, targetSlaveId,
                                              masterMessage)) {
            // Check if this message is for us (or broadcast)
            if (targetSlaveId == deviceId || targetSlaveId == BROADCAST_ID) {
                std::cout << "[INFO] SlaveDevice: Processing Master2Slave "
                             "message for device 0x"
                          << std::hex << targetSlaveId << ", Message ID: 0x"
                          << static_cast<int>(masterMessage->getMessageId())
                          << std::dec << std::endl;

                // Process message and create response
                auto response =
                    messageProcessor->processAndCreateResponse(*masterMessage);

                if (response) {
                    std::cout
                        << "[INFO] SlaveDevice: Generated response message"
                        << std::endl;

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
                        std::cout << "[INFO] SlaveDevice: Packing "
                                     "Slave2Backend message"
                                  << std::endl;
                        responseData = processor.packSlave2BackendMessage(
                            deviceId, deviceStatus, *response);
                    } else {
                        responseData = processor.packSlave2MasterMessage(
                            deviceId, *response);
                    }

                    std::cout << "[INFO] SlaveDevice: Sending response:"
                              << std::endl;

                    // Send all fragments
                    for (const auto &fragment : responseData) {
                        networkManager->sendResponse(fragment);
                    }
                }
            } else {
                std::cout << "[DEBUG] SlaveDevice: Message not for this device "
                             "(target: 0x"
                          << std::hex << targetSlaveId << ", our ID: 0x"
                          << deviceId << ")" << std::dec << std::endl;
            }
        } else {
            std::cout
                << "[ERROR] SlaveDevice: Failed to parse Master2Slave packet"
                << std::endl;
        }
    } else {
        std::cout << "[WARN] SlaveDevice: Unsupported packet type for Slave: 0x"
                  << std::hex << static_cast<int>(frame.packetId) << std::dec
                  << std::endl;
    }
}

void SlaveDevice::run() {
    std::cout << "[INFO] SlaveDevice: Slave device started" << std::endl;
    std::cout << "[INFO] SlaveDevice: Device ID: 0x" << std::hex << deviceId
              << std::dec << std::endl;
    std::cout << "[INFO] SlaveDevice: Handling Master2Slave broadcast packets"
              << std::endl;
    std::cout << "[INFO] SlaveDevice: Sending responses to Master on port 8080"
              << std::endl;

    char buffer[1024];
    sockaddr_in senderAddr;

    auto &processor = networkManager->getProcessor();

    while (true) {
        // 处理采集状态（状态机）
        if (isConfigured && deviceState == SlaveDeviceState::COLLECTING) {
            continuityCollector->processCollection();

            // 检查是否完成采集
            if (continuityCollector->isCollectionComplete()) {
                std::cout << "[INFO] SlaveDevice: Data collection completed "
                             "automatically"
                          << std::endl;
                deviceState = SlaveDeviceState::COLLECTION_COMPLETE;
            }
        }

        // 接收数据（非阻塞）
        int bytesReceived =
            networkManager->receiveData(buffer, sizeof(buffer), senderAddr);

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