#pragma once

#include "CommandTracking.h"
#include "DeviceManager.h"
#include "MessageHandlers.h"
#include "WhtsProtocol.h"
#include <memory>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

using namespace WhtsProtocol;

class MasterServer {
  private:
    SOCKET sock;
    sockaddr_in serverAddr;
    sockaddr_in backendAddr;        // Backend address (port 8079)
    sockaddr_in slaveBroadcastAddr; // Slave broadcast address (port 8081)
    ProtocolProcessor processor;
    uint16_t port;
    DeviceManager deviceManager;
    std::unordered_map<uint8_t, std::unique_ptr<IMessageHandler>>
        messageHandlers;
    std::vector<PendingCommand> pendingCommands;
    std::vector<PingSession> activePingSessions;

  public:
    MasterServer(uint16_t listenPort = 8080);
    ~MasterServer();

    // Utility methods
    static std::vector<uint8_t> hexStringToBytes(const std::string &hex);
    static uint32_t getCurrentTimestamp();
    void printBytes(const std::vector<uint8_t> &data,
                    const std::string &description);
    std::string bytesToHexString(const std::vector<uint8_t> &bytes);

    // Core processing methods
    void processBackend2MasterMessage(const Message &message,
                                      const sockaddr_in &clientAddr);
    void processSlave2MasterMessage(uint32_t slaveId, const Message &message,
                                    const sockaddr_in &clientAddr);
    void processFrame(Frame &frame, const sockaddr_in &clientAddr);
    void run();

    // Message sending methods
    void sendResponseToBackend(std::unique_ptr<Message> response,
                               const sockaddr_in &clientAddr);
    void sendCommandToSlave(uint32_t slaveId, std::unique_ptr<Message> command,
                            const sockaddr_in &clientAddr);
    void sendCommandToSlaveWithRetry(uint32_t slaveId,
                                     std::unique_ptr<Message> command,
                                     const sockaddr_in &clientAddr,
                                     uint8_t maxRetries = 3);

    // Command management
    void processPendingCommands();
    void addPingSession(uint32_t targetId, uint8_t pingMode,
                        uint16_t totalCount, uint16_t interval,
                        const sockaddr_in &clientAddr);
    void processPingSessions();

    // 数据采集管理
    void processDataCollection();

    // Device management
    DeviceManager &getDeviceManager() { return deviceManager; }
    ProtocolProcessor &getProcessor() { return processor; }
    SOCKET getSocket() { return sock; }

    // Register message handlers
    void registerMessageHandler(uint8_t messageId,
                                std::unique_ptr<IMessageHandler> handler);

  private:
    void initializeMessageHandlers();
};