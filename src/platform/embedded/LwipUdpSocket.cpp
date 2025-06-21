#include "LwipUdpSocket.h"
#include <cstring>
#include <iostream>

namespace Platform {
namespace Embedded {

#ifdef USE_LWIP

LwipUdpSocket::LwipUdpSocket()
    : conn(nullptr), isInitialized(false), isBound(false) {}

LwipUdpSocket::~LwipUdpSocket() { close(); }

bool LwipUdpSocket::initialize() {
    if (isInitialized) {
        return true;
    }

    conn = netconn_new(NETCONN_UDP);
    if (conn == nullptr) {
        std::cerr << "[ERROR] LwipUdpSocket: Failed to create UDP connection"
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
        bindAddr = stringToIpAddr(address);
    }

    err_t err = netconn_bind(conn, &bindAddr, port);
    if (err != ERR_OK) {
        std::cerr << "[ERROR] LwipUdpSocket: Bind failed, error: "
                  << static_cast<int>(err) << std::endl;
        return false;
    }

    isBound = true;
    localAddress = NetworkAddress(address.empty() ? "0.0.0.0" : address, port);
    return true;
}

bool LwipUdpSocket::setBroadcast(bool enable) {
    // LWIP默认支持广播
    return true;
}

bool LwipUdpSocket::setNonBlocking(bool nonBlocking) {
    if (!isInitialized) {
        return false;
    }

    if (nonBlocking) {
        netconn_set_nonblocking(conn, 1);
    } else {
        netconn_set_nonblocking(conn, 0);
    }
    return true;
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

    ip_addr_t destAddr = stringToIpAddr(targetAddr.ip);
    struct netbuf *buf = netbuf_new();
    if (buf == nullptr) {
        if (callback) {
            callback(false, 0);
        }
        return false;
    }

    void *payload = netbuf_alloc(buf, data.size());
    if (payload == nullptr) {
        netbuf_delete(buf);
        if (callback) {
            callback(false, 0);
        }
        return false;
    }

    memcpy(payload, data.data(), data.size());

    err_t err = netconn_sendto(conn, buf, &destAddr, targetAddr.port);
    netbuf_delete(buf);

    bool success = (err == ERR_OK);
    if (callback) {
        callback(success, success ? data.size() : 0);
    }

    return success;
}

bool LwipUdpSocket::broadcast(const std::vector<uint8_t> &data, uint16_t port,
                              UdpSendCallback callback) {
    NetworkAddress broadcastAddr("255.255.255.255", port);
    return sendTo(data, broadcastAddr, callback);
}

int LwipUdpSocket::receiveFrom(uint8_t *buffer, size_t bufferSize,
                               NetworkAddress &senderAddr) {
    if (!isInitialized || !buffer || bufferSize == 0) {
        return -1;
    }

    struct netbuf *buf;
    err_t err = netconn_recv(conn, &buf);
    if (err != ERR_OK) {
        return -1;
    }

    size_t len = netbuf_len(buf);
    if (len > bufferSize) {
        len = bufferSize;
    }

    netbuf_copy(buf, buffer, len);

    // 获取发送方地址
    ip_addr_t *addr;
    u16_t port;
    netbuf_fromaddr(buf, &addr, &port);
    senderAddr.ip = ipAddrToString(*addr);
    senderAddr.port = port;

    netbuf_delete(buf);
    return static_cast<int>(len);
}

void LwipUdpSocket::setReceiveCallback(UdpReceiveCallback callback) {
    receiveCallback = callback;
}

void LwipUdpSocket::startAsyncReceive() {
    // LWIP是事件驱动的，这里可以设置为非阻塞模式
    setNonBlocking(true);
}

void LwipUdpSocket::stopAsyncReceive() { setNonBlocking(false); }

void LwipUdpSocket::close() {
    if (conn != nullptr) {
        netconn_close(conn);
        netconn_delete(conn);
        conn = nullptr;
    }
    isInitialized = false;
    isBound = false;
}

NetworkAddress LwipUdpSocket::getLocalAddress() const { return localAddress; }

bool LwipUdpSocket::isOpen() const {
    return isInitialized && (conn != nullptr);
}

void LwipUdpSocket::processEvents() {
    if (!isInitialized || !receiveCallback) {
        return;
    }

    // 简单的轮询接收
    uint8_t buffer[1024];
    NetworkAddress senderAddr;
    int bytesReceived = receiveFrom(buffer, sizeof(buffer), senderAddr);

    if (bytesReceived > 0) {
        std::vector<uint8_t> data(buffer, buffer + bytesReceived);
        receiveCallback(data, senderAddr);
    }
}

ip_addr_t LwipUdpSocket::stringToIpAddr(const std::string &ipStr) {
    ip_addr_t addr;
    if (ipaddr_aton(ipStr.c_str(), &addr) == 0) {
        ip_addr_set_any(IP_IS_V4_VAL(addr), &addr);
    }
    return addr;
}

std::string LwipUdpSocket::ipAddrToString(const ip_addr_t &addr) {
    char ipStr[IPADDR_STRLEN_MAX];
    ipaddr_ntoa_r(&addr, ipStr, sizeof(ipStr));
    return std::string(ipStr);
}

#endif // USE_LWIP

} // namespace Embedded
} // namespace Platform