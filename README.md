# WhtsProtocol - 无线线束测试系统协议库

这是一个基于 Wireless Harness Test System Protocol V1.7 规范实现的 C++ 协议库，提供完整的消息打包和解析功能。

## 特性

- ✅ **完整的协议支持**: 实现了协议文档中定义的所有消息类型
- ✅ **嵌入式友好**: 使用标准C++14，适用于嵌入式环境
- ✅ **类型安全**: 强类型的消息定义，避免编程错误
- ✅ **自动字节序处理**: 自动处理小端序数据格式
- ✅ **帧分片支持**: 支持大数据包的分片传输
- ✅ **错误处理**: 完善的错误检查和处理机制
- ✅ **易于使用**: 简洁的API设计，便于集成

## 协议支持

### 支持的包类型
- Master2Slave (0x00) - 主机到从机
- Slave2Master (0x01) - 从机到主机  
- Backend2Master (0x02) - 上位机到主机
- Master2Backend (0x03) - 主机到上位机
- Slave2Backend (0x04) - 从机到上位机

### 支持的消息类型
- **同步消息**: 系统同步和模式切换
- **配置消息**: 导通检测、阻值检测、卡钉配置
- **数据消息**: 导通数据、阻值数据、卡钉数据
- **控制消息**: 复位、Ping测试、ID分配
- **状态消息**: 设备状态报告和响应

## 构建和安装

### 环境要求
- C++14 或更高版本
- CMake 3.10 或更高版本
- 支持的编译器: GCC, Clang, MSVC

### Windows 构建
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Linux 构建
```bash
mkdir build
cd build
cmake ..
make -j4
```

### 运行测试
```bash
./bin/WhtsProtocolTest
```

### 查看示例
```bash
./bin/WhtsProtocolExample
```

## 快速开始

### 1. 包含头文件
```cpp
#include "WhtsProtocol.h"
using namespace WhtsProtocol;
```

### 2. 创建协议处理器
```cpp
ProtocolProcessor processor;
```

### 3. 打包消息示例
```cpp
// 创建同步消息
Master2Slave::SyncMessage syncMsg;
syncMsg.mode = 0;  // 导通检测模式
syncMsg.timestamp = getCurrentTimestamp();

// 打包消息
uint32_t destinationId = 0x12345678;
auto packedData = processor.packMaster2SlaveMessage(destinationId, syncMsg);

// 通过串口/网络发送 packedData
```

### 4. 解析消息示例
```cpp
// 从串口/网络接收数据到 receivedData
std::vector<uint8_t> receivedData = /* 接收的数据 */;

// 解析帧
Frame frame;
if (processor.parseFrame(receivedData, frame)) {
    // 解析Master2Slave包
    uint32_t destinationId;
    std::unique_ptr<Message> message;
    if (processor.parseMaster2SlavePacket(frame.payload, destinationId, message)) {
        // 检查消息类型
        auto syncMsg = dynamic_cast<Master2Slave::SyncMessage*>(message.get());
        if (syncMsg) {
            std::cout << "收到同步消息，模式: " << (int)syncMsg->mode << std::endl;
        }
    }
}
```

## API 文档

### 核心类

#### `ProtocolProcessor`
协议处理器，提供消息打包和解析功能。

**主要方法：**
- `packMaster2SlaveMessage()` - 打包主机到从机消息
- `packSlave2MasterMessage()` - 打包从机到主机消息  
- `packSlave2BackendMessage()` - 打包从机到上位机消息
- `parseFrame()` - 解析接收到的帧
- `parseMaster2SlavePacket()` - 解析主机到从机包
- `parseSlave2MasterPacket()` - 解析从机到主机包
- `parseSlave2BackendPacket()` - 解析从机到上位机包

#### `Frame`
帧结构，包含协议帧的所有字段。

**字段：**
- `delimiter1, delimiter2` - 帧定界符 (0xAB, 0xCD)
- `packetId` - 包类型ID
- `fragmentsSequence` - 分片序号
- `moreFragmentsFlag` - 更多分片标志
- `packetLength` - 包长度
- `payload` - 包载荷

#### `DeviceStatus`
设备状态位结构，用于Slave2Backend消息。

**状态位：**
- `colorSensor` - 颜色传感器状态
- `sleeveLimit` - 限位开关状态  
- `electromagnetUnlockButton` - 电磁锁解锁按钮
- `batteryLowAlarm` - 电池低电量报警
- `pressureSensor` - 气压传感器
- `electromagneticLock1/2` - 电磁锁状态
- `accessory1/2` - 辅件状态

### 消息类

所有消息类都继承自 `Message` 基类，提供统一的接口：
- `serialize()` - 序列化为字节数组
- `deserialize()` - 从字节数组反序列化
- `getMessageId()` - 获取消息ID

#### Master2Slave 消息
- `SyncMessage` - 同步消息
- `ConductionConfigMessage` - 导通配置
- `ResistanceConfigMessage` - 阻值配置
- `ClipConfigMessage` - 卡钉配置
- `ReadConductionDataMessage` - 读取导通数据
- `ReadResistanceDataMessage` - 读取阻值数据
- `ReadClipDataMessage` - 读取卡钉数据
- `RstMessage` - 复位消息
- `PingReqMessage` - Ping请求
- `ShortIdAssignMessage` - 短ID分配

#### Slave2Master 消息
- `ConductionConfigResponseMessage` - 导通配置响应
- `ResistanceConfigResponseMessage` - 阻值配置响应
- `ClipConfigResponseMessage` - 卡钉配置响应
- `RstResponseMessage` - 复位响应
- `PingRspMessage` - Ping响应
- `AnnounceMessage` - 设备宣告
- `ShortIdConfirmMessage` - 短ID确认

#### Slave2Backend 消息
- `ConductionDataMessage` - 导通数据
- `ResistanceDataMessage` - 阻值数据
- `ClipDataMessage` - 卡钉数据

## 完整示例

### 主机端示例
```cpp
#include "WhtsProtocol.h"
#include <iostream>

int main() {
    ProtocolProcessor processor;
    
    // 配置从机导通检测
    Master2Slave::ConductionConfigMessage config;
    config.timeSlot = 1;
    config.interval = 100;  // 100ms
    config.totalConductionNum = 64;
    config.startConductionNum = 0;
    config.conductionNum = 32;
    
    // 发送给从机
    uint32_t slaveId = 0x12345678;
    auto data = processor.packMaster2SlaveMessage(slaveId, config);
    
    // 发送数据到串口/网络
    sendToSerial(data);
    
    return 0;
}
```

### 从机端示例
```cpp
#include "WhtsProtocol.h"
#include <iostream>

int main() {
    ProtocolProcessor processor;
    
    while (true) {
        // 从串口/网络接收数据
        auto receivedData = receiveFromSerial();
        
        Frame frame;
        if (processor.parseFrame(receivedData, frame)) {
            if (frame.packetId == static_cast<uint8_t>(PacketId::MASTER_TO_SLAVE)) {
                uint32_t destinationId;
                std::unique_ptr<Message> message;
                
                if (processor.parseMaster2SlavePacket(frame.payload, destinationId, message)) {
                    // 处理不同类型的消息
                    if (auto configMsg = dynamic_cast<Master2Slave::ConductionConfigMessage*>(message.get())) {
                        // 处理导通配置
                        std::cout << "收到导通配置，时隙: " << (int)configMsg->timeSlot << std::endl;
                        
                        // 发送响应
                        Slave2Master::ConductionConfigResponseMessage response;
                        response.status = 0;  // 成功
                        response.timeSlot = configMsg->timeSlot;
                        // ... 设置其他字段
                        
                        auto responseData = processor.packSlave2MasterMessage(getMySlaveId(), response);
                        sendToSerial(responseData);
                    }
                }
            }
        }
    }
    
    return 0;
}
```

## 集成到现有项目

### 静态库方式
1. 编译生成 `libWhtsProtocol.a`
2. 在项目中包含 `WhtsProtocol.h`
3. 链接静态库

### 源码集成方式
1. 将 `WhtsProtocol.h` 和 `WhtsProtocol.cpp` 复制到项目中
2. 添加到编译列表
3. 包含头文件即可使用

## 注意事项

1. **字节序**: 协议使用小端序，库会自动处理
2. **内存管理**: 使用智能指针管理消息对象，无需手动释放
3. **线程安全**: 类不是线程安全的，多线程使用时需要额外同步
4. **错误处理**: 所有解析函数返回bool表示成功/失败，使用前请检查返回值
5. **分片支持**: 大数据包会自动分片，接收端需要重组

## 许可证

本项目基于协议文档 "Wireless Harness Test System Protocol V1.7" 实现，请确保使用时遵循相关规范。

## 贡献

欢迎提交Issue和Pull Request来改进这个库。

## 版本历史

- v1.0.0 - 初始版本，实现完整的协议支持 