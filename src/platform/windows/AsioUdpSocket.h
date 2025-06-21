#pragma once

#include "../../interface/IUdpSocket.h"

// ASIO相关头文件
#ifdef USE_ASIO
#include <asio.hpp>
#include <asio/ip/udp.hpp>
#include <asio/system_timer.hpp>
#endif

#include <atomic>
#include <mutex>
#include <queue>
#include <thread>

namespace HAL {
namespace Network {

#ifdef USE_ASIO

/**
 * 基于ASIO的UDP套接字实现
 * 提供高性能的异步网络I/O
 */
class AsioUdpSocket : public IUdpSocket {
  private:
    asio::io_context ioContext;
    asio::ip::udp::socket socket;
    asio::ip::udp::endpoint localEndpoint;

    std::thread ioThread;
    std::atomic<bool> isRunning;
    std::atomic<bool> isInitialized;
    std::atomic<bool> isBound;

    UdpReceiveCallback receiveCallback;

    // 接收缓冲区
    static constexpr size_t RECEIVE_BUFFER_SIZE = 65536;
    std::array<uint8_t, RECEIVE_BUFFER_SIZE> receiveBuffer;
    asio::ip::udp::endpoint senderEndpoint;

    // 发送队列（用于批量发送优化）
    struct SendRequest {
        std::vector<uint8_t> data;
        asio::ip::udp::endpoint endpoint;
        UdpSendCallback callback;

        SendRequest(const std::vector<uint8_t> &d,
                    const asio::ip::udp::endpoint &ep,
                    UdpSendCallback cb = nullptr)
            : data(d), endpoint(ep), callback(cb) {}
    };

    std::queue<SendRequest> sendQueue;
    std::mutex sendQueueMutex;
    std::atomic<bool> isSending;

    // 辅助方法
    asio::ip::udp::endpoint createEndpoint(const NetworkAddress &addr) const;
    NetworkAddress
    createNetworkAddress(const asio::ip::udp::endpoint &endpoint) const;
    void startReceive();
    void handleReceive(const asio::error_code &error, size_t bytesReceived);
    void processSendQueue();
    void handleSend(const asio::error_code &error, size_t bytesSent,
                    UdpSendCallback callback);
    void runIoContext();

  public:
    AsioUdpSocket();
    ~AsioUdpSocket() override;

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

    // ASIO特定方法
    asio::io_context &getIoContext() { return ioContext; }
    const asio::ip::udp::socket &getSocket() const { return socket; }
};

/**
 * ASIO UDP套接字工厂
 */
class AsioUdpSocketFactory : public IUdpSocketFactory {
  public:
    std::unique_ptr<IUdpSocket> createUdpSocket() override {
        return std::make_unique<AsioUdpSocket>();
    }
};

#else

/**
 * 当未定义USE_ASIO时的空实现
 */
class AsioUdpSocket : public IUdpSocket {
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

class AsioUdpSocketFactory : public IUdpSocketFactory {
  public:
    std::unique_ptr<IUdpSocket> createUdpSocket() override {
        return std::make_unique<AsioUdpSocket>();
    }
};

#endif // USE_ASIO

} // namespace Network
} // namespace HAL