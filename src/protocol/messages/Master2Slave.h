#ifndef WHTS_PROTOCOL_MASTER2SLAVE_H
#define WHTS_PROTOCOL_MASTER2SLAVE_H

#include "../Common.h"
#include "Message.h"

namespace WhtsProtocol {
namespace Master2Slave {

class SyncMessage : public Message {
  public:
    uint8_t mode;
    uint32_t timestamp;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::SYNC_MSG);
    }
};

class ConductionConfigMessage : public Message {
  public:
    uint8_t timeSlot;
    uint8_t interval;
    uint16_t totalConductionNum;
    uint16_t startConductionNum;
    uint16_t conductionNum;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::CONDUCTION_CFG_MSG);
    }
};

class ResistanceConfigMessage : public Message {
  public:
    uint8_t timeSlot;
    uint8_t interval;
    uint16_t totalNum;
    uint16_t startNum;
    uint16_t num;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::RESISTANCE_CFG_MSG);
    }
};

class ClipConfigMessage : public Message {
  public:
    uint8_t interval;
    uint8_t mode;
    uint16_t clipPin;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::CLIP_CFG_MSG);
    }
};

class ReadConductionDataMessage : public Message {
  public:
    uint8_t reserve;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::READ_COND_DATA_MSG);
    }
};

class ReadResistanceDataMessage : public Message {
  public:
    uint8_t reserve;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::READ_RES_DATA_MSG);
    }
};

class ReadClipDataMessage : public Message {
  public:
    uint8_t reserve;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::READ_CLIP_DATA_MSG);
    }
};

class RstMessage : public Message {
  public:
    uint8_t lockStatus;
    uint16_t clipLed;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::RST_MSG);
    }
};

class PingReqMessage : public Message {
  public:
    uint16_t sequenceNumber;
    uint32_t timestamp;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::PING_REQ_MSG);
    }
};

class ShortIdAssignMessage : public Message {
  public:
    uint8_t shortId;

    std::vector<uint8_t> serialize() const override;
    bool deserialize(const std::vector<uint8_t> &data) override;
    uint8_t getMessageId() const override {
        return static_cast<uint8_t>(Master2SlaveMessageId::SHORT_ID_ASSIGN_MSG);
    }
};

} // namespace Master2Slave
} // namespace WhtsProtocol

#endif // WHTS_PROTOCOL_MASTER2SLAVE_H