#pragma once

#include "IUdpSocket.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Interface {

/**
 * 网络事件类型
 */
enum class NetworkEventType {
    DATA_RECEIVED,
    DATA_SENT,
    CONNECTION_ERROR,
    SOCKET_CLOSED
};

/**
 * 网络事件结构
 */
struct NetworkEvent {
    NetworkEventType type;
    std::string socketId;
    std::vector<uint8_t> data;
    NetworkAddress remoteAddr;
    std::string errorMessage;

    NetworkEvent(NetworkEventType t, const std::string &id = "")
        : type(t), socketId(id) {}
};

/**
 * 网络事件回调函数类型
 */
using NetworkEventCallback = std::function<void(const NetworkEvent &event)>;

/**
 * 网络管理器抽象接口
 * 提供统一的网络通信接口，支持多个UDP套接字
 */
class INetwork {
  public:
    virtual ~INetwork() = default;

    /**
     * 初始化网络管理器
     * @param factory UDP套接字工厂
     * @return 是否成功初始化
     */
    virtual bool initialize(std::unique_ptr<IUdpSocketFactory> factory) = 0;

    /**
     * 创建UDP套接字
     * @param socketId 套接字ID（可选，为空则自动生成）
     * @return 套接字ID，失败返回空字符串
     */
    virtual std::string createUdpSocket(const std::string &socketId = "") = 0;

    /**
     * 绑定套接字到指定地址和端口
     * @param socketId 套接字ID
     * @param address 绑定地址，空字符串表示绑定所有接口
     * @param port 绑定端口
     * @return 是否成功绑定
     */
    virtual bool bindSocket(const std::string &socketId,
                            const std::string &address, uint16_t port) = 0;

    /**
     * 设置套接字广播模式
     * @param socketId 套接字ID
     * @param enable 是否启用广播
     * @return 是否成功设置
     */
    virtual bool setSocketBroadcast(const std::string &socketId,
                                    bool enable) = 0;

    /**
     * 设置套接字非阻塞模式
     * @param socketId 套接字ID
     * @param nonBlocking 是否设置为非阻塞模式
     * @return 是否成功设置
     */
    virtual bool setSocketNonBlocking(const std::string &socketId,
                                      bool nonBlocking) = 0;

    /**
     * 发送数据到指定地址
     * @param socketId 套接字ID
     * @param data 要发送的数据
     * @param targetAddr 目标地址
     * @return 是否成功发起发送
     */
    virtual bool sendTo(const std::string &socketId,
                        const std::vector<uint8_t> &data,
                        const NetworkAddress &targetAddr) = 0;

    /**
     * 广播数据到指定端口
     * @param socketId 套接字ID
     * @param data 要发送的数据
     * @param port 目标端口
     * @return 是否成功发起广播
     */
    virtual bool broadcast(const std::string &socketId,
                           const std::vector<uint8_t> &data, uint16_t port) = 0;

    /**
     * 接收数据（非阻塞）
     * @param socketId 套接字ID
     * @param buffer 接收缓冲区
     * @param bufferSize 缓冲区大小
     * @param senderAddr 发送方地址（输出参数）
     * @return 接收到的字节数，-1表示没有数据或错误
     */
    virtual int receiveFrom(const std::string &socketId, uint8_t *buffer,
                            size_t bufferSize, NetworkAddress &senderAddr) = 0;

    /**
     * 关闭套接字
     * @param socketId 套接字ID
     * @return 是否成功关闭
     */
    virtual bool closeSocket(const std::string &socketId) = 0;

    /**
     * 获取套接字本地地址
     * @param socketId 套接字ID
     * @return 本地地址，失败返回无效地址
     */
    virtual NetworkAddress
    getSocketLocalAddress(const std::string &socketId) const = 0;

    /**
     * 检查套接字是否已打开
     * @param socketId 套接字ID
     * @return 是否已打开
     */
    virtual bool isSocketOpen(const std::string &socketId) const = 0;

    /**
     * 设置网络事件回调
     * @param callback 事件回调函数
     */
    virtual void setEventCallback(NetworkEventCallback callback) = 0;

    /**
     * 开始网络事件循环
     */
    virtual void start() = 0;

    /**
     * 停止网络事件循环
     */
    virtual void stop() = 0;

    /**
     * 处理网络事件（用于轮询模式）
     * 在主循环中调用以处理网络I/O事件
     */
    virtual void processEvents() = 0;

    /**
     * 获取所有套接字ID列表
     */
    virtual std::vector<std::string> getSocketIds() const = 0;

    /**
     * 清理所有套接字
     */
    virtual void cleanup() = 0;
};

} // namespace Interface