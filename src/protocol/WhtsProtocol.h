#ifndef WHTS_PROTOCOL_H
#define WHTS_PROTOCOL_H

// 包含所有子模块
#include "Common.h"
#include "DeviceStatus.h"
#include "Frame.h"
#include "messages/Backend2Master.h"
#include "messages/Master2Backend.h"
#include "messages/Master2Slave.h"
#include "messages/Message.h"
#include "messages/Slave2Backend.h"
#include "messages/Slave2Master.h"
#include "utils/ByteUtils.h"

// 其他消息类型头文件将在后续添加
// #include "messages/Slave2Master.h"
// #include "messages/Backend2Master.h"
// #include "messages/Master2Backend.h"
// #include "messages/Slave2Backend.h"

#include <map>
#include <memory>
#include <queue>

namespace WhtsProtocol {

// 前向声明
class ProtocolProcessor;

// 协议处理器类 - 简化版本，完整版本需要单独的文件
class ProtocolProcessor {
  public:
    ProtocolProcessor();
    ~ProtocolProcessor();

    // 设置最大传输单元大小 (MTU)
    void setMTU(size_t mtu) { mtu_ = mtu; }
    size_t getMTU() const { return mtu_; }

    // 核心功能接口
    std::vector<std::vector<uint8_t>>
    packMaster2SlaveMessage(uint32_t destinationId, const Message &message);

    void processReceivedData(const std::vector<uint8_t> &data);
    bool getNextCompleteFrame(Frame &frame);
    void clearReceiveBuffer();

    // 解析接口
    bool parseMaster2SlavePacket(const std::vector<uint8_t> &payload,
                                 uint32_t &destinationId,
                                 std::unique_ptr<Message> &message);

    bool parseSlave2MasterPacket(const std::vector<uint8_t> &payload,
                                 uint32_t &slaveId,
                                 std::unique_ptr<Message> &message);

    // 其他解析方法...

  private:
    size_t mtu_;
    std::vector<uint8_t> receiveBuffer_;
    std::queue<Frame> completeFrames_;

    static constexpr size_t DEFAULT_MTU = 100;
    static constexpr size_t MAX_RECEIVE_BUFFER_SIZE = 4096;
};

} // namespace WhtsProtocol

#endif // WHTS_PROTOCOL_H