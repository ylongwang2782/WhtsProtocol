#ifndef VIRTUAL_GPIO_H
#define VIRTUAL_GPIO_H

#include "IGpio.h"
#include <cstdint>

namespace HAL {

// 虚拟GPIO实现类（用于仿真和测试）
class VirtualGpio : public IGpio {
  private:
    static constexpr uint8_t MAX_PINS = 64;

    struct PinState {
        GpioMode mode;
        GpioState state;
        bool initialized;

        PinState()
            : mode(GpioMode::INPUT), state(GpioState::LOW), initialized(false) {
        }
    };

    PinState pins_[MAX_PINS];
    uint32_t simulationCounter_; // 用于模拟GPIO状态变化

  public:
    VirtualGpio();
    virtual ~VirtualGpio();

    // IGpio接口实现
    bool init(const GpioConfig &config) override;
    GpioState read(uint8_t pin) override;
    bool write(uint8_t pin, GpioState state) override;
    bool setMode(uint8_t pin, GpioMode mode) override;
    std::vector<GpioState>
    readMultiple(const std::vector<uint8_t> &pins) override;
    bool deinit(uint8_t pin) override;

    // 虚拟GPIO特有方法
    // 设置模拟状态（用于测试）
    void setSimulatedState(uint8_t pin, GpioState state);

    // 获取引脚信息
    bool isPinInitialized(uint8_t pin) const;
    GpioMode getPinMode(uint8_t pin) const;

    // 重置所有引脚
    void resetAllPins();

    // 模拟导通测试环境（为ContinuityCollector提供测试数据）
    void simulateContinuityPattern(uint8_t numPins, uint32_t pattern);
};

} // namespace HAL

#endif // VIRTUAL_GPIO_H