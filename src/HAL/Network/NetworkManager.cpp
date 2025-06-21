#include "NetworkManager.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>

namespace HAL {
namespace Network {

NetworkManager::NetworkManager() : isRunning(false) {}

NetworkManager::~NetworkManager() { cleanup(); }

bool NetworkManager::initialize(std::unique_ptr<IUdpSocketFactory> factory) {
    if (!factory) {
        std::cerr << "[ERROR] NetworkManager: Invalid socket factory"
                  << std::endl;
        return false;
    }

    socketFactory = std::move(factory);
    return true;
}

std::string NetworkManager::generateSocketId() {
    static int counter = 0;
    std::ostringstream oss;
    oss << "socket_" << ++counter;
    return oss.str();
}

std::string NetworkManager::createUdpSocket(const std::string &socketId) {
    if (!socketFactory) {
        std::cerr << "[ERROR] NetworkManager: Socket factory not initialized"
                  << std::endl;
        return "";
    }

    std::string id = socketId.empty() ? generateSocketId() : socketId;

    // 检查ID是否已存在
    if (sockets.find(id) != sockets.end()) {
        std::cerr << "[ERROR] NetworkManager: Socket ID already exists: " << id
                  << std::endl;
        return "";
    }

    auto socket = socketFactory->createUdpSocket();
    if (!socket) {
        std::cerr << "[ERROR] NetworkManager: Failed to create UDP socket"
                  << std::endl;
        return "";
    }

    if (!socket->initialize()) {
        std::cerr << "[ERROR] NetworkManager: Failed to initialize UDP socket"
                  << std::endl;
        return "";
    }

    // 设置接收回调
    socket->setReceiveCallback([this, id](const std::vector<uint8_t> &data,
                                          const NetworkAddress &senderAddr) {
        handleSocketReceive(id, data, senderAddr);
    });

    sockets[id] = std::move(socket);

    std::cout << "[INFO] NetworkManager: Created UDP socket: " << id
              << std::endl;
    return id;
}

bool NetworkManager::bindSocket(const std::string &socketId,
                                const std::string &address, uint16_t port) {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        std::cerr << "[ERROR] NetworkManager: Socket not found: " << socketId
                  << std::endl;
        return false;
    }

    bool result = it->second->bind(address, port);
    if (result) {
        std::cout << "[INFO] NetworkManager: Socket " << socketId
                  << " bound to " << (address.empty() ? "0.0.0.0" : address)
                  << ":" << port << std::endl;
    } else {
        std::cerr << "[ERROR] NetworkManager: Failed to bind socket "
                  << socketId << std::endl;
    }

    return result;
}

bool NetworkManager::setSocketBroadcast(const std::string &socketId,
                                        bool enable) {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        std::cerr << "[ERROR] NetworkManager: Socket not found: " << socketId
                  << std::endl;
        return false;
    }

    return it->second->setBroadcast(enable);
}

bool NetworkManager::setSocketNonBlocking(const std::string &socketId,
                                          bool nonBlocking) {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        std::cerr << "[ERROR] NetworkManager: Socket not found: " << socketId
                  << std::endl;
        return false;
    }

    return it->second->setNonBlocking(nonBlocking);
}

bool NetworkManager::sendTo(const std::string &socketId,
                            const std::vector<uint8_t> &data,
                            const NetworkAddress &targetAddr) {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        std::cerr << "[ERROR] NetworkManager: Socket not found: " << socketId
                  << std::endl;
        return false;
    }

    return it->second->sendTo(
        data, targetAddr, [this, socketId](bool success, size_t bytesSent) {
            handleSocketSend(socketId, success, bytesSent);
        });
}

bool NetworkManager::broadcast(const std::string &socketId,
                               const std::vector<uint8_t> &data,
                               uint16_t port) {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        std::cerr << "[ERROR] NetworkManager: Socket not found: " << socketId
                  << std::endl;
        return false;
    }

    return it->second->broadcast(
        data, port, [this, socketId](bool success, size_t bytesSent) {
            handleSocketSend(socketId, success, bytesSent);
        });
}

int NetworkManager::receiveFrom(const std::string &socketId, uint8_t *buffer,
                                size_t bufferSize, NetworkAddress &senderAddr) {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        std::cerr << "[ERROR] NetworkManager: Socket not found: " << socketId
                  << std::endl;
        return -1;
    }

    return it->second->receiveFrom(buffer, bufferSize, senderAddr);
}

bool NetworkManager::closeSocket(const std::string &socketId) {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        std::cerr << "[ERROR] NetworkManager: Socket not found: " << socketId
                  << std::endl;
        return false;
    }

    it->second->close();
    sockets.erase(it);

    std::cout << "[INFO] NetworkManager: Closed socket: " << socketId
              << std::endl;

    // 发送套接字关闭事件
    if (eventCallback) {
        NetworkEvent event(NetworkEventType::SOCKET_CLOSED, socketId);
        eventCallback(event);
    }

    return true;
}

NetworkAddress
NetworkManager::getSocketLocalAddress(const std::string &socketId) const {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        return NetworkAddress(); // 返回无效地址
    }

    return it->second->getLocalAddress();
}

bool NetworkManager::isSocketOpen(const std::string &socketId) const {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        return false;
    }

    return it->second->isOpen();
}

void NetworkManager::setEventCallback(NetworkEventCallback callback) {
    eventCallback = callback;
}

void NetworkManager::start() {
    isRunning = true;

    // 启动所有套接字的异步接收
    for (auto &pair : sockets) {
        pair.second->startAsyncReceive();
    }

    std::cout << "[INFO] NetworkManager: Started with " << sockets.size()
              << " sockets" << std::endl;
}

void NetworkManager::stop() {
    isRunning = false;

    // 停止所有套接字的异步接收
    for (auto &pair : sockets) {
        pair.second->stopAsyncReceive();
    }

    std::cout << "[INFO] NetworkManager: Stopped" << std::endl;
}

void NetworkManager::processEvents() {
    if (!isRunning) {
        return;
    }

    // 处理所有套接字的事件
    for (auto &pair : sockets) {
        pair.second->processEvents();
    }
}

std::vector<std::string> NetworkManager::getSocketIds() const {
    std::vector<std::string> ids;
    ids.reserve(sockets.size());

    for (const auto &pair : sockets) {
        ids.push_back(pair.first);
    }

    return ids;
}

void NetworkManager::cleanup() {
    stop();
    sockets.clear();
    socketFactory.reset();
    std::cout << "[INFO] NetworkManager: Cleaned up" << std::endl;
}

void NetworkManager::handleSocketReceive(const std::string &socketId,
                                         const std::vector<uint8_t> &data,
                                         const NetworkAddress &senderAddr) {
    if (eventCallback) {
        NetworkEvent event(NetworkEventType::DATA_RECEIVED, socketId);
        event.data = data;
        event.remoteAddr = senderAddr;
        eventCallback(event);
    }
}

void NetworkManager::handleSocketSend(const std::string &socketId, bool success,
                                      size_t bytesSent) {
    if (eventCallback) {
        NetworkEvent event(NetworkEventType::DATA_SENT, socketId);
        if (!success) {
            event.type = NetworkEventType::CONNECTION_ERROR;
            event.errorMessage = "Send failed";
        }
        eventCallback(event);
    }
}

} // namespace Network
} // namespace HAL