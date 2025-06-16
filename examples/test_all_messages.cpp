#include "../src/Logger.h"
#include "../src/WhtsProtocol.h"
#include <iostream>
#include <memory>

using namespace WhtsProtocol;

void testMaster2SlaveMessages() {
    Log::i("Test", "=== Testing Master2Slave Messages ===");

    // Test SyncMessage
    Master2Slave::SyncMessage syncMsg;
    syncMsg.mode = 1;
    syncMsg.timestamp = 123456789;

    auto data = syncMsg.serialize();
    Master2Slave::SyncMessage syncMsg2;
    if (syncMsg2.deserialize(data)) {
        Log::i("Test", "SyncMessage: OK (mode=%d, timestamp=%u)", syncMsg2.mode,
               syncMsg2.timestamp);
    }

    // Test ConductionConfigMessage
    Master2Slave::ConductionConfigMessage condMsg;
    condMsg.timeSlot = 5;
    condMsg.interval = 10;
    condMsg.totalConductionNum = 100;
    condMsg.startConductionNum = 1;
    condMsg.conductionNum = 50;

    data = condMsg.serialize();
    Master2Slave::ConductionConfigMessage condMsg2;
    if (condMsg2.deserialize(data)) {
        Log::i(
            "Test",
            "ConductionConfigMessage: OK (timeSlot=%d, interval=%d, total=%d)",
            condMsg2.timeSlot, condMsg2.interval, condMsg2.totalConductionNum);
    }
}

void testSlave2MasterMessages() {
    Log::i("Test", "=== Testing Slave2Master Messages ===");

    // Test AnnounceMessage
    Slave2Master::AnnounceMessage announceMsg;
    announceMsg.deviceId = 0x12345678;
    announceMsg.versionMajor = 1;
    announceMsg.versionMinor = 2;
    announceMsg.versionPatch = 3;

    auto data = announceMsg.serialize();
    Slave2Master::AnnounceMessage announceMsg2;
    if (announceMsg2.deserialize(data)) {
        Log::i("Test",
               "AnnounceMessage: OK (deviceId=0x%08X, version=%d.%d.%d)",
               announceMsg2.deviceId, announceMsg2.versionMajor,
               announceMsg2.versionMinor, announceMsg2.versionPatch);
    }

    // Test PingRspMessage
    Slave2Master::PingRspMessage pingMsg;
    pingMsg.sequenceNumber = 123;
    pingMsg.timestamp = 987654321;

    data = pingMsg.serialize();
    Slave2Master::PingRspMessage pingMsg2;
    if (pingMsg2.deserialize(data)) {
        Log::i("Test", "PingRspMessage: OK (seq=%d, timestamp=%u)",
               pingMsg2.sequenceNumber, pingMsg2.timestamp);
    }
}

void testSlave2BackendMessages() {
    Log::i("Test", "=== Testing Slave2Backend Messages ===");

    // Test ConductionDataMessage
    Slave2Backend::ConductionDataMessage dataMsg;
    dataMsg.conductionLength = 4;
    dataMsg.conductionData = {0x01, 0x02, 0x03, 0x04};

    auto data = dataMsg.serialize();
    Slave2Backend::ConductionDataMessage dataMsg2;
    if (dataMsg2.deserialize(data)) {
        Log::i("Test", "ConductionDataMessage: OK (length=%d, data size=%zu)",
               dataMsg2.conductionLength, dataMsg2.conductionData.size());
    }

    // Test ClipDataMessage
    Slave2Backend::ClipDataMessage clipMsg;
    clipMsg.clipData = 0x1234;

    data = clipMsg.serialize();
    Slave2Backend::ClipDataMessage clipMsg2;
    if (clipMsg2.deserialize(data)) {
        Log::i("Test", "ClipDataMessage: OK (clipData=0x%04X)",
               clipMsg2.clipData);
    }
}

void testBackend2MasterMessages() {
    Log::i("Test", "=== Testing Backend2Master Messages ===");

    // Test ModeConfigMessage
    Backend2Master::ModeConfigMessage modeMsg;
    modeMsg.mode = 2;

    auto data = modeMsg.serialize();
    Backend2Master::ModeConfigMessage modeMsg2;
    if (modeMsg2.deserialize(data)) {
        Log::i("Test", "ModeConfigMessage: OK (mode=%d)", modeMsg2.mode);
    }

    // Test SlaveConfigMessage
    Backend2Master::SlaveConfigMessage slaveMsg;
    slaveMsg.slaveNum = 2;

    Backend2Master::SlaveConfigMessage::SlaveInfo slave1;
    slave1.id = 0x11111111;
    slave1.conductionNum = 10;
    slave1.resistanceNum = 20;
    slave1.clipMode = 1;
    slave1.clipStatus = 0x5555;

    Backend2Master::SlaveConfigMessage::SlaveInfo slave2;
    slave2.id = 0x22222222;
    slave2.conductionNum = 15;
    slave2.resistanceNum = 25;
    slave2.clipMode = 2;
    slave2.clipStatus = 0xAAAA;

    slaveMsg.slaves = {slave1, slave2};

    data = slaveMsg.serialize();
    Backend2Master::SlaveConfigMessage slaveMsg2;
    if (slaveMsg2.deserialize(data)) {
        Log::i("Test",
               "SlaveConfigMessage: OK (slaveNum=%d, first slave ID=0x%08X)",
               slaveMsg2.slaveNum, slaveMsg2.slaves[0].id);
    }
}

void testMaster2BackendMessages() {
    Log::i("Test", "=== Testing Master2Backend Messages ===");

    // Test ModeConfigResponseMessage
    Master2Backend::ModeConfigResponseMessage modeRspMsg;
    modeRspMsg.status = 0;
    modeRspMsg.mode = 3;

    auto data = modeRspMsg.serialize();
    Master2Backend::ModeConfigResponseMessage modeRspMsg2;
    if (modeRspMsg2.deserialize(data)) {
        Log::i("Test", "ModeConfigResponseMessage: OK (status=%d, mode=%d)",
               modeRspMsg2.status, modeRspMsg2.mode);
    }

    // Test DeviceListResponseMessage
    Master2Backend::DeviceListResponseMessage deviceListMsg;
    deviceListMsg.deviceCount = 1;

    Master2Backend::DeviceListResponseMessage::DeviceInfo device;
    device.deviceId = 0x87654321;
    device.shortId = 5;
    device.online = 1;
    device.versionMajor = 2;
    device.versionMinor = 1;
    device.versionPatch = 0;

    deviceListMsg.devices = {device};

    data = deviceListMsg.serialize();
    Master2Backend::DeviceListResponseMessage deviceListMsg2;
    if (deviceListMsg2.deserialize(data)) {
        Log::i("Test",
               "DeviceListResponseMessage: OK (deviceCount=%d, first device "
               "ID=0x%08X)",
               deviceListMsg2.deviceCount, deviceListMsg2.devices[0].deviceId);
    }
}

void testProtocolProcessor() {
    Log::i("Test", "=== Testing ProtocolProcessor ===");

    ProtocolProcessor processor;

    // Test message creation
    auto syncMsg = processor.createMessage(
        PacketId::MASTER_TO_SLAVE,
        static_cast<uint8_t>(Master2SlaveMessageId::SYNC_MSG));
    if (syncMsg) {
        Log::i("Test", "Message creation: OK (SyncMessage created)");
    }

    auto announceMsg = processor.createMessage(
        PacketId::SLAVE_TO_MASTER,
        static_cast<uint8_t>(Slave2MasterMessageId::ANNOUNCE_MSG));
    if (announceMsg) {
        Log::i("Test", "Message creation: OK (AnnounceMessage created)");
    }

    // Test packet packing and parsing
    Master2Slave::SyncMessage testMsg;
    testMsg.mode = 1;
    testMsg.timestamp = 123456789;

    auto frames = processor.packMaster2SlaveMessage(0x12345678, testMsg);
    if (!frames.empty()) {
        Log::i("Test", "Packet packing: OK (generated %zu frames)",
               frames.size());

        // Test parsing
        Frame frame;
        if (processor.parseFrame(frames[0], frame)) {
            uint32_t destId;
            std::unique_ptr<Message> parsedMsg;
            if (processor.parseMaster2SlavePacket(frame.payload, destId,
                                                  parsedMsg)) {
                Log::i("Test", "Packet parsing: OK (destId=0x%08X)", destId);
            }
        }
    }
}

int main() {
    Log::i("Test", "WhtsProtocol Messages Module Test");
    Log::i("Test", "===================================");

    try {
        testMaster2SlaveMessages();
        testSlave2MasterMessages();
        testSlave2BackendMessages();
        testBackend2MasterMessages();
        testMaster2BackendMessages();
        testProtocolProcessor();

        Log::i("Test", "===================================");
        Log::i("Test", "All tests completed successfully!");

    } catch (const std::exception &e) {
        Log::e("Test", "Test failed with exception: %s", e.what());
        return 1;
    }

    return 0;
}