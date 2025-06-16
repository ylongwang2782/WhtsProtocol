#ifndef WHTS_PROTOCOL_H
#define WHTS_PROTOCOL_H

/**
 * @file WhtsProtocol.h
 * @brief WhtsProtocol库的主要头文件
 *
 * 这个文件提供了协议库的统一入口点，包含了所有必要的子模块。
 * 用户只需要包含这个头文件即可使用完整的协议库功能。
 *
 * 模块化结构：
 * - protocol/Common.h - 通用常量和枚举定义
 * - protocol/DeviceStatus.h - 设备状态管理
 * - protocol/Frame.h - 帧结构定义和序列化
 * - protocol/ProtocolProcessor.h - 协议处理器（核心功能）
 * - protocol/messages/ - 所有消息类型的定义和实现
 * - protocol/utils/ - 工具函数库
 */

// 包含协议模块头文件，提供统一入口
#include "protocol/WhtsProtocol.h"

// Master2Slave 消息类
namespace Master2Slave {

class SyncMessage : public Message {
  public:
    uint8_t mode;
    uint32_t timestamp;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::SYNC_MSG);
    }
};

class ConductionConfigMessage : public Message {
  public:
    uint8_t timeSlot;
    uint8_t interval;
    uint16_t totalConductionNum;
    uint16_t startConductionNum;
    uint16_t conductionNum;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::CONDUCTION_CFG_MSG);
    }
};

class ResistanceConfigMessage : public Message {
  public:
    uint8_t timeSlot;
    uint8_t interval;
    uint16_t totalNum;
    uint16_t startNum;
    uint16_t num;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::RESISTANCE_CFG_MSG);
    }
};

class ClipConfigMessage : public Message {
  public:
    uint8_t interval;
    uint8_t mode;
    uint16_t clipPin;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::CLIP_CFG_MSG);
    }
};

class ReadConductionDataMessage : public Message {
  public:
    uint8_t reserve;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::READ_COND_DATA_MSG);
    }
};

class ReadResistanceDataMessage : public Message {
  public:
    uint8_t reserve;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::READ_RES_DATA_MSG);
    }
};

class ReadClipDataMessage : public Message {
  public:
    uint8_t reserve;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::READ_CLIP_DATA_MSG);
    }
};

class RstMessage : public Message {
  public:
    uint8_t lockStatus;
    uint16_t clipLed;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::RST_MSG);
    }
};

class PingReqMessage : public Message {
  public:
    uint16_t sequenceNumber;
    uint32_t timestamp;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::PING_REQ_MSG);
    }
};

class ShortIdAssignMessage : public Message {
  public:
    uint8_t shortId;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::SHORT_ID_ASSIGN_MSG);
    }
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
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Slave2MasterMessageId::CONDUCTION_CFG_MSG);
    }
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
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Slave2MasterMessageId::RESISTANCE_CFG_MSG);
    }
};

class ClipConfigResponseMessage : public Message {
  public:
    uint8_t status;
    uint8_t interval;
    uint8_t mode;
    uint16_t clipPin;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Slave2MasterMessageId::CLIP_CFG_MSG);
    }
};

class RstResponseMessage : public Message {
  public:
    uint8_t status;
    uint8_t lockStatus;
    uint16_t clipLed;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Slave2MasterMessageId::RST_MSG);
    }
};

class PingRspMessage : public Message {
  public:
    uint16_t sequenceNumber;
    uint32_t timestamp;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Slave2MasterMessageId::PING_RSP_MSG);
    }
};

class AnnounceMessage : public Message {
  public:
    uint32_t deviceId;
    uint8_t versionMajor;
    uint8_t versionMinor;
    uint16_t versionPatch;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Slave2MasterMessageId::ANNOUNCE_MSG);
    }
};

class ShortIdConfirmMessage : public Message {
  public:
    uint8_t status;
    uint8_t shortId;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(
            Slave2MasterMessageId::SHORT_ID_CONFIRM_MSG);
    }
};

} // namespace Slave2Master

// Slave2Backend 消息类
namespace Slave2Backend {

class ConductionDataMessage : public Message {
  public:
    uint16_t conductionLength;
    std::vector<uint8_t> conductionData;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(
            Slave2BackendMessageId::CONDUCTION_DATA_MSG);
    }
};

class ResistanceDataMessage : public Message {
  public:
    uint16_t resistanceLength;
    std::vector<uint8_t> resistanceData;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(
            Slave2BackendMessageId::RESISTANCE_DATA_MSG);
    }
};

class ClipDataMessage : public Message {
  public:
    uint16_t clipData;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Slave2BackendMessageId::CLIP_DATA_MSG);
    }
};

} // namespace Slave2Backend

// Backend2Master 消息类
namespace Backend2Master {

class SlaveConfigMessage : public Message {
  public:
    struct SlaveInfo {
        uint32_t id;
        uint8_t conductionNum;
        uint8_t resistanceNum;
        uint8_t clipMode;
        uint16_t clipStatus;
    };

    uint8_t slaveNum;
    std::vector<SlaveInfo> slaves;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Backend2MasterMessageId::SLAVE_CFG_MSG);
    }
};

class ModeConfigMessage : public Message {
  public:
    uint8_t mode;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Backend2MasterMessageId::MODE_CFG_MSG);
    }
};

class RstMessage : public Message {
  public:
    struct SlaveRstInfo {
        uint32_t id;
        uint8_t lock;
        uint16_t clipStatus;
    };

    uint8_t slaveNum;
    std::vector<SlaveRstInfo> slaves;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Backend2MasterMessageId::RST_MSG);
    }
};

class CtrlMessage : public Message {
  public:
    uint8_t runningStatus;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Backend2MasterMessageId::CTRL_MSG);
    }
};

class PingCtrlMessage : public Message {
  public:
    uint8_t pingMode;
    uint16_t pingCount;
    uint16_t interval;
    uint32_t destinationId;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Backend2MasterMessageId::PING_CTRL_MSG);
    }
};

class DeviceListReqMessage : public Message {
  public:
    uint8_t reserve;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(
            Backend2MasterMessageId::DEVICE_LIST_REQ_MSG);
    }
};

} // namespace Backend2Master

// Master2Backend 消息类
namespace Master2Backend {

class SlaveConfigResponseMessage : public Message {
  public:
    struct SlaveInfo {
        uint32_t id;
        uint8_t conductionNum;
        uint8_t resistanceNum;
        uint8_t clipMode;
        uint16_t clipStatus;
    };

    uint8_t status;
    uint8_t slaveNum;
    std::vector<SlaveInfo> slaves;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2BackendMessageId::SLAVE_CFG_RSP_MSG);
    }
};

class ModeConfigResponseMessage : public Message {
  public:
    uint8_t status;
    uint8_t mode;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2BackendMessageId::MODE_CFG_RSP_MSG);
    }
};

class RstResponseMessage : public Message {
  public:
    struct SlaveRstInfo {
        uint32_t id;
        uint8_t lock;
        uint16_t clipStatus;
    };

    uint8_t status;
    uint8_t slaveNum;
    std::vector<SlaveRstInfo> slaves;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2BackendMessageId::RST_RSP_MSG);
    }
};

class CtrlResponseMessage : public Message {
  public:
    uint8_t status;
    uint8_t runningStatus;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2BackendMessageId::CTRL_RSP_MSG);
    }
};

class PingResponseMessage : public Message {
  public:
    uint8_t pingMode;
    uint16_t totalCount;
    uint16_t successCount;
    uint32_t destinationId;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2BackendMessageId::PING_RES_MSG);
    }
};

class DeviceListResponseMessage : public Message {
  public:
    struct DeviceInfo {
        uint32_t deviceId;
        uint8_t shortId;
        uint8_t online;
        uint8_t versionMajor;
        uint8_t versionMinor;
        uint16_t versionPatch;
    };

    uint8_t deviceCount;
    std::vector<DeviceInfo> devices;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(
            Master2BackendMessageId::DEVICE_LIST_RSP_MSG);
    }
};

} // namespace Master2Backend

// 协议处理器类
class ProtocolProcessor {
  public:
    ProtocolProcessor();
    ~ProtocolProcessor();

    // 设置最大传输单元大小 (MTU)
    void setMTU(size_t mtu) { mtu_ = mtu; }
    size_t getMTU() const { return mtu_; }

    // 打包Master2Slave消息 (支持自动分片)
    std::vector<std::vector<uint8_t>>
    packMaster2SlaveMessage(uint32_t destinationId, const Message &message);

    // 打包Slave2Master消息 (支持自动分片)
    std::vector<std::vector<uint8_t>>
    packSlave2MasterMessage(uint32_t slaveId, const Message &message);

    // 打包Slave2Backend消息 (支持自动分片)
    std::vector<std::vector<uint8_t>>
    packSlave2BackendMessage(uint32_t slaveId, const DeviceStatus &deviceStatus,
                             const Message &message);

    // 打包Backend2Master消息 (支持自动分片)
    std::vector<std::vector<uint8_t>>
    packBackend2MasterMessage(const Message &message);

    // 打包Master2Backend消息 (支持自动分片)
    std::vector<std::vector<uint8_t>>
    packMaster2BackendMessage(const Message &message);

    // 兼容旧接口 - 单帧打包
    std::vector<uint8_t> packMaster2SlaveMessageSingle(
        uint32_t destinationId, const Message &message,
        uint8_t fragmentsSequence = 0, uint8_t moreFragmentsFlag = 0);

    std::vector<uint8_t>
    packSlave2MasterMessageSingle(uint32_t slaveId, const Message &message,
                                  uint8_t fragmentsSequence = 0,
                                  uint8_t moreFragmentsFlag = 0);

    std::vector<uint8_t> packSlave2BackendMessageSingle(
        uint32_t slaveId, const DeviceStatus &deviceStatus,
        const Message &message, uint8_t fragmentsSequence = 0,
        uint8_t moreFragmentsFlag = 0);

    std::vector<uint8_t>
    packBackend2MasterMessageSingle(const Message &message,
                                    uint8_t fragmentsSequence = 0,
                                    uint8_t moreFragmentsFlag = 0);

    std::vector<uint8_t>
    packMaster2BackendMessageSingle(const Message &message,
                                    uint8_t fragmentsSequence = 0,
                                    uint8_t moreFragmentsFlag = 0);

    // 处理接收到的原始数据 (支持粘包处理)
    void processReceivedData(const std::vector<uint8_t> &data);

    // 获取完整的已解析帧
    bool getNextCompleteFrame(Frame &frame);

    // 清空接收缓冲区
    void clearReceiveBuffer();

    // 解析单个帧
    bool parseFrame(const std::vector<uint8_t> &data, Frame &frame);

    // 根据Packet ID和Message ID创建对应的消息对象
    std::unique_ptr<Message> createMessage(PacketId packetId,
                                           uint8_t messageId);

    // 解析Master2Slave包
    bool parseMaster2SlavePacket(const std::vector<uint8_t> &payload,
                                 uint32_t &destinationId,
                                 std::unique_ptr<Message> &message);

    // 解析Slave2Master包
    bool parseSlave2MasterPacket(const std::vector<uint8_t> &payload,
                                 uint32_t &slaveId,
                                 std::unique_ptr<Message> &message);

    // 解析Slave2Backend包
    bool parseSlave2BackendPacket(const std::vector<uint8_t> &payload,
                                  uint32_t &slaveId, DeviceStatus &deviceStatus,
                                  std::unique_ptr<Message> &message);

    // 解析Backend2Master包
    bool parseBackend2MasterPacket(const std::vector<uint8_t> &payload,
                                   std::unique_ptr<Message> &message);

    // 解析Master2Backend包
    bool parseMaster2BackendPacket(const std::vector<uint8_t> &payload,
                                   std::unique_ptr<Message> &message);

  private:
    // 分片相关的私有结构
    struct FragmentInfo {
        uint8_t packetId;
        uint32_t sourceId;
        uint8_t totalFragments;
        std::map<uint8_t, std::vector<uint8_t>> fragments; // 序号 -> 分片数据
        uint32_t timestamp;                                // 用于超时处理

        bool isComplete() const { return fragments.size() == totalFragments; }
    };

    // 帧分片
    std::vector<std::vector<uint8_t>>
    fragmentFrame(const std::vector<uint8_t> &frameData);

    // 分片重组
    bool reassembleFragments(const Frame &frame,
                             std::vector<uint8_t> &completeFrame);

    // 从接收缓冲区中提取完整帧
    bool extractCompleteFrames();

    // 查找帧头
    size_t findFrameHeader(const std::vector<uint8_t> &buffer, size_t startPos);

    // 工具函数
    void writeUint16LE(std::vector<uint8_t> &buffer, uint16_t value);
    void writeUint32LE(std::vector<uint8_t> &buffer, uint32_t value);
    uint16_t readUint16LE(const std::vector<uint8_t> &buffer, size_t offset);
    uint32_t readUint32LE(const std::vector<uint8_t> &buffer, size_t offset);

    // 生成分片的唯一ID
    uint64_t generateFragmentId(uint8_t packetId);

    // 清理超时的分片
    void cleanupExpiredFragments();

  private:
    size_t mtu_;                         // 最大传输单元大小，默认100字节
    std::vector<uint8_t> receiveBuffer_; // 接收缓冲区
    std::queue<Frame> completeFrames_;   // 完整帧队列
    std::map<uint64_t, FragmentInfo> fragmentMap_; // 分片重组映射
    static constexpr uint32_t FRAGMENT_TIMEOUT_MS =
        5000;                                  // 分片超时时间（毫秒）
    static constexpr size_t DEFAULT_MTU = 100; // 默认MTU大小
    static constexpr size_t MAX_RECEIVE_BUFFER_SIZE =
        4096; // 最大接收缓冲区大小
};

} // namespace WhtsProtocol

#endif // WHTS_PROTOCOL_H