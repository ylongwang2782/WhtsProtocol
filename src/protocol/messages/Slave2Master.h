#ifndef WHTS_PROTOCOL_SLAVE2MASTER_H
#define WHTS_PROTOCOL_SLAVE2MASTER_H

#include "../Common.h"
#include "Message.h"

namespace WhtsProtocol {
namespace Slave2Master {

class ConductionConfigResponseMessage : public Message {
  public:
    uint8_t status;
    uint8_t timeSlot;
    uint8_t interval;
    uint16_t totalConductionNum;
    uint16_t startConductionNum;
    uint16_t conductionNum;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(
            Slave2MasterMessageId::CONDUCTION_CFG_RSP_MSG);
    }
};

class ResistanceConfigResponseMessage : public Message {
  public:
    uint8_t status;
    uint8_t timeSlot;
    uint8_t interval;
    uint16_t totalConductionNum;
    uint16_t startConductionNum;
    uint16_t conductionNum;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(
            Slave2MasterMessageId::RESISTANCE_CFG_RSP_MSG);
    }
};

class ClipConfigResponseMessage : public Message {
  public:
    uint8_t status;
    uint8_t interval;
    uint8_t mode;
    uint16_t clipPin;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Slave2MasterMessageId::CLIP_CFG_RSP_MSG);
    }
};

class RstResponseMessage : public Message {
  public:
    uint8_t status;
    uint8_t lockStatus;
    uint16_t clipLed;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Slave2MasterMessageId::RST_RSP_MSG);
    }
};

class PingRspMessage : public Message {
  public:
    uint16_t sequenceNumber;
    uint32_t timestamp;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Slave2MasterMessageId::PING_RSP_MSG);
    }
};

class AnnounceMessage : public Message {
  public:
    uint32_t deviceId;
    uint8_t versionMajor;
    uint8_t versionMinor;
    uint16_t versionPatch;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Slave2MasterMessageId::ANNOUNCE_MSG);
    }
};

class ShortIdConfirmMessage : public Message {
  public:
    uint8_t status;
    uint8_t shortId;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(
            Slave2MasterMessageId::SHORT_ID_CONFIRM_MSG);
    }
};

} // namespace Slave2Master
} // namespace WhtsProtocol

#endif // WHTS_PROTOCOL_SLAVE2MASTER_H