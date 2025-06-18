#ifndef GPIO_FACTORY_H
#define GPIO_FACTORY_H

#include "IGpio.h"
#include <memory>

namespace HAL {

// GPIO工厂类
class GpioFactory {
  public:
    // 创建虚拟GPIO实例（用于仿真和测试）
    static std::unique_ptr<IGpio> createVirtualGpio();

    // 创建硬件GPIO实例（用于真实硬件平台）
    static std::unique_ptr<IGpio> createHardwareGpio();

    // 根据配置创建GPIO实例
    enum class GpioType {
        VIRTUAL, // 虚拟GPIO
        HARDWARE // 硬件GPIO
    };

    static std::unique_ptr<IGpio> createGpio(GpioType type);
};

} // namespace HAL

#endif // GPIO_FACTORY_H