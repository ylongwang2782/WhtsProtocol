#include "WhtsProtocol.h"
#include "Logger.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace WhtsProtocol {

// Helper function to convert bytes to hex string
std::string bytesToHexString(const std::vector<uint8_t> &data,
                             size_t maxBytes) {
    std::stringstream ss;
    size_t count = std::min(data.size(), maxBytes);
    for (size_t i = 0; i < count; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(data[i]);
        if (i < count - 1)
            ss << " ";
    }
    if (data.size() > maxBytes) {
        ss << "...";
    }
    return ss.str();
}

uint16_t DeviceStatus::toUint16() const {
    uint16_t result = 0;
    if (colorSensor)
        result |= (1 << 0);
    if (sleeveLimit)
        result |= (1 << 1);
    if (electromagnetUnlockButton)
        result |= (1 << 2);
    if (batteryLowAlarm)
        result |= (1 << 3);
    if (pressureSensor)
        result |= (1 << 4);
    if (electromagneticLock1)
        result |= (1 << 5);
    if (electromagneticLock2)
        result |= (1 << 6);
    if (accessory1)
        result |= (1 << 7);
    if (accessory2)
        result |= (1 << 8);
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
Frame::Frame()
    : delimiter1(FRAME_DELIMITER_1), delimiter2(FRAME_DELIMITER_2), packetId(0),
      fragmentsSequence(0), moreFragmentsFlag(0), packetLength(0) {}

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

bool Frame::deserialize(const std::vector<uint8_t> &data, Frame &frame) {
    if (data.size() < 7)
        return false;

    frame.delimiter1 = data[0];
    frame.delimiter2 = data[1];
    frame.packetId = data[2];
    frame.fragmentsSequence = data[3];
    frame.moreFragmentsFlag = data[4];

    // 小端序读取长度
    frame.packetLength = data[5] | (data[6] << 8);

    if (data.size() < 7 + frame.packetLength)
        return false;

    frame.payload.assign(data.begin() + 7,
                         data.begin() + 7 + frame.packetLength);
    return frame.isValid();
}

// ProtocolProcessor 实现
ProtocolProcessor::ProtocolProcessor() : mtu_(DEFAULT_MTU) {}
ProtocolProcessor::~ProtocolProcessor() {}

void ProtocolProcessor::writeUint16LE(std::vector<uint8_t> &buffer,
                                      uint16_t value) {
    buffer.push_back(value & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
}

void ProtocolProcessor::writeUint32LE(std::vector<uint8_t> &buffer,
                                      uint32_t value) {
    buffer.push_back(value & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 24) & 0xFF);
}

uint16_t ProtocolProcessor::readUint16LE(const std::vector<uint8_t> &buffer,
                                         size_t offset) {
    if (offset + 1 >= buffer.size())
        return 0;
    return buffer[offset] | (buffer[offset + 1] << 8);
}

uint32_t ProtocolProcessor::readUint32LE(const std::vector<uint8_t> &buffer,
                                         size_t offset) {
    if (offset + 3 >= buffer.size())
        return 0;
    return buffer[offset] | (buffer[offset + 1] << 8) |
           (buffer[offset + 2] << 16) | (buffer[offset + 3] << 24);
}

std::vector<uint8_t> ProtocolProcessor::packMaster2SlaveMessageSingle(
    uint32_t destinationId, const Message &message, uint8_t fragmentsSequence,
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

std::vector<uint8_t> ProtocolProcessor::packSlave2MasterMessageSingle(
    uint32_t slaveId, const Message &message, uint8_t fragmentsSequence,
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

std::vector<uint8_t> ProtocolProcessor::packSlave2BackendMessageSingle(
    uint32_t slaveId, const DeviceStatus &deviceStatus, const Message &message,
    uint8_t fragmentsSequence, uint8_t moreFragmentsFlag) {
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

std::vector<uint8_t> ProtocolProcessor::packBackend2MasterMessageSingle(
    const Message &message, uint8_t fragmentsSequence,
    uint8_t moreFragmentsFlag) {
    Frame frame;
    frame.packetId = static_cast<uint8_t>(PacketId::BACKEND_TO_MASTER);
    frame.fragmentsSequence = fragmentsSequence;
    frame.moreFragmentsFlag = moreFragmentsFlag;

    // 构建载荷
    std::vector<uint8_t> payload;
    payload.push_back(message.getMessageId());

    auto messageData = message.serialize();
    payload.insert(payload.end(), messageData.begin(), messageData.end());

    frame.payload = payload;
    frame.packetLength = static_cast<uint16_t>(payload.size());

    return frame.serialize();
}

std::vector<uint8_t> ProtocolProcessor::packMaster2BackendMessageSingle(
    const Message &message, uint8_t fragmentsSequence,
    uint8_t moreFragmentsFlag) {
    Frame frame;
    frame.packetId = static_cast<uint8_t>(PacketId::MASTER_TO_BACKEND);
    frame.fragmentsSequence = fragmentsSequence;
    frame.moreFragmentsFlag = moreFragmentsFlag;

    // 构建载荷
    std::vector<uint8_t> payload;
    payload.push_back(message.getMessageId());

    auto messageData = message.serialize();
    payload.insert(payload.end(), messageData.begin(), messageData.end());

    frame.payload = payload;
    frame.packetLength = static_cast<uint16_t>(payload.size());

    return frame.serialize();
}

bool ProtocolProcessor::parseFrame(const std::vector<uint8_t> &data,
                                   Frame &frame) {
    return Frame::deserialize(data, frame);
}

std::unique_ptr<Message> ProtocolProcessor::createMessage(PacketId packetId,
                                                          uint8_t messageId) {
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
            return std::make_unique<
                Slave2Master::ConductionConfigResponseMessage>();
        case Slave2MasterMessageId::RESISTANCE_CFG_MSG:
            return std::make_unique<
                Slave2Master::ResistanceConfigResponseMessage>();
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

    case PacketId::BACKEND_TO_MASTER:
        switch (static_cast<Backend2MasterMessageId>(messageId)) {
        case Backend2MasterMessageId::SLAVE_CFG_MSG:
            return std::make_unique<Backend2Master::SlaveConfigMessage>();
        case Backend2MasterMessageId::MODE_CFG_MSG:
            return std::make_unique<Backend2Master::ModeConfigMessage>();
        case Backend2MasterMessageId::RST_MSG:
            return std::make_unique<Backend2Master::RstMessage>();
        case Backend2MasterMessageId::CTRL_MSG:
            return std::make_unique<Backend2Master::CtrlMessage>();
        case Backend2MasterMessageId::PING_CTRL_MSG:
            return std::make_unique<Backend2Master::PingCtrlMessage>();
        case Backend2MasterMessageId::DEVICE_LIST_REQ_MSG:
            return std::make_unique<Backend2Master::DeviceListReqMessage>();
        }
        break;

    case PacketId::MASTER_TO_BACKEND:
        switch (static_cast<Master2BackendMessageId>(messageId)) {
        case Master2BackendMessageId::SLAVE_CFG_RSP_MSG:
            return std::make_unique<Master2Backend::SlaveConfigResponseMessage>();
        case Master2BackendMessageId::MODE_CFG_RSP_MSG:
            return std::make_unique<Master2Backend::ModeConfigResponseMessage>();
        case Master2BackendMessageId::RST_RSP_MSG:
            return std::make_unique<Master2Backend::RstResponseMessage>();
        case Master2BackendMessageId::CTRL_RSP_MSG:
            return std::make_unique<Master2Backend::CtrlResponseMessage>();
        case Master2BackendMessageId::PING_RES_MSG:
            return std::make_unique<Master2Backend::PingResponseMessage>();
        case Master2BackendMessageId::DEVICE_LIST_RSP_MSG:
            return std::make_unique<Master2Backend::DeviceListResponseMessage>();
        }
        break;

    default:
        break;
    }
    return nullptr;
}

bool ProtocolProcessor::parseMaster2SlavePacket(
    const std::vector<uint8_t> &payload, uint32_t &destinationId,
    std::unique_ptr<Message> &message) {
    if (payload.size() < 5)
        return false;

    uint8_t messageId = payload[0];
    destinationId = readUint32LE(payload, 1);

    message = createMessage(PacketId::MASTER_TO_SLAVE, messageId);
    if (!message)
        return false;

    std::vector<uint8_t> messageData(payload.begin() + 5, payload.end());
    return message->deserialize(messageData);
}

bool ProtocolProcessor::parseSlave2MasterPacket(
    const std::vector<uint8_t> &payload, uint32_t &slaveId,
    std::unique_ptr<Message> &message) {
    if (payload.size() < 5)
        return false;

    uint8_t messageId = payload[0];
    slaveId = readUint32LE(payload, 1);

    message = createMessage(PacketId::SLAVE_TO_MASTER, messageId);
    if (!message)
        return false;

    std::vector<uint8_t> messageData(payload.begin() + 5, payload.end());
    return message->deserialize(messageData);
}

bool ProtocolProcessor::parseSlave2BackendPacket(
    const std::vector<uint8_t> &payload, uint32_t &slaveId,
    DeviceStatus &deviceStatus, std::unique_ptr<Message> &message) {
    if (payload.size() < 7)
        return false;

    uint8_t messageId = payload[0];
    slaveId = readUint32LE(payload, 1);
    deviceStatus.fromUint16(readUint16LE(payload, 5));

    message = createMessage(PacketId::SLAVE_TO_BACKEND, messageId);
    if (!message)
        return false;

    std::vector<uint8_t> messageData(payload.begin() + 7, payload.end());
    return message->deserialize(messageData);
}

bool ProtocolProcessor::parseBackend2MasterPacket(
    const std::vector<uint8_t> &payload, std::unique_ptr<Message> &message) {
    if (payload.size() < 1)
        return false;

    uint8_t messageId = payload[0];

    message = createMessage(PacketId::BACKEND_TO_MASTER, messageId);
    if (!message)
        return false;

    std::vector<uint8_t> messageData(payload.begin() + 1, payload.end());
    return message->deserialize(messageData);
}

bool ProtocolProcessor::parseMaster2BackendPacket(
    const std::vector<uint8_t> &payload, std::unique_ptr<Message> &message) {
    if (payload.size() < 1)
        return false;

    uint8_t messageId = payload[0];

    message = createMessage(PacketId::MASTER_TO_BACKEND, messageId);
    if (!message)
        return false;

    std::vector<uint8_t> messageData(payload.begin() + 1, payload.end());
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

bool SyncMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 5)
        return false;
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

bool ConductionConfigMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 8)
        return false;
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

bool ResistanceConfigMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 8)
        return false;
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

bool ClipConfigMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 4)
        return false;
    interval = data[0];
    mode = data[1];
    clipPin = data[2] | (data[3] << 8);
    return true;
}

std::vector<uint8_t> ReadConductionDataMessage::serialize() const {
    return {reserve};
}

bool ReadConductionDataMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 1)
        return false;
    reserve = data[0];
    return true;
}

std::vector<uint8_t> ReadResistanceDataMessage::serialize() const {
    return {reserve};
}

bool ReadResistanceDataMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 1)
        return false;
    reserve = data[0];
    return true;
}

std::vector<uint8_t> ReadClipDataMessage::serialize() const {
    return {reserve};
}

bool ReadClipDataMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 1)
        return false;
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

bool RstMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 3)
        return false;
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

bool PingReqMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 6)
        return false;
    sequenceNumber = data[0] | (data[1] << 8);
    timestamp = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
    return true;
}

std::vector<uint8_t> ShortIdAssignMessage::serialize() const {
    return {shortId};
}

bool ShortIdAssignMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 1)
        return false;
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

bool ConductionConfigResponseMessage::deserialize(
    const std::vector<uint8_t> &data) {
    if (data.size() < 9)
        return false;
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

bool ResistanceConfigResponseMessage::deserialize(
    const std::vector<uint8_t> &data) {
    if (data.size() < 9)
        return false;
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

bool ClipConfigResponseMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 5)
        return false;
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

bool RstResponseMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 4)
        return false;
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

bool PingRspMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 6)
        return false;
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

bool AnnounceMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 8)
        return false;
    deviceId = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    versionMajor = data[4];
    versionMinor = data[5];
    versionPatch = data[6] | (data[7] << 8);
    return true;
}

std::vector<uint8_t> ShortIdConfirmMessage::serialize() const {
    return {status, shortId};
}

bool ShortIdConfirmMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 2)
        return false;
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

bool ConductionDataMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 2)
        return false;
    conductionLength = data[0] | (data[1] << 8);
    if (data.size() < 2 + conductionLength)
        return false;
    conductionData.assign(data.begin() + 2,
                          data.begin() + 2 + conductionLength);
    return true;
}

std::vector<uint8_t> ResistanceDataMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(resistanceLength & 0xFF);
    result.push_back((resistanceLength >> 8) & 0xFF);
    result.insert(result.end(), resistanceData.begin(), resistanceData.end());
    return result;
}

bool ResistanceDataMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 2)
        return false;
    resistanceLength = data[0] | (data[1] << 8);
    if (data.size() < 2 + resistanceLength)
        return false;
    resistanceData.assign(data.begin() + 2,
                          data.begin() + 2 + resistanceLength);
    return true;
}

std::vector<uint8_t> ClipDataMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(clipData & 0xFF);
    result.push_back((clipData >> 8) & 0xFF);
    return result;
}

bool ClipDataMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 2)
        return false;
    clipData = data[0] | (data[1] << 8);
    return true;
}

} // namespace Slave2Backend

// Backend2Master 消息实现
namespace Backend2Master {

std::vector<uint8_t> SlaveConfigMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(slaveNum);
    
    for (const auto& slave : slaves) {
        // Write slave ID (4 bytes, little endian)
        result.push_back(slave.id & 0xFF);
        result.push_back((slave.id >> 8) & 0xFF);
        result.push_back((slave.id >> 16) & 0xFF);
        result.push_back((slave.id >> 24) & 0xFF);
        
        result.push_back(slave.conductionNum);
        result.push_back(slave.resistanceNum);
        result.push_back(slave.clipMode);
        
        // Write clip status (2 bytes, little endian)
        result.push_back(slave.clipStatus & 0xFF);
        result.push_back((slave.clipStatus >> 8) & 0xFF);
    }
    
    return result;
}

bool SlaveConfigMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 1) return false;
    
    slaveNum = data[0];
    slaves.clear();
    
    size_t offset = 1;
    for (uint8_t i = 0; i < slaveNum; ++i) {
        if (offset + 9 > data.size()) return false; // Each slave info is 9 bytes
        
        SlaveInfo slave;
        slave.id = data[offset] | (data[offset + 1] << 8) | 
                   (data[offset + 2] << 16) | (data[offset + 3] << 24);
        slave.conductionNum = data[offset + 4];
        slave.resistanceNum = data[offset + 5];
        slave.clipMode = data[offset + 6];
        slave.clipStatus = data[offset + 7] | (data[offset + 8] << 8);
        
        slaves.push_back(slave);
        offset += 9;
    }
    
    return true;
}

std::vector<uint8_t> ModeConfigMessage::serialize() const {
    return {mode};
}

bool ModeConfigMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 1) return false;
    mode = data[0];
    return true;
}

std::vector<uint8_t> RstMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(slaveNum);
    
    for (const auto& slave : slaves) {
        // Write slave ID (4 bytes, little endian)
        result.push_back(slave.id & 0xFF);
        result.push_back((slave.id >> 8) & 0xFF);
        result.push_back((slave.id >> 16) & 0xFF);
        result.push_back((slave.id >> 24) & 0xFF);
        
        result.push_back(slave.lock);
        
        // Write clip status (2 bytes, little endian)
        result.push_back(slave.clipStatus & 0xFF);
        result.push_back((slave.clipStatus >> 8) & 0xFF);
    }
    
    return result;
}

bool RstMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 1) return false;
    
    slaveNum = data[0];
    slaves.clear();
    
    size_t offset = 1;
    for (uint8_t i = 0; i < slaveNum; ++i) {
        if (offset + 7 > data.size()) return false; // Each slave rst info is 7 bytes
        
        SlaveRstInfo slave;
        slave.id = data[offset] | (data[offset + 1] << 8) | 
                   (data[offset + 2] << 16) | (data[offset + 3] << 24);
        slave.lock = data[offset + 4];
        slave.clipStatus = data[offset + 5] | (data[offset + 6] << 8);
        
        slaves.push_back(slave);
        offset += 7;
    }
    
    return true;
}

std::vector<uint8_t> CtrlMessage::serialize() const {
    return {runningStatus};
}

bool CtrlMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 1) return false;
    runningStatus = data[0];
    return true;
}

std::vector<uint8_t> PingCtrlMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(pingMode);
    
    // Write ping count (2 bytes, little endian)
    result.push_back(pingCount & 0xFF);
    result.push_back((pingCount >> 8) & 0xFF);
    
    // Write interval (2 bytes, little endian)
    result.push_back(interval & 0xFF);
    result.push_back((interval >> 8) & 0xFF);
    
    // Write destination ID (4 bytes, little endian)
    result.push_back(destinationId & 0xFF);
    result.push_back((destinationId >> 8) & 0xFF);
    result.push_back((destinationId >> 16) & 0xFF);
    result.push_back((destinationId >> 24) & 0xFF);
    
    return result;
}

bool PingCtrlMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 9) return false;
    
    pingMode = data[0];
    pingCount = data[1] | (data[2] << 8);
    interval = data[3] | (data[4] << 8);
    destinationId = data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);
    
    return true;
}

std::vector<uint8_t> DeviceListReqMessage::serialize() const {
    return {reserve};
}

bool DeviceListReqMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 1) return false;
    reserve = data[0];
    return true;
}

} // namespace Backend2Master

// Master2Backend 消息实现
namespace Master2Backend {

std::vector<uint8_t> SlaveConfigResponseMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(status);
    result.push_back(slaveNum);
    
    for (const auto& slave : slaves) {
        // Write slave ID (4 bytes, little endian)
        result.push_back(slave.id & 0xFF);
        result.push_back((slave.id >> 8) & 0xFF);
        result.push_back((slave.id >> 16) & 0xFF);
        result.push_back((slave.id >> 24) & 0xFF);
        
        result.push_back(slave.conductionNum);
        result.push_back(slave.resistanceNum);
        result.push_back(slave.clipMode);
        
        // Write clip status (2 bytes, little endian)
        result.push_back(slave.clipStatus & 0xFF);
        result.push_back((slave.clipStatus >> 8) & 0xFF);
    }
    
    return result;
}

bool SlaveConfigResponseMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 2) return false;
    
    status = data[0];
    slaveNum = data[1];
    slaves.clear();
    
    size_t offset = 2;
    for (uint8_t i = 0; i < slaveNum; ++i) {
        if (offset + 9 > data.size()) return false; // Each slave info is 9 bytes
        
        SlaveInfo slave;
        slave.id = data[offset] | (data[offset + 1] << 8) | 
                   (data[offset + 2] << 16) | (data[offset + 3] << 24);
        slave.conductionNum = data[offset + 4];
        slave.resistanceNum = data[offset + 5];
        slave.clipMode = data[offset + 6];
        slave.clipStatus = data[offset + 7] | (data[offset + 8] << 8);
        
        slaves.push_back(slave);
        offset += 9;
    }
    
    return true;
}

std::vector<uint8_t> ModeConfigResponseMessage::serialize() const {
    return {status, mode};
}

bool ModeConfigResponseMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 2) return false;
    status = data[0];
    mode = data[1];
    return true;
}

std::vector<uint8_t> RstResponseMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(status);
    result.push_back(slaveNum);
    
    for (const auto& slave : slaves) {
        // Write slave ID (4 bytes, little endian)
        result.push_back(slave.id & 0xFF);
        result.push_back((slave.id >> 8) & 0xFF);
        result.push_back((slave.id >> 16) & 0xFF);
        result.push_back((slave.id >> 24) & 0xFF);
        
        result.push_back(slave.lock);
        
        // Write clip status (2 bytes, little endian)
        result.push_back(slave.clipStatus & 0xFF);
        result.push_back((slave.clipStatus >> 8) & 0xFF);
    }
    
    return result;
}

bool RstResponseMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 2) return false;
    
    status = data[0];
    slaveNum = data[1];
    slaves.clear();
    
    size_t offset = 2;
    for (uint8_t i = 0; i < slaveNum; ++i) {
        if (offset + 7 > data.size()) return false; // Each slave rst info is 7 bytes
        
        SlaveRstInfo slave;
        slave.id = data[offset] | (data[offset + 1] << 8) | 
                   (data[offset + 2] << 16) | (data[offset + 3] << 24);
        slave.lock = data[offset + 4];
        slave.clipStatus = data[offset + 5] | (data[offset + 6] << 8);
        
        slaves.push_back(slave);
        offset += 7;
    }
    
    return true;
}

std::vector<uint8_t> CtrlResponseMessage::serialize() const {
    return {status, runningStatus};
}

bool CtrlResponseMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 2) return false;
    status = data[0];
    runningStatus = data[1];
    return true;
}

std::vector<uint8_t> PingResponseMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(pingMode);
    
    // Write total count (2 bytes, little endian)
    result.push_back(totalCount & 0xFF);
    result.push_back((totalCount >> 8) & 0xFF);
    
    // Write success count (2 bytes, little endian)
    result.push_back(successCount & 0xFF);
    result.push_back((successCount >> 8) & 0xFF);
    
    // Write destination ID (4 bytes, little endian)
    result.push_back(destinationId & 0xFF);
    result.push_back((destinationId >> 8) & 0xFF);
    result.push_back((destinationId >> 16) & 0xFF);
    result.push_back((destinationId >> 24) & 0xFF);
    
    return result;
}

bool PingResponseMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 9) return false;
    
    pingMode = data[0];
    totalCount = data[1] | (data[2] << 8);
    successCount = data[3] | (data[4] << 8);
    destinationId = data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);
    
    return true;
}

std::vector<uint8_t> DeviceListResponseMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(deviceCount);
    
    for (const auto& device : devices) {
        // Write device ID (4 bytes, little endian)
        result.push_back(device.deviceId & 0xFF);
        result.push_back((device.deviceId >> 8) & 0xFF);
        result.push_back((device.deviceId >> 16) & 0xFF);
        result.push_back((device.deviceId >> 24) & 0xFF);
        
        result.push_back(device.shortId);
        result.push_back(device.online);
        result.push_back(device.versionMajor);
        result.push_back(device.versionMinor);
        
        // Write version patch (2 bytes, little endian)
        result.push_back(device.versionPatch & 0xFF);
        result.push_back((device.versionPatch >> 8) & 0xFF);
    }
    
    return result;
}

bool DeviceListResponseMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 1) return false;
    
    deviceCount = data[0];
    devices.clear();
    
    size_t offset = 1;
    for (uint8_t i = 0; i < deviceCount; ++i) {
        if (offset + 10 > data.size()) return false; // Each device info is 10 bytes
        
        DeviceInfo device;
        device.deviceId = data[offset] | (data[offset + 1] << 8) | 
                         (data[offset + 2] << 16) | (data[offset + 3] << 24);
        device.shortId = data[offset + 4];
        device.online = data[offset + 5];
        device.versionMajor = data[offset + 6];
        device.versionMinor = data[offset + 7];
        device.versionPatch = data[offset + 8] | (data[offset + 9] << 8);
        
        devices.push_back(device);
        offset += 10;
    }
    
    return true;
}

} // namespace Master2Backend

// ProtocolProcessor 分片和粘包处理功能实现

// 支持自动分片的打包函数
std::vector<std::vector<uint8_t>>
ProtocolProcessor::packMaster2SlaveMessage(uint32_t destinationId,
                                           const Message &message) {
    // 首先生成单个完整帧
    auto completeFrame =
        packMaster2SlaveMessageSingle(destinationId, message, 0, 0);

    // 检查是否需要分片
    if (completeFrame.size() <= mtu_) {
        // 不需要分片，直接返回
        return {completeFrame};
    }

    // 需要分片
    return fragmentFrame(completeFrame);
}

std::vector<std::vector<uint8_t>>
ProtocolProcessor::packSlave2MasterMessage(uint32_t slaveId,
                                           const Message &message) {
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

std::vector<std::vector<uint8_t>>
ProtocolProcessor::packSlave2BackendMessage(uint32_t slaveId,
                                            const DeviceStatus &deviceStatus,
                                            const Message &message) {
    // 首先生成单个完整帧
    auto completeFrame =
        packSlave2BackendMessageSingle(slaveId, deviceStatus, message, 0, 0);

    // 检查是否需要分片
    if (completeFrame.size() <= mtu_) {
        // 不需要分片，直接返回
        return {completeFrame};
    }

    // 需要分片
    return fragmentFrame(completeFrame);
}

std::vector<std::vector<uint8_t>>
ProtocolProcessor::packBackend2MasterMessage(const Message &message) {
    // 首先生成单个完整帧
    auto completeFrame = packBackend2MasterMessageSingle(message, 0, 0);

    // 检查是否需要分片
    if (completeFrame.size() <= mtu_) {
        // 不需要分片，直接返回
        return {completeFrame};
    }

    // 需要分片
    return fragmentFrame(completeFrame);
}

std::vector<std::vector<uint8_t>>
ProtocolProcessor::packMaster2BackendMessage(const Message &message) {
    // 首先生成单个完整帧
    auto completeFrame = packMaster2BackendMessageSingle(message, 0, 0);

    // 检查是否需要分片
    if (completeFrame.size() <= mtu_) {
        // 不需要分片，直接返回
        return {completeFrame};
    }

    // 需要分片
    return fragmentFrame(completeFrame);
}

// 分片功能实现
std::vector<std::vector<uint8_t>>
ProtocolProcessor::fragmentFrame(const std::vector<uint8_t> &frameData) {
    std::vector<std::vector<uint8_t>> fragments;

    Log::i("ProtocolProcessor",
           "Starting frame fragmentation, original frame size: %zu bytes, MTU: "
           "%zu",
           frameData.size(), mtu_);

    if (frameData.size() < 7) { // Minimum frame header is 7 bytes
        Log::w("ProtocolProcessor",
               "Frame data too small for fragmentation: %zu bytes",
               frameData.size());
        return {frameData};
    }

    // Parse original frame header
    uint8_t packetId = frameData[2];
    Log::d("ProtocolProcessor", "Original frame PacketId: 0x%02X", packetId);

    // Calculate effective payload size per fragment (MTU - 7 bytes frame
    // header)
    size_t fragmentPayloadSize = mtu_ - 7;
    Log::d("ProtocolProcessor", "Maximum payload size per fragment: %zu bytes",
           fragmentPayloadSize);

    // Get original payload (starting from 7th byte)
    std::vector<uint8_t> originalPayload(frameData.begin() + 7,
                                         frameData.end());
    Log::d("ProtocolProcessor", "Original payload size: %zu bytes",
           originalPayload.size());

    // Calculate how many fragments are needed
    uint8_t totalFragments = static_cast<uint8_t>(
        (originalPayload.size() + fragmentPayloadSize - 1) /
        fragmentPayloadSize);
    Log::i("ProtocolProcessor", "Total fragments needed: %d", totalFragments);

    // Generate each fragment
    for (uint8_t i = 0; i < totalFragments; ++i) {
        std::vector<uint8_t> fragment;

        // Add frame header
        fragment.push_back(FRAME_DELIMITER_1);
        fragment.push_back(FRAME_DELIMITER_2);
        fragment.push_back(packetId);
        fragment.push_back(i); // Fragment sequence number
        fragment.push_back((i == totalFragments - 1) ? 0
                                                     : 1); // moreFragmentsFlag

        // Calculate payload for this fragment
        size_t startPos = i * fragmentPayloadSize;
        size_t endPos =
            std::min(startPos + fragmentPayloadSize, originalPayload.size());
        size_t fragmentSize = endPos - startPos;

        // Add length field
        fragment.push_back(fragmentSize & 0xFF);
        fragment.push_back((fragmentSize >> 8) & 0xFF);

        // Add fragment payload
        fragment.insert(fragment.end(), originalPayload.begin() + startPos,
                        originalPayload.begin() + endPos);

        Log::d("ProtocolProcessor",
               "Fragment #%d/%d, sequence=%d, more_fragments=%d, "
               "fragment_size=%zu, payload_size=%zu",
               i, totalFragments - 1, i, (i == totalFragments - 1) ? 0 : 1,
               fragment.size(), fragmentSize);

        fragments.push_back(fragment);
    }

    Log::i("ProtocolProcessor",
           "Fragmentation completed, generated %zu fragments",
           fragments.size());
    return fragments;
}

// Process received raw data (supports packet concatenation handling)
void ProtocolProcessor::processReceivedData(const std::vector<uint8_t> &data) {
    Log::i("ProtocolProcessor",
           "Received new data, size: %zu bytes, prefix: %s", data.size(),
           bytesToHexString(data, 8).c_str());

    // Prevent receive buffer from becoming too large
    if (receiveBuffer_.size() + data.size() > MAX_RECEIVE_BUFFER_SIZE) {
        Log::w("ProtocolProcessor",
               "Receive buffer will exceed maximum limit, clearing buffer. "
               "Current size: %zu, new data size: %zu, max limit: %zu",
               receiveBuffer_.size(), data.size(), MAX_RECEIVE_BUFFER_SIZE);
        // Clear buffer to prevent memory overflow
        receiveBuffer_.clear();
    }

    // Add new data to receive buffer
    receiveBuffer_.insert(receiveBuffer_.end(), data.begin(), data.end());
    Log::d("ProtocolProcessor", "Current receive buffer size: %zu bytes",
           receiveBuffer_.size());

    // Try to extract complete frames from buffer
    bool framesExtracted = extractCompleteFrames();
    Log::d("ProtocolProcessor", "Frame extraction result: %s",
           framesExtracted ? "frames found" : "no frames found");

    // Clean up expired fragments
    cleanupExpiredFragments();
}

// Extract complete frames from receive buffer
bool ProtocolProcessor::extractCompleteFrames() {
    bool foundFrames = false;
    size_t pos = 0;

    Log::d(
        "ProtocolProcessor",
        "Starting frame extraction from receive buffer, buffer size: %zu bytes",
        receiveBuffer_.size());

    while (pos < receiveBuffer_.size()) {
        // Find frame header
        size_t frameStart = findFrameHeader(receiveBuffer_, pos);
        if (frameStart == SIZE_MAX) {
            Log::d("ProtocolProcessor",
                   "No frame header found, skipping current data");
            break; // No frame header found
        }

        Log::d("ProtocolProcessor", "Frame header found at position: %zu",
               frameStart);

        // Check if there's enough data to read frame length
        if (frameStart + 7 > receiveBuffer_.size()) {
            Log::d("ProtocolProcessor", "Insufficient data to read frame "
                                        "length, waiting for more data");
            break; // Not enough data, wait for more
        }

        // 读取帧长度
        uint16_t frameLength = readUint16LE(receiveBuffer_, frameStart + 5);
        size_t totalFrameSize = 7 + frameLength;

        Log::d("ProtocolProcessor",
               "Frame payload length: %d, total frame size: %zu", frameLength,
               totalFrameSize);

        // 检查是否有完整的帧
        if (frameStart + totalFrameSize > receiveBuffer_.size()) {
            Log::d(
                "ProtocolProcessor",
                "Incomplete frame, waiting for more data. Need: %zu, have: %zu",
                frameStart + totalFrameSize, receiveBuffer_.size());
            break; // 帧不完整，等待更多数据
        }

        // 提取完整帧数据
        std::vector<uint8_t> frameData(receiveBuffer_.begin() + frameStart,
                                       receiveBuffer_.begin() + frameStart +
                                           totalFrameSize);

        Log::i(
            "ProtocolProcessor",
            "Extracted complete frame data, size: %zu bytes, frame prefix: %s",
            frameData.size(), bytesToHexString(frameData, 16).c_str());

        // 解析帧
        Frame frame;
        if (Frame::deserialize(frameData, frame)) {
            Log::i(
                "ProtocolProcessor",
                "Frame parsed successfully, PacketId: 0x%02X, "
                "fragment_sequence: %d, more_fragments: %d, payload_length: %d",
                frame.packetId, frame.fragmentsSequence,
                frame.moreFragmentsFlag, frame.packetLength);

            // 检查是否是分片
            if (frame.moreFragmentsFlag || frame.fragmentsSequence > 0) {
                Log::i("ProtocolProcessor",
                       "Fragment frame detected, starting fragment reassembly");
                // 处理分片重组
                std::vector<uint8_t> completeFrame;
                if (reassembleFragments(frame, completeFrame)) {
                    Log::i("ProtocolProcessor",
                           "Fragment reassembly completed, reassembled frame "
                           "size: %zu bytes",
                           completeFrame.size());
                    // 分片重组完成，解析完整帧
                    Frame completedFrame;
                    if (Frame::deserialize(completeFrame, completedFrame)) {
                        Log::i("ProtocolProcessor",
                               "Reassembled frame parsed successfully, "
                               "PacketId: 0x%02X, payload_length: %d",
                               completedFrame.packetId,
                               completedFrame.packetLength);
                        completeFrames_.push(completedFrame);
                        foundFrames = true;
                    } else {
                        Log::e("ProtocolProcessor",
                               "Failed to parse reassembled frame");
                    }
                } else {
                    Log::d("ProtocolProcessor",
                           "Fragment reassembly not complete, waiting for more "
                           "fragments");
                }
            } else {
                Log::i("ProtocolProcessor",
                       "Single complete frame, adding to complete frame queue");
                // 单个完整帧
                completeFrames_.push(frame);
                foundFrames = true;
            }
        } else {
            Log::e("ProtocolProcessor", "Frame parsing failed");
        }

        // 移动到下一个位置
        pos = frameStart + totalFrameSize;
        Log::d("ProtocolProcessor", "Moving to next position: %zu", pos);
    }

    // 清理已处理的数据
    if (pos > 0) {
        Log::d("ProtocolProcessor",
               "Cleaning processed data, from 0 to %zu, remaining %zu bytes",
               pos, receiveBuffer_.size() - pos);
        receiveBuffer_.erase(receiveBuffer_.begin(),
                             receiveBuffer_.begin() + pos);
    }

    return foundFrames;
}

// 查找帧头
size_t ProtocolProcessor::findFrameHeader(const std::vector<uint8_t> &buffer,
                                          size_t startPos) {
    for (size_t i = startPos; i < buffer.size() - 1; ++i) {
        if (buffer[i] == FRAME_DELIMITER_1 &&
            buffer[i + 1] == FRAME_DELIMITER_2) {
            return i;
        }
    }
    return SIZE_MAX;
}

// 分片重组
bool ProtocolProcessor::reassembleFragments(
    const Frame &frame, std::vector<uint8_t> &completeFrame) {
    Log::i("ProtocolProcessor",
           "Starting fragment reassembly, fragment_sequence: %d, "
           "more_fragments: %d",
           frame.fragmentsSequence, frame.moreFragmentsFlag);

    // 从帧载荷中提取源ID (假设载荷格式为: MessageId + SourceId + ...)
    if (frame.payload.size() < 5) {
        Log::e("ProtocolProcessor",
               "Fragment payload too small to extract source ID, payload size: "
               "%zu",
               frame.payload.size());
        return false; // 载荷太小
    }

    uint8_t messageId = frame.payload[0];
    uint32_t sourceId = readUint32LE(frame.payload, 1); // 跳过MessageId
    uint64_t fragmentId = generateFragmentId(frame.packetId);

    Log::d("ProtocolProcessor",
           "Fragment info - MessageId: 0x%02X, SourceId: 0x%08X, generated "
           "FragmentId: 0x%016llX",
           messageId, sourceId, fragmentId);

    // 查找或创建分片信息
    auto &fragmentInfo = fragmentMap_[fragmentId];
    fragmentInfo.packetId = frame.packetId;
    fragmentInfo.timestamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    // 存储分片数据
    fragmentInfo.fragments[frame.fragmentsSequence] = frame.payload;
    Log::d("ProtocolProcessor",
           "Storing fragment data, sequence: %d, payload size: %zu, collected "
           "fragments: %zu",
           frame.fragmentsSequence, frame.payload.size(),
           fragmentInfo.fragments.size());

    // 如果这是最后一个分片，计算总分片数
    if (frame.moreFragmentsFlag == 0) {
        fragmentInfo.totalFragments = frame.fragmentsSequence + 1;
        Log::i("ProtocolProcessor",
               "Received last fragment, total fragments set to: %d",
               fragmentInfo.totalFragments);
    }

    // 检查是否收集完所有分片
    if (fragmentInfo.totalFragments > 0 && fragmentInfo.isComplete()) {
        Log::i("ProtocolProcessor",
               "All fragments collected, starting complete frame reassembly, "
               "total fragments: %d",
               fragmentInfo.totalFragments);

        // 重组完整载荷
        std::vector<uint8_t> completePayload;

        // 检查一下分片顺序是否正确
        std::string fragmentOrder = "Fragment order: ";
        for (const auto &entry : fragmentInfo.fragments) {
            fragmentOrder += std::to_string(entry.first) + " ";
        }
        Log::d("ProtocolProcessor", "%s", fragmentOrder.c_str());

        // Check for duplicate MessageIds before reassembly (this could be an
        // issue) This code is commented out as it's for debugging purposes only

        // Fix: Don't duplicate header information (MessageId + SourceId) for
        // each fragment Only keep complete payload in the first fragment
        // (sequence 0) Subsequent fragments only keep the data portion

        // First add the complete payload of the first fragment
        auto firstFragment = fragmentInfo.fragments.find(0);
        if (firstFragment != fragmentInfo.fragments.end()) {
            Log::d("ProtocolProcessor",
                   "Adding first fragment complete payload, size: %zu",
                   firstFragment->second.size());
            completePayload.insert(completePayload.end(),
                                   firstFragment->second.begin(),
                                   firstFragment->second.end());
        } else {
            Log::e("ProtocolProcessor",
                   "Critical error: missing first fragment (sequence 0)");
            return false;
        }

        // Then add the data portion of subsequent fragments (skip header
        // information)
        for (uint8_t i = 0; i < fragmentInfo.totalFragments; ++i) {
            auto it = fragmentInfo.fragments.find(i);
            if (it != fragmentInfo.fragments.end()) {
                completePayload.insert(completePayload.end(),
                                       it->second.begin(), it->second.end());
            }
        }

        Log::d("ProtocolProcessor", "Reassembled complete payload size: %zu",
               completePayload.size());

        // Build complete frame
        completeFrame.clear();
        completeFrame.push_back(FRAME_DELIMITER_1);
        completeFrame.push_back(FRAME_DELIMITER_2);
        completeFrame.push_back(frame.packetId);
        completeFrame.push_back(0); // fragmentsSequence = 0
        completeFrame.push_back(0); // moreFragmentsFlag = 0

        // Add payload length
        uint16_t payloadLength = static_cast<uint16_t>(completePayload.size());
        completeFrame.push_back(payloadLength & 0xFF);
        completeFrame.push_back((payloadLength >> 8) & 0xFF);

        Log::d("ProtocolProcessor",
               "Setting complete frame header, PacketId: 0x%02X, payload "
               "length: %d",
               frame.packetId, payloadLength);

        // Add payload
        completeFrame.insert(completeFrame.end(), completePayload.begin(),
                             completePayload.end());

        Log::i("ProtocolProcessor",
               "Complete frame reassembly finished, total size: %zu bytes, "
               "frame data prefix: %s",
               completeFrame.size(),
               bytesToHexString(completeFrame, 16).c_str());

        // Clean up fragment information
        fragmentMap_.erase(fragmentId);
        Log::d("ProtocolProcessor",
               "Cleaning fragment info, current fragment map size: %zu",
               fragmentMap_.size());

        return true;
    } else {
        if (fragmentInfo.totalFragments == 0) {
            Log::d("ProtocolProcessor",
                   "Last fragment not yet received, cannot determine total "
                   "fragment count");
        } else {
            Log::d("ProtocolProcessor",
                   "Fragment collection incomplete, collected: %zu, total "
                   "fragments: %d",
                   fragmentInfo.fragments.size(), fragmentInfo.totalFragments);
        }
    }

    return false; // Haven't collected all fragments yet
}

// Get next complete frame
bool ProtocolProcessor::getNextCompleteFrame(Frame &frame) {
    if (completeFrames_.empty()) {
        return false;
    }

    frame = completeFrames_.front();
    completeFrames_.pop();
    return true;
}

// Clear receive buffer
void ProtocolProcessor::clearReceiveBuffer() {
    receiveBuffer_.clear();
    while (!completeFrames_.empty()) {
        completeFrames_.pop();
    }
    fragmentMap_.clear();
}

// Generate fragment ID
uint64_t ProtocolProcessor::generateFragmentId(uint8_t packetId) {
    return packetId;
}

// Clean up expired fragments
void ProtocolProcessor::cleanupExpiredFragments() {
    // Fragment timeout cleanup is currently disabled
    // This function can be implemented to clean up expired fragments
    // based on timestamp if needed in the future
}

} // namespace WhtsProtocol