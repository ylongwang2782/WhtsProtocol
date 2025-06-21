// GPIO模块的统一实现文件
// 这个文件现在主要用于向后兼容，实际实现已经分散到各个具体的实现文件中：
// - VirtualGpio.cpp - 虚拟GPIO实现
// - HardwareGpio.cpp - 硬件GPIO实现
// - GpioFactory.cpp - GPIO工厂实现

#include "Gpio.h"

// 这个文件现在基本为空，因为所有实现都已经移到对应的具体实现文件中
// 保留这个文件主要是为了向后兼容和满足CMake构建系统的要求

namespace HAL {
// 如果需要，可以在这里添加一些模块级别的初始化代码
} // namespace HAL