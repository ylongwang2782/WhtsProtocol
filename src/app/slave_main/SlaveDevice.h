#pragma once

#include "../../Adapter/NetworkFactory.h"
#include "../NetworkManager.h"
#include "ContinuityCollector.h"
#include "MessageProcessor.h"
#include "SlaveDeviceState.h"
#include "WhtsProtocol.h"
#include <memory>
#include <mutex>

using namespace Adapter;
using namespace Interface;
using namespace App;

namespace SlaveApp {

/**
 * SlaveDevice 类实现了从机设备的功能
 *
 * 工作流程：
 * 1. 接收 ConductionConfigMessage 进行一次性配置，配置会被保存
 * 2. 接收 SyncMessage
 * 开始数据采集（可多次发送，每次都会使用保存的配置进行新的数据采集）
 * 3. 接收 ReadConductionDataMessage 获取最新采集的数据
 * 4. 可以重复步骤2和3多次，无需重新配置
 * 5. 如需重置设备状态但保留配置，可发送 RstMessage
 *
 * 优化说明：
 * - 使用状态机替代线程模型，提高稳定性
 * - 在主循环中定期处理采集状态
 * - 接收 Sync Message 后立即执行一次数据采集，确保快速响应
 * - 非阻塞套接字实现，确保数据采集过程中仍能接收网络消息
 * - 直接使用共用的NetworkManager，无需适配器层
 */
class SlaveDevice {
  private:
    std::unique_ptr<NetworkManager> networkManager;
    std::string mainSocketId;
    NetworkAddress serverAddr;
    NetworkAddress masterAddr;
    WhtsProtocol::ProtocolProcessor processor;

    std::unique_ptr<MessageProcessor> messageProcessor;
    std::unique_ptr<Adapter::ContinuityCollector> continuityCollector;

    // 状态管理
    SlaveDeviceState deviceState;
    Adapter::CollectorConfig currentConfig;
    bool isConfigured;
    std::mutex stateMutex;

    uint16_t port;
    uint32_t deviceId;

  public:
    SlaveDevice(uint16_t listenPort = 8081, uint32_t id = 0x3732485B);
    ~SlaveDevice() = default;

    /**
     * 初始化设备
     * @return 是否成功初始化
     */
    bool initialize();

    /**
     * 处理接收到的帧
     * @param frame 接收到的帧
     * @param senderAddr 发送方地址
     */
    void processFrame(WhtsProtocol::Frame &frame,
                      const NetworkAddress &senderAddr);

    /**
     * 运行主循环
     */
    void run();
};

} // namespace SlaveApp