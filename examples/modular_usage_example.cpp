#include <iostream>
#include <vector>

// 使用新的模块化头文件
#include "protocol/Common.h"
#include "protocol/DeviceStatus.h"
#include "protocol/Frame.h"
#include "protocol/messages/Message.h"
#include "protocol/utils/ByteUtils.h"

using namespace WhtsProtocol;

int main() {
    std::cout << "WhtsProtocol 模块化使用示例\n";
    std::cout << "============================\n\n";

    // 1. 使用公共定义
    std::cout << "1. 协议常量:\n";
    std::cout << "   帧分隔符1: 0x" << std::hex
              << static_cast<int>(FRAME_DELIMITER_1) << std::endl;
    std::cout << "   帧分隔符2: 0x" << std::hex
              << static_cast<int>(FRAME_DELIMITER_2) << std::endl;
    std::cout << "   广播ID: 0x" << std::hex << BROADCAST_ID << std::endl;
    std::cout << std::dec << std::endl;

    // 2. 使用设备状态
    std::cout << "2. 设备状态操作:\n";
    DeviceStatus status = {0};
    status.colorSensor = true;
    status.batteryLowAlarm = true;
    status.electromagneticLock1 = true;

    uint16_t statusValue = status.toUint16();
    std::cout << "   状态值: 0x" << std::hex << statusValue << std::endl;

    DeviceStatus status2;
    status2.fromUint16(statusValue);
    std::cout << "   颜色传感器: " << (status2.colorSensor ? "开启" : "关闭")
              << std::endl;
    std::cout << "   电池低电量: " << (status2.batteryLowAlarm ? "是" : "否")
              << std::endl;
    std::cout << std::dec << std::endl;

    // 3. 使用字节工具
    std::cout << "3. 字节工具操作:\n";
    std::vector<uint8_t> buffer;
    ByteUtils::writeUint32LE(buffer, 0x12345678);
    ByteUtils::writeUint16LE(buffer, 0xABCD);

    std::cout << "   写入数据: " << ByteUtils::bytesToHexString(buffer)
              << std::endl;

    uint32_t value32 = ByteUtils::readUint32LE(buffer, 0);
    uint16_t value16 = ByteUtils::readUint16LE(buffer, 4);
    std::cout << "   读取32位: 0x" << std::hex << value32 << std::endl;
    std::cout << "   读取16位: 0x" << std::hex << value16 << std::endl;
    std::cout << std::dec << std::endl;

    // 4. 使用帧结构
    std::cout << "4. 帧结构操作:\n";
    Frame frame;
    frame.packetId = static_cast<uint8_t>(PacketId::MASTER_TO_SLAVE);
    frame.fragmentsSequence = 0;
    frame.moreFragmentsFlag = 0;
    frame.payload = {0x10, 0x78, 0x56, 0x34, 0x12, 0x01, 0x02, 0x03};
    frame.packetLength = static_cast<uint16_t>(frame.payload.size());

    auto serialized = frame.serialize();
    std::cout << "   序列化帧: " << ByteUtils::bytesToHexString(serialized)
              << std::endl;

    Frame parsedFrame;
    bool success = Frame::deserialize(serialized, parsedFrame);
    std::cout << "   解析成功: " << (success ? "是" : "否") << std::endl;
    std::cout << "   包ID: 0x" << std::hex
              << static_cast<int>(parsedFrame.packetId) << std::endl;
    std::cout << "   载荷长度: " << std::dec << parsedFrame.packetLength
              << std::endl;
    std::cout << std::endl;

    // 5. 使用枚举
    std::cout << "5. 消息ID枚举:\n";
    auto syncMsgId = static_cast<uint8_t>(Master2SlaveMessageId::SYNC_MSG);
    auto pingMsgId = static_cast<uint8_t>(Master2SlaveMessageId::PING_REQ_MSG);
    std::cout << "   同步消息ID: 0x" << std::hex << static_cast<int>(syncMsgId)
              << std::endl;
    std::cout << "   Ping请求ID: 0x" << std::hex << static_cast<int>(pingMsgId)
              << std::endl;
    std::cout << std::dec << std::endl;

    std::cout << "模块化协议库使用示例完成！\n";
    std::cout << "\n优势:\n";
    std::cout << "- 只需包含需要的模块\n";
    std::cout << "- 编译时间更短\n";
    std::cout << "- 代码更清晰\n";
    std::cout << "- 便于维护和扩展\n";

    return 0;
}