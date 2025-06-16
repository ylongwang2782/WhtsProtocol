#ifndef WHTS_PROTOCOL_MASTER2BACKEND_H
#define WHTS_PROTOCOL_MASTER2BACKEND_H

#include "../Common.h"
#include "../utils/ByteUtils.h"
#include "Message.h"

namespace WhtsProtocol {
namespace Master2Backend {

class SlaveConfigResponseMessage : public Message {
  public:
    struct SlaveInfo {
        uint32_t id;
        uint8_t conductionNum;
        uint8_t resistanceNum;
        uint8_t clipMode;
        uint16_t clipStatus;
    };

    uint8_t status;
    uint8_t slaveNum;
    std::vector<SlaveInfo> slaves;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2BackendMessageId::SLAVE_CFG_RSP_MSG);
    }
};

class ModeConfigResponseMessage : public Message {
  public:
    uint8_t status;
    uint8_t mode;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2BackendMessageId::MODE_CFG_RSP_MSG);
    }
};

class RstResponseMessage : public Message {
  public:
    struct SlaveRstInfo {
        uint32_t id;
        uint8_t lock;
        uint16_t clipStatus;
    };

    uint8_t status;
    uint8_t slaveNum;
    std::vector<SlaveRstInfo> slaves;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2BackendMessageId::RST_RSP_MSG);
    }
};

class CtrlResponseMessage : public Message {
  public:
    uint8_t status;
    uint8_t runningStatus;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2BackendMessageId::CTRL_RSP_MSG);
    }
};

class PingResponseMessage : public Message {
  public:
    uint8_t pingMode;
    uint16_t totalCount;
    uint16_t successCount;
    uint32_t destinationId;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2BackendMessageId::PING_RES_MSG);
    }
};

class DeviceListResponseMessage : public Message {
  public:
    struct DeviceInfo {
        uint32_t deviceId;
        uint8_t shortId;
        uint8_t online;
        uint8_t versionMajor;
        uint8_t versionMinor;
        uint16_t versionPatch;
    };

    uint8_t deviceCount;
    std::vector<DeviceInfo> devices;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(
            Master2BackendMessageId::DEVICE_LIST_RSP_MSG);
    }
};

} // namespace Master2Backend
} // namespace WhtsProtocol

#endif // WHTS_PROTOCOL_MASTER2BACKEND_H