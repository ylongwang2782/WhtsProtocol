#pragma once

#include "WhtsProtocol.h"
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

namespace SlaveApp {

/**
 * 网络管理器类
 * 负责处理UDP套接字通信
 */
class NetworkManager {
  private:
    SOCKET sock;
    sockaddr_in serverAddr;
    sockaddr_in masterAddr;
    uint16_t port;
    uint32_t deviceId;
    WhtsProtocol::ProtocolProcessor processor;

  public:
    NetworkManager(uint16_t listenPort = 8081, uint32_t deviceId = 0x3732485B);
    ~NetworkManager();

    /**
     * 初始化网络连接
     * @return 是否成功初始化
     */
    bool initialize();

    /**
     * 设置为非阻塞模式
     */
    void setNonBlocking();

    /**
     * 接收数据
     * @param buffer 接收缓冲区
     * @param bufferSize 缓冲区大小
     * @param senderAddr 发送方地址
     * @return 接收到的字节数，-1表示没有数据或错误
     */
    int receiveData(char *buffer, size_t bufferSize, sockaddr_in &senderAddr);

    /**
     * 发送响应数据
     * @param data 要发送的数据
     */
    void sendResponse(const std::vector<uint8_t> &data);

    /**
     * 获取协议处理器的引用
     */
    WhtsProtocol::ProtocolProcessor &getProcessor() { return processor; }

    /**
     * 获取设备ID
     */
    uint32_t getDeviceId() const { return deviceId; }
};

} // namespace SlaveApp