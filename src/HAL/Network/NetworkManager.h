#pragma once

#include "IUdpSocket.h"
#include <functional>
#include <memory>
#include <unordered_map>


namespace HAL {
namespace Network {

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
 * 跨平台网络管理器
 * 提供统一的网络通信接口，支持多个UDP套接字
 */
class NetworkManager {
  private:
    std::unique_ptr<IUdpSocketFactory> socketFactory;
    std::unordered_map<std::string, std::unique_ptr<IUdpSocket>> sockets;
    NetworkEventCallback eventCallback;
    bool isRunning;

    // 生成套接字ID
    std::string generateSocketId();

    // 内部事件处理
    void handleSocketReceive(const std::string &socketId,
                             const std::vector<uint8_t> &data,
                             const NetworkAddress &senderAddr);
    void handleSocketSend(const std::string &socketId, bool success,
                          size_t bytesSent);

  public:
    NetworkManager();
    ~NetworkManager();

    /**
     * 初始化网络管理器
     * @param factory UDP套接字工厂
     * @return 是否成功初始化
     */
    bool initialize(std::unique_ptr<IUdpSocketFactory> factory);

    /**
     * 创建UDP套接字
     * @param socketId 套接字ID（可选，为空则自动生成）
     * @return 套接字ID，失败返回空字符串
     */
    std::string createUdpSocket(const std::string &socketId = "");

    /**
     * 绑定套接字到指定地址和端口
     * @param socketId 套接字ID
     * @param address 绑定地址，空字符串表示绑定所有接口
     * @param port 绑定端口
     * @return 是否成功绑定
     */
    bool bindSocket(const std::string &socketId, const std::string &address,
                    uint16_t port);

    /**
     * 设置套接字广播模式
     * @param socketId 套接字ID
     * @param enable 是否启用广播
     * @return 是否成功设置
     */
    bool setSocketBroadcast(const std::string &socketId, bool enable);

    /**
     * 设置套接字非阻塞模式
     * @param socketId 套接字ID
     * @param nonBlocking 是否设置为非阻塞模式
     * @return 是否成功设置
     */
    bool setSocketNonBlocking(const std::string &socketId, bool nonBlocking);

    /**
     * 发送数据到指定地址
     * @param socketId 套接字ID
     * @param data 要发送的数据
     * @param targetAddr 目标地址
     * @return 是否成功发起发送
     */
    bool sendTo(const std::string &socketId, const std::vector<uint8_t> &data,
                const NetworkAddress &targetAddr);

    /**
     * 广播数据到指定端口
     * @param socketId 套接字ID
     * @param data 要发送的数据
     * @param port 目标端口
     * @return 是否成功发起广播
     */
    bool broadcast(const std::string &socketId,
                   const std::vector<uint8_t> &data, uint16_t port);

    /**
     * 接收数据（非阻塞）
     * @param socketId 套接字ID
     * @param buffer 接收缓冲区
     * @param bufferSize 缓冲区大小
     * @param senderAddr 发送方地址（输出参数）
     * @return 接收到的字节数，-1表示没有数据或错误
     */
    int receiveFrom(const std::string &socketId, uint8_t *buffer,
                    size_t bufferSize, NetworkAddress &senderAddr);

    /**
     * 关闭套接字
     * @param socketId 套接字ID
     * @return 是否成功关闭
     */
    bool closeSocket(const std::string &socketId);

    /**
     * 获取套接字本地地址
     * @param socketId 套接字ID
     * @return 本地地址，失败返回无效地址
     */
    NetworkAddress getSocketLocalAddress(const std::string &socketId) const;

    /**
     * 检查套接字是否已打开
     * @param socketId 套接字ID
     * @return 是否已打开
     */
    bool isSocketOpen(const std::string &socketId) const;

    /**
     * 设置网络事件回调
     * @param callback 事件回调函数
     */
    void setEventCallback(NetworkEventCallback callback);

    /**
     * 开始网络事件循环
     */
    void start();

    /**
     * 停止网络事件循环
     */
    void stop();

    /**
     * 处理网络事件（用于轮询模式）
     * 在主循环中调用以处理网络I/O事件
     */
    void processEvents();

    /**
     * 获取所有套接字ID列表
     */
    std::vector<std::string> getSocketIds() const;

    /**
     * 清理所有套接字
     */
    void cleanup();
};

} // namespace Network
} // namespace HAL