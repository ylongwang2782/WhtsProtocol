#ifndef HARDWARE_GPIO_H
#define HARDWARE_GPIO_H

#include "../../interface/IGpio.h"
#include <cstdint>

namespace HAL {

// 硬件GPIO实现类（用于真实硬件平台）
// 这是一个模板实现，需要根据具体硬件平台进行适配
class HardwareGpio : public IGpio {
  private:
    static constexpr uint8_t MAX_PINS = 64;

    struct PinState {
        GpioMode mode;
        bool initialized;

        PinState() : mode(GpioMode::INPUT), initialized(false) {}
    };

    PinState pins_[MAX_PINS];

    // 平台相关的私有方法（需要根据具体硬件实现）
    bool platformInit(uint8_t pin, GpioMode mode, GpioState initState);
    bool platformDeinit(uint8_t pin);
    GpioState platformRead(uint8_t pin);
    bool platformWrite(uint8_t pin, GpioState state);
    bool platformSetMode(uint8_t pin, GpioMode mode);

  public:
    HardwareGpio();
    virtual ~HardwareGpio();

    // IGpio接口实现
    bool init(const GpioConfig &config) override;
    GpioState read(uint8_t pin) override;
    bool write(uint8_t pin, GpioState state) override;
    bool setMode(uint8_t pin, GpioMode mode) override;
    std::vector<GpioState>
    readMultiple(const std::vector<uint8_t> &pins) override;
    bool deinit(uint8_t pin) override;

    // 硬件GPIO特有方法
    // 获取引脚信息
    bool isPinInitialized(uint8_t pin) const;
    GpioMode getPinMode(uint8_t pin) const;

    // 重置所有引脚
    void resetAllPins();
};

} // namespace HAL

#endif // HARDWARE_GPIO_H