#ifndef WHTS_PROTOCOL_H
#define WHTS_PROTOCOL_H

#include <cstdint>
#include <vector>
#include <memory>
#include <array>
#include <map>
#include <queue>

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
    
    // 设置最大传输单元大小 (MTU)
    void setMTU(size_t mtu) { mtu_ = mtu; }
    size_t getMTU() const { return mtu_; }
    
    // 打包Master2Slave消息 (支持自动分片)
    std::vector<std::vector<uint8_t>> packMaster2SlaveMessage(uint32_t destinationId, const Message& message);
    
    // 打包Slave2Master消息 (支持自动分片)
    std::vector<std::vector<uint8_t>> packSlave2MasterMessage(uint32_t slaveId, const Message& message);
    
    // 打包Slave2Backend消息 (支持自动分片)
    std::vector<std::vector<uint8_t>> packSlave2BackendMessage(uint32_t slaveId, const DeviceStatus& deviceStatus, 
                                                               const Message& message);
    
    // 兼容旧接口 - 单帧打包
    std::vector<uint8_t> packMaster2SlaveMessageSingle(uint32_t destinationId, const Message& message, 
                                                        uint8_t fragmentsSequence = 0, uint8_t moreFragmentsFlag = 0);
    
    std::vector<uint8_t> packSlave2MasterMessageSingle(uint32_t slaveId, const Message& message,
                                                        uint8_t fragmentsSequence = 0, uint8_t moreFragmentsFlag = 0);
    
    std::vector<uint8_t> packSlave2BackendMessageSingle(uint32_t slaveId, const DeviceStatus& deviceStatus, 
                                                         const Message& message, uint8_t fragmentsSequence = 0, 
                                                         uint8_t moreFragmentsFlag = 0);
    
    // 处理接收到的原始数据 (支持粘包处理)
    void processReceivedData(const std::vector<uint8_t>& data);
    
    // 获取完整的已解析帧
    bool getNextCompleteFrame(Frame& frame);
    
    // 清空接收缓冲区
    void clearReceiveBuffer();
    
    // 解析单个帧
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
    // 分片相关的私有结构
    struct FragmentInfo {
        uint8_t packetId;
        uint32_t sourceId;
        uint8_t totalFragments;
        std::map<uint8_t, std::vector<uint8_t>> fragments;  // 序号 -> 分片数据
        uint32_t timestamp;  // 用于超时处理
        
        bool isComplete() const {
            return fragments.size() == totalFragments;
        }
    };
    
    // 帧分片
    std::vector<std::vector<uint8_t>> fragmentFrame(const std::vector<uint8_t>& frameData);
    
    // 分片重组
    bool reassembleFragments(const Frame& frame, std::vector<uint8_t>& completeFrame);
    
    // 从接收缓冲区中提取完整帧
    bool extractCompleteFrames();
    
    // 查找帧头
    size_t findFrameHeader(const std::vector<uint8_t>& buffer, size_t startPos);
    
    // 工具函数
    void writeUint16LE(std::vector<uint8_t>& buffer, uint16_t value);
    void writeUint32LE(std::vector<uint8_t>& buffer, uint32_t value);
    uint16_t readUint16LE(const std::vector<uint8_t>& buffer, size_t offset);
    uint32_t readUint32LE(const std::vector<uint8_t>& buffer, size_t offset);
    
    // 生成分片的唯一ID
    uint64_t generateFragmentId(uint8_t packetId, uint32_t sourceId);
    
    // 清理超时的分片
    void cleanupExpiredFragments();
    
private:
    size_t mtu_;  // 最大传输单元大小，默认100字节
    std::vector<uint8_t> receiveBuffer_;  // 接收缓冲区
    std::queue<Frame> completeFrames_;  // 完整帧队列
    std::map<uint64_t, FragmentInfo> fragmentMap_;  // 分片重组映射
    static constexpr uint32_t FRAGMENT_TIMEOUT_MS = 5000;  // 分片超时时间（毫秒）
    static constexpr size_t DEFAULT_MTU = 100;  // 默认MTU大小
    static constexpr size_t MAX_RECEIVE_BUFFER_SIZE = 4096;  // 最大接收缓冲区大小
};

} // namespace WhtsProtocol

#endif // WHTS_PROTOCOL_H 