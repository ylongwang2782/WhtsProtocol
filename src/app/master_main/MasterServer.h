#pragma once

#include "../../Adapter/NetworkFactory.h"
#include "../NetworkManager.h"
#include "CommandTracking.h"
#include "DeviceManager.h"
#include "MessageHandlers.h"
#include "WhtsProtocol.h"
#include <memory>
#include <unordered_map>
#include <vector>

using namespace WhtsProtocol;
using namespace HAL::Network;

class MasterServer {
  private:
    std::unique_ptr<NetworkManager> networkManager;
    std::string mainSocketId;
    NetworkAddress serverAddr;
    NetworkAddress backendAddr;        // Backend address (port 8079)
    NetworkAddress slaveBroadcastAddr; // Slave broadcast address (port 8081)
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
    uint32_t getCurrentTimestampMs();
    void printBytes(const std::vector<uint8_t> &data,
                    const std::string &description);
    std::string bytesToHexString(const std::vector<uint8_t> &bytes);

    // Core processing methods
    void processBackend2MasterMessage(const Message &message,
                                      const NetworkAddress &clientAddr);
    void processSlave2MasterMessage(uint32_t slaveId, const Message &message,
                                    const NetworkAddress &clientAddr);
    void processFrame(Frame &frame, const NetworkAddress &clientAddr);
    void run();

    // Message sending methods
    void sendResponseToBackend(std::unique_ptr<Message> response,
                               const NetworkAddress &clientAddr);
    void sendCommandToSlave(uint32_t slaveId, std::unique_ptr<Message> command,
                            const NetworkAddress &clientAddr);
    void sendCommandToSlaveWithRetry(uint32_t slaveId,
                                     std::unique_ptr<Message> command,
                                     const NetworkAddress &clientAddr,
                                     uint8_t maxRetries = 3);

    // Command management
    void processPendingCommands();
    void addPingSession(uint32_t targetId, uint8_t pingMode,
                        uint16_t totalCount, uint16_t interval,
                        const NetworkAddress &clientAddr);
    void processPingSessions();

    // 数据采集管理
    void processDataCollection();

    // Device management
    DeviceManager &getDeviceManager() { return deviceManager; }
    ProtocolProcessor &getProcessor() { return processor; }
    NetworkManager *getNetworkManager() { return networkManager.get(); }

    // Register message handlers
    void registerMessageHandler(uint8_t messageId,
                                std::unique_ptr<IMessageHandler> handler);

  private:
    void initializeMessageHandlers();
    void onNetworkEvent(const NetworkEvent &event);
};