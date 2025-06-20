#pragma once

#include "ContinuityCollector.h"
#include "SlaveDeviceState.h"
#include "WhtsProtocol.h"
#include <memory>
#include <mutex>

namespace SlaveApp {

/**
 * 消息处理器类
 * 负责处理从Master接收到的各种消息并生成相应的响应
 */
class MessageProcessor {
  private:
    uint32_t deviceId;
    SlaveDeviceState &deviceState;
    Adapter::CollectorConfig &currentConfig;
    bool &isConfigured;
    std::mutex &stateMutex;
    std::unique_ptr<Adapter::ContinuityCollector> &continuityCollector;

    // Get the current timestamp
    uint32_t getCurrentTimestamp();

  public:
    MessageProcessor(
        uint32_t deviceId, SlaveDeviceState &deviceState,
        Adapter::CollectorConfig &currentConfig, bool &isConfigured,
        std::mutex &stateMutex,
        std::unique_ptr<Adapter::ContinuityCollector> &continuityCollector);

    /**
     * 处理Master2Slave消息并生成响应
     * @param request 接收到的消息
     * @return 生成的响应消息，如果不需要响应则返回nullptr
     */
    std::unique_ptr<WhtsProtocol::Message>
    processAndCreateResponse(const WhtsProtocol::Message &request);

    /**
     * 重置设备状态
     */
    void resetDevice();
};

} // namespace SlaveApp