#include "src/WhtsProtocol.h"
#include <iostream>
#include <cassert>

using namespace WhtsProtocol;

// 快速验证基本功能
int main() {
    std::cout << "WHTS协议快速验证程序" << std::endl;
    std::cout << "===================" << std::endl;
    
    int tests = 0;
    int passed = 0;
    
    // 测试1: 基本对象创建
    std::cout << "测试1: 基本对象创建..." << std::flush;
    try {
        ProtocolProcessor processor;
        Frame frame;
        DeviceStatus status{};
        Master2Slave::SyncMessage msg;
        tests++;
        passed++;
        std::cout << " ✓" << std::endl;
    } catch (...) {
        tests++;
        std::cout << " ✗" << std::endl;
    }
    
    // 测试2: 消息序列化
    std::cout << "测试2: 消息序列化..." << std::flush;
    try {
        Master2Slave::SyncMessage msg;
        msg.mode = 1;
        msg.timestamp = 12345;
        
        auto data = msg.serialize();
        Master2Slave::SyncMessage msg2;
        bool result = msg2.deserialize(data);
        
        tests++;
        if (result && msg2.mode == msg.mode && msg2.timestamp == msg.timestamp) {
            passed++;
            std::cout << " ✓" << std::endl;
        } else {
            std::cout << " ✗" << std::endl;
        }
    } catch (...) {
        tests++;
        std::cout << " ✗" << std::endl;
    }
    
    // 测试3: 帧打包
    std::cout << "测试3: 帧打包解析..." << std::flush;
    try {
        ProtocolProcessor processor;
        Master2Slave::PingReqMessage msg;
        msg.sequenceNumber = 0x1234;
        msg.timestamp = 0x56789ABC;
        
        auto frameData = processor.packMaster2SlaveMessageSingle(0x11223344, msg);
        
        Frame frame;
        bool parseResult = processor.parseFrame(frameData, frame);
        
        tests++;
        if (parseResult && frame.isValid()) {
            passed++;
            std::cout << " ✓" << std::endl;
        } else {
            std::cout << " ✗" << std::endl;
        }
    } catch (...) {
        tests++;
        std::cout << " ✗" << std::endl;
    }
    
    // 测试4: 分片功能
    std::cout << "测试4: 分片功能..." << std::flush;
    try {
        ProtocolProcessor processor;
        processor.setMTU(50);
        
        Slave2Backend::ConductionDataMessage msg;
        msg.conductionLength = 100;
        msg.conductionData.resize(100, 0xAA);
        
        auto fragments = processor.packSlave2BackendMessage(0x12345678, DeviceStatus{}, msg);
        
        tests++;
        if (fragments.size() > 1) {
            passed++;
            std::cout << " ✓ (生成 " << fragments.size() << " 个分片)" << std::endl;
        } else {
            std::cout << " ✗" << std::endl;
        }
    } catch (...) {
        tests++;
        std::cout << " ✗" << std::endl;
    }
    
    // 测试5: 粘包处理
    std::cout << "测试5: 粘包处理..." << std::flush;
    try {
        ProtocolProcessor processor;
        
        Master2Slave::ShortIdAssignMessage msg1;
        msg1.shortId = 1;
        Master2Slave::ShortIdAssignMessage msg2;
        msg2.shortId = 2;
        
        auto frame1 = processor.packMaster2SlaveMessageSingle(0x1001, msg1);
        auto frame2 = processor.packMaster2SlaveMessageSingle(0x1002, msg2);
        
        // 创建粘包
        std::vector<uint8_t> stickyData;
        stickyData.insert(stickyData.end(), frame1.begin(), frame1.end());
        stickyData.insert(stickyData.end(), frame2.begin(), frame2.end());
        
        processor.processReceivedData(stickyData);
        
        Frame frame;
        int count = 0;
        while (processor.getNextCompleteFrame(frame)) {
            count++;
        }
        
        tests++;
        if (count == 2) {
            passed++;
            std::cout << " ✓ (分离出 " << count << " 个帧)" << std::endl;
        } else {
            std::cout << " ✗ (分离出 " << count << " 个帧，期望2个)" << std::endl;
        }
    } catch (...) {
        tests++;
        std::cout << " ✗" << std::endl;
    }
    
    // 测试6: 设备状态
    std::cout << "测试6: 设备状态..." << std::flush;
    try {
        DeviceStatus status{};
        status.colorSensor = true;
        status.batteryLowAlarm = true;
        
        uint16_t statusValue = status.toUint16();
        DeviceStatus status2;
        status2.fromUint16(statusValue);
        
        tests++;
        if (status2.colorSensor && status2.batteryLowAlarm) {
            passed++;
            std::cout << " ✓" << std::endl;
        } else {
            std::cout << " ✗" << std::endl;
        }
    } catch (...) {
        tests++;
        std::cout << " ✗" << std::endl;
    }
    
    // 显示结果
    std::cout << std::endl;
    std::cout << "验证结果: " << passed << "/" << tests << " 测试通过";
    
    if (passed == tests) {
        std::cout << " 🎉" << std::endl;
        std::cout << std::endl;
        std::cout << "✅ 基本功能验证通过！" << std::endl;
        std::cout << "建议运行完整验证: verify_protocol.bat" << std::endl;
        return 0;
    } else {
        std::cout << " ❌" << std::endl;
        std::cout << std::endl;
        std::cout << "❌ 发现问题，请检查代码！" << std::endl;
        return 1;
    }
} 