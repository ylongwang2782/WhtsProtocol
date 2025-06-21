#pragma once

#include "../../Adapter/NetworkFactory.h"
#include "../NetworkManager.h"
#include "WhtsProtocol.h"
#include <memory>
#include <vector>

using namespace HAL::Network;

namespace SlaveApp {

/**
 * 网络管理器类
 * 负责处理UDP套接字通信
 * 使用项目统一的NetworkManager接口库
 */
class NetworkManager {
  private:
    std::unique_ptr<HAL::Network::NetworkManager> networkManager;
    std::string mainSocketId;
    NetworkAddress serverAddr;
    NetworkAddress masterAddr;
    uint16_t port;
    uint32_t deviceId;
    WhtsProtocol::ProtocolProcessor processor;

  public:
    NetworkManager(uint16_t listenPort = 8081, uint32_t deviceId = 0x3732485B);
    ~NetworkManager();

    /**
     * 初始化网络连接
     * @return 是否成功初始化
     */
    bool initialize();

    /**
     * 设置为非阻塞模式
     */
    void setNonBlocking();

    /**
     * 接收数据
     * @param buffer 接收缓冲区
     * @param bufferSize 缓冲区大小
     * @param senderAddr 发送方地址
     * @return 接收到的字节数，-1表示没有数据或错误
     */
    int receiveData(char *buffer, size_t bufferSize,
                    NetworkAddress &senderAddr);

    /**
     * 发送响应数据
     * @param data 要发送的数据
     */
    void sendResponse(const std::vector<uint8_t> &data);

    /**
     * 获取协议处理器的引用
     */
    WhtsProtocol::ProtocolProcessor &getProcessor() { return processor; }

    /**
     * 获取设备ID
     */
    uint32_t getDeviceId() const { return deviceId; }
};

} // namespace SlaveApp