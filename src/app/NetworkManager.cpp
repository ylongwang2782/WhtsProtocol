#include "NetworkManager.h"
#include "Logger.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>

namespace App {

NetworkManager::NetworkManager() : isRunning(false) {}

NetworkManager::~NetworkManager() { cleanup(); }

bool NetworkManager::initialize(std::unique_ptr<IUdpSocketFactory> factory) {
    if (!factory) {
        Log::e("NetworkManager", "Invalid socket factory");
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
    std::string id = socketId.empty() ? generateSocketId() : socketId;

    if (sockets.find(id) != sockets.end()) {
        Log::e("NetworkManager", "Socket ID already exists: " + id);
        return "";
    }

    auto socket = socketFactory->createUdpSocket();
    if (!socket) {
        Log::e("NetworkManager", "Failed to create UDP socket");
        return "";
    }

    if (!socket->initialize()) {
        Log::e("NetworkManager", "Failed to initialize UDP socket");
        return "";
    }

    socket->setReceiveCallback([this, id](const std::vector<uint8_t> &data,
                                          const NetworkAddress &senderAddr) {
        handleSocketReceive(id, data, senderAddr);
    });

    sockets[id] = std::move(socket);
    Log::i("NetworkManager", "Created UDP socket: " + id);

    return id;
}

bool NetworkManager::bindSocket(const std::string &socketId,
                                const std::string &address, uint16_t port) {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        Log::e("NetworkManager", "Socket not found: " + socketId);
        return false;
    }

    bool result = it->second->bind(address, port);
    if (result) {
        it->second->startAsyncReceive();

        Log::i("NetworkManager", "Socket " + socketId + " bound to " +
                                     (address.empty() ? "0.0.0.0" : address) +
                                     ":" + std::to_string(port));
    } else {
        Log::e("NetworkManager", "Failed to bind socket " + socketId);
    }

    return result;
}

bool NetworkManager::setSocketBroadcast(const std::string &socketId,
                                        bool enable) {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        Log::e("NetworkManager", "Socket not found: " + socketId);
        return false;
    }

    return it->second->setBroadcast(enable);
}

bool NetworkManager::setSocketNonBlocking(const std::string &socketId,
                                          bool nonBlocking) {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        Log::e("NetworkManager", "Socket not found: " + socketId);
        return false;
    }

    return it->second->setNonBlocking(nonBlocking);
}

bool NetworkManager::sendTo(const std::string &socketId,
                            const std::vector<uint8_t> &data,
                            const NetworkAddress &targetAddr) {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        Log::e("NetworkManager", "Socket not found: " + socketId);
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
        Log::e("NetworkManager", "Socket not found: " + socketId);
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
        Log::e("NetworkManager", "Socket not found: " + socketId);
        return -1;
    }

    return it->second->receiveFrom(buffer, bufferSize, senderAddr);
}

bool NetworkManager::closeSocket(const std::string &socketId) {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        Log::e("NetworkManager", "Socket not found: " + socketId);
        return false;
    }

    it->second->close();
    sockets.erase(it);
    std::cout << "[INFO] NetworkManager: Closed socket: " << socketId
              << std::endl;
    return true;
}

NetworkAddress
NetworkManager::getSocketLocalAddress(const std::string &socketId) const {
    auto it = sockets.find(socketId);
    if (it == sockets.end()) {
        Log::e("NetworkManager", "Socket not found: " + socketId);
        return NetworkAddress();
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

    // 启动所有socket的异步接收
    for (auto &pair : sockets) {
        pair.second->startAsyncReceive();
    }

    Log::i("NetworkManager", "Started");
}

void NetworkManager::stop() {
    isRunning = false;
    Log::i("NetworkManager", "Stopped");
}

void NetworkManager::processEvents() {
    if (!isRunning) {
        return;
    }

    // Process events for all sockets
    for (auto &pair : sockets) {
        pair.second->processEvents();
    }
}

std::vector<std::string> NetworkManager::getSocketIds() const {
    std::vector<std::string> ids;
    for (const auto &pair : sockets) {
        ids.push_back(pair.first);
    }
    return ids;
}

void NetworkManager::cleanup() {
    for (auto &pair : sockets) {
        pair.second->close();
    }
    sockets.clear();
    isRunning = false;
    Log::i("NetworkManager", "Cleaned up all sockets");
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

} // namespace App