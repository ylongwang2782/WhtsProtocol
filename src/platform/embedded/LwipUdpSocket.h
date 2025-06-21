#pragma once

#include "../../interface/IUdpSocket.h"

// lwip相关头文件
#ifdef USE_LWIP
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#endif

#include <mutex>
#include <queue>

namespace HAL {
namespace Network {

#ifdef USE_LWIP

/**
 * 接收数据包结构
 */
struct ReceivedPacket {
    std::vector<uint8_t> data;
    NetworkAddress senderAddr;

    ReceivedPacket(const std::vector<uint8_t> &d, const NetworkAddress &addr)
        : data(d), senderAddr(addr) {}
};

/**
 * 嵌入式平台UDP套接字实现
 * 使用lwip协议栈
 */
class LwipUdpSocket : public IUdpSocket {
  private:
    struct udp_pcb *udpPcb;
    NetworkAddress localAddress;
    bool isInitialized;
    bool isBound;
    UdpReceiveCallback receiveCallback;

    // 接收数据队列（用于非阻塞接收）
    std::queue<ReceivedPacket> receiveQueue;
    std::mutex queueMutex;

    // lwip回调函数
    static void udpReceiveCallback(void *arg, struct udp_pcb *pcb,
                                   struct pbuf *p, const ip_addr_t *addr,
                                   u16_t port);

    // 辅助方法
    ip_addr_t createIpAddr(const std::string &ipStr) const;
    NetworkAddress createNetworkAddress(const ip_addr_t *addr,
                                        u16_t port) const;
    bool isValidIpAddress(const std::string &ip) const;

  public:
    LwipUdpSocket();
    ~LwipUdpSocket() override;

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
 * lwip UDP套接字工厂
 */
class LwipUdpSocketFactory : public IUdpSocketFactory {
  public:
    std::unique_ptr<IUdpSocket> createUdpSocket() override {
        return std::make_unique<LwipUdpSocket>();
    }
};

#else

/**
 * 当未定义USE_LWIP时的空实现
 */
class LwipUdpSocket : public IUdpSocket {
  public:
    bool initialize() override { return false; }
    bool bind(const std::string &address, uint16_t port) override {
        return false;
    }
    bool setBroadcast(bool enable) override { return false; }
    bool setNonBlocking(bool nonBlocking) override { return false; }
    bool sendTo(const std::vector<uint8_t> &data,
                const NetworkAddress &targetAddr,
                UdpSendCallback callback = nullptr) override {
        return false;
    }
    bool broadcast(const std::vector<uint8_t> &data, uint16_t port,
                   UdpSendCallback callback = nullptr) override {
        return false;
    }
    int receiveFrom(uint8_t *buffer, size_t bufferSize,
                    NetworkAddress &senderAddr) override {
        return -1;
    }
    void setReceiveCallback(UdpReceiveCallback callback) override {}
    void startAsyncReceive() override {}
    void stopAsyncReceive() override {}
    void close() override {}
    NetworkAddress getLocalAddress() const override { return NetworkAddress(); }
    bool isOpen() const override { return false; }
    void processEvents() override {}
};

class LwipUdpSocketFactory : public IUdpSocketFactory {
  public:
    std::unique_ptr<IUdpSocket> createUdpSocket() override {
        return std::make_unique<LwipUdpSocket>();
    }
};

#endif // USE_LWIP

} // namespace Network
} // namespace HAL