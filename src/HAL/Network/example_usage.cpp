#include "NetworkFactory.h"
#include "NetworkManager.h"
#include <chrono>
#include <iostream>
#include <thread>

using namespace HAL::Network;

/**
 * 网络抽象层使用示例
 * 展示如何使用跨平台的网络通信接口
 */
class NetworkExample {
  private:
    std::unique_ptr<NetworkManager> networkManager;
    std::string serverSocketId;
    std::string clientSocketId;
    bool running = false;

  public:
    bool initialize() {
        // 创建网络管理器（自动选择当前平台的实现）
        networkManager = NetworkFactory::createNetworkManager();
        if (!networkManager) {
            std::cerr << "[ERROR] Failed to create network manager"
                      << std::endl;
            return false;
        }

        // 设置网络事件回调
        networkManager->setEventCallback(
            [this](const NetworkEvent &event) { handleNetworkEvent(event); });

        return true;
    }

    bool setupServer(uint16_t port) {
        // 创建服务器套接字
        serverSocketId = networkManager->createUdpSocket("server");
        if (serverSocketId.empty()) {
            std::cerr << "[ERROR] Failed to create server socket" << std::endl;
            return false;
        }

        // 绑定到指定端口
        if (!networkManager->bindSocket(serverSocketId, "", port)) {
            std::cerr << "[ERROR] Failed to bind server socket to port " << port
                      << std::endl;
            return false;
        }

        // 启用广播接收
        networkManager->setSocketBroadcast(serverSocketId, true);
        networkManager->setSocketNonBlocking(serverSocketId, true);

        std::cout << "[INFO] Server socket created and bound to port " << port
                  << std::endl;
        return true;
    }

    bool setupClient() {
        // 创建客户端套接字
        clientSocketId = networkManager->createUdpSocket("client");
        if (clientSocketId.empty()) {
            std::cerr << "[ERROR] Failed to create client socket" << std::endl;
            return false;
        }

        // 启用广播发送
        networkManager->setSocketBroadcast(clientSocketId, true);
        networkManager->setSocketNonBlocking(clientSocketId, true);

        std::cout << "[INFO] Client socket created" << std::endl;
        return true;
    }

    void sendTestMessage() {
        if (clientSocketId.empty()) {
            return;
        }

        // 发送测试消息
        std::string message = "Hello from cross-platform network layer!";
        std::vector<uint8_t> data(message.begin(), message.end());

        NetworkAddress targetAddr("127.0.0.1", 8080);

        if (networkManager->sendTo(clientSocketId, data, targetAddr)) {
            std::cout << "[INFO] Sent message: " << message << std::endl;
        } else {
            std::cerr << "[ERROR] Failed to send message" << std::endl;
        }
    }

    void sendBroadcastMessage() {
        if (clientSocketId.empty()) {
            return;
        }

        // 发送广播消息
        std::string message = "Broadcast message from network layer!";
        std::vector<uint8_t> data(message.begin(), message.end());

        if (networkManager->broadcast(clientSocketId, data, 8080)) {
            std::cout << "[INFO] Sent broadcast message: " << message
                      << std::endl;
        } else {
            std::cerr << "[ERROR] Failed to send broadcast message"
                      << std::endl;
        }
    }

    void run() {
        running = true;
        networkManager->start();

        std::cout << "[INFO] Network example started" << std::endl;
        std::cout << "[INFO] Current platform: "
                  << NetworkFactory::getPlatformName(
                         NetworkFactory::getCurrentPlatform())
                  << std::endl;

        int messageCount = 0;
        while (running && messageCount < 5) {
            // 处理网络事件
            networkManager->processEvents();

            // 发送测试消息
            if (messageCount % 2 == 0) {
                sendTestMessage();
            } else {
                sendBroadcastMessage();
            }

            messageCount++;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        networkManager->stop();
        std::cout << "[INFO] Network example finished" << std::endl;
    }

    void stop() { running = false; }

  private:
    void handleNetworkEvent(const NetworkEvent &event) {
        switch (event.type) {
        case NetworkEventType::DATA_RECEIVED:
            std::cout << "[INFO] Received data from " << event.remoteAddr.ip
                      << ":" << event.remoteAddr.port << " on socket "
                      << event.socketId << std::endl;
            std::cout << "[INFO] Data: "
                      << std::string(event.data.begin(), event.data.end())
                      << std::endl;
            break;

        case NetworkEventType::DATA_SENT:
            std::cout << "[INFO] Data sent successfully on socket "
                      << event.socketId << std::endl;
            break;

        case NetworkEventType::CONNECTION_ERROR:
            std::cerr << "[ERROR] Connection error on socket " << event.socketId
                      << ": " << event.errorMessage << std::endl;
            break;

        case NetworkEventType::SOCKET_CLOSED:
            std::cout << "[INFO] Socket closed: " << event.socketId
                      << std::endl;
            break;

        default:
            std::cout << "[INFO] Unknown network event on socket "
                      << event.socketId << std::endl;
            break;
        }
    }
};

/**
 * 主函数 - 演示网络抽象层的使用
 */
int main() {
    std::cout << "=== Cross-Platform Network Layer Example ===" << std::endl;

    // 检查平台支持
    auto currentPlatform = NetworkFactory::getCurrentPlatform();
    std::cout << "[INFO] Detected platform: "
              << NetworkFactory::getPlatformName(currentPlatform) << std::endl;

    if (!NetworkFactory::isPlatformSupported(currentPlatform)) {
        std::cerr << "[ERROR] Current platform is not supported" << std::endl;
        return 1;
    }

    NetworkExample example;

    // 初始化
    if (!example.initialize()) {
        std::cerr << "[ERROR] Failed to initialize network example"
                  << std::endl;
        return 1;
    }

    // 设置服务器和客户端
    if (!example.setupServer(8080)) {
        std::cerr << "[ERROR] Failed to setup server" << std::endl;
        return 1;
    }

    if (!example.setupClient()) {
        std::cerr << "[ERROR] Failed to setup client" << std::endl;
        return 1;
    }

    // 运行示例
    try {
        example.run();
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] Exception in network example: " << e.what()
                  << std::endl;
        return 1;
    }

    std::cout << "[INFO] Example completed successfully" << std::endl;
    return 0;
}