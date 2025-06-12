#include "WhtsProtocol.h"
#include <iostream>
#include <cassert>

using namespace WhtsProtocol;

void testFragmentation() {
    std::cout << "=== 测试帧分片功能 ===" << std::endl;
    
    ProtocolProcessor processor;
    processor.setMTU(50);  // 设置MTU为50字节
    
    // 创建一个大消息（会被分片）
    Master2Slave::SyncMessage syncMsg;
    syncMsg.mode = 1;
    syncMsg.timestamp = 12345678;
    
    // 生成大量数据让帧超过MTU
    std::cout << "原始消息大小较小，需要创建更大的消息来测试分片..." << std::endl;
    
    // 使用带数据的消息
    Slave2Backend::ConductionDataMessage conductionMsg;
    conductionMsg.conductionLength = 200;
    conductionMsg.conductionData.resize(200, 0xAA);  // 填充200字节数据
    
    // 打包消息（会自动分片）
    auto fragments = processor.packSlave2BackendMessage(0x12345678, DeviceStatus{}, conductionMsg);
    
    std::cout << "原始帧被分为 " << fragments.size() << " 个分片" << std::endl;
    
    // 显示每个分片的信息
    for (size_t i = 0; i < fragments.size(); ++i) {
        std::cout << "分片 " << i << ": 大小 " << fragments[i].size() << " 字节" << std::endl;
        
        // 解析分片头信息
        if (fragments[i].size() >= 7) {
            uint8_t fragSeq = fragments[i][3];
            uint8_t moreFlag = fragments[i][4];
            uint16_t payloadLen = fragments[i][5] | (fragments[i][6] << 8);
            
            std::cout << "  分片序号: " << (int)fragSeq 
                     << ", 更多分片: " << (moreFlag ? "是" : "否")
                     << ", 载荷长度: " << payloadLen << std::endl;
        }
    }
}

void testStickyPackets() {
    std::cout << "\n=== 测试粘包处理功能 ===" << std::endl;
    
    ProtocolProcessor processor;
    
    // 创建多个小消息
    Master2Slave::PingReqMessage ping1;
    ping1.sequenceNumber = 1;
    ping1.timestamp = 11111;
    
    Master2Slave::PingReqMessage ping2;
    ping2.sequenceNumber = 2;
    ping2.timestamp = 22222;
    
    // 分别打包
    auto frame1 = processor.packMaster2SlaveMessageSingle(0x1001, ping1);
    auto frame2 = processor.packMaster2SlaveMessageSingle(0x1002, ping2);
    
    std::cout << "帧1大小: " << frame1.size() << " 字节" << std::endl;
    std::cout << "帧2大小: " << frame2.size() << " 字节" << std::endl;
    
    // 模拟粘包：将两个帧连接在一起
    std::vector<uint8_t> stickyData;
    stickyData.insert(stickyData.end(), frame1.begin(), frame1.end());
    stickyData.insert(stickyData.end(), frame2.begin(), frame2.end());
    
    std::cout << "粘包数据总大小: " << stickyData.size() << " 字节" << std::endl;
    
    // 处理粘包数据
    processor.processReceivedData(stickyData);
    
    // 获取解析出的帧
    Frame receivedFrame;
    int frameCount = 0;
    while (processor.getNextCompleteFrame(receivedFrame)) {
        frameCount++;
        std::cout << "解析出帧 " << frameCount << ": " 
                 << "PacketId=" << (int)receivedFrame.packetId
                 << ", 载荷大小=" << receivedFrame.payload.size() << std::endl;
    }
    
    std::cout << "总共解析出 " << frameCount << " 个完整帧" << std::endl;
}

void testFragmentReassembly() {
    std::cout << "\n=== 测试分片重组功能 ===" << std::endl;
    
    ProtocolProcessor senderProcessor;
    ProtocolProcessor receiverProcessor;
    
    // 发送端设置小MTU
    senderProcessor.setMTU(30);
    
    // 创建大消息
    Slave2Backend::ResistanceDataMessage resistanceMsg;
    resistanceMsg.resistanceLength = 100;
    resistanceMsg.resistanceData.resize(100, 0x55);
    
    DeviceStatus status{};
    status.colorSensor = true;
    status.batteryLowAlarm = true;
    
    // 分片打包
    auto fragments = senderProcessor.packSlave2BackendMessage(0x87654321, status, resistanceMsg);
    std::cout << "发送端生成 " << fragments.size() << " 个分片" << std::endl;
    
    // 模拟网络传输：接收端分别接收每个分片
    for (size_t i = 0; i < fragments.size(); ++i) {
        std::cout << "接收分片 " << i << std::endl;
        receiverProcessor.processReceivedData(fragments[i]);
    }
    
    // 检查是否重组成功
    Frame reassembledFrame;
    if (receiverProcessor.getNextCompleteFrame(reassembledFrame)) {
        std::cout << "分片重组成功！" << std::endl;
        std::cout << "重组后帧载荷大小: " << reassembledFrame.payload.size() << " 字节" << std::endl;
        
        // 解析重组后的消息
        uint32_t slaveId;
        DeviceStatus receivedStatus;
        std::unique_ptr<Message> message;
        
        if (senderProcessor.parseSlave2BackendPacket(reassembledFrame.payload, slaveId, receivedStatus, message)) {
            std::cout << "消息解析成功！" << std::endl;
            std::cout << "从机ID: 0x" << std::hex << slaveId << std::dec << std::endl;
            std::cout << "设备状态: colorSensor=" << receivedStatus.colorSensor 
                     << ", batteryLowAlarm=" << receivedStatus.batteryLowAlarm << std::endl;
        }
    } else {
        std::cout << "分片重组失败！" << std::endl;
    }
}

void testMixedScenario() {
    std::cout << "\n=== 测试混合场景（粘包+分片） ===" << std::endl;
    
    ProtocolProcessor processor;
    processor.setMTU(40);
    
    // 创建一个小消息（不会分片）
    Master2Slave::ShortIdAssignMessage shortMsg;
    shortMsg.shortId = 5;
    auto smallFrame = processor.packMaster2SlaveMessageSingle(0x1111, shortMsg);
    
    // 创建一个大消息（会分片）
    Slave2Backend::ConductionDataMessage largeMsg;
    largeMsg.conductionLength = 80;
    largeMsg.conductionData.resize(80, 0xCC);
    auto largeFragments = processor.packSlave2BackendMessage(0x2222, DeviceStatus{}, largeMsg);
    
    // 模拟复杂的网络传输场景：
    // 1. 小帧和大帧的第一个分片粘在一起
    std::vector<uint8_t> mixedData1;
    mixedData1.insert(mixedData1.end(), smallFrame.begin(), smallFrame.end());
    mixedData1.insert(mixedData1.end(), largeFragments[0].begin(), largeFragments[0].end());
    
    // 2. 剩余分片也可能粘在一起
    std::vector<uint8_t> mixedData2;
    for (size_t i = 1; i < largeFragments.size(); ++i) {
        mixedData2.insert(mixedData2.end(), largeFragments[i].begin(), largeFragments[i].end());
    }
    
    std::cout << "发送混合数据包1: " << mixedData1.size() << " 字节" << std::endl;
    std::cout << "发送混合数据包2: " << mixedData2.size() << " 字节" << std::endl;
    
    // 接收端处理
    ProtocolProcessor receiver;
    receiver.processReceivedData(mixedData1);
    receiver.processReceivedData(mixedData2);
    
    // 检查接收结果
    Frame frame;
    int count = 0;
    while (receiver.getNextCompleteFrame(frame)) {
        count++;
        std::cout << "接收到完整帧 " << count << ": PacketId=" << (int)frame.packetId 
                 << ", 载荷大小=" << frame.payload.size() << std::endl;
    }
    
    std::cout << "总共接收到 " << count << " 个完整帧" << std::endl;
}

int main() {
    std::cout << "WHTS协议分片和粘包处理测试" << std::endl;
    std::cout << "==============================" << std::endl;
    
    try {
        testFragmentation();
        testStickyPackets();
        testFragmentReassembly();
        testMixedScenario();
        
        std::cout << "\n所有测试完成！" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 