# 跨平台网络抽象层设计文档

## 概述

本文档描述了为WhtsProtocol项目设计的跨平台网络抽象层，该抽象层提供统一的网络通信接口，支持Windows平台（使用原生socket/asio）和嵌入式平台（使用lwip）。

## 设计目标

1. **跨平台兼容性**: 支持Windows、Linux和嵌入式平台
2. **统一接口**: 提供一致的API，隐藏平台差异
3. **高性能**: 最小化抽象层开销
4. **易于移植**: 简化代码在不同平台间的移植工作
5. **可扩展性**: 支持未来添加新的网络协议栈

## 架构设计

### 核心组件

```
┌─────────────────────────────────────────────────────┐
│                 应用层代码                            │
├─────────────────────────────────────────────────────┤
│              NetworkManager                         │
│            (网络管理器统一接口)                        │
├─────────────────────────────────────────────────────┤
│              NetworkFactory                         │
│             (平台工厂选择器)                          │
├─────────────────────────────────────────────────────┤
│     IUdpSocket     │    IUdpSocketFactory           │
│    (UDP套接字接口)   │   (套接字工厂接口)               │
├─────────────────────────────────────────────────────┤
│  WindowsUdpSocket  │    LwipUdpSocket               │
│   (Windows实现)     │    (嵌入式实现)                 │
├─────────────────────────────────────────────────────┤
│   Windows Socket   │      lwip TCP/IP               │
│      API          │        Stack                   │
└─────────────────────────────────────────────────────┘
```

### 主要类说明

#### 1. IUdpSocket (UDP套接字抽象接口)
- 定义了UDP通信的统一接口
- 支持同步和异步操作
- 提供发送、接收、广播等功能

#### 2. NetworkManager (网络管理器)
- 管理多个UDP套接字
- 提供事件驱动的网络通信
- 统一的错误处理和事件回调

#### 3. NetworkFactory (网络工厂)
- 根据编译时配置自动选择平台实现
- 提供统一的创建接口

## 使用方法

### 基本使用流程

```cpp
#include "NetworkFactory.h"
#include "NetworkManager.h"

// 1. 创建网络管理器
auto networkManager = NetworkFactory::createNetworkManager();

// 2. 设置事件回调
networkManager->setEventCallback([](const NetworkEvent& event) {
    // 处理网络事件
});

// 3. 创建UDP套接字
std::string socketId = networkManager->createUdpSocket();

// 4. 绑定到端口
networkManager->bindSocket(socketId, "", 8080);

// 5. 启动网络管理器
networkManager->start();

// 6. 发送数据
std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
NetworkAddress target("192.168.1.100", 8081);
networkManager->sendTo(socketId, data, target);

// 7. 处理网络事件
while (running) {
    networkManager->processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
```

### 平台配置

#### Windows平台
```cmake
# CMakeLists.txt
# 默认使用Windows Socket API实现
# 未来可以添加ASIO支持
```

#### 嵌入式平台 (lwip)
```cmake
# CMakeLists.txt
set(USE_LWIP ON)
# 需要链接lwip库
find_package(lwip REQUIRED)
target_link_libraries(your_target PRIVATE lwip)
```

### 事件处理

网络管理器支持以下事件类型：

```cpp
enum class NetworkEventType {
    DATA_RECEIVED,      // 数据接收
    DATA_SENT,          // 数据发送完成
    CONNECTION_ERROR,   // 连接错误
    SOCKET_CLOSED       // 套接字关闭
};
```

事件回调示例：
```cpp
networkManager->setEventCallback([](const NetworkEvent& event) {
    switch (event.type) {
        case NetworkEventType::DATA_RECEIVED:
            // 处理接收到的数据
            processReceivedData(event.data, event.remoteAddr);
            break;
        case NetworkEventType::CONNECTION_ERROR:
            // 处理连接错误
            handleError(event.errorMessage);
            break;
        // ... 其他事件处理
    }
});
```

## 平台特定实现

### Windows实现 (WindowsUdpSocket)
- 使用Windows Socket API
- 支持IPv4 UDP通信
- 非阻塞I/O模式
- 广播支持

### 嵌入式实现 (LwipUdpSocket)
- 使用lwip协议栈
- 事件驱动的异步I/O
- 内存优化的数据处理
- 支持lwip的所有网络特性

## 编译配置

### CMake选项
```cmake
# 启用lwip支持（嵌入式平台）
option(USE_LWIP "Enable lwip support for embedded platforms" OFF)

# 未来可以添加的选项
option(USE_ASIO "Use ASIO instead of raw sockets on Windows" OFF)
option(ENABLE_IPV6 "Enable IPv6 support" OFF)
```

### 预处理器宏
- `USE_LWIP`: 启用lwip实现
- `_WIN32`: Windows平台检测
- `__linux__`: Linux平台检测

## 移植现有代码

### 从原生socket代码迁移

**原始代码:**
```cpp
// 旧的socket代码
SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
bind(sock, ...);
sendto(sock, data, size, 0, addr, addrlen);
```

**新的抽象层代码:**
```cpp
// 使用网络抽象层
auto networkManager = NetworkFactory::createNetworkManager();
std::string socketId = networkManager->createUdpSocket();
networkManager->bindSocket(socketId, address, port);
networkManager->sendTo(socketId, data, targetAddr);
```

### 迁移步骤

1. **替换socket创建**: 使用`NetworkManager::createUdpSocket()`
2. **替换绑定操作**: 使用`NetworkManager::bindSocket()`
3. **替换发送操作**: 使用`NetworkManager::sendTo()`或`broadcast()`
4. **替换接收操作**: 使用事件回调或`receiveFrom()`
5. **添加事件处理**: 实现`NetworkEventCallback`

## 性能考虑

### 内存管理
- 使用智能指针管理资源
- 避免不必要的数据拷贝
- lwip实现使用pbuf优化内存使用

### 线程安全
- NetworkManager是线程安全的
- 套接字操作使用适当的同步机制
- 事件回调在网络线程中执行

### 性能优化
- 批量处理网络事件
- 非阻塞I/O减少延迟
- 平台特定优化

## 扩展性

### 添加新的网络协议栈
1. 实现`IUdpSocket`接口
2. 创建对应的工厂类
3. 在`NetworkFactory`中添加平台支持
4. 更新CMake配置

### 添加TCP支持
1. 定义`ITcpSocket`接口
2. 实现平台特定的TCP套接字
3. 扩展`NetworkManager`支持TCP

## 示例项目

参考`src/HAL/Network/example_usage.cpp`文件，该文件展示了：
- 如何初始化网络管理器
- 如何创建和配置套接字
- 如何发送和接收数据
- 如何处理网络事件

## 故障排除

### 常见问题

1. **编译错误**: 确保包含正确的头文件和链接库
2. **运行时错误**: 检查网络权限和防火墙设置
3. **性能问题**: 调整事件处理频率和缓冲区大小

### 调试技巧
- 启用详细日志输出
- 使用网络抓包工具验证数据传输
- 检查平台特定的网络配置

## 未来规划

1. **ASIO集成**: 在Windows平台支持ASIO异步I/O
2. **IPv6支持**: 扩展协议支持
3. **SSL/TLS**: 添加安全传输层
4. **性能优化**: 零拷贝数据传输
5. **更多平台**: 支持macOS、FreeBSD等

## 总结

这个跨平台网络抽象层为WhtsProtocol项目提供了：
- 统一的网络编程接口
- 简化的平台移植过程
- 高性能的网络通信能力
- 良好的可扩展性和维护性

通过使用这个抽象层，开发者可以编写一次代码，在多个平台上运行，大大提高了开发效率和代码的可维护性。 