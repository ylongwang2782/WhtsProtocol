#include "WindowsUdpSocket.h"
#include <cstring>
#include <iostream>

namespace Platform {
namespace Windows {

WindowsUdpSocket::WindowsUdpSocket()
    : sock(INVALID_SOCKET), isInitialized(false), isBound(false),
      isNonBlocking(false) {
    memset(&localAddr, 0, sizeof(localAddr));
}

WindowsUdpSocket::~WindowsUdpSocket() { close(); }

bool WindowsUdpSocket::initializeWinsock() {
#ifdef _WIN32
    static bool wsaInitialized = false;
    if (!wsaInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "[ERROR] WindowsUdpSocket: WSAStartup failed"
                      << std::endl;
            return false;
        }
        wsaInitialized = true;
    }
#endif
    return true;
}

void WindowsUdpSocket::cleanupWinsock() {
#ifdef _WIN32
    // 注意：这里不调用WSACleanup()，因为可能有多个socket实例
    // WSACleanup应该在应用程序退出时调用
#endif
}

bool WindowsUdpSocket::initialize() {
    if (isInitialized) {
        return true;
    }

    if (!initializeWinsock()) {
        return false;
    }

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "[ERROR] WindowsUdpSocket: Socket creation failed"
                  << std::endl;
        return false;
    }

    isInitialized = true;
    return true;
}

bool WindowsUdpSocket::bind(const std::string &address, uint16_t port) {
    if (!isInitialized) {
        std::cerr << "[ERROR] WindowsUdpSocket: Socket not initialized"
                  << std::endl;
        return false;
    }

    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(port);

    if (address.empty()) {
        localAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        localAddr.sin_addr.s_addr = inet_addr(address.c_str());
        if (localAddr.sin_addr.s_addr == INADDR_NONE) {
            std::cerr << "[ERROR] WindowsUdpSocket: Invalid address: "
                      << address << std::endl;
            return false;
        }
    }

    if (::bind(sock, (sockaddr *)&localAddr, sizeof(localAddr)) ==
        SOCKET_ERROR) {
        int error = WSAGetLastError();
        std::cerr << "[ERROR] WindowsUdpSocket: Bind failed on "
                  << (address.empty() ? "0.0.0.0" : address) << ":" << port
                  << " - Error code: " << error << std::endl;
        return false;
    }

    isBound = true;
    localAddress = NetworkAddress(address.empty() ? "0.0.0.0" : address, port);
    return true;
}

bool WindowsUdpSocket::setBroadcast(bool enable) {
    if (!isInitialized) {
        return false;
    }

    int broadcast = enable ? 1 : 0;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&broadcast,
                   sizeof(broadcast)) == SOCKET_ERROR) {
        std::cerr << "[WARN] WindowsUdpSocket: Failed to set broadcast option"
                  << std::endl;
        return false;
    }
    return true;
}

bool WindowsUdpSocket::setNonBlocking(bool nonBlocking) {
    if (!isInitialized) {
        return false;
    }

#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR) {
        std::cerr << "[ERROR] WindowsUdpSocket: Failed to set non-blocking mode"
                  << std::endl;
        return false;
    }
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }

    if (nonBlocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(sock, F_SETFL, flags) == -1) {
        std::cerr << "[ERROR] WindowsUdpSocket: Failed to set non-blocking mode"
                  << std::endl;
        return false;
    }
#endif

    isNonBlocking = nonBlocking;
    return true;
}

sockaddr_in WindowsUdpSocket::createSockAddr(const NetworkAddress &addr) const {
    sockaddr_in sockAddr;
    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(addr.port);
    sockAddr.sin_addr.s_addr = inet_addr(addr.ip.c_str());
    return sockAddr;
}

NetworkAddress
WindowsUdpSocket::createNetworkAddress(const sockaddr_in &addr) const {
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ipStr, INET_ADDRSTRLEN);
    return NetworkAddress(std::string(ipStr), ntohs(addr.sin_port));
}

bool WindowsUdpSocket::sendTo(const std::vector<uint8_t> &data,
                              const NetworkAddress &targetAddr,
                              UdpSendCallback callback) {
    if (!isInitialized || data.empty()) {
        if (callback) {
            callback(false, 0);
        }
        return false;
    }

    sockaddr_in targetSockAddr = createSockAddr(targetAddr);

    int bytesSent = sendto(sock, reinterpret_cast<const char *>(data.data()),
                           static_cast<int>(data.size()), 0,
                           (sockaddr *)&targetSockAddr, sizeof(targetSockAddr));

    bool success = (bytesSent != SOCKET_ERROR);
    size_t actualBytesSent = success ? static_cast<size_t>(bytesSent) : 0;

    if (callback) {
        callback(success, actualBytesSent);
    }

    return success;
}

bool WindowsUdpSocket::broadcast(const std::vector<uint8_t> &data,
                                 uint16_t port, UdpSendCallback callback) {
    // 使用本地广播地址
    NetworkAddress broadcastAddr("127.255.255.255", port);
    return sendTo(data, broadcastAddr, callback);
}

int WindowsUdpSocket::receiveFrom(uint8_t *buffer, size_t bufferSize,
                                  NetworkAddress &senderAddr) {
    if (!isInitialized || !buffer || bufferSize == 0) {
        return -1;
    }

    sockaddr_in senderSockAddr;
    socklen_t senderAddrLen = sizeof(senderSockAddr);

    int bytesReceived = recvfrom(sock, reinterpret_cast<char *>(buffer),
                                 static_cast<int>(bufferSize), 0,
                                 (sockaddr *)&senderSockAddr, &senderAddrLen);

    if (bytesReceived == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            std::cerr << "[ERROR] WindowsUdpSocket: Receive failed - Error: "
                      << error << std::endl;
        }
        return -1;
    }

    senderAddr = createNetworkAddress(senderSockAddr);
    return bytesReceived;
}

void WindowsUdpSocket::setReceiveCallback(UdpReceiveCallback callback) {
    receiveCallback = callback;
}

void WindowsUdpSocket::startAsyncReceive() {
    // Windows原生socket不支持真正的异步接收
    // 这里只设置非阻塞模式，需要通过processEvents轮询
    setNonBlocking(true);
}

void WindowsUdpSocket::stopAsyncReceive() {
    // 停止异步接收，恢复阻塞模式
    setNonBlocking(false);
}

void WindowsUdpSocket::processEvents() {
    if (!isInitialized || !receiveCallback) {
        return;
    }

    // 简单的轮询接收
    uint8_t buffer[4096];
    NetworkAddress senderAddr;
    int bytesReceived = receiveFrom(buffer, sizeof(buffer), senderAddr);

    if (bytesReceived > 0) {
        std::vector<uint8_t> data(buffer, buffer + bytesReceived);
        receiveCallback(data, senderAddr);
    }
}

void WindowsUdpSocket::close() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    isInitialized = false;
    isBound = false;
    isNonBlocking = false;
    cleanupWinsock();
}

NetworkAddress WindowsUdpSocket::getLocalAddress() const {
    return localAddress;
}

bool WindowsUdpSocket::isOpen() const {
    return isInitialized && (sock != INVALID_SOCKET);
}

} // namespace Windows
} // namespace Platform