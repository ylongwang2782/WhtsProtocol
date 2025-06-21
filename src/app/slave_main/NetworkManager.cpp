#include "NetworkManager.h"
#include "../Logger.h"
#include <iostream>

namespace SlaveApp {

NetworkManager::NetworkManager(uint16_t listenPort, uint32_t deviceId)
    : port(listenPort), deviceId(deviceId) {

    // 创建网络管理器
    networkManager = NetworkFactory::createNetworkManager();
    if (!networkManager) {
        throw std::runtime_error("Failed to create network manager");
    }
}

NetworkManager::~NetworkManager() {
    if (networkManager) {
        networkManager->cleanup();
    }
}

bool NetworkManager::initialize() {
    // 创建主套接字
    mainSocketId = networkManager->createUdpSocket("slave_main");
    if (mainSocketId.empty()) {
        Log::e("NetworkManager", "Failed to create main UDP socket");
        return false;
    }

    // 启用广播功能
    if (!networkManager->setSocketBroadcast(mainSocketId, true)) {
        Log::w("NetworkManager", "Failed to enable broadcast option");
    }

    // 配置服务器地址 (Slave监听端口8081接收广播)
    serverAddr = NetworkAddress("0.0.0.0", port);

    // 配置主机地址 (Master使用端口8080)
    masterAddr = NetworkAddress("127.0.0.1", 8080);

    // 绑定套接字
    if (!networkManager->bindSocket(mainSocketId, serverAddr.ip,
                                    serverAddr.port)) {
        Log::e("NetworkManager", "Failed to bind socket");
        return false;
    }

    processor.setMTU(100);

    Log::i("NetworkManager",
           "Network initialized - Device ID: 0x%08X, listening on port %d",
           deviceId, port);
    Log::i("NetworkManager", "Master communication port: 8080");
    Log::i("NetworkManager", "Wireless broadcast reception enabled");

    return true;
}

void NetworkManager::setNonBlocking() {
    if (!mainSocketId.empty()) {
        networkManager->setSocketNonBlocking(mainSocketId, true);
    }
}

int NetworkManager::receiveData(char *buffer, size_t bufferSize,
                                NetworkAddress &senderAddr) {
    if (mainSocketId.empty()) {
        return -1;
    }

    return networkManager->receiveFrom(mainSocketId,
                                       reinterpret_cast<uint8_t *>(buffer),
                                       bufferSize, senderAddr);
}

void NetworkManager::sendResponse(const std::vector<uint8_t> &data) {
    if (mainSocketId.empty() || data.empty()) {
        return;
    }

    // 发送响应到主机端口8080
    networkManager->sendTo(mainSocketId, data, masterAddr);
}

} // namespace SlaveApp