#include "AsioUdpSocket.h"
#include <chrono>
#include <iostream>

namespace HAL {
namespace Network {

#ifdef USE_ASIO

AsioUdpSocket::AsioUdpSocket()
    : socket(ioContext), isRunning(false), isInitialized(false), isBound(false),
      isSending(false) {}

AsioUdpSocket::~AsioUdpSocket() { close(); }

bool AsioUdpSocket::initialize() {
    if (isInitialized.load()) {
        return true;
    }

    try {
        // 创建UDP套接字
        socket.open(asio::ip::udp::v4());
        isInitialized.store(true);

        std::cout << "[INFO] AsioUdpSocket: Socket initialized successfully"
                  << std::endl;
        return true;
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] AsioUdpSocket: Failed to initialize socket: "
                  << e.what() << std::endl;
        return false;
    }
}

bool AsioUdpSocket::bind(const std::string &address, uint16_t port) {
    if (!isInitialized.load()) {
        std::cerr << "[ERROR] AsioUdpSocket: Socket not initialized"
                  << std::endl;
        return false;
    }

    try {
        asio::ip::address bindAddr;
        if (address.empty()) {
            bindAddr = asio::ip::address_v4::any();
        } else {
            bindAddr = asio::ip::address::from_string(address);
        }

        localEndpoint = asio::ip::udp::endpoint(bindAddr, port);
        socket.bind(localEndpoint);
        isBound.store(true);

        std::cout << "[INFO] AsioUdpSocket: Socket bound to "
                  << localEndpoint.address().to_string() << ":"
                  << localEndpoint.port() << std::endl;
        return true;
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] AsioUdpSocket: Failed to bind socket: "
                  << e.what() << std::endl;
        return false;
    }
}

bool AsioUdpSocket::setBroadcast(bool enable) {
    if (!isInitialized.load()) {
        return false;
    }

    try {
        socket.set_option(asio::socket_base::broadcast(enable));
        return true;
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] AsioUdpSocket: Failed to set broadcast option: "
                  << e.what() << std::endl;
        return false;
    }
}

bool AsioUdpSocket::setNonBlocking(bool nonBlocking) {
    if (!isInitialized.load()) {
        return false;
    }

    try {
        socket.non_blocking(nonBlocking);
        return true;
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] AsioUdpSocket: Failed to set non-blocking mode: "
                  << e.what() << std::endl;
        return false;
    }
}

asio::ip::udp::endpoint
AsioUdpSocket::createEndpoint(const NetworkAddress &addr) const {
    try {
        auto address = asio::ip::address::from_string(addr.ip);
        return asio::ip::udp::endpoint(address, addr.port);
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] AsioUdpSocket: Invalid address: " << addr.ip
                  << ":" << addr.port << std::endl;
        return asio::ip::udp::endpoint();
    }
}

NetworkAddress AsioUdpSocket::createNetworkAddress(
    const asio::ip::udp::endpoint &endpoint) const {
    return NetworkAddress(endpoint.address().to_string(), endpoint.port());
}

bool AsioUdpSocket::sendTo(const std::vector<uint8_t> &data,
                           const NetworkAddress &targetAddr,
                           UdpSendCallback callback) {
    if (!isInitialized.load() || data.empty()) {
        if (callback) {
            callback(false, 0);
        }
        return false;
    }

    auto endpoint = createEndpoint(targetAddr);
    if (endpoint.port() == 0) {
        if (callback) {
            callback(false, 0);
        }
        return false;
    }

    // 添加到发送队列
    {
        std::lock_guard<std::mutex> lock(sendQueueMutex);
        sendQueue.emplace(data, endpoint, callback);
    }

    // 触发发送处理
    asio::post(ioContext, [this]() { processSendQueue(); });

    return true;
}

bool AsioUdpSocket::broadcast(const std::vector<uint8_t> &data, uint16_t port,
                              UdpSendCallback callback) {
    // 使用广播地址
    NetworkAddress broadcastAddr("255.255.255.255", port);
    return sendTo(data, broadcastAddr, callback);
}

void AsioUdpSocket::processSendQueue() {
    if (isSending.load()) {
        return; // 已经在发送中
    }

    std::lock_guard<std::mutex> lock(sendQueueMutex);
    if (sendQueue.empty()) {
        return;
    }

    isSending.store(true);
    SendRequest request = std::move(sendQueue.front());
    sendQueue.pop();

    // 异步发送
    socket.async_send_to(asio::buffer(request.data), request.endpoint,
                         [this, callback = request.callback](
                             const asio::error_code &error, size_t bytesSent) {
                             handleSend(error, bytesSent, callback);
                         });
}

void AsioUdpSocket::handleSend(const asio::error_code &error, size_t bytesSent,
                               UdpSendCallback callback) {
    bool success = !error;

    if (callback) {
        callback(success, success ? bytesSent : 0);
    }

    if (error) {
        std::cerr << "[ERROR] AsioUdpSocket: Send failed: " << error.message()
                  << std::endl;
    }

    isSending.store(false);

    // 继续处理队列中的下一个发送请求
    asio::post(ioContext, [this]() { processSendQueue(); });
}

int AsioUdpSocket::receiveFrom(uint8_t *buffer, size_t bufferSize,
                               NetworkAddress &senderAddr) {
    if (!isInitialized.load() || !buffer || bufferSize == 0) {
        return -1;
    }

    try {
        asio::ip::udp::endpoint senderEndpoint;
        asio::error_code error;

        size_t bytesReceived = socket.receive_from(
            asio::buffer(buffer, bufferSize), senderEndpoint, 0, error);

        if (error) {
            if (error == asio::error::would_block) {
                return 0; // 非阻塞模式下没有数据
            }
            return -1; // 其他错误
        }

        senderAddr = createNetworkAddress(senderEndpoint);
        return static_cast<int>(bytesReceived);
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] AsioUdpSocket: Receive failed: " << e.what()
                  << std::endl;
        return -1;
    }
}

void AsioUdpSocket::setReceiveCallback(UdpReceiveCallback callback) {
    receiveCallback = callback;
}

void AsioUdpSocket::startAsyncReceive() {
    if (!isInitialized.load() || !isBound.load()) {
        std::cerr << "[ERROR] AsioUdpSocket: Cannot start receive - socket not "
                     "properly initialized"
                  << std::endl;
        return;
    }

    if (!isRunning.load()) {
        isRunning.store(true);

        // 启动IO线程
        ioThread = std::thread([this]() { runIoContext(); });

        // 开始异步接收
        startReceive();

        std::cout << "[INFO] AsioUdpSocket: Started async receive" << std::endl;
    }
}

void AsioUdpSocket::stopAsyncReceive() {
    if (isRunning.load()) {
        isRunning.store(false);

        // 停止IO上下文
        ioContext.stop();

        // 等待IO线程结束
        if (ioThread.joinable()) {
            ioThread.join();
        }

        std::cout << "[INFO] AsioUdpSocket: Stopped async receive" << std::endl;
    }
}

void AsioUdpSocket::startReceive() {
    if (!isRunning.load()) {
        return;
    }

    socket.async_receive_from(
        asio::buffer(receiveBuffer), senderEndpoint,
        [this](const asio::error_code &error, size_t bytesReceived) {
            handleReceive(error, bytesReceived);
        });
}

void AsioUdpSocket::handleReceive(const asio::error_code &error,
                                  size_t bytesReceived) {
    if (!error && bytesReceived > 0) {
        if (receiveCallback) {
            std::vector<uint8_t> data(receiveBuffer.begin(),
                                      receiveBuffer.begin() + bytesReceived);
            NetworkAddress senderAddr = createNetworkAddress(senderEndpoint);
            receiveCallback(data, senderAddr);
        }
    } else if (error && error != asio::error::operation_aborted) {
        std::cerr << "[ERROR] AsioUdpSocket: Receive error: " << error.message()
                  << std::endl;
    }

    // 继续接收下一个数据包
    if (isRunning.load()) {
        startReceive();
    }
}

void AsioUdpSocket::runIoContext() {
    try {
        // 重新启动IO上下文
        ioContext.restart();

        // 运行IO上下文
        ioContext.run();

        std::cout << "[INFO] AsioUdpSocket: IO context finished" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] AsioUdpSocket: IO context error: " << e.what()
                  << std::endl;
    }
}

void AsioUdpSocket::processEvents() {
    // ASIO使用自己的事件循环，这里可以处理一些轻量级任务
    if (ioContext.stopped() && isRunning.load()) {
        // 如果IO上下文停止了但应该运行，重新启动
        ioContext.restart();
    }
}

void AsioUdpSocket::close() {
    stopAsyncReceive();

    if (socket.is_open()) {
        try {
            socket.close();
        } catch (const std::exception &e) {
            std::cerr << "[ERROR] AsioUdpSocket: Error closing socket: "
                      << e.what() << std::endl;
        }
    }

    isInitialized.store(false);
    isBound.store(false);

    // 清空发送队列
    {
        std::lock_guard<std::mutex> lock(sendQueueMutex);
        while (!sendQueue.empty()) {
            sendQueue.pop();
        }
    }

    std::cout << "[INFO] AsioUdpSocket: Socket closed" << std::endl;
}

NetworkAddress AsioUdpSocket::getLocalAddress() const {
    if (!isBound.load()) {
        return NetworkAddress();
    }

    try {
        auto endpoint = socket.local_endpoint();
        return createNetworkAddress(endpoint);
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] AsioUdpSocket: Failed to get local address: "
                  << e.what() << std::endl;
        return NetworkAddress();
    }
}

bool AsioUdpSocket::isOpen() const {
    return isInitialized.load() && socket.is_open();
}

#endif // USE_ASIO

} // namespace Network
} // namespace HAL