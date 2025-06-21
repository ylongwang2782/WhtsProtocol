#pragma once

#include "../interface/INetwork.h"
#include "../interface/IUdpSocket.h"
#include <functional>
#include <memory>
#include <unordered_map>

using namespace Interface;

namespace App {

/**
 * 跨平台网络管理器实现
 * 实现Interface::INetwork接口，提供统一的网络通信接口，支持多个UDP套接字
 */
class NetworkManager : public INetwork {
  private:
    std::unique_ptr<IUdpSocketFactory> socketFactory;
    std::unordered_map<std::string, std::unique_ptr<IUdpSocket>> sockets;
    NetworkEventCallback eventCallback;
    bool isRunning;

    // 生成套接字ID
    std::string generateSocketId();

    // 内部事件处理
    void handleSocketReceive(const std::string &socketId,
                             const std::vector<uint8_t> &data,
                             const NetworkAddress &senderAddr);
    void handleSocketSend(const std::string &socketId, bool success,
                          size_t bytesSent);

  public:
    NetworkManager();
    ~NetworkManager() override;

    // INetwork接口实现
    bool initialize(std::unique_ptr<IUdpSocketFactory> factory) override;
    std::string createUdpSocket(const std::string &socketId = "") override;
    bool bindSocket(const std::string &socketId, const std::string &address,
                    uint16_t port) override;
    bool setSocketBroadcast(const std::string &socketId, bool enable) override;
    bool setSocketNonBlocking(const std::string &socketId,
                              bool nonBlocking) override;
    bool sendTo(const std::string &socketId, const std::vector<uint8_t> &data,
                const NetworkAddress &targetAddr) override;
    bool broadcast(const std::string &socketId,
                   const std::vector<uint8_t> &data, uint16_t port) override;
    int receiveFrom(const std::string &socketId, uint8_t *buffer,
                    size_t bufferSize, NetworkAddress &senderAddr) override;
    bool closeSocket(const std::string &socketId) override;
    NetworkAddress
    getSocketLocalAddress(const std::string &socketId) const override;
    bool isSocketOpen(const std::string &socketId) const override;
    void setEventCallback(NetworkEventCallback callback) override;
    void start() override;
    void stop() override;
    void processEvents() override;
    std::vector<std::string> getSocketIds() const override;
    void cleanup() override;
};

} // namespace App