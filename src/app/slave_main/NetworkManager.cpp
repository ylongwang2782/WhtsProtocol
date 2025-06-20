#include "NetworkManager.h"
#include "../Logger.h"
#include <iostream>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

namespace SlaveApp {

NetworkManager::NetworkManager(uint16_t listenPort, uint32_t deviceId)
    : port(listenPort), deviceId(deviceId), sock(INVALID_SOCKET) {}

NetworkManager::~NetworkManager() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

bool NetworkManager::initialize() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        Log::e("NetworkManager", "WSAStartup failed");
        return false;
    }
#endif

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        Log::e("NetworkManager", "Socket creation failed");
        return false;
    }

    // Enable broadcast reception for the socket (to receive wireless
    // broadcasts)
    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&broadcast,
                   sizeof(broadcast)) == SOCKET_ERROR) {
        Log::w("NetworkManager", "Failed to enable broadcast option");
    }

    // Configure server address (Slave listens on port 8081 for broadcasts)
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr =
        INADDR_ANY; // Listen on all interfaces for broadcasts
    serverAddr.sin_port = htons(port);

    // Configure master address (Master uses port 8080)
    masterAddr.sin_family = AF_INET;
    masterAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost
    masterAddr.sin_port = htons(8080);

    if (bind(sock, (sockaddr *)&serverAddr, sizeof(serverAddr)) ==
        SOCKET_ERROR) {
        Log::e("NetworkManager", "Bind failed");
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    }

    processor.setMTU(100);

    Log::i("NetworkManager",
           "Network initialized - Device ID: 0x%08X, listening on port %d",
           deviceId, port);
    Log::i("NetworkManager", "Master communication port: 8080");
    Log::i("NetworkManager", "Wireless broadcast reception enabled");

    return true;
}

void NetworkManager::setNonBlocking() {
    if (sock == INVALID_SOCKET) {
        return;
    }

#ifdef _WIN32
    u_long mode = 1; // 1为非阻塞
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

int NetworkManager::receiveData(char *buffer, size_t bufferSize,
                                sockaddr_in &senderAddr) {
    if (sock == INVALID_SOCKET) {
        return -1;
    }

    socklen_t senderAddrLen = sizeof(senderAddr);
    return recvfrom(sock, buffer, static_cast<int>(bufferSize), 0,
                    (sockaddr *)&senderAddr, &senderAddrLen);
}

void NetworkManager::sendResponse(const std::vector<uint8_t> &data) {
    if (sock == INVALID_SOCKET || data.empty()) {
        return;
    }

    // Send response to master on port 8080
    sendto(sock, reinterpret_cast<const char *>(data.data()),
           static_cast<int>(data.size()), 0, (sockaddr *)&masterAddr,
           sizeof(masterAddr));
}

} // namespace SlaveApp