#include "GpioFactory.h"
#include "../platform/embedded/HardwareGpio.h"
#include "../platform/windows/VirtualGpio.h"

namespace HAL {

// 统一的GPIO创建接口 - 根据CMake配置自动选择实现
std::unique_ptr<IGpio> GpioFactory::createGpio() {
#if defined(GPIO_USE_HARDWARE)
    return createHardwareGpio();
#elif defined(GPIO_USE_VIRTUAL)
    return createVirtualGpio();
#else
    // 默认使用虚拟GPIO（用于开发和测试）
    return createVirtualGpio();
#endif
}

std::unique_ptr<IGpio> GpioFactory::createVirtualGpio() {
    return std::make_unique<VirtualGpio>();
}

std::unique_ptr<IGpio> GpioFactory::createHardwareGpio() {
    return std::make_unique<HardwareGpio>();
}

std::unique_ptr<IGpio> GpioFactory::createGpio(GpioType type) {
    switch (type) {
    case GpioType::VIRTUAL:
        return createVirtualGpio();
    case GpioType::HARDWARE:
        return createHardwareGpio();
    default:
        return createVirtualGpio(); // 默认返回虚拟GPIO
    }
}

} // namespace HAL