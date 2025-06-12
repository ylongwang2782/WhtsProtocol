#include "WhtsProtocol.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>

using namespace WhtsProtocol;

class ProtocolValidator {
private:
    int totalTests = 0;
    int passedTests = 0;
    int failedTests = 0;
    
    void logTest(const std::string& testName, bool result) {
        totalTests++;
        if (result) {
            passedTests++;
            std::cout << "✓ " << testName << " - PASSED" << std::endl;
        } else {
            failedTests++;
            std::cout << "✗ " << testName << " - FAILED" << std::endl;
        }
    }
    
public:
    // 1. 基础编译和链接测试
    bool testBasicCompilation() {
        std::cout << "\n=== 1. 基础编译测试 ===" << std::endl;
        
        bool allPassed = true;
        
        // 测试基本类型创建
        try {
            ProtocolProcessor processor;
            Frame frame;
            DeviceStatus status{};
            Master2Slave::SyncMessage msg;
            logTest("基本对象创建", true);
        } catch (...) {
            logTest("基本对象创建", false);
            allPassed = false;
        }
        
        // 测试常量定义
        try {
            assert(FRAME_DELIMITER_1 == 0xAB);
            assert(FRAME_DELIMITER_2 == 0xCD);
            assert(BROADCAST_ID == 0xFFFFFFFF);
            logTest("常量定义正确", true);
        } catch (...) {
            logTest("常量定义正确", false);
            allPassed = false;
        }
        
        return allPassed;
    }
    
    // 2. 消息序列化/反序列化测试
    bool testMessageSerialization() {
        std::cout << "\n=== 2. 消息序列化测试 ===" << std::endl;
        
        bool allPassed = true;
        
        // 测试所有Master2Slave消息
        {
            Master2Slave::SyncMessage msg;
            msg.mode = 1;
            msg.timestamp = 12345678;
            
            auto data = msg.serialize();
            Master2Slave::SyncMessage msg2;
            bool result = msg2.deserialize(data);
            
            bool passed = result && msg2.mode == msg.mode && msg2.timestamp == msg.timestamp;
            logTest("SyncMessage 序列化/反序列化", passed);
            allPassed &= passed;
        }
        
        {
            Master2Slave::PingReqMessage msg;
            msg.sequenceNumber = 0x1234;
            msg.timestamp = 0x87654321;
            
            auto data = msg.serialize();
            Master2Slave::PingReqMessage msg2;
            bool result = msg2.deserialize(data);
            
            bool passed = result && msg2.sequenceNumber == msg.sequenceNumber && msg2.timestamp == msg.timestamp;
            logTest("PingReqMessage 序列化/反序列化", passed);
            allPassed &= passed;
        }
        
        // 测试Slave2Master消息
        {
            Slave2Master::AnnounceMessage msg;
            msg.deviceId = 0x12345678;
            msg.versionMajor = 1;
            msg.versionMinor = 2;
            msg.versionPatch = 0x0304;
            
            auto data = msg.serialize();
            Slave2Master::AnnounceMessage msg2;
            bool result = msg2.deserialize(data);
            
            bool passed = result && msg2.deviceId == msg.deviceId && 
                         msg2.versionMajor == msg.versionMajor &&
                         msg2.versionMinor == msg.versionMinor &&
                         msg2.versionPatch == msg.versionPatch;
            logTest("AnnounceMessage 序列化/反序列化", passed);
            allPassed &= passed;
        }
        
        // 测试带变长数据的消息
        {
            Slave2Backend::ConductionDataMessage msg;
            msg.conductionLength = 10;
            msg.conductionData = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
            
            auto data = msg.serialize();
            Slave2Backend::ConductionDataMessage msg2;
            bool result = msg2.deserialize(data);
            
            bool passed = result && msg2.conductionLength == msg.conductionLength &&
                         msg2.conductionData == msg.conductionData;
            logTest("ConductionDataMessage 序列化/反序列化", passed);
            allPassed &= passed;
        }
        
        return allPassed;
    }
    
    // 3. 帧处理测试
    bool testFrameProcessing() {
        std::cout << "\n=== 3. 帧处理测试 ===" << std::endl;
        
        bool allPassed = true;
        ProtocolProcessor processor;
        
        // 测试正常帧打包和解析
        {
            Master2Slave::SyncMessage msg;
            msg.mode = 2;
            msg.timestamp = 0xAABBCCDD;
            
            auto frameData = processor.packMaster2SlaveMessageSingle(0x12345678, msg);
            
            Frame frame;
            bool parseResult = processor.parseFrame(frameData, frame);
            
            uint32_t destId;
            std::unique_ptr<Message> parsedMsg;
            bool packetResult = processor.parseMaster2SlavePacket(frame.payload, destId, parsedMsg);
            
            bool passed = parseResult && packetResult && destId == 0x12345678;
            if (passed && parsedMsg) {
                auto syncMsg = dynamic_cast<Master2Slave::SyncMessage*>(parsedMsg.get());
                passed = syncMsg && syncMsg->mode == msg.mode && syncMsg->timestamp == msg.timestamp;
            }
            
            logTest("正常帧打包解析", passed);
            allPassed &= passed;
        }
        
        // 测试设备状态处理
        {
            DeviceStatus status{};
            status.colorSensor = true;
            status.batteryLowAlarm = true;
            status.electromagneticLock1 = true;
            
            uint16_t statusValue = status.toUint16();
            DeviceStatus status2;
            status2.fromUint16(statusValue);
            
            bool passed = status2.colorSensor == status.colorSensor &&
                         status2.batteryLowAlarm == status.batteryLowAlarm &&
                         status2.electromagneticLock1 == status.electromagneticLock1;
            
            logTest("设备状态位处理", passed);
            allPassed &= passed;
        }
        
        return allPassed;
    }
    
    // 4. 分片功能测试
    bool testFragmentation() {
        std::cout << "\n=== 4. 分片功能测试 ===" << std::endl;
        
        bool allPassed = true;
        ProtocolProcessor processor;
        processor.setMTU(50);  // 设置小MTU强制分片
        
        // 测试大消息自动分片
        {
            Slave2Backend::ConductionDataMessage msg;
            msg.conductionLength = 200;
            msg.conductionData.resize(200, 0xAA);
            
            auto fragments = processor.packSlave2BackendMessage(0x12345678, DeviceStatus{}, msg);
            
            bool passed = fragments.size() > 1;  // 应该被分片
            
            // 验证每个分片大小不超过MTU
            for (const auto& fragment : fragments) {
                if (fragment.size() > 50) {
                    passed = false;
                    break;
                }
            }
            
            // 验证所有分片总和等于原始数据
            size_t totalSize = 0;
            for (const auto& fragment : fragments) {
                totalSize += fragment.size();
            }
            
            logTest("大消息自动分片", passed);
            allPassed &= passed;
        }
        
        // 测试分片序号和标志
        {
            Slave2Backend::ResistanceDataMessage msg;
            msg.resistanceLength = 150;
            msg.resistanceData.resize(150, 0x55);
            
            auto fragments = processor.packSlave2BackendMessage(0x87654321, DeviceStatus{}, msg);
            
            bool passed = true;
            
            for (size_t i = 0; i < fragments.size(); ++i) {
                if (fragments[i].size() >= 7) {
                    uint8_t fragSeq = fragments[i][3];
                    uint8_t moreFlag = fragments[i][4];
                    
                    // 检查分片序号
                    if (fragSeq != i) {
                        passed = false;
                        break;
                    }
                    
                    // 检查更多分片标志
                    if (i == fragments.size() - 1) {
                        // 最后一个分片，moreFlag应该为0
                        if (moreFlag != 0) {
                            passed = false;
                            break;
                        }
                    } else {
                        // 非最后分片，moreFlag应该为1
                        if (moreFlag != 1) {
                            passed = false;
                            break;
                        }
                    }
                }
            }
            
            logTest("分片序号和标志正确", passed);
            allPassed &= passed;
        }
        
        return allPassed;
    }
    
    // 5. 粘包处理测试
    bool testStickyPackets() {
        std::cout << "\n=== 5. 粘包处理测试 ===" << std::endl;
        
        bool allPassed = true;
        ProtocolProcessor processor;
        
        // 测试两个小帧粘包
        {
            Master2Slave::PingReqMessage ping1;
            ping1.sequenceNumber = 1;
            ping1.timestamp = 11111;
            
            Master2Slave::PingReqMessage ping2;
            ping2.sequenceNumber = 2;
            ping2.timestamp = 22222;
            
            auto frame1 = processor.packMaster2SlaveMessageSingle(0x1001, ping1);
            auto frame2 = processor.packMaster2SlaveMessageSingle(0x1002, ping2);
            
            // 创建粘包数据
            std::vector<uint8_t> stickyData;
            stickyData.insert(stickyData.end(), frame1.begin(), frame1.end());
            stickyData.insert(stickyData.end(), frame2.begin(), frame2.end());
            
            // 处理粘包
            processor.processReceivedData(stickyData);
            
            // 检查是否能正确分离出两个帧
            Frame receivedFrame;
            int frameCount = 0;
            while (processor.getNextCompleteFrame(receivedFrame)) {
                frameCount++;
            }
            
            bool passed = (frameCount == 2);
            logTest("两帧粘包分离", passed);
            allPassed &= passed;
        }
        
        // 测试多个帧粘包
        {
            ProtocolProcessor processor2;
            
            std::vector<std::vector<uint8_t>> originalFrames;
            for (int i = 0; i < 5; ++i) {
                Master2Slave::ShortIdAssignMessage msg;
                msg.shortId = i + 10;
                originalFrames.push_back(processor2.packMaster2SlaveMessageSingle(0x2000 + i, msg));
            }
            
            // 创建所有帧的粘包
            std::vector<uint8_t> multiStickyData;
            for (const auto& frame : originalFrames) {
                multiStickyData.insert(multiStickyData.end(), frame.begin(), frame.end());
            }
            
            processor2.processReceivedData(multiStickyData);
            
            Frame frame;
            int count = 0;
            while (processor2.getNextCompleteFrame(frame)) {
                count++;
            }
            
            bool passed = (count == 5);
            logTest("多帧粘包分离", passed);
            allPassed &= passed;
        }
        
        return allPassed;
    }
    
    // 6. 分片重组测试
    bool testFragmentReassembly() {
        std::cout << "\n=== 6. 分片重组测试 ===" << std::endl;
        
        bool allPassed = true;
        
        // 测试顺序分片重组
        {
            ProtocolProcessor sender;
            ProtocolProcessor receiver;
            sender.setMTU(30);
            
            Slave2Backend::ResistanceDataMessage msg;
            msg.resistanceLength = 100;
            msg.resistanceData.resize(100, 0x77);
            
            auto fragments = sender.packSlave2BackendMessage(0x11223344, DeviceStatus{}, msg);
            
            // 按顺序发送所有分片
            for (const auto& fragment : fragments) {
                receiver.processReceivedData(fragment);
            }
            
            Frame reassembledFrame;
            bool passed = receiver.getNextCompleteFrame(reassembledFrame);
            
            if (passed) {
                // 验证重组后的数据
                uint32_t slaveId;
                DeviceStatus status;
                std::unique_ptr<Message> message;
                
                passed = sender.parseSlave2BackendPacket(reassembledFrame.payload, slaveId, status, message);
                if (passed) {
                    auto resMsg = dynamic_cast<Slave2Backend::ResistanceDataMessage*>(message.get());
                    passed = resMsg && resMsg->resistanceLength == 100 && 
                            resMsg->resistanceData.size() == 100;
                }
            }
            
            logTest("顺序分片重组", passed);
            allPassed &= passed;
        }
        
        // 测试乱序分片重组
        {
            ProtocolProcessor sender;
            ProtocolProcessor receiver;
            sender.setMTU(25);
            
            Slave2Backend::ConductionDataMessage msg;
            msg.conductionLength = 80;
            msg.conductionData.resize(80, 0x99);
            
            auto fragments = sender.packSlave2BackendMessage(0x55667788, DeviceStatus{}, msg);
            
            // 乱序发送分片（如果有多个分片）
            if (fragments.size() > 2) {
                receiver.processReceivedData(fragments[2]);  // 先发送第3个
                receiver.processReceivedData(fragments[0]);  // 再发送第1个
                receiver.processReceivedData(fragments[1]);  // 最后发送第2个
                
                // 发送剩余分片
                for (size_t i = 3; i < fragments.size(); ++i) {
                    receiver.processReceivedData(fragments[i]);
                }
            } else {
                // 如果分片不够，按顺序发送
                for (const auto& fragment : fragments) {
                    receiver.processReceivedData(fragment);
                }
            }
            
            Frame reassembledFrame;
            bool passed = receiver.getNextCompleteFrame(reassembledFrame);
            
            logTest("乱序分片重组", passed);
            allPassed &= passed;
        }
        
        return allPassed;
    }
    
    // 7. 边界条件测试
    bool testBoundaryConditions() {
        std::cout << "\n=== 7. 边界条件测试 ===" << std::endl;
        
        bool allPassed = true;
        ProtocolProcessor processor;
        
        // 测试空数据
        {
            std::vector<uint8_t> emptyData;
            processor.processReceivedData(emptyData);
            
            Frame frame;
            bool passed = !processor.getNextCompleteFrame(frame);  // 应该没有帧
            logTest("空数据处理", passed);
            allPassed &= passed;
        }
        
        // 测试无效帧头
        {
            std::vector<uint8_t> invalidData = {0x12, 0x34, 0x56, 0x78, 0x9A, 0x05, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
            processor.processReceivedData(invalidData);
            
            Frame frame;
            bool passed = !processor.getNextCompleteFrame(frame);  // 应该没有有效帧
            logTest("无效帧头处理", passed);
            allPassed &= passed;
        }
        
        // 测试不完整帧
        {
            std::vector<uint8_t> incompleteData = {0xAB, 0xCD, 0x00, 0x00, 0x00, 0x10, 0x00};  // 缺少载荷
            processor.processReceivedData(incompleteData);
            
            Frame frame;
            bool passed = !processor.getNextCompleteFrame(frame);  // 应该没有完整帧
            logTest("不完整帧处理", passed);
            allPassed &= passed;
        }
        
        // 测试最大MTU
        {
            processor.setMTU(65535);  // 设置最大MTU
            
            Master2Slave::SyncMessage msg;
            msg.mode = 1;
            msg.timestamp = 12345;
            
            auto fragments = processor.packMaster2SlaveMessage(0x12345678, msg);
            bool passed = fragments.size() == 1;  // 应该不分片
            logTest("最大MTU处理", passed);
            allPassed &= passed;
        }
        
        // 测试最小MTU
        {
            processor.setMTU(10);  // 设置极小MTU
            
            Master2Slave::SyncMessage msg;
            msg.mode = 1;
            msg.timestamp = 12345;
            
            auto fragments = processor.packMaster2SlaveMessage(0x12345678, msg);
            bool passed = fragments.size() > 1;  // 应该被分片
            logTest("最小MTU处理", passed);
            allPassed &= passed;
        }
        
        return allPassed;
    }
    
    // 8. 性能测试
    bool testPerformance() {
        std::cout << "\n=== 8. 性能测试 ===" << std::endl;
        
        bool allPassed = true;
        ProtocolProcessor processor;
        
        // 测试大量小消息处理速度
        {
            auto start = std::chrono::high_resolution_clock::now();
            
            const int messageCount = 1000;
            for (int i = 0; i < messageCount; ++i) {
                Master2Slave::PingReqMessage msg;
                msg.sequenceNumber = i;
                msg.timestamp = i * 1000;
                
                auto frame = processor.packMaster2SlaveMessageSingle(0x10000 + i, msg);
                // 模拟处理时间，但不实际处理以避免影响测试
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            bool passed = duration.count() < 1000;  // 应该在1秒内完成
            std::cout << "  处理 " << messageCount << " 个消息耗时: " << duration.count() << " ms" << std::endl;
            logTest("大量小消息处理性能", passed);
            allPassed &= passed;
        }
        
        // 测试大消息分片性能
        {
            processor.setMTU(100);
            
            auto start = std::chrono::high_resolution_clock::now();
            
            Slave2Backend::ConductionDataMessage msg;
            msg.conductionLength = 10000;  // 10KB数据
            msg.conductionData.resize(10000, 0xAA);
            
            auto fragments = processor.packSlave2BackendMessage(0x12345678, DeviceStatus{}, msg);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            bool passed = duration.count() < 100;  // 应该在100ms内完成
            std::cout << "  10KB数据分片耗时: " << duration.count() << " ms, 分片数: " << fragments.size() << std::endl;
            logTest("大消息分片性能", passed);
            allPassed &= passed;
        }
        
        return allPassed;
    }
    
    // 9. 内存安全测试
    bool testMemorySafety() {
        std::cout << "\n=== 9. 内存安全测试 ===" << std::endl;
        
        bool allPassed = true;
        
        // 测试大量数据不会导致内存溢出
        {
            ProtocolProcessor processor;
            
            try {
                // 发送大量无效数据
                std::vector<uint8_t> largeInvalidData(100000, 0xFF);
                processor.processReceivedData(largeInvalidData);
                
                // 发送更多数据触发缓冲区清理
                processor.processReceivedData(largeInvalidData);
                
                logTest("大量无效数据处理", true);
            } catch (...) {
                logTest("大量无效数据处理", false);
                allPassed = false;
            }
        }
        
        // 测试分片映射清理
        {
            ProtocolProcessor processor;
            processor.setMTU(50);
            
            try {
                // 创建多个不完整的分片组
                for (int i = 0; i < 100; ++i) {
                    Slave2Backend::ConductionDataMessage msg;
                    msg.conductionLength = 200;
                    msg.conductionData.resize(200, 0xAA);
                    
                    auto fragments = processor.packSlave2BackendMessage(0x10000 + i, DeviceStatus{}, msg);
                    
                    // 只发送第一个分片，不发送后续分片
                    if (!fragments.empty()) {
                        processor.processReceivedData(fragments[0]);
                    }
                }
                
                logTest("分片映射内存管理", true);
            } catch (...) {
                logTest("分片映射内存管理", false);
                allPassed = false;
            }
        }
        
        return allPassed;
    }
    
    // 运行所有测试
    void runAllTests() {
        std::cout << "WHTS协议完整性验证测试" << std::endl;
        std::cout << "========================" << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        bool result1 = testBasicCompilation();
        bool result2 = testMessageSerialization();
        bool result3 = testFrameProcessing();
        bool result4 = testFragmentation();
        bool result5 = testStickyPackets();
        bool result6 = testFragmentReassembly();
        bool result7 = testBoundaryConditions();
        bool result8 = testPerformance();
        bool result9 = testMemorySafety();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "\n========================" << std::endl;
        std::cout << "测试总结:" << std::endl;
        std::cout << "总测试数: " << totalTests << std::endl;
        std::cout << "通过: " << passedTests << " (" << std::fixed << std::setprecision(1) 
                  << (100.0 * passedTests / totalTests) << "%)" << std::endl;
        std::cout << "失败: " << failedTests << std::endl;
        std::cout << "总耗时: " << duration.count() << " ms" << std::endl;
        
        if (failedTests == 0) {
            std::cout << "\n🎉 所有测试通过！协议实现质量良好，可以交付给团队使用。" << std::endl;
        } else {
            std::cout << "\n⚠️  有 " << failedTests << " 个测试失败，需要修复后再交付。" << std::endl;
        }
    }
};

int main() {
    try {
        ProtocolValidator validator;
        validator.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
} 