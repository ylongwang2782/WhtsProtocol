#pragma once

#include "../../interface/IUdpSocket.h"

using namespace Interface;

// 条件编译：只有定义了USE_LWIP才使用LWIP实现
#ifdef USE_LWIP

// LWIP相关头文件
extern "C" {
#include "lwip/api.h"
#include "lwip/arch.h"
#include "lwip/inet.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/udp.h"

}

#include <functional>
#include <memory>
#include <vector>

namespace Platform {
namespace Embedded {

/**
 * LWIP UDP套接字实现
 * 基于LWIP的嵌入式系统UDP实现
 */
class LwipUdpSocket : public IUdpSocket {
  private:
    struct netconn *conn;
    bool isInitialized;
    bool isBound;
    NetworkAddress localAddress;
    UdpReceiveCallback receiveCallback;

    // 辅助方法
    ip_addr_t stringToIpAddr(const std::string &ipStr);
    std::string ipAddrToString(const ip_addr_t &addr);

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
 * LWIP UDP套接字工厂
 */
class LwipUdpSocketFactory : public IUdpSocketFactory {
  public:
    std::unique_ptr<IUdpSocket> createUdpSocket() override {
        return std::make_unique<LwipUdpSocket>();
    }
};

} // namespace Embedded
} // namespace Platform

#else

namespace Platform {
namespace Embedded {

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

} // namespace Embedded
} // namespace Platform

#endif // USE_LWIP