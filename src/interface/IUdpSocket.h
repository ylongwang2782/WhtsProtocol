#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Interface {

/**
 * 网络地址结构
 */
struct NetworkAddress {
    std::string ip;
    uint16_t port;

    NetworkAddress(const std::string &address = "", uint16_t p = 0)
        : ip(address), port(p) {}

    bool isValid() const { return !ip.empty() && port > 0; }
};

/**
 * UDP套接字接收回调函数类型
 * @param data 接收到的数据
 * @param senderAddr 发送方地址
 */
using UdpReceiveCallback = std::function<void(
    const std::vector<uint8_t> &data, const NetworkAddress &senderAddr)>;

/**
 * UDP套接字发送完成回调函数类型
 * @param success 是否发送成功
 * @param bytesSent 发送的字节数
 */
using UdpSendCallback = std::function<void(bool success, size_t bytesSent)>;

/**
 * UDP套接字抽象接口
 * 提供跨平台的UDP通信能力
 */
class IUdpSocket {
  public:
    virtual ~IUdpSocket() = default;

    /**
     * 初始化套接字
     * @return 是否成功初始化
     */
    virtual bool initialize() = 0;

    /**
     * 绑定到指定地址和端口
     * @param address 绑定地址，空字符串表示绑定所有接口
     * @param port 绑定端口
     * @return 是否成功绑定
     */
    virtual bool bind(const std::string &address, uint16_t port) = 0;

    /**
     * 设置广播模式
     * @param enable 是否启用广播
     * @return 是否成功设置
     */
    virtual bool setBroadcast(bool enable) = 0;

    /**
     * 设置非阻塞模式
     * @param nonBlocking 是否设置为非阻塞模式
     * @return 是否成功设置
     */
    virtual bool setNonBlocking(bool nonBlocking) = 0;

    /**
     * 发送数据到指定地址
     * @param data 要发送的数据
     * @param targetAddr 目标地址
     * @param callback 发送完成回调（可选）
     * @return 是否成功发起发送
     */
    virtual bool sendTo(const std::vector<uint8_t> &data,
                        const NetworkAddress &targetAddr,
                        UdpSendCallback callback = nullptr) = 0;

    /**
     * 广播数据到指定端口
     * @param data 要发送的数据
     * @param port 目标端口
     * @param callback 发送完成回调（可选）
     * @return 是否成功发起广播
     */
    virtual bool broadcast(const std::vector<uint8_t> &data, uint16_t port,
                           UdpSendCallback callback = nullptr) = 0;

    /**
     * 接收数据（非阻塞）
     * @param buffer 接收缓冲区
     * @param bufferSize 缓冲区大小
     * @param senderAddr 发送方地址（输出参数）
     * @return 接收到的字节数，-1表示没有数据或错误
     */
    virtual int receiveFrom(uint8_t *buffer, size_t bufferSize,
                            NetworkAddress &senderAddr) = 0;

    /**
     * 设置接收回调函数（用于异步接收）
     * @param callback 接收回调函数
     */
    virtual void setReceiveCallback(UdpReceiveCallback callback) = 0;

    /**
     * 开始异步接收（如果平台支持）
     */
    virtual void startAsyncReceive() = 0;

    /**
     * 停止异步接收
     */
    virtual void stopAsyncReceive() = 0;

    /**
     * 关闭套接字
     */
    virtual void close() = 0;

    /**
     * 获取本地绑定地址
     */
    virtual NetworkAddress getLocalAddress() const = 0;

    /**
     * 检查套接字是否已打开
     */
    virtual bool isOpen() const = 0;

    /**
     * 处理网络事件（用于轮询模式）
     * 在主循环中调用以处理网络I/O事件
     */
    virtual void processEvents() = 0;
};

/**
 * UDP套接字工厂接口
 */
class IUdpSocketFactory {
  public:
    virtual ~IUdpSocketFactory() = default;

    /**
     * 创建UDP套接字实例
     * @return UDP套接字智能指针
     */
    virtual std::unique_ptr<IUdpSocket> createUdpSocket() = 0;
};

} // namespace Interface