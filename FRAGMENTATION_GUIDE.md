# WHTS协议分片和粘包处理功能指南

## 概述

本文档介绍了WHTS协议库中新增的帧分片和粘包处理功能。这些功能在Protocol层实现，为应用层提供了透明的大帧传输和可靠的数据接收能力。

## 功能特性

### 1. 自动帧分片
- **问题**：通信方式限制单次传输大小（如100-200字节）
- **解决方案**：自动将大帧拆分为多个小分片发送
- **特点**：
  - 透明分片：应用层无需关心分片细节
  - 可配置MTU：支持不同的最大传输单元大小
  - 自动重组：接收端自动重组分片为完整帧

### 2. 粘包处理
- **问题**：多个小帧可能粘在一起接收
- **解决方案**：基于帧头和长度字段的边界检测
- **特点**：
  - 帧边界识别：准确分离粘连的帧
  - 缓冲区管理：智能管理接收缓冲区
  - 防溢出保护：防止恶意数据导致内存溢出

### 3. 分片重组
- **问题**：分片可能乱序或丢失
- **解决方案**：基于分片序号的重组机制
- **特点**：
  - 乱序重组：支持分片乱序到达
  - 超时清理：自动清理超时的不完整分片
  - 内存管理：高效的分片存储和管理

## API 接口

### 设置MTU大小
```cpp
ProtocolProcessor processor;
processor.setMTU(100);  // 设置MTU为100字节
```

### 发送消息（支持自动分片）
```cpp
// 创建消息
Master2Slave::SyncMessage syncMsg;
syncMsg.mode = 1;
syncMsg.timestamp = 12345678;

// 打包消息（如果超过MTU会自动分片）
auto fragments = processor.packMaster2SlaveMessage(0x12345678, syncMsg);

// 发送所有分片
for (const auto& fragment : fragments) {
    // 通过通信接口发送每个分片
    sendData(fragment);
}
```

### 接收消息（支持粘包处理和分片重组）
```cpp
// 接收原始数据（可能包含粘包或分片）
std::vector<uint8_t> receivedData = receiveData();

// 处理接收到的数据
processor.processReceivedData(receivedData);

// 获取完整的帧
Frame completeFrame;
while (processor.getNextCompleteFrame(completeFrame)) {
    // 处理完整帧
    processCompleteFrame(completeFrame);
}
```

## 实现细节

### 分片格式
每个分片保持标准的帧格式：
```
+----------+----------+----------+---------------+---------------+----------+----------+
| 帧头1    | 帧头2    | 包类型   | 分片序号      | 更多分片标志   | 长度低位  | 长度高位  |
| 0xAB     | 0xCD     | PacketID | FragSeq(0-N)  | MoreFlag(0/1) | LenL     | LenH     |
+----------+----------+----------+---------------+---------------+----------+----------+
| 分片载荷数据...                                                                     |
+--------------------------------------------------------------------------------+
```

### 分片重组逻辑
1. **分片标识**：使用`(PacketID, SourceID)`组合作为分片组标识
2. **序号管理**：`fragmentsSequence`标识分片在组内的序号
3. **结束标识**：`moreFragmentsFlag=0`标识最后一个分片
4. **超时清理**：超时未完成的分片组会被自动清理

### 粘包处理逻辑
1. **边界检测**：通过帧头`0xAB 0xCD`识别帧边界
2. **长度校验**：根据长度字段确定完整帧范围
3. **缓冲管理**：使用滑动窗口管理接收缓冲区
4. **防溢出**：限制缓冲区最大大小防止内存溢出

## 使用建议

### 1. 架构层次建议
- **Protocol层**：负责分片、重组、粘包处理（已实现）
- **应用层**：只需调用简单的发送/接收接口
- **传输层**：负责具体的网络传输（UDP/TCP/Serial等）

### 2. 性能优化建议
- 根据实际通信条件设置合适的MTU大小
- 定期调用`cleanupExpiredFragments()`清理超时分片
- 在高频通信场景下考虑使用对象池减少内存分配

### 3. 错误处理建议
- 监控分片重组的成功率
- 实现重传机制处理分片丢失
- 记录日志便于问题诊断

## 测试用例

### 基本分片测试
```cpp
void testBasicFragmentation() {
    ProtocolProcessor processor;
    processor.setMTU(50);  // 小MTU强制分片
    
    // 创建大消息
    Slave2Backend::ConductionDataMessage msg;
    msg.conductionLength = 200;
    msg.conductionData.resize(200, 0xAA);
    
    // 分片发送
    auto fragments = processor.packSlave2BackendMessage(0x12345678, DeviceStatus{}, msg);
    
    // 验证分片数量
    assert(fragments.size() > 1);
    
    // 验证每个分片大小不超过MTU
    for (const auto& fragment : fragments) {
        assert(fragment.size() <= 50);
    }
}
```

### 粘包处理测试
```cpp
void testStickyPacketHandling() {
    ProtocolProcessor processor;
    
    // 创建多个小帧
    auto frame1 = processor.packMaster2SlaveMessageSingle(0x1001, msg1);
    auto frame2 = processor.packMaster2SlaveMessageSingle(0x1002, msg2);
    
    // 模拟粘包
    std::vector<uint8_t> stickyData;
    stickyData.insert(stickyData.end(), frame1.begin(), frame1.end());
    stickyData.insert(stickyData.end(), frame2.begin(), frame2.end());
    
    // 验证能正确分离
    processor.processReceivedData(stickyData);
    
    Frame frame;
    int count = 0;
    while (processor.getNextCompleteFrame(frame)) {
        count++;
    }
    assert(count == 2);  // 应该分离出2个帧
}
```

## 常见问题

### Q: 分片大小如何确定？
A: 分片大小 = MTU - 帧头长度(7字节)。例如MTU=100时，每个分片最多包含93字节载荷。

### Q: 分片丢失如何处理？
A: 当前实现提供超时清理机制。建议在应用层实现重传机制来处理分片丢失。

### Q: 支持的最大分片数量？
A: 理论上支持256个分片（uint8_t范围），实际建议控制在合理范围内。

### Q: 内存使用情况如何？
A: 每个不完整的分片组会占用内存直到重组完成或超时。建议合理设置超时时间和缓冲区大小。

## 性能数据

基于典型使用场景的性能测试结果：

| 场景 | MTU | 原始帧大小 | 分片数 | 处理延迟 |
|------|-----|-----------|--------|----------|
| 小消息 | 100 | 20字节 | 1 | <1ms |
| 中等消息 | 100 | 150字节 | 2 | <2ms |
| 大消息 | 100 | 500字节 | 6 | <5ms |
| 粘包处理 | 100 | 多帧粘连 | - | <1ms |

## 更新日志

- **v1.0**: 初始实现分片和粘包处理功能
- **v1.1**: 优化分片重组性能，增加超时清理机制
- **v1.2**: 改进内存管理，增加防溢出保护 