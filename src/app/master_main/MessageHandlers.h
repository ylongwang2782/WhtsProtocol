#pragma once

#include "WhtsProtocol.h"
#include <memory>
#include <string>
#include <vector>

using namespace WhtsProtocol;

// Forward declarations
class MasterServer;

// Message handler interface for extensible message processing
class IMessageHandler {
  public:
    virtual ~IMessageHandler() = default;
    virtual std::unique_ptr<Message> processMessage(const Message &message,
                                                    MasterServer *server) = 0;
    virtual void executeActions(const Message &message,
                                MasterServer *server) = 0;
};

// Action result structure
struct ActionResult {
    bool success;
    std::string errorMessage;
    std::vector<uint32_t> affectedSlaves;

    ActionResult(bool s = true, const std::string &err = "")
        : success(s), errorMessage(err) {}
};

// Slave Configuration Message Handler
class SlaveConfigHandler : public IMessageHandler {
  public:
    std::unique_ptr<Message> processMessage(const Message &message,
                                            MasterServer *server) override;
    void executeActions(const Message &message, MasterServer *server) override;
};

// Mode Configuration Message Handler
class ModeConfigHandler : public IMessageHandler {
  public:
    std::unique_ptr<Message> processMessage(const Message &message,
                                            MasterServer *server) override;
    void executeActions(const Message &message, MasterServer *server) override;
};

// Reset Message Handler
class ResetHandler : public IMessageHandler {
  public:
    std::unique_ptr<Message> processMessage(const Message &message,
                                            MasterServer *server) override;
    void executeActions(const Message &message, MasterServer *server) override;
};

// Control Message Handler
class ControlHandler : public IMessageHandler {
  public:
    std::unique_ptr<Message> processMessage(const Message &message,
                                            MasterServer *server) override;
    void executeActions(const Message &message, MasterServer *server) override;
};

// Ping Control Message Handler
class PingControlHandler : public IMessageHandler {
  public:
    std::unique_ptr<Message> processMessage(const Message &message,
                                            MasterServer *server) override;
    void executeActions(const Message &message, MasterServer *server) override;
};

// Device List Request Handler
class DeviceListHandler : public IMessageHandler {
  public:
    std::unique_ptr<Message> processMessage(const Message &message,
                                            MasterServer *server) override;
    void executeActions(const Message &message, MasterServer *server) override;
};