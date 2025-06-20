#include "NetworkFactory.h"
#include "NetworkManager.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>


using namespace HAL::Network;

/**
 * ASIO网络抽象层使用示例
 * 展示高性能异步I/O的使用方法
 */
class AsioNetworkExample {
  private:
    std::unique_ptr<NetworkManager> networkManager;
    std::string serverSocketId;
    std::string clientSocketId;
    std::atomic<bool> running = false;
    std::atomic<int> messagesReceived = 0;
    std::atomic<int> messagesSent = 0;

  public:
    bool initialize() {
        // 强制使用ASIO实现
        auto platform = PlatformType::WINDOWS_ASIO;
#ifdef __linux__
        platform = PlatformType::LINUX_ASIO;
#endif

        // 检查ASIO支持
        if (!NetworkFactory::isPlatformSupported(platform)) {
            std::cerr << "[ERROR] ASIO is not supported on this platform"
                      << std::endl;
            std::cerr << "[INFO] Please compile with -DUSE_ASIO=ON"
                      << std::endl;
            return false;
        }

        // 创建使用ASIO的网络管理器
        networkManager = NetworkFactory::createNetworkManager(platform);
        if (!networkManager) {
            std::cerr << "[ERROR] Failed to create ASIO network manager"
                      << std::endl;
            return false;
        }

        // 设置网络事件回调
        networkManager->setEventCallback(
            [this](const NetworkEvent &event) { handleNetworkEvent(event); });

        std::cout << "[INFO] ASIO Network Example initialized successfully"
                  << std::endl;
        std::cout << "[INFO] Platform: "
                  << NetworkFactory::getPlatformName(platform) << std::endl;
        return true;
    }

    bool setupHighPerformanceServer(uint16_t port) {
        // 创建服务器套接字
        serverSocketId = networkManager->createUdpSocket("asio_server");
        if (serverSocketId.empty()) {
            std::cerr << "[ERROR] Failed to create ASIO server socket"
                      << std::endl;
            return false;
        }

        // 绑定到指定端口
        if (!networkManager->bindSocket(serverSocketId, "", port)) {
            std::cerr << "[ERROR] Failed to bind ASIO server socket to port "
                      << port << std::endl;
            return false;
        }

        // 启用广播接收
        networkManager->setSocketBroadcast(serverSocketId, true);

        std::cout << "[INFO] ASIO Server socket created and bound to port "
                  << port << std::endl;
        std::cout << "[INFO] Server ready for high-performance async I/O"
                  << std::endl;
        return true;
    }

    bool setupHighPerformanceClient() {
        // 创建客户端套接字
        clientSocketId = networkManager->createUdpSocket("asio_client");
        if (clientSocketId.empty()) {
            std::cerr << "[ERROR] Failed to create ASIO client socket"
                      << std::endl;
            return false;
        }

        // 启用广播发送
        networkManager->setSocketBroadcast(clientSocketId, true);

        std::cout << "[INFO] ASIO Client socket created" << std::endl;
        std::cout << "[INFO] Client ready for high-performance async I/O"
                  << std::endl;
        return true;
    }

    void sendHighVolumeMessages() {
        if (clientSocketId.empty()) {
            return;
        }

        // 发送大量消息测试异步性能
        for (int i = 0; i < 100; ++i) {
            std::string message =
                "ASIO High-Performance Message #" + std::to_string(i);
            std::vector<uint8_t> data(message.begin(), message.end());

            NetworkAddress targetAddr("127.0.0.1", 9090);

            if (networkManager->sendTo(clientSocketId, data, targetAddr)) {
                messagesSent++;
            }

            // 模拟高频发送
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::cout << "[INFO] Sent " << messagesSent.load()
                  << " high-volume messages" << std::endl;
    }

    void sendBurstMessages() {
        if (clientSocketId.empty()) {
            return;
        }

        // 突发发送测试
        std::cout << "[INFO] Starting burst message test..." << std::endl;

        auto startTime = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 1000; ++i) {
            std::string message = "Burst-" + std::to_string(i);
            std::vector<uint8_t> data(message.begin(), message.end());

            NetworkAddress targetAddr("127.0.0.1", 9090);
            networkManager->sendTo(clientSocketId, data, targetAddr);
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        std::cout << "[INFO] Burst test completed: 1000 messages in "
                  << duration.count() << "ms" << std::endl;
    }

    void runPerformanceTest() {
        running = true;
        networkManager->start();

        std::cout << "[INFO] ASIO Performance Test Started" << std::endl;
        std::cout << "[INFO] Testing high-performance async I/O capabilities"
                  << std::endl;

        // 测试1: 高频消息发送
        std::thread highVolumeThread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            sendHighVolumeMessages();
        });

        // 测试2: 突发消息发送
        std::thread burstThread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            sendBurstMessages();
        });

        // 主循环：处理网络事件
        auto startTime = std::chrono::steady_clock::now();
        while (running) {
            networkManager->processEvents();

            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                currentTime - startTime);

            if (elapsed.count() >= 10) {
                running = false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // 等待测试线程完成
        if (highVolumeThread.joinable()) {
            highVolumeThread.join();
        }
        if (burstThread.joinable()) {
            burstThread.join();
        }

        networkManager->stop();

        std::cout << "[INFO] Performance test completed" << std::endl;
        std::cout << "[INFO] Messages sent: " << messagesSent.load()
                  << std::endl;
        std::cout << "[INFO] Messages received: " << messagesReceived.load()
                  << std::endl;
    }

    void stop() { running = false; }

  private:
    void handleNetworkEvent(const NetworkEvent &event) {
        switch (event.type) {
        case NetworkEventType::DATA_RECEIVED:
            messagesReceived++;
            if (messagesReceived.load() % 100 == 0) {
                std::cout << "[INFO] Received " << messagesReceived.load()
                          << " messages (latest from " << event.remoteAddr.ip
                          << ":" << event.remoteAddr.port << ")" << std::endl;
            }
            break;

        case NetworkEventType::DATA_SENT:
            // 高频发送时不打印每条消息
            break;

        case NetworkEventType::CONNECTION_ERROR:
            std::cerr << "[ERROR] ASIO Connection error on socket "
                      << event.socketId << ": " << event.errorMessage
                      << std::endl;
            break;

        case NetworkEventType::SOCKET_CLOSED:
            std::cout << "[INFO] ASIO Socket closed: " << event.socketId
                      << std::endl;
            break;

        default:
            break;
        }
    }
};

/**
 * 比较不同实现的性能
 */
void comparePerformance() {
    std::cout << "\n=== Performance Comparison ===" << std::endl;

    // 检查可用的实现
    std::vector<PlatformType> platforms = {
        PlatformType::WINDOWS, PlatformType::WINDOWS_ASIO, PlatformType::LINUX,
        PlatformType::LINUX_ASIO};

    for (auto platform : platforms) {
        if (NetworkFactory::isPlatformSupported(platform)) {
            std::cout << "[AVAILABLE] "
                      << NetworkFactory::getPlatformName(platform) << std::endl;
        } else {
            std::cout << "[NOT AVAILABLE] "
                      << NetworkFactory::getPlatformName(platform) << std::endl;
        }
    }
}

/**
 * 主函数 - 演示ASIO网络抽象层的高性能特性
 */
int main() {
    std::cout << "=== ASIO High-Performance Network Layer Example ==="
              << std::endl;

    // 比较性能
    comparePerformance();

    AsioNetworkExample example;

    // 初始化
    if (!example.initialize()) {
        std::cerr << "[ERROR] Failed to initialize ASIO network example"
                  << std::endl;
        return 1;
    }

    // 设置高性能服务器和客户端
    if (!example.setupHighPerformanceServer(9090)) {
        std::cerr << "[ERROR] Failed to setup ASIO server" << std::endl;
        return 1;
    }

    if (!example.setupHighPerformanceClient()) {
        std::cerr << "[ERROR] Failed to setup ASIO client" << std::endl;
        return 1;
    }

    // 运行性能测试
    try {
        example.runPerformanceTest();
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] Exception in ASIO example: " << e.what()
                  << std::endl;
        return 1;
    }

    std::cout << "[INFO] ASIO example completed successfully" << std::endl;
    std::cout
        << "[INFO] ASIO provides superior performance for high-volume async I/O"
        << std::endl;
    return 0;
}