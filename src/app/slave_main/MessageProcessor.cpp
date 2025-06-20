#include "MessageProcessor.h"
#include <chrono>
#include <iostream>

using namespace WhtsProtocol;

namespace SlaveApp {

MessageProcessor::MessageProcessor(
    uint32_t deviceId, SlaveDeviceState &deviceState,
    Adapter::CollectorConfig &currentConfig, bool &isConfigured,
    std::mutex &stateMutex,
    std::unique_ptr<Adapter::ContinuityCollector> &continuityCollector)
    : deviceId(deviceId), deviceState(deviceState),
      currentConfig(currentConfig), isConfigured(isConfigured),
      stateMutex(stateMutex), continuityCollector(continuityCollector) {}

uint32_t MessageProcessor::getCurrentTimestamp() {
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

void MessageProcessor::resetDevice() {
    std::lock_guard<std::mutex> lock(stateMutex);
    // 保留配置，但重置状态
    deviceState = SlaveDeviceState::CONFIGURED;
    std::cout << "[INFO] SlaveDevice: Device reset to CONFIGURED state, "
                 "configuration preserved"
              << std::endl;
}

std::unique_ptr<Message>
MessageProcessor::processAndCreateResponse(const Message &request) {
    switch (request.getMessageId()) {
    case static_cast<uint8_t>(Master2SlaveMessageId::SYNC_MSG): {
        auto syncMsg =
            dynamic_cast<const Master2Slave::SyncMessage *>(&request);
        if (syncMsg) {
            std::cout
                << "[INFO] MessageProcessor: Processing sync message - Mode: "
                << static_cast<int>(syncMsg->mode)
                << ", Timestamp: " << syncMsg->timestamp << std::endl;

            // 根据新逻辑：收到Sync Message后开始采集，不需要每次都配置
            std::lock_guard<std::mutex> lock(stateMutex);
            if (isConfigured) {
                // 如果已配置，无论当前状态如何，都可以开始新的数据采集
                std::cout << "[INFO] MessageProcessor: Starting data "
                             "collection based on sync message"
                          << std::endl;

                // 开始采集
                if (continuityCollector->startCollection()) {
                    deviceState = SlaveDeviceState::COLLECTING;
                    std::cout << "[INFO] MessageProcessor: Data collection "
                                 "started successfully"
                              << std::endl;

                    // 立即处理一次采集状态，确保快速响应
                    continuityCollector->processCollection();
                } else {
                    std::cout << "[ERROR] MessageProcessor: Failed to start "
                                 "data collection"
                              << std::endl;
                    deviceState = SlaveDeviceState::DEV_ERR;
                }
            } else {
                std::cout << "[WARN] MessageProcessor: Device not configured, "
                             "cannot start collection"
                          << std::endl;
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
            std::cout << "[INFO] MessageProcessor: Processing conduction "
                         "configuration - Time slot: "
                      << static_cast<int>(configMsg->timeSlot)
                      << ", Interval: " << static_cast<int>(configMsg->interval)
                      << "ms" << std::endl;

            // 根据新逻辑：收到Conduction Config
            // message后配置ContinuityCollector 并且保存配置，后续可以重复使用
            std::lock_guard<std::mutex> lock(stateMutex);

            // 创建采集器配置
            currentConfig = Adapter::CollectorConfig(
                static_cast<uint8_t>(configMsg->conductionNum), // 导通检测数量
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
                std::cout << "[INFO] MessageProcessor: ContinuityCollector "
                             "configured successfully - "
                          << "Pins: " << static_cast<int>(currentConfig.num)
                          << ", Start: "
                          << static_cast<int>(currentConfig.startDetectionNum)
                          << ", Total: "
                          << static_cast<int>(currentConfig.totalDetectionNum)
                          << ", Interval: " << currentConfig.interval << "ms"
                          << std::endl;
                std::cout
                    << "[INFO] MessageProcessor: Configuration saved for "
                       "future use. Send Sync message to start collection."
                    << std::endl;
            } else {
                isConfigured = false;
                deviceState = SlaveDeviceState::DEV_ERR;
                std::cout << "[ERROR] MessageProcessor: Failed to configure "
                             "ContinuityCollector"
                          << std::endl;
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
            std::cout << "[INFO] MessageProcessor: Processing resistance "
                         "configuration - Time slot: "
                      << static_cast<int>(configMsg->timeSlot)
                      << ", Interval: " << static_cast<int>(configMsg->interval)
                      << "ms" << std::endl;

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
            std::cout << "[INFO] MessageProcessor: Processing clip "
                         "configuration - Interval: "
                      << static_cast<int>(configMsg->interval)
                      << "ms, Mode: " << static_cast<int>(configMsg->mode)
                      << std::endl;

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
            std::cout
                << "[INFO] MessageProcessor: Processing read conduction data"
                << std::endl;

            auto response =
                std::make_unique<Slave2Backend::ConductionDataMessage>();

            // 根据TODO要求：从已配置的Collector获得数据并创建response
            std::lock_guard<std::mutex> lock(stateMutex);

            if (isConfigured && continuityCollector) {
                // 检查采集状态
                if (deviceState == SlaveDeviceState::COLLECTING) {
                    // 处理采集状态，快速完成数据收集
                    while (!continuityCollector->isCollectionComplete()) {
                        continuityCollector->processCollection();
                    }
                    deviceState = SlaveDeviceState::COLLECTION_COMPLETE;
                }

                // 无论当前状态，只要已配置过，都尝试获取最新数据
                // 从采集器获取数据
                response->conductionData = continuityCollector->getDataVector();
                response->conductionLength = response->conductionData.size();

                if (response->conductionLength > 0) {
                    std::cout << "[INFO] MessageProcessor: Retrieved "
                              << response->conductionLength
                              << " bytes of conduction data" << std::endl;
                } else {
                    std::cout << "[WARN] MessageProcessor: No collection data "
                                 "available, device state: "
                              << static_cast<int>(deviceState) << std::endl;
                }
            } else {
                std::cout << "[WARN] MessageProcessor: Device not configured "
                             "or collector not available"
                          << std::endl;
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
            std::cout
                << "[INFO] MessageProcessor: Processing read resistance data"
                << std::endl;

            auto response =
                std::make_unique<Slave2Backend::ResistanceDataMessage>();
            response->resistanceLength = 1;
            response->resistanceData = {0x90};
            return std::move(response);
        }
        break;
    }

    case static_cast<uint8_t>(Master2SlaveMessageId::READ_CLIP_DATA_MSG): {
        const auto *readClipDataMsg =
            dynamic_cast<const Master2Slave::ReadClipDataMessage *>(&request);
        if (readClipDataMsg) {
            std::cout << "[INFO] MessageProcessor: Processing read clip data"
                      << std::endl;

            auto response = std::make_unique<Slave2Backend::ClipDataMessage>();
            response->clipData = 0xFF;
            return std::move(response);
        }
        break;
    }

    case static_cast<uint8_t>(Master2SlaveMessageId::PING_REQ_MSG): {
        const auto *pingMsg =
            dynamic_cast<const Master2Slave::PingReqMessage *>(&request);
        if (pingMsg) {
            std::cout << "[INFO] MessageProcessor: Processing Ping request - "
                         "Sequence number: "
                      << pingMsg->sequenceNumber
                      << ", Timestamp: " << pingMsg->timestamp << std::endl;

            auto response = std::make_unique<Slave2Master::PingRspMessage>();
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
            std::cout << "[INFO] MessageProcessor: Processing reset message - "
                         "Lock status: "
                      << static_cast<int>(rstMsg->lockStatus) << std::endl;

            // 重置设备状态，但保留配置
            resetDevice();

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
            dynamic_cast<const Master2Slave::ShortIdAssignMessage *>(&request);
        if (assignMsg) {
            std::cout << "[INFO] MessageProcessor: Processing short ID "
                         "assignment - Short ID: "
                      << static_cast<int>(assignMsg->shortId) << std::endl;

            auto response =
                std::make_unique<Slave2Master::ShortIdConfirmMessage>();
            response->status = 0; // Success
            response->shortId = assignMsg->shortId;
            return std::move(response);
        }
        break;
    }

    default:
        std::cout << "[WARN] MessageProcessor: Unknown message type: 0x"
                  << std::hex << static_cast<int>(request.getMessageId())
                  << std::dec << std::endl;
        break;
    }

    return nullptr;
}

} // namespace SlaveApp