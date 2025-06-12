#ifndef WHTS_PROTOCOL_H
#define WHTS_PROTOCOL_H

#include <cstdint>
#include <vector>
#include <memory>
#include <array>

namespace WhtsProtocol {

// 协议常量
constexpr uint8_t FRAME_DELIMITER_1 = 0xAB;
constexpr uint8_t FRAME_DELIMITER_2 = 0xCD;
constexpr uint32_t BROADCAST_ID = 0xFFFFFFFF;

// Packet ID 枚举
enum class PacketId : uint8_t {
    MASTER_TO_SLAVE = 0x00,
    SLAVE_TO_MASTER = 0x01,
    BACKEND_TO_MASTER = 0x02,
    MASTER_TO_BACKEND = 0x03,
    SLAVE_TO_BACKEND = 0x04
};

// Master2Slave Message ID 枚举
enum class Master2SlaveMessageId : uint8_t {
    SYNC_MSG = 0x00,
    CONDUCTION_CFG_MSG = 0x10,
    RESISTANCE_CFG_MSG = 0x11,
    CLIP_CFG_MSG = 0x12,
    READ_COND_DATA_MSG = 0x20,
    READ_RES_DATA_MSG = 0x21,
    READ_CLIP_DATA_MSG = 0x22,
    RST_MSG = 0x30,
    PING_REQ_MSG = 0x40,
    SHORT_ID_ASSIGN_MSG = 0x50
};

// Slave2Master Message ID 枚举
enum class Slave2MasterMessageId : uint8_t {
    CONDUCTION_CFG_MSG = 0x10,
    RESISTANCE_CFG_MSG = 0x11,
    CLIP_CFG_MSG = 0x22,
    RST_MSG = 0x30,
    PING_RSP_MSG = 0x41,
    ANNOUNCE_MSG = 0x50,
    SHORT_ID_CONFIRM_MSG = 0x51
};

// Backend2Master Message ID 枚举
enum class Backend2MasterMessageId : uint8_t {
    SLAVE_CFG_MSG = 0x00,
    MODE_CFG_MSG = 0x01,
    RST_MSG = 0x02,
    CTRL_MSG = 0x03,
    PING_CTRL_MSG = 0x10,
    DEVICE_LIST_REQ_MSG = 0x11
};

// Master2Backend Message ID 枚举
enum class Master2BackendMessageId : uint8_t {
    SLAVE_CFG_MSG = 0x00,
    MODE_CFG_MSG = 0x01,
    RST_MSG = 0x02,
    CTRL_MSG = 0x03,
    PING_RES_MSG = 0x04,
    DEVICE_LIST_RSP_MSG = 0x05
};

// Slave2Backend Message ID 枚举
enum class Slave2BackendMessageId : uint8_t {
    CONDUCTION_DATA_MSG = 0x00,
    RESISTANCE_DATA_MSG = 0x01,
    CLIP_DATA_MSG = 0x02
};

// 设备状态位结构
struct DeviceStatus {
    bool colorSensor : 1;
    bool sleeveLimit : 1;
    bool electromagnetUnlockButton : 1;
    bool batteryLowAlarm : 1;
    bool pressureSensor : 1;
    bool electromagneticLock1 : 1;
    bool electromagneticLock2 : 1;
    bool accessory1 : 1;
    bool accessory2 : 1;
    uint8_t reserved : 7;
    
    uint16_t toUint16() const;
    void fromUint16(uint16_t status);
};

// 帧结构
struct Frame {
    uint8_t delimiter1;
    uint8_t delimiter2;
    uint8_t packetId;
    uint8_t fragmentsSequence;
    uint8_t moreFragmentsFlag;
    uint16_t packetLength;
    std::vector<uint8_t> payload;
    
    Frame();
    bool isValid() const;
    std::vector<uint8_t> serialize() const;
    static bool deserialize(const std::vector<uint8_t>& data, Frame& frame);
};

// 基础消息类
class Message {
public:
    virtual ~Message() = default;
    virtual std::vector<uint8_t> serialize() const = 0;
    virtual bool deserialize(const std::vector<uint8_t>& data) = 0;
    virtual uint8_t getMessageId() const = 0;
};

// Master2Slave 消息类
namespace Master2Slave {

class SyncMessage : public Message {
public:
    uint8_t mode;
    uint32_t timestamp;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Master2SlaveMessageId::SYNC_MSG); }
};

class ConductionConfigMessage : public Message {
public:
    uint8_t timeSlot;
    uint8_t interval;
    uint16_t totalConductionNum;
    uint16_t startConductionNum;
    uint16_t conductionNum;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Master2SlaveMessageId::CONDUCTION_CFG_MSG); }
};

class ResistanceConfigMessage : public Message {
public:
    uint8_t timeSlot;
    uint8_t interval;
    uint16_t totalNum;
    uint16_t startNum;
    uint16_t num;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Master2SlaveMessageId::RESISTANCE_CFG_MSG); }
};

class ClipConfigMessage : public Message {
public:
    uint8_t interval;
    uint8_t mode;
    uint16_t clipPin;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Master2SlaveMessageId::CLIP_CFG_MSG); }
};

class ReadConductionDataMessage : public Message {
public:
    uint8_t reserve;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Master2SlaveMessageId::READ_COND_DATA_MSG); }
};

class ReadResistanceDataMessage : public Message {
public:
    uint8_t reserve;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Master2SlaveMessageId::READ_RES_DATA_MSG); }
};

class ReadClipDataMessage : public Message {
public:
    uint8_t reserve;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Master2SlaveMessageId::READ_CLIP_DATA_MSG); }
};

class RstMessage : public Message {
public:
    uint8_t lockStatus;
    uint16_t clipLed;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Master2SlaveMessageId::RST_MSG); }
};

class PingReqMessage : public Message {
public:
    uint16_t sequenceNumber;
    uint32_t timestamp;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Master2SlaveMessageId::PING_REQ_MSG); }
};

class ShortIdAssignMessage : public Message {
public:
    uint8_t shortId;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Master2SlaveMessageId::SHORT_ID_ASSIGN_MSG); }
};

} // namespace Master2Slave

// Slave2Master 消息类
namespace Slave2Master {

class ConductionConfigResponseMessage : public Message {
public:
    uint8_t status;
    uint8_t timeSlot;
    uint8_t interval;
    uint16_t totalConductionNum;
    uint16_t startConductionNum;
    uint16_t conductionNum;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Slave2MasterMessageId::CONDUCTION_CFG_MSG); }
};

class ResistanceConfigResponseMessage : public Message {
public:
    uint8_t status;
    uint8_t timeSlot;
    uint8_t interval;
    uint16_t totalConductionNum;
    uint16_t startConductionNum;
    uint16_t conductionNum;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Slave2MasterMessageId::RESISTANCE_CFG_MSG); }
};

class ClipConfigResponseMessage : public Message {
public:
    uint8_t status;
    uint8_t interval;
    uint8_t mode;
    uint16_t clipPin;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Slave2MasterMessageId::CLIP_CFG_MSG); }
};

class RstResponseMessage : public Message {
public:
    uint8_t status;
    uint8_t lockStatus;
    uint16_t clipLed;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Slave2MasterMessageId::RST_MSG); }
};

class PingRspMessage : public Message {
public:
    uint16_t sequenceNumber;
    uint32_t timestamp;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Slave2MasterMessageId::PING_RSP_MSG); }
};

class AnnounceMessage : public Message {
public:
    uint32_t deviceId;
    uint8_t versionMajor;
    uint8_t versionMinor;
    uint16_t versionPatch;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Slave2MasterMessageId::ANNOUNCE_MSG); }
};

class ShortIdConfirmMessage : public Message {
public:
    uint8_t status;
    uint8_t shortId;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Slave2MasterMessageId::SHORT_ID_CONFIRM_MSG); }
};

} // namespace Slave2Master

// Slave2Backend 消息类
namespace Slave2Backend {

class ConductionDataMessage : public Message {
public:
    uint16_t conductionLength;
    std::vector<uint8_t> conductionData;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Slave2BackendMessageId::CONDUCTION_DATA_MSG); }
};

class ResistanceDataMessage : public Message {
public:
    uint16_t resistanceLength;
    std::vector<uint8_t> resistanceData;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Slave2BackendMessageId::RESISTANCE_DATA_MSG); }
};

class ClipDataMessage : public Message {
public:
    uint16_t clipData;
    
    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t>& data) override;
    uint8_t getMessageId() const override { return static_cast<uint8_t>(Slave2BackendMessageId::CLIP_DATA_MSG); }
};

} // namespace Slave2Backend

// 协议处理器类
class ProtocolProcessor {
public:
    ProtocolProcessor();
    ~ProtocolProcessor();
    
    // 打包Master2Slave消息
    std::vector<uint8_t> packMaster2SlaveMessage(uint32_t destinationId, const Message& message, 
                                                  uint8_t fragmentsSequence = 0, uint8_t moreFragmentsFlag = 0);
    
    // 打包Slave2Master消息
    std::vector<uint8_t> packSlave2MasterMessage(uint32_t slaveId, const Message& message,
                                                  uint8_t fragmentsSequence = 0, uint8_t moreFragmentsFlag = 0);
    
    // 打包Slave2Backend消息
    std::vector<uint8_t> packSlave2BackendMessage(uint32_t slaveId, const DeviceStatus& deviceStatus, 
                                                   const Message& message, uint8_t fragmentsSequence = 0, 
                                                   uint8_t moreFragmentsFlag = 0);
    
    // 解析接收到的数据
    bool parseFrame(const std::vector<uint8_t>& data, Frame& frame);
    
    // 根据Packet ID和Message ID创建对应的消息对象
    std::unique_ptr<Message> createMessage(PacketId packetId, uint8_t messageId);
    
    // 解析Master2Slave包
    bool parseMaster2SlavePacket(const std::vector<uint8_t>& payload, uint32_t& destinationId, 
                                 std::unique_ptr<Message>& message);
    
    // 解析Slave2Master包
    bool parseSlave2MasterPacket(const std::vector<uint8_t>& payload, uint32_t& slaveId, 
                                 std::unique_ptr<Message>& message);
    
    // 解析Slave2Backend包
    bool parseSlave2BackendPacket(const std::vector<uint8_t>& payload, uint32_t& slaveId, 
                                  DeviceStatus& deviceStatus, std::unique_ptr<Message>& message);

private:
    void writeUint16LE(std::vector<uint8_t>& buffer, uint16_t value);
    void writeUint32LE(std::vector<uint8_t>& buffer, uint32_t value);
    uint16_t readUint16LE(const std::vector<uint8_t>& buffer, size_t offset);
    uint32_t readUint32LE(const std::vector<uint8_t>& buffer, size_t offset);
};

} // namespace WhtsProtocol

#endif // WHTS_PROTOCOL_H 