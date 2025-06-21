#include "MessageHandlers.h"
#include "../../interface/IUdpSocket.h"
#include "../Logger.h"
#include "MasterServer.h"


using namespace HAL::Network;

// Slave Configuration Message Handler
std::unique_ptr<Message>
SlaveConfigHandler::processMessage(const Message &message,
                                   MasterServer *server) {
    const auto *configMsg =
        dynamic_cast<const Backend2Master::SlaveConfigMessage *>(&message);
    if (!configMsg)
        return nullptr;

    Log::i("SlaveConfigHandler", "Processing slave config message");

    auto response =
        std::make_unique<Master2Backend::SlaveConfigResponseMessage>();
    response->status = 0; // Success
    response->slaveNum = configMsg->slaveNum;

    // Copy slaves info
    for (const auto &slave : configMsg->slaves) {
        Master2Backend::SlaveConfigResponseMessage::SlaveInfo slaveInfo;
        slaveInfo.id = slave.id;
        slaveInfo.conductionNum = slave.conductionNum;
        slaveInfo.resistanceNum = slave.resistanceNum;
        slaveInfo.clipMode = slave.clipMode;
        slaveInfo.clipStatus = slave.clipStatus;
        response->slaves.push_back(slaveInfo);
    }

    return std::move(response);
}

void SlaveConfigHandler::executeActions(const Message &message,
                                        MasterServer *server) {
    const auto *configMsg =
        dynamic_cast<const Backend2Master::SlaveConfigMessage *>(&message);
    if (!configMsg)
        return;

    // Store slave configurations in device manager
    for (const auto &slave : configMsg->slaves) {
        server->getDeviceManager().addSlave(slave.id);
        server->getDeviceManager().setSlaveConfig(slave.id, slave);
        Log::i("SlaveConfigHandler",
               "Stored config for slave 0x%08X: Conduction=%d, Resistance=%d, "
               "ClipMode=%d",
               slave.id, static_cast<int>(slave.conductionNum),
               static_cast<int>(slave.resistanceNum),
               static_cast<int>(slave.clipMode));
    }

    Log::i("SlaveConfigHandler", "Configuration actions executed for %d slaves",
           static_cast<int>(configMsg->slaveNum));
}

// Mode Configuration Message Handler
std::unique_ptr<Message>
ModeConfigHandler::processMessage(const Message &message,
                                  MasterServer *server) {
    const auto *modeMsg =
        dynamic_cast<const Backend2Master::ModeConfigMessage *>(&message);
    if (!modeMsg)
        return nullptr;

    Log::i("ModeConfigHandler", "Processing mode config message - Mode: %d",
           static_cast<int>(modeMsg->mode));

    auto response =
        std::make_unique<Master2Backend::ModeConfigResponseMessage>();
    response->status = 0; // Success
    response->mode = modeMsg->mode;

    return std::move(response);
}

void ModeConfigHandler::executeActions(const Message &message,
                                       MasterServer *server) {
    const auto *modeMsg =
        dynamic_cast<const Backend2Master::ModeConfigMessage *>(&message);
    if (!modeMsg)
        return;

    // Set the mode in device manager
    server->getDeviceManager().setCurrentMode(modeMsg->mode);

    Log::i("ModeConfigHandler", "Mode set to %d",
           static_cast<int>(modeMsg->mode));

    // Get connected slaves and send configuration based on mode
    auto connectedSlaves = server->getDeviceManager().getConnectedSlaves();

    for (uint32_t slaveId : connectedSlaves) {
        if (server->getDeviceManager().hasSlaveConfig(slaveId)) {
            const auto &slaveConfig =
                server->getDeviceManager().getSlaveConfig(slaveId);

            switch (modeMsg->mode) {
            case 0: // Conduction mode
                if (slaveConfig.conductionNum > 0) {
                    auto condCmd = std::make_unique<
                        Master2Slave::ConductionConfigMessage>();
                    condCmd->timeSlot = 1;
                    condCmd->interval = 100; // 100ms default
                    condCmd->totalConductionNum = slaveConfig.conductionNum;
                    condCmd->startConductionNum = 0;
                    condCmd->conductionNum = slaveConfig.conductionNum;

                    // Use retry mechanism for important configuration commands
                    server->sendCommandToSlaveWithRetry(
                        slaveId, std::move(condCmd), NetworkAddress{}, 3);
                    Log::i("ModeConfigHandler",
                           "Sent conduction config to slave 0x%08X", slaveId);
                }
                break;

            case 1: // Resistance mode
                if (slaveConfig.resistanceNum > 0) {
                    auto resCmd = std::make_unique<
                        Master2Slave::ResistanceConfigMessage>();
                    resCmd->timeSlot = 1;
                    resCmd->interval = 100; // 100ms default
                    resCmd->totalNum = slaveConfig.resistanceNum;
                    resCmd->startNum = 0;
                    resCmd->num = slaveConfig.resistanceNum;

                    server->sendCommandToSlaveWithRetry(
                        slaveId, std::move(resCmd), NetworkAddress{}, 3);
                    Log::i("ModeConfigHandler",
                           "Sent resistance config to slave 0x%08X", slaveId);
                }
                break;

            case 2: // Clip mode
            {
                auto clipCmd =
                    std::make_unique<Master2Slave::ClipConfigMessage>();
                clipCmd->interval = 100; // 100ms default
                clipCmd->mode = slaveConfig.clipMode;
                clipCmd->clipPin = slaveConfig.clipStatus;

                server->sendCommandToSlaveWithRetry(slaveId, std::move(clipCmd),
                                                    NetworkAddress{}, 3);
                Log::i("ModeConfigHandler", "Sent clip config to slave 0x%08X",
                       slaveId);
            } break;

            default:
                Log::w("ModeConfigHandler", "Unknown mode: %d",
                       static_cast<int>(modeMsg->mode));
                break;
            }
        } else {
            Log::w("ModeConfigHandler",
                   "No configuration found for slave 0x%08X", slaveId);
        }
    }

    Log::i("ModeConfigHandler",
           "Mode configuration applied: %d, sent to %zu slaves",
           static_cast<int>(modeMsg->mode), connectedSlaves.size());
}

// Reset Message Handler
std::unique_ptr<Message> ResetHandler::processMessage(const Message &message,
                                                      MasterServer *server) {
    const auto *rstMsg =
        dynamic_cast<const Backend2Master::RstMessage *>(&message);
    if (!rstMsg)
        return nullptr;

    Log::i("ResetHandler", "Processing reset message - Slave count: %d",
           static_cast<int>(rstMsg->slaveNum));

    for (const auto &slave : rstMsg->slaves) {
        Log::i("ResetHandler",
               "  Reset Slave ID: 0x%08X, Lock: %d, Clip status: 0x%04X",
               slave.id, static_cast<int>(slave.lock), slave.clipStatus);
    }

    auto response = std::make_unique<Master2Backend::RstResponseMessage>();
    response->status = 0; // Success
    response->slaveNum = rstMsg->slaveNum;

    // Copy slaves reset info
    for (const auto &slave : rstMsg->slaves) {
        Master2Backend::RstResponseMessage::SlaveRstInfo slaveRstInfo;
        slaveRstInfo.id = slave.id;
        slaveRstInfo.lock = slave.lock;
        slaveRstInfo.clipStatus = slave.clipStatus;
        response->slaves.push_back(slaveRstInfo);
    }

    return std::move(response);
}

void ResetHandler::executeActions(const Message &message,
                                  MasterServer *server) {
    const auto *rstMsg =
        dynamic_cast<const Backend2Master::RstMessage *>(&message);
    if (!rstMsg)
        return;

    // Send reset commands to specified slaves with retry mechanism
    int successCount = 0;
    for (const auto &slave : rstMsg->slaves) {
        if (server->getDeviceManager().isSlaveConnected(slave.id)) {
            auto resetCmd = std::make_unique<Master2Slave::RstMessage>();
            resetCmd->lockStatus = slave.lock;
            resetCmd->clipLed = slave.clipStatus;

            server->sendCommandToSlaveWithRetry(slave.id, std::move(resetCmd),
                                                NetworkAddress{}, 3);
            successCount++;
            Log::i(
                "ResetHandler",
                "Sent reset command to slave 0x%08X (lock=%d, clipLed=0x%04X)",
                slave.id, static_cast<int>(slave.lock), slave.clipStatus);
        } else {
            Log::w("ResetHandler",
                   "Slave 0x%08X is not connected, skipping reset", slave.id);
        }
    }

    Log::i("ResetHandler", "Reset commands sent to %d/%d slaves", successCount,
           static_cast<int>(rstMsg->slaveNum));
}

// Control Message Handler
std::unique_ptr<Message> ControlHandler::processMessage(const Message &message,
                                                        MasterServer *server) {
    const auto *controlMsg =
        dynamic_cast<const Backend2Master::CtrlMessage *>(&message);
    if (!controlMsg)
        return nullptr;

    Log::i("ControlHandler", "Processing control message - Running status: %d",
           static_cast<int>(controlMsg->runningStatus));

    auto response = std::make_unique<Master2Backend::CtrlResponseMessage>();
    response->status = 0; // Success
    response->runningStatus = controlMsg->runningStatus;

    return std::move(response);
}

void ControlHandler::executeActions(const Message &message,
                                    MasterServer *server) {
    const auto *controlMsg =
        dynamic_cast<const Backend2Master::CtrlMessage *>(&message);
    if (!controlMsg)
        return;

    auto &deviceManager = server->getDeviceManager();

    // 保存当前运行状态
    deviceManager.setSystemRunningStatus(controlMsg->runningStatus);

    Log::i("ControlHandler", "Setting system running status to %d",
           static_cast<int>(controlMsg->runningStatus));

    // 根据运行状态执行操作
    switch (controlMsg->runningStatus) {
    case 0: // 停止
        Log::i("ControlHandler", "Stopping all operations");

        // 停止所有数据采集
        deviceManager.resetDataCollection();

        // 向所有从机发送停止信号
        for (uint32_t slaveId : deviceManager.getConnectedSlaves()) {
            if (deviceManager.hasSlaveConfig(slaveId)) {
                // 发送同步消息但设置模式为0（停止）
                auto syncCmd = std::make_unique<Master2Slave::SyncMessage>();
                syncCmd->mode = 0; // 停止模式
                syncCmd->timestamp = server->getCurrentTimestampMs();

                server->sendCommandToSlaveWithRetry(slaveId, std::move(syncCmd),
                                                    NetworkAddress{}, 1);
            }
        }
        break;

    case 1: // 启动
        Log::i("ControlHandler", "Starting operations in mode %d",
               static_cast<int>(deviceManager.getCurrentMode()));

        // 根据当前模式启动相应操作
        if (deviceManager.getCurrentMode() <=
            2) { // 0=Conduction, 1=Resistance, 2=Clip
            // 启动数据采集模式
            deviceManager.startDataCollection();

            // 数据采集的循环流程将由processDataCollection处理
            Log::i("ControlHandler",
                   "Started data collection in mode %d, cycle will be managed "
                   "by processDataCollection",
                   deviceManager.getCurrentMode());
        } else {
            Log::w("ControlHandler", "Unsupported mode: %d",
                   static_cast<int>(deviceManager.getCurrentMode()));
        }
        break;

    case 2: // 重置
        Log::i("ControlHandler", "Resetting all devices");

        // 重置所有从机状态
        for (uint32_t slaveId : deviceManager.getConnectedSlaves()) {
            if (deviceManager.hasSlaveConfig(slaveId)) {
                auto resetCmd = std::make_unique<Master2Slave::RstMessage>();
                resetCmd->lockStatus = 0; // 解锁
                resetCmd->clipLed = 0;    // 关闭LED

                server->sendCommandToSlaveWithRetry(
                    slaveId, std::move(resetCmd), NetworkAddress{}, 1);
            }
        }

        // 重置设备管理器状态
        deviceManager.resetDataCollection();
        break;

    default:
        Log::w("ControlHandler", "Unknown running status: %d",
               static_cast<int>(controlMsg->runningStatus));
        break;
    }
}

// Ping Control Message Handler
std::unique_ptr<Message>
PingControlHandler::processMessage(const Message &message,
                                   MasterServer *server) {
    const auto *pingMsg =
        dynamic_cast<const Backend2Master::PingCtrlMessage *>(&message);
    if (!pingMsg)
        return nullptr;

    Log::i("PingControlHandler",
           "Processing ping control message - Mode: %d, Count: %d, Interval: "
           "%d, Target: 0x%08X",
           static_cast<int>(pingMsg->pingMode), pingMsg->pingCount,
           pingMsg->interval, pingMsg->destinationId);

    auto response = std::make_unique<Master2Backend::PingResponseMessage>();
    response->pingMode = pingMsg->pingMode;
    response->totalCount = pingMsg->pingCount;
    response->successCount = pingMsg->pingCount - 1; // Simulate 1 failure
    response->destinationId = pingMsg->destinationId;

    return std::move(response);
}

void PingControlHandler::executeActions(const Message &message,
                                        MasterServer *server) {
    const auto *pingMsg =
        dynamic_cast<const Backend2Master::PingCtrlMessage *>(&message);
    if (!pingMsg)
        return;

    // Add ping session to the server
    server->addPingSession(pingMsg->destinationId, pingMsg->pingMode,
                           pingMsg->pingCount, pingMsg->interval,
                           NetworkAddress{});

    Log::i("PingControlHandler",
           "Added ping session for target 0x%08X (mode=%d, count=%d, "
           "interval=%d)",
           pingMsg->destinationId, static_cast<int>(pingMsg->pingMode),
           pingMsg->pingCount, pingMsg->interval);
}

// Device List Request Handler
std::unique_ptr<Message>
DeviceListHandler::processMessage(const Message &message,
                                  MasterServer *server) {
    const auto *deviceListMsg =
        dynamic_cast<const Backend2Master::DeviceListReqMessage *>(&message);
    if (!deviceListMsg)
        return nullptr;

    Log::i("DeviceListHandler", "Processing device list request");

    auto connectedSlaves = server->getDeviceManager().getConnectedSlaves();

    auto response =
        std::make_unique<Master2Backend::DeviceListResponseMessage>();
    response->deviceCount = static_cast<uint8_t>(connectedSlaves.size());

    // Add connected slaves to response
    for (uint32_t slaveId : connectedSlaves) {
        Master2Backend::DeviceListResponseMessage::DeviceInfo deviceInfo;
        deviceInfo.deviceId = slaveId;
        deviceInfo.shortId =
            server->getDeviceManager().getSlaveShortId(slaveId);
        deviceInfo.online = 1; // Connected
        deviceInfo.versionMajor = 1;
        deviceInfo.versionMinor = 2;
        deviceInfo.versionPatch = 3;
        response->devices.push_back(deviceInfo);
    }

    Log::i("DeviceListHandler", "Returning %d connected devices",
           response->deviceCount);

    return std::move(response);
}

void DeviceListHandler::executeActions(const Message &message,
                                       MasterServer *server) {
    // No additional actions needed for device list request
    Log::d("DeviceListHandler", "Device list request processed");
}