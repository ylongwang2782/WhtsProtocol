#ifndef GPIO_H
#define GPIO_H

// GPIO模块统一包含头文件
// 这个文件提供了GPIO模块的完整接口，包括：
// - GPIO接口定义 (IGpio)
// - 虚拟GPIO实现 (VirtualGpio) - 用于仿真和测试
// - 硬件GPIO实现 (HardwareGpio) - 用于真实硬件平台
// - GPIO工厂类 (GpioFactory) - 统一的实例创建接口

#include "../../Adapter/Gpio/GpioFactory.h"
#include "../../interface/IGpio.h"
#include "../../platform/embedded/HardwareGpio.h"
#include "../../platform/windows/VirtualGpio.h"

// 为了向后兼容，导出常用的类型和类到全局命名空间
namespace HAL {
// 重新导出主要接口，保持向后兼容性
using Gpio = IGpio;
} // namespace HAL

#endif // GPIO_H