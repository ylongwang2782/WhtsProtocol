#pragma once

#include "WhtsProtocol.h"
#include <memory>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

using namespace WhtsProtocol;

// Command tracking for timeout and retry management
struct PendingCommand {
    uint32_t slaveId;
    std::unique_ptr<Message> command;
    sockaddr_in clientAddr;
    uint32_t timestamp;
    uint8_t retryCount;
    uint8_t maxRetries;

    PendingCommand(uint32_t id, std::unique_ptr<Message> cmd,
                   const sockaddr_in &addr, uint8_t maxRetry = 3)
        : slaveId(id), command(std::move(cmd)), clientAddr(addr), timestamp(0),
          retryCount(0), maxRetries(maxRetry) {}
};

// Ping session tracking
struct PingSession {
    uint32_t targetId;
    uint8_t pingMode;
    uint16_t totalCount;
    uint16_t currentCount;
    uint16_t successCount;
    uint16_t interval;
    uint32_t lastPingTime;
    sockaddr_in clientAddr;

    PingSession(uint32_t target, uint8_t mode, uint16_t total,
                uint16_t intervalMs, const sockaddr_in &addr)
        : targetId(target), pingMode(mode), totalCount(total), currentCount(0),
          successCount(0), interval(intervalMs), lastPingTime(0),
          clientAddr(addr) {}
};