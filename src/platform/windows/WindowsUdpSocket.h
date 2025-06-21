#pragma once

#include "../../interface/IUdpSocket.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

using namespace Interface;

namespace Platform {
namespace Windows {

/**
 * Windows平台UDP套接字实现
 * 使用Windows Socket API（后续可替换为asio）
 */
class WindowsUdpSocket : public IUdpSocket {
  private:
    SOCKET sock;
    sockaddr_in localAddr;
    bool isInitialized;
    bool isBound;
    bool isNonBlocking;
    UdpReceiveCallback receiveCallback;
    NetworkAddress localAddress;

    // 辅助方法
    sockaddr_in createSockAddr(const NetworkAddress &addr) const;
    NetworkAddress createNetworkAddress(const sockaddr_in &addr) const;
    bool initializeWinsock();
    void cleanupWinsock();

  public:
    WindowsUdpSocket();
    ~WindowsUdpSocket() override;

    // IUdpSocket接口实现
    bool initialize() override;
    bool bind(const std::string &address, uint16_t port) override;
    bool setBroadcast(bool enable) override;
    bool setNonBlocking(bool nonBlocking) override;

    bool sendTo(const std::vector<uint8_t> &data,
                const NetworkAddress &targetAddr,
                UdpSendCallback callback = nullptr) override;

    bool broadcast(const std::vector<uint8_t> &data, uint16_t port,
                   UdpSendCallback callback = nullptr) override;

    int receiveFrom(uint8_t *buffer, size_t bufferSize,
                    NetworkAddress &senderAddr) override;

    void setReceiveCallback(UdpReceiveCallback callback) override;
    void startAsyncReceive() override;
    void stopAsyncReceive() override;
    void close() override;

    NetworkAddress getLocalAddress() const override;
    bool isOpen() const override;
    void processEvents() override;
};

/**
 * Windows UDP套接字工厂
 */
class WindowsUdpSocketFactory : public IUdpSocketFactory {
  public:
    std::unique_ptr<IUdpSocket> createUdpSocket() override {
        return std::make_unique<WindowsUdpSocket>();
    }
};

} // namespace Windows
} // namespace Platform