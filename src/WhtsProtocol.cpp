#include "WhtsProtocol.h"
#include <cstring>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <string>

namespace WhtsProtocol {

// DeviceStatus 实现
uint16_t DeviceStatus::toUint16() const {
    uint16_t result = 0;
    if (colorSensor) result |= (1 << 0);
    if (sleeveLimit) result |= (1 << 1);
    if (electromagnetUnlockButton) result |= (1 << 2);
    if (batteryLowAlarm) result |= (1 << 3);
    if (pressureSensor) result |= (1 << 4);
    if (electromagneticLock1) result |= (1 << 5);
    if (electromagneticLock2) result |= (1 << 6);
    if (accessory1) result |= (1 << 7);
    if (accessory2) result |= (1 << 8);
    return result;
}

void DeviceStatus::fromUint16(uint16_t status) {
    colorSensor = (status & (1 << 0)) != 0;
    sleeveLimit = (status & (1 << 1)) != 0;
    electromagnetUnlockButton = (status & (1 << 2)) != 0;
    batteryLowAlarm = (status & (1 << 3)) != 0;
    pressureSensor = (status & (1 << 4)) != 0;
    electromagneticLock1 = (status & (1 << 5)) != 0;
    electromagneticLock2 = (status & (1 << 6)) != 0;
    accessory1 = (status & (1 << 7)) != 0;
    accessory2 = (status & (1 << 8)) != 0;
    reserved = 0;
}

// Frame 实现
Frame::Frame() : delimiter1(FRAME_DELIMITER_1), delimiter2(FRAME_DELIMITER_2), 
                 packetId(0), fragmentsSequence(0), moreFragmentsFlag(0), packetLength(0) {}

bool Frame::isValid() const {
    return delimiter1 == FRAME_DELIMITER_1 && delimiter2 == FRAME_DELIMITER_2;
}

std::vector<uint8_t> Frame::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(7 + payload.size());
    
    result.push_back(delimiter1);
    result.push_back(delimiter2);
    result.push_back(packetId);
    result.push_back(fragmentsSequence);
    result.push_back(moreFragmentsFlag);
    
    // 小端序写入长度
    result.push_back(packetLength & 0xFF);
    result.push_back((packetLength >> 8) & 0xFF);
    
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

bool Frame::deserialize(const std::vector<uint8_t>& data, Frame& frame) {
    if (data.size() < 7) return false;
    
    frame.delimiter1 = data[0];
    frame.delimiter2 = data[1];
    frame.packetId = data[2];
    frame.fragmentsSequence = data[3];
    frame.moreFragmentsFlag = data[4];
    
    // 小端序读取长度
    frame.packetLength = data[5] | (data[6] << 8);
    
    if (data.size() < 7 + frame.packetLength) return false;
    
    frame.payload.assign(data.begin() + 7, data.begin() + 7 + frame.packetLength);
    return frame.isValid();
}

// ProtocolProcessor 实现
ProtocolProcessor::ProtocolProcessor() : mtu_(DEFAULT_MTU) {}
ProtocolProcessor::~ProtocolProcessor() {}

void ProtocolProcessor::writeUint16LE(std::vector<uint8_t>& buffer, uint16_t value) {
    buffer.push_back(value & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
}

void ProtocolProcessor::writeUint32LE(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(value & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 24) & 0xFF);
}

uint16_t ProtocolProcessor::readUint16LE(const std::vector<uint8_t>& buffer, size_t offset) {
    if (offset + 1 >= buffer.size()) return 0;
    return buffer[offset] | (buffer[offset + 1] << 8);
}

uint32_t ProtocolProcessor::readUint32LE(const std::vector<uint8_t>& buffer, size_t offset) {
    if (offset + 3 >= buffer.size()) return 0;
    return buffer[offset] | (buffer[offset + 1] << 8) | 
           (buffer[offset + 2] << 16) | (buffer[offset + 3] << 24);
}

std::vector<uint8_t> ProtocolProcessor::packMaster2SlaveMessageSingle(uint32_t destinationId, 
                                                                     const Message& message,
                                                                     uint8_t fragmentsSequence, 
                                                                     uint8_t moreFragmentsFlag) {
    Frame frame;
    frame.packetId = static_cast<uint8_t>(PacketId::MASTER_TO_SLAVE);
    frame.fragmentsSequence = fragmentsSequence;
    frame.moreFragmentsFlag = moreFragmentsFlag;
    
    // 构建载荷
    std::vector<uint8_t> payload;
    payload.push_back(message.getMessageId());
    writeUint32LE(payload, destinationId);
    
    auto messageData = message.serialize();
    payload.insert(payload.end(), messageData.begin(), messageData.end());
    
    frame.payload = payload;
    frame.packetLength = static_cast<uint16_t>(payload.size());
    
    return frame.serialize();
}

std::vector<uint8_t> ProtocolProcessor::packSlave2MasterMessageSingle(uint32_t slaveId, 
                                                                     const Message& message,
                                                                     uint8_t fragmentsSequence, 
                                                                     uint8_t moreFragmentsFlag) {
    Frame frame;
    frame.packetId = static_cast<uint8_t>(PacketId::SLAVE_TO_MASTER);
    frame.fragmentsSequence = fragmentsSequence;
    frame.moreFragmentsFlag = moreFragmentsFlag;
    
    // 构建载荷
    std::vector<uint8_t> payload;
    payload.push_back(message.getMessageId());
    writeUint32LE(payload, slaveId);
    
    auto messageData = message.serialize();
    payload.insert(payload.end(), messageData.begin(), messageData.end());
    
    frame.payload = payload;
    frame.packetLength = static_cast<uint16_t>(payload.size());
    
    return frame.serialize();
}

std::vector<uint8_t> ProtocolProcessor::packSlave2BackendMessageSingle(uint32_t slaveId, 
                                                                      const DeviceStatus& deviceStatus,
                                                                      const Message& message,
                                                                      uint8_t fragmentsSequence, 
                                                                      uint8_t moreFragmentsFlag) {
    Frame frame;
    frame.packetId = static_cast<uint8_t>(PacketId::SLAVE_TO_BACKEND);
    frame.fragmentsSequence = fragmentsSequence;
    frame.moreFragmentsFlag = moreFragmentsFlag;
    
    // 构建载荷
    std::vector<uint8_t> payload;
    payload.push_back(message.getMessageId());
    writeUint32LE(payload, slaveId);
    writeUint16LE(payload, deviceStatus.toUint16());
    
    auto messageData = message.serialize();
    payload.insert(payload.end(), messageData.begin(), messageData.end());
    
    frame.payload = payload;
    frame.packetLength = static_cast<uint16_t>(payload.size());
    
    return frame.serialize();
}

bool ProtocolProcessor::parseFrame(const std::vector<uint8_t>& data, Frame& frame) {
    return Frame::deserialize(data, frame);
}

std::unique_ptr<Message> ProtocolProcessor::createMessage(PacketId packetId, uint8_t messageId) {
    switch (packetId) {
        case PacketId::MASTER_TO_SLAVE:
            switch (static_cast<Master2SlaveMessageId>(messageId)) {
                case Master2SlaveMessageId::SYNC_MSG:
                    return std::make_unique<Master2Slave::SyncMessage>();
                case Master2SlaveMessageId::CONDUCTION_CFG_MSG:
                    return std::make_unique<Master2Slave::ConductionConfigMessage>();
                case Master2SlaveMessageId::RESISTANCE_CFG_MSG:
                    return std::make_unique<Master2Slave::ResistanceConfigMessage>();
                case Master2SlaveMessageId::CLIP_CFG_MSG:
                    return std::make_unique<Master2Slave::ClipConfigMessage>();
                case Master2SlaveMessageId::READ_COND_DATA_MSG:
                    return std::make_unique<Master2Slave::ReadConductionDataMessage>();
                case Master2SlaveMessageId::READ_RES_DATA_MSG:
                    return std::make_unique<Master2Slave::ReadResistanceDataMessage>();
                case Master2SlaveMessageId::READ_CLIP_DATA_MSG:
                    return std::make_unique<Master2Slave::ReadClipDataMessage>();
                case Master2SlaveMessageId::RST_MSG:
                    return std::make_unique<Master2Slave::RstMessage>();
                case Master2SlaveMessageId::PING_REQ_MSG:
                    return std::make_unique<Master2Slave::PingReqMessage>();
                case Master2SlaveMessageId::SHORT_ID_ASSIGN_MSG:
                    return std::make_unique<Master2Slave::ShortIdAssignMessage>();
            }
            break;
            
        case PacketId::SLAVE_TO_MASTER:
            switch (static_cast<Slave2MasterMessageId>(messageId)) {
                case Slave2MasterMessageId::CONDUCTION_CFG_MSG:
                    return std::make_unique<Slave2Master::ConductionConfigResponseMessage>();
                case Slave2MasterMessageId::RESISTANCE_CFG_MSG:
                    return std::make_unique<Slave2Master::ResistanceConfigResponseMessage>();
                case Slave2MasterMessageId::CLIP_CFG_MSG:
                    return std::make_unique<Slave2Master::ClipConfigResponseMessage>();
                case Slave2MasterMessageId::RST_MSG:
                    return std::make_unique<Slave2Master::RstResponseMessage>();
                case Slave2MasterMessageId::PING_RSP_MSG:
                    return std::make_unique<Slave2Master::PingRspMessage>();
                case Slave2MasterMessageId::ANNOUNCE_MSG:
                    return std::make_unique<Slave2Master::AnnounceMessage>();
                case Slave2MasterMessageId::SHORT_ID_CONFIRM_MSG:
                    return std::make_unique<Slave2Master::ShortIdConfirmMessage>();
            }
            break;
            
        case PacketId::SLAVE_TO_BACKEND:
            switch (static_cast<Slave2BackendMessageId>(messageId)) {
                case Slave2BackendMessageId::CONDUCTION_DATA_MSG:
                    return std::make_unique<Slave2Backend::ConductionDataMessage>();
                case Slave2BackendMessageId::RESISTANCE_DATA_MSG:
                    return std::make_unique<Slave2Backend::ResistanceDataMessage>();
                case Slave2BackendMessageId::CLIP_DATA_MSG:
                    return std::make_unique<Slave2Backend::ClipDataMessage>();
            }
            break;
            
        default:
            break;
    }
    return nullptr;
}

bool ProtocolProcessor::parseMaster2SlavePacket(const std::vector<uint8_t>& payload, 
                                                 uint32_t& destinationId, 
                                                 std::unique_ptr<Message>& message) {
    if (payload.size() < 5) return false;
    
    uint8_t messageId = payload[0];
    destinationId = readUint32LE(payload, 1);
    
    message = createMessage(PacketId::MASTER_TO_SLAVE, messageId);
    if (!message) return false;
    
    std::vector<uint8_t> messageData(payload.begin() + 5, payload.end());
    return message->deserialize(messageData);
}

bool ProtocolProcessor::parseSlave2MasterPacket(const std::vector<uint8_t>& payload, 
                                                 uint32_t& slaveId, 
                                                 std::unique_ptr<Message>& message) {
    if (payload.size() < 5) return false;
    
    uint8_t messageId = payload[0];
    slaveId = readUint32LE(payload, 1);
    
    message = createMessage(PacketId::SLAVE_TO_MASTER, messageId);
    if (!message) return false;
    
    std::vector<uint8_t> messageData(payload.begin() + 5, payload.end());
    return message->deserialize(messageData);
}

bool ProtocolProcessor::parseSlave2BackendPacket(const std::vector<uint8_t>& payload, 
                                                  uint32_t& slaveId, 
                                                  DeviceStatus& deviceStatus,
                                                  std::unique_ptr<Message>& message) {
    if (payload.size() < 7) return false;
    
    uint8_t messageId = payload[0];
    slaveId = readUint32LE(payload, 1);
    deviceStatus.fromUint16(readUint16LE(payload, 5));
    
    message = createMessage(PacketId::SLAVE_TO_BACKEND, messageId);
    if (!message) return false;
    
    std::vector<uint8_t> messageData(payload.begin() + 7, payload.end());
    return message->deserialize(messageData);
}

// Master2Slave 消息实现
namespace Master2Slave {

std::vector<uint8_t> SyncMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(mode);
    result.push_back(timestamp & 0xFF);
    result.push_back((timestamp >> 8) & 0xFF);
    result.push_back((timestamp >> 16) & 0xFF);
    result.push_back((timestamp >> 24) & 0xFF);
    return result;
}

bool SyncMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 5) return false;
    mode = data[0];
    timestamp = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
    return true;
}

std::vector<uint8_t> ConductionConfigMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(timeSlot);
    result.push_back(interval);
    result.push_back(totalConductionNum & 0xFF);
    result.push_back((totalConductionNum >> 8) & 0xFF);
    result.push_back(startConductionNum & 0xFF);
    result.push_back((startConductionNum >> 8) & 0xFF);
    result.push_back(conductionNum & 0xFF);
    result.push_back((conductionNum >> 8) & 0xFF);
    return result;
}

bool ConductionConfigMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 8) return false;
    timeSlot = data[0];
    interval = data[1];
    totalConductionNum = data[2] | (data[3] << 8);
    startConductionNum = data[4] | (data[5] << 8);
    conductionNum = data[6] | (data[7] << 8);
    return true;
}

std::vector<uint8_t> ResistanceConfigMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(timeSlot);
    result.push_back(interval);
    result.push_back(totalNum & 0xFF);
    result.push_back((totalNum >> 8) & 0xFF);
    result.push_back(startNum & 0xFF);
    result.push_back((startNum >> 8) & 0xFF);
    result.push_back(num & 0xFF);
    result.push_back((num >> 8) & 0xFF);
    return result;
}

bool ResistanceConfigMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 8) return false;
    timeSlot = data[0];
    interval = data[1];
    totalNum = data[2] | (data[3] << 8);
    startNum = data[4] | (data[5] << 8);
    num = data[6] | (data[7] << 8);
    return true;
}

std::vector<uint8_t> ClipConfigMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(interval);
    result.push_back(mode);
    result.push_back(clipPin & 0xFF);
    result.push_back((clipPin >> 8) & 0xFF);
    return result;
}

bool ClipConfigMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return false;
    interval = data[0];
    mode = data[1];
    clipPin = data[2] | (data[3] << 8);
    return true;
}

std::vector<uint8_t> ReadConductionDataMessage::serialize() const {
    return {reserve};
}

bool ReadConductionDataMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 1) return false;
    reserve = data[0];
    return true;
}

std::vector<uint8_t> ReadResistanceDataMessage::serialize() const {
    return {reserve};
}

bool ReadResistanceDataMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 1) return false;
    reserve = data[0];
    return true;
}

std::vector<uint8_t> ReadClipDataMessage::serialize() const {
    return {reserve};
}

bool ReadClipDataMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 1) return false;
    reserve = data[0];
    return true;
}

std::vector<uint8_t> RstMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(lockStatus);
    result.push_back(clipLed & 0xFF);
    result.push_back((clipLed >> 8) & 0xFF);
    return result;
}

bool RstMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 3) return false;
    lockStatus = data[0];
    clipLed = data[1] | (data[2] << 8);
    return true;
}

std::vector<uint8_t> PingReqMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(sequenceNumber & 0xFF);
    result.push_back((sequenceNumber >> 8) & 0xFF);
    result.push_back(timestamp & 0xFF);
    result.push_back((timestamp >> 8) & 0xFF);
    result.push_back((timestamp >> 16) & 0xFF);
    result.push_back((timestamp >> 24) & 0xFF);
    return result;
}

bool PingReqMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 6) return false;
    sequenceNumber = data[0] | (data[1] << 8);
    timestamp = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
    return true;
}

std::vector<uint8_t> ShortIdAssignMessage::serialize() const {
    return {shortId};
}

bool ShortIdAssignMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 1) return false;
    shortId = data[0];
    return true;
}

} // namespace Master2Slave

// Slave2Master 消息实现
namespace Slave2Master {

std::vector<uint8_t> ConductionConfigResponseMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(status);
    result.push_back(timeSlot);
    result.push_back(interval);
    result.push_back(totalConductionNum & 0xFF);
    result.push_back((totalConductionNum >> 8) & 0xFF);
    result.push_back(startConductionNum & 0xFF);
    result.push_back((startConductionNum >> 8) & 0xFF);
    result.push_back(conductionNum & 0xFF);
    result.push_back((conductionNum >> 8) & 0xFF);
    return result;
}

bool ConductionConfigResponseMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 9) return false;
    status = data[0];
    timeSlot = data[1];
    interval = data[2];
    totalConductionNum = data[3] | (data[4] << 8);
    startConductionNum = data[5] | (data[6] << 8);
    conductionNum = data[7] | (data[8] << 8);
    return true;
}

std::vector<uint8_t> ResistanceConfigResponseMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(status);
    result.push_back(timeSlot);
    result.push_back(interval);
    result.push_back(totalConductionNum & 0xFF);
    result.push_back((totalConductionNum >> 8) & 0xFF);
    result.push_back(startConductionNum & 0xFF);
    result.push_back((startConductionNum >> 8) & 0xFF);
    result.push_back(conductionNum & 0xFF);
    result.push_back((conductionNum >> 8) & 0xFF);
    return result;
}

bool ResistanceConfigResponseMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 9) return false;
    status = data[0];
    timeSlot = data[1];
    interval = data[2];
    totalConductionNum = data[3] | (data[4] << 8);
    startConductionNum = data[5] | (data[6] << 8);
    conductionNum = data[7] | (data[8] << 8);
    return true;
}

std::vector<uint8_t> ClipConfigResponseMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(status);
    result.push_back(interval);
    result.push_back(mode);
    result.push_back(clipPin & 0xFF);
    result.push_back((clipPin >> 8) & 0xFF);
    return result;
}

bool ClipConfigResponseMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 5) return false;
    status = data[0];
    interval = data[1];
    mode = data[2];
    clipPin = data[3] | (data[4] << 8);
    return true;
}

std::vector<uint8_t> RstResponseMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(status);
    result.push_back(lockStatus);
    result.push_back(clipLed & 0xFF);
    result.push_back((clipLed >> 8) & 0xFF);
    return result;
}

bool RstResponseMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return false;
    status = data[0];
    lockStatus = data[1];
    clipLed = data[2] | (data[3] << 8);
    return true;
}

std::vector<uint8_t> PingRspMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(sequenceNumber & 0xFF);
    result.push_back((sequenceNumber >> 8) & 0xFF);
    result.push_back(timestamp & 0xFF);
    result.push_back((timestamp >> 8) & 0xFF);
    result.push_back((timestamp >> 16) & 0xFF);
    result.push_back((timestamp >> 24) & 0xFF);
    return result;
}

bool PingRspMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 6) return false;
    sequenceNumber = data[0] | (data[1] << 8);
    timestamp = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
    return true;
}

std::vector<uint8_t> AnnounceMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(deviceId & 0xFF);
    result.push_back((deviceId >> 8) & 0xFF);
    result.push_back((deviceId >> 16) & 0xFF);
    result.push_back((deviceId >> 24) & 0xFF);
    result.push_back(versionMajor);
    result.push_back(versionMinor);
    result.push_back(versionPatch & 0xFF);
    result.push_back((versionPatch >> 8) & 0xFF);
    return result;
}

bool AnnounceMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 8) return false;
    deviceId = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    versionMajor = data[4];
    versionMinor = data[5];
    versionPatch = data[6] | (data[7] << 8);
    return true;
}

std::vector<uint8_t> ShortIdConfirmMessage::serialize() const {
    return {status, shortId};
}

bool ShortIdConfirmMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    status = data[0];
    shortId = data[1];
    return true;
}

} // namespace Slave2Master

// Slave2Backend 消息实现
namespace Slave2Backend {

std::vector<uint8_t> ConductionDataMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(conductionLength & 0xFF);
    result.push_back((conductionLength >> 8) & 0xFF);
    result.insert(result.end(), conductionData.begin(), conductionData.end());
    return result;
}

bool ConductionDataMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    conductionLength = data[0] | (data[1] << 8);
    if (data.size() < 2 + conductionLength) return false;
    conductionData.assign(data.begin() + 2, data.begin() + 2 + conductionLength);
    return true;
}

std::vector<uint8_t> ResistanceDataMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(resistanceLength & 0xFF);
    result.push_back((resistanceLength >> 8) & 0xFF);
    result.insert(result.end(), resistanceData.begin(), resistanceData.end());
    return result;
}

bool ResistanceDataMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    resistanceLength = data[0] | (data[1] << 8);
    if (data.size() < 2 + resistanceLength) return false;
    resistanceData.assign(data.begin() + 2, data.begin() + 2 + resistanceLength);
    return true;
}

std::vector<uint8_t> ClipDataMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(clipData & 0xFF);
    result.push_back((clipData >> 8) & 0xFF);
    return result;
}

bool ClipDataMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;
    clipData = data[0] | (data[1] << 8);
    return true;
}

} // namespace Slave2Backend

// ProtocolProcessor 分片和粘包处理功能实现

// 支持自动分片的打包函数
std::vector<std::vector<uint8_t>> ProtocolProcessor::packMaster2SlaveMessage(uint32_t destinationId, const Message& message) {
    // 首先生成单个完整帧
    auto completeFrame = packMaster2SlaveMessageSingle(destinationId, message, 0, 0);
    
    // 检查是否需要分片
    if (completeFrame.size() <= mtu_) {
        // 不需要分片，直接返回
        return {completeFrame};
    }
    
    // 需要分片
    return fragmentFrame(completeFrame);
}

std::vector<std::vector<uint8_t>> ProtocolProcessor::packSlave2MasterMessage(uint32_t slaveId, const Message& message) {
    // 首先生成单个完整帧
    auto completeFrame = packSlave2MasterMessageSingle(slaveId, message, 0, 0);
    
    // 检查是否需要分片
    if (completeFrame.size() <= mtu_) {
        // 不需要分片，直接返回
        return {completeFrame};
    }
    
    // 需要分片
    return fragmentFrame(completeFrame);
}

std::vector<std::vector<uint8_t>> ProtocolProcessor::packSlave2BackendMessage(uint32_t slaveId, const DeviceStatus& deviceStatus, const Message& message) {
    // 首先生成单个完整帧
    auto completeFrame = packSlave2BackendMessageSingle(slaveId, deviceStatus, message, 0, 0);
    
    // 检查是否需要分片
    if (completeFrame.size() <= mtu_) {
        // 不需要分片，直接返回
        return {completeFrame};
    }
    
    // 需要分片
    return fragmentFrame(completeFrame);
}

// 分片功能实现
std::vector<std::vector<uint8_t>> ProtocolProcessor::fragmentFrame(const std::vector<uint8_t>& frameData) {
    std::vector<std::vector<uint8_t>> fragments;
    
    if (frameData.size() < 7) {  // 帧头最小7字节
        return {frameData};
    }
    
    // 解析原始帧头
    uint8_t packetId = frameData[2];
    
    // 计算每个分片的有效载荷大小 (MTU - 帧头7字节)
    size_t fragmentPayloadSize = mtu_ - 7;
    
    // 获取原始载荷（从第7字节开始）
    std::vector<uint8_t> originalPayload(frameData.begin() + 7, frameData.end());
    
    // 计算需要多少个分片
    uint8_t totalFragments = static_cast<uint8_t>((originalPayload.size() + fragmentPayloadSize - 1) / fragmentPayloadSize);
    
    // 生成每个分片
    for (uint8_t i = 0; i < totalFragments; ++i) {
        std::vector<uint8_t> fragment;
        
        // 添加帧头
        fragment.push_back(FRAME_DELIMITER_1);
        fragment.push_back(FRAME_DELIMITER_2);
        fragment.push_back(packetId);
        fragment.push_back(i);  // 分片序号
        fragment.push_back((i == totalFragments - 1) ? 0 : 1);  // moreFragmentsFlag
        
        // 计算这个分片的载荷
        size_t startPos = i * fragmentPayloadSize;
        size_t endPos = std::min(startPos + fragmentPayloadSize, originalPayload.size());
        size_t fragmentSize = endPos - startPos;
        
        // 添加长度字段
        fragment.push_back(fragmentSize & 0xFF);
        fragment.push_back((fragmentSize >> 8) & 0xFF);
        
        // 添加分片载荷
        fragment.insert(fragment.end(), 
                       originalPayload.begin() + startPos, 
                       originalPayload.begin() + endPos);
        
        fragments.push_back(fragment);
    }
    
    return fragments;
}

// 处理接收到的原始数据（支持粘包处理）
void ProtocolProcessor::processReceivedData(const std::vector<uint8_t>& data) {
    // 防止接收缓冲区过大
    if (receiveBuffer_.size() + data.size() > MAX_RECEIVE_BUFFER_SIZE) {
        // 清空缓冲区，防止内存溢出
        receiveBuffer_.clear();
    }
    
    // 将新数据添加到接收缓冲区
    receiveBuffer_.insert(receiveBuffer_.end(), data.begin(), data.end());
    
    // 尝试从缓冲区中提取完整帧
    extractCompleteFrames();
    
    // 清理超时的分片
    cleanupExpiredFragments();
}

// 从接收缓冲区中提取完整帧
bool ProtocolProcessor::extractCompleteFrames() {
    bool foundFrames = false;
    size_t pos = 0;
    
    while (pos < receiveBuffer_.size()) {
        // 查找帧头
        size_t frameStart = findFrameHeader(receiveBuffer_, pos);
        if (frameStart == SIZE_MAX) {
            break;  // 没有找到帧头
        }
        
        // 检查是否有足够的数据读取帧长度
        if (frameStart + 7 > receiveBuffer_.size()) {
            break;  // 数据不够，等待更多数据
        }
        
        // 读取帧长度
        uint16_t frameLength = readUint16LE(receiveBuffer_, frameStart + 5);
        size_t totalFrameSize = 7 + frameLength;
        
        // 检查是否有完整的帧
        if (frameStart + totalFrameSize > receiveBuffer_.size()) {
            break;  // 帧不完整，等待更多数据
        }
        
        // 提取完整帧数据
        std::vector<uint8_t> frameData(receiveBuffer_.begin() + frameStart,
                                      receiveBuffer_.begin() + frameStart + totalFrameSize);
        
        // 解析帧
        Frame frame;
        if (Frame::deserialize(frameData, frame)) {
            // 检查是否是分片
            if (frame.moreFragmentsFlag || frame.fragmentsSequence > 0) {
                // 处理分片重组
                std::vector<uint8_t> completeFrame;
                if (reassembleFragments(frame, completeFrame)) {
                    // 分片重组完成，解析完整帧
                    Frame completedFrame;
                    if (Frame::deserialize(completeFrame, completedFrame)) {
                        completeFrames_.push(completedFrame);
                        foundFrames = true;
                    }
                }
            } else {
                // 单个完整帧
                completeFrames_.push(frame);
                foundFrames = true;
            }
        }
        
        // 移动到下一个位置
        pos = frameStart + totalFrameSize;
    }
    
    // 清理已处理的数据
    if (pos > 0) {
        receiveBuffer_.erase(receiveBuffer_.begin(), receiveBuffer_.begin() + pos);
    }
    
    return foundFrames;
}

// 查找帧头
size_t ProtocolProcessor::findFrameHeader(const std::vector<uint8_t>& buffer, size_t startPos) {
    for (size_t i = startPos; i < buffer.size() - 1; ++i) {
        if (buffer[i] == FRAME_DELIMITER_1 && buffer[i + 1] == FRAME_DELIMITER_2) {
            return i;
        }
    }
    return SIZE_MAX;
}

// 分片重组
bool ProtocolProcessor::reassembleFragments(const Frame& frame, std::vector<uint8_t>& completeFrame) {
    // 从帧载荷中提取源ID (假设载荷格式为: MessageId + SourceId + ...)
    if (frame.payload.size() < 5) {
        return false;  // 载荷太小
    }
    
    uint32_t sourceId = readUint32LE(frame.payload, 1);  // 跳过MessageId
    uint64_t fragmentId = generateFragmentId(frame.packetId, sourceId);
    
    // 查找或创建分片信息
    auto& fragmentInfo = fragmentMap_[fragmentId];
    fragmentInfo.packetId = frame.packetId;
    fragmentInfo.sourceId = sourceId;
    fragmentInfo.timestamp = 0;  // 这里应该使用实际的时间戳
    
    // 存储分片数据
    fragmentInfo.fragments[frame.fragmentsSequence] = frame.payload;
    
    // 如果这是最后一个分片，计算总分片数
    if (frame.moreFragmentsFlag == 0) {
        fragmentInfo.totalFragments = frame.fragmentsSequence + 1;
    }
    
    // 检查是否收集完所有分片
    if (fragmentInfo.totalFragments > 0 && fragmentInfo.isComplete()) {
        // 重组完整载荷
        std::vector<uint8_t> completePayload;
        for (uint8_t i = 0; i < fragmentInfo.totalFragments; ++i) {
            auto it = fragmentInfo.fragments.find(i);
            if (it != fragmentInfo.fragments.end()) {
                completePayload.insert(completePayload.end(), 
                                     it->second.begin(), it->second.end());
            }
        }
        
        // 构建完整帧
        completeFrame.clear();
        completeFrame.push_back(FRAME_DELIMITER_1);
        completeFrame.push_back(FRAME_DELIMITER_2);
        completeFrame.push_back(frame.packetId);
        completeFrame.push_back(0);  // fragmentsSequence = 0
        completeFrame.push_back(0);  // moreFragmentsFlag = 0
        
        // 添加载荷长度
        uint16_t payloadLength = static_cast<uint16_t>(completePayload.size());
        completeFrame.push_back(payloadLength & 0xFF);
        completeFrame.push_back((payloadLength >> 8) & 0xFF);
        
        // 添加载荷
        completeFrame.insert(completeFrame.end(), completePayload.begin(), completePayload.end());
        
        // 清理分片信息
        fragmentMap_.erase(fragmentId);
        
        return true;
    }
    
    return false;  // 还没有收集完所有分片
}

// 获取下一个完整帧
bool ProtocolProcessor::getNextCompleteFrame(Frame& frame) {
    if (completeFrames_.empty()) {
        return false;
    }
    
    frame = completeFrames_.front();
    completeFrames_.pop();
    return true;
}

// 清空接收缓冲区
void ProtocolProcessor::clearReceiveBuffer() {
    receiveBuffer_.clear();
    while (!completeFrames_.empty()) {
        completeFrames_.pop();
    }
    fragmentMap_.clear();
}

// 生成分片ID
uint64_t ProtocolProcessor::generateFragmentId(uint8_t packetId, uint32_t sourceId) {
    return (static_cast<uint64_t>(packetId) << 32) | sourceId;
}

// 清理超时分片
void ProtocolProcessor::cleanupExpiredFragments() {
    // 这里应该实现基于时间戳的超时清理
    // 为简化实现，暂时跳过
    // TODO: 实现基于实际时间戳的超时清理
}

} // namespace WhtsProtocol 