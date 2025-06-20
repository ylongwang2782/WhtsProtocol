#include "LwipUdpSocket.h"
#include <cstring>
#include <iostream>


namespace HAL {
namespace Network {

#ifdef USE_LWIP

LwipUdpSocket::LwipUdpSocket()
    : udpPcb(nullptr), isInitialized(false), isBound(false) {}

LwipUdpSocket::~LwipUdpSocket() { close(); }

bool LwipUdpSocket::initialize() {
    if (isInitialized) {
        return true;
    }

    udpPcb = udp_new();
    if (udpPcb == nullptr) {
        std::cerr << "[ERROR] LwipUdpSocket: Failed to create UDP PCB"
                  << std::endl;
        return false;
    }

    isInitialized = true;
    return true;
}

bool LwipUdpSocket::bind(const std::string &address, uint16_t port) {
    if (!isInitialized) {
        std::cerr << "[ERROR] LwipUdpSocket: Socket not initialized"
                  << std::endl;
        return false;
    }

    ip_addr_t bindAddr;
    if (address.empty()) {
        ip_addr_set_any(IP_IS_V4_VAL(bindAddr), &bindAddr);
    } else {
        bindAddr = createIpAddr(address);
        if (ip_addr_isany(&bindAddr) && !address.empty()) {
            std::cerr << "[ERROR] LwipUdpSocket: Invalid address: " << address
                      << std::endl;
            return false;
        }
    }

    err_t err = udp_bind(udpPcb, &bindAddr, port);
    if (err != ERR_OK) {
        std::cerr << "[ERROR] LwipUdpSocket: Bind failed, error: "
                  << static_cast<int>(err) << std::endl;
        return false;
    }

    // 设置接收回调
    udp_recv(udpPcb, udpReceiveCallback, this);

    isBound = true;
    localAddress = NetworkAddress(address.empty() ? "0.0.0.0" : address, port);
    return true;
}

bool LwipUdpSocket::setBroadcast(bool enable) {
    if (!isInitialized) {
        return false;
    }

    // lwip默认支持广播，这里可以根据需要进行配置
    // 在某些lwip配置中可能需要设置SO_BROADCAST选项
    if (enable) {
        // 启用广播（lwip通常默认启用）
        udpPcb->so_options |= SOF_BROADCAST;
    } else {
        // 禁用广播
        udpPcb->so_options &= ~SOF_BROADCAST;
    }

    return true;
}

bool LwipUdpSocket::setNonBlocking(bool nonBlocking) {
    // lwip是事件驱动的，本质上是非阻塞的
    // 这个方法主要影响receiveFrom的行为
    return true;
}

ip_addr_t LwipUdpSocket::createIpAddr(const std::string &ipStr) const {
    ip_addr_t addr;
    if (ipaddr_aton(ipStr.c_str(), &addr) == 0) {
        // 解析失败，返回any地址
        ip_addr_set_any(IP_IS_V4_VAL(addr), &addr);
    }
    return addr;
}

NetworkAddress LwipUdpSocket::createNetworkAddress(const ip_addr_t *addr,
                                                   u16_t port) const {
    char ipStr[IPADDR_STRLEN_MAX];
    ipaddr_ntoa_r(addr, ipStr, sizeof(ipStr));
    return NetworkAddress(std::string(ipStr), port);
}

bool LwipUdpSocket::isValidIpAddress(const std::string &ip) const {
    ip_addr_t addr;
    return ipaddr_aton(ip.c_str(), &addr) != 0;
}

void LwipUdpSocket::udpReceiveCallback(void *arg, struct udp_pcb *pcb,
                                       struct pbuf *p, const ip_addr_t *addr,
                                       u16_t port) {
    LwipUdpSocket *socket = static_cast<LwipUdpSocket *>(arg);
    if (socket == nullptr || p == nullptr) {
        if (p != nullptr) {
            pbuf_free(p);
        }
        return;
    }

    // 提取数据
    std::vector<uint8_t> data(p->tot_len);
    pbuf_copy_partial(p, data.data(), p->tot_len, 0);

    // 创建发送方地址
    NetworkAddress senderAddr = socket->createNetworkAddress(addr, port);

    // 释放pbuf
    pbuf_free(p);

    // 如果有接收回调，直接调用
    if (socket->receiveCallback) {
        socket->receiveCallback(data, senderAddr);
    } else {
        // 否则放入队列
        std::lock_guard<std::mutex> lock(socket->queueMutex);
        socket->receiveQueue.emplace(data, senderAddr);
    }
}

bool LwipUdpSocket::sendTo(const std::vector<uint8_t> &data,
                           const NetworkAddress &targetAddr,
                           UdpSendCallback callback) {
    if (!isInitialized || data.empty()) {
        if (callback) {
            callback(false, 0);
        }
        return false;
    }

    // 创建目标地址
    ip_addr_t destAddr = createIpAddr(targetAddr.ip);

    // 分配pbuf
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, data.size(), PBUF_RAM);
    if (p == nullptr) {
        if (callback) {
            callback(false, 0);
        }
        return false;
    }

    // 复制数据到pbuf
    memcpy(p->payload, data.data(), data.size());

    // 发送数据
    err_t err = udp_sendto(udpPcb, p, &destAddr, targetAddr.port);

    // 释放pbuf
    pbuf_free(p);

    bool success = (err == ERR_OK);
    if (callback) {
        callback(success, success ? data.size() : 0);
    }

    return success;
}

bool LwipUdpSocket::broadcast(const std::vector<uint8_t> &data, uint16_t port,
                              UdpSendCallback callback) {
    // 使用广播地址
    NetworkAddress broadcastAddr("255.255.255.255", port);
    return sendTo(data, broadcastAddr, callback);
}

int LwipUdpSocket::receiveFrom(uint8_t *buffer, size_t bufferSize,
                               NetworkAddress &senderAddr) {
    if (!isInitialized || !buffer || bufferSize == 0) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(queueMutex);
    if (receiveQueue.empty()) {
        return 0; // 没有数据
    }

    // 获取队列中的第一个数据包
    ReceivedPacket packet = receiveQueue.front();
    receiveQueue.pop();

    // 复制数据到缓冲区
    size_t copySize = std::min(packet.data.size(), bufferSize);
    memcpy(buffer, packet.data.data(), copySize);
    senderAddr = packet.senderAddr;

    return static_cast<int>(copySize);
}

void LwipUdpSocket::setReceiveCallback(UdpReceiveCallback callback) {
    receiveCallback = callback;
}

void LwipUdpSocket::startAsyncReceive() {
    // lwip本身就是异步的，接收回调已经在bind时设置
}

void LwipUdpSocket::stopAsyncReceive() {
    // 移除接收回调
    if (udpPcb != nullptr) {
        udp_recv(udpPcb, nullptr, nullptr);
    }
}

void LwipUdpSocket::processEvents() {
    // lwip的事件处理通常在主循环中通过sys_check_timeouts()等函数处理
    // 这里可以处理队列中的数据或其他事件
    if (receiveCallback) {
        std::lock_guard<std::mutex> lock(queueMutex);
        while (!receiveQueue.empty()) {
            ReceivedPacket packet = receiveQueue.front();
            receiveQueue.pop();
            receiveCallback(packet.data, packet.senderAddr);
        }
    }
}

void LwipUdpSocket::close() {
    if (udpPcb != nullptr) {
        udp_remove(udpPcb);
        udpPcb = nullptr;
    }

    isInitialized = false;
    isBound = false;

    // 清空接收队列
    std::lock_guard<std::mutex> lock(queueMutex);
    while (!receiveQueue.empty()) {
        receiveQueue.pop();
    }
}

NetworkAddress LwipUdpSocket::getLocalAddress() const { return localAddress; }

bool LwipUdpSocket::isOpen() const {
    return isInitialized && (udpPcb != nullptr);
}

#endif // USE_LWIP

} // namespace Network
} // namespace HAL