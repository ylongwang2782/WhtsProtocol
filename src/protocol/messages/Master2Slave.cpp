#include "Master2Slave.h"

namespace WhtsProtocol {
namespace Master2Slave {

// SyncMessage 实现
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

// ConductionConfigMessage 实现
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

// ResistanceConfigMessage 实现
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

// ClipConfigMessage 实现
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

// ReadConductionDataMessage 实现
std::vector<uint8_t> ReadConductionDataMessage::serialize() const {
    return {reserve};
}

bool ReadConductionDataMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 1)
        return false;
    reserve = data[0];
    return true;
}

// ReadResistanceDataMessage 实现
std::vector<uint8_t> ReadResistanceDataMessage::serialize() const {
    return {reserve};
}

bool ReadResistanceDataMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 1)
        return false;
    reserve = data[0];
    return true;
}

// ReadClipDataMessage 实现
std::vector<uint8_t> ReadClipDataMessage::serialize() const {
    return {reserve};
}

bool ReadClipDataMessage::deserialize(const std::vector<uint8_t> &data) {
    if (data.size() < 1)
        return false;
    reserve = data[0];
    return true;
}

// RstMessage 实现
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

// PingReqMessage 实现
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

// ShortIdAssignMessage 实现
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
} // namespace WhtsProtocol