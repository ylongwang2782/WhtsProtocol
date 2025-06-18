#include "GpioFactory.h"
#include "HardwareGpio.h"
#include "VirtualGpio.h"

namespace HAL {

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