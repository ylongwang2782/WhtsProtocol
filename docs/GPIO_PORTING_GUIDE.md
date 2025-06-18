# GPIO 模块嵌入式移植指南

## 概述

GPIO模块已经重构为完全模块化的架构，将接口定义与具体实现分离，便于快速移植到不同的嵌入式平台。

## 架构设计

### 文件结构
```
src/HAL/
├── IGpio.h              # GPIO接口定义（平台无关）
├── VirtualGpio.h/cpp    # 虚拟GPIO实现（用于仿真测试）
├── HardwareGpio.h/cpp   # 硬件GPIO实现模板
├── GpioFactory.h/cpp    # GPIO工厂类
└── Gpio.h               # 统一包含头文件（向后兼容）
```

### 设计原则

1. **接口分离**: `IGpio`定义了与平台无关的GPIO操作接口
2. **实现分离**: 虚拟实现和硬件实现完全分离
3. **工厂模式**: 通过工厂类统一创建不同类型的GPIO实例
4. **向后兼容**: 保持现有代码的兼容性

## 移植步骤

### 1. 理解接口定义

`IGpio`接口定义了所有GPIO操作：

```cpp
class IGpio {
public:
    virtual bool init(const GpioConfig &config) = 0;
    virtual GpioState read(uint8_t pin) = 0;
    virtual bool write(uint8_t pin, GpioState state) = 0;
    virtual bool setMode(uint8_t pin, GpioMode mode) = 0;
    virtual std::vector<GpioState> readMultiple(const std::vector<uint8_t> &pins) = 0;
    virtual bool deinit(uint8_t pin) = 0;
};
```

### 2. 实现平台相关的GPIO类

有两种方法实现硬件GPIO：

#### 方法1: 修改现有的HardwareGpio类

修改 `src/HAL/HardwareGpio.cpp` 中的平台相关方法：

```cpp
bool HardwareGpio::platformInit(uint8_t pin, GpioMode mode, GpioState initState) {
    // 在这里实现你的平台特定初始化代码
    // 例如 STM32:
    // GPIO_InitTypeDef GPIO_InitStruct = {0};
    // GPIO_InitStruct.Pin = convertPinNumber(pin);
    // GPIO_InitStruct.Mode = convertGpioMode(mode);
    // HAL_GPIO_Init(getGpioPort(pin), &GPIO_InitStruct);
    
    return true;
}

GpioState HardwareGpio::platformRead(uint8_t pin) {
    // 实现GPIO读取
    // 例如 STM32:
    // GPIO_PinState state = HAL_GPIO_ReadPin(getGpioPort(pin), convertPinNumber(pin));
    // return (state == GPIO_PIN_SET) ? GpioState::HIGH : GpioState::LOW;
    
    return GpioState::LOW;
}

bool HardwareGpio::platformWrite(uint8_t pin, GpioState state) {
    // 实现GPIO写入
    // 例如 STM32:
    // HAL_GPIO_WritePin(getGpioPort(pin), convertPinNumber(pin),
    //                   state == GpioState::HIGH ? GPIO_PIN_SET : GPIO_PIN_RESET);
    
    return true;
}
```

#### 方法2: 创建新的平台特定GPIO类

创建新的GPIO实现类（推荐用于多平台支持）：

```cpp
// src/HAL/Stm32Gpio.h
#ifndef STM32_GPIO_H
#define STM32_GPIO_H

#include "IGpio.h"
#include <stm32f4xx_hal_gpio.h>  // 根据你的STM32型号调整

namespace HAL {

class Stm32Gpio : public IGpio {
private:
    GPIO_TypeDef* getGpioPort(uint8_t pin);
    uint16_t convertPinNumber(uint8_t pin);
    uint32_t convertGpioMode(GpioMode mode);
    uint32_t convertPullMode(GpioMode mode);

public:
    Stm32Gpio();
    virtual ~Stm32Gpio();

    // IGpio接口实现
    bool init(const GpioConfig &config) override;
    GpioState read(uint8_t pin) override;
    bool write(uint8_t pin, GpioState state) override;
    bool setMode(uint8_t pin, GpioMode mode) override;
    std::vector<GpioState> readMultiple(const std::vector<uint8_t> &pins) override;
    bool deinit(uint8_t pin) override;
};

} // namespace HAL

#endif // STM32_GPIO_H
```

### 3. 实现平台转换函数

你需要实现引脚号和模式的转换函数：

```cpp
// STM32 示例
GPIO_TypeDef* Stm32Gpio::getGpioPort(uint8_t pin) {
    // 根据引脚号返回对应的GPIO端口
    if (pin < 16) return GPIOA;
    else if (pin < 32) return GPIOB;
    else if (pin < 48) return GPIOC;
    // ... 更多端口
    return GPIOA; // 默认端口
}

uint16_t Stm32Gpio::convertPinNumber(uint8_t pin) {
    // 将逻辑引脚号转换为STM32的引脚掩码
    uint8_t portPin = pin % 16;
    return (1 << portPin);
}

uint32_t Stm32Gpio::convertGpioMode(GpioMode mode) {
    switch (mode) {
    case GpioMode::INPUT: return GPIO_MODE_INPUT;
    case GpioMode::OUTPUT: return GPIO_MODE_OUTPUT_PP;
    case GpioMode::INPUT_PULLUP: return GPIO_MODE_INPUT;
    case GpioMode::INPUT_PULLDOWN: return GPIO_MODE_INPUT;
    default: return GPIO_MODE_INPUT;
    }
}
```

### 4. 更新GPIO工厂类

修改 `GpioFactory.cpp` 以支持你的新GPIO实现：

```cpp
#include "GpioFactory.h"
#include "VirtualGpio.h"
#include "Stm32Gpio.h"  // 包含你的平台实现

std::unique_ptr<IGpio> GpioFactory::createHardwareGpio() {
#ifdef STM32_PLATFORM
    return std::make_unique<Stm32Gpio>();
#elif defined(ESP32_PLATFORM)
    return std::make_unique<Esp32Gpio>();
#else
    // 默认返回虚拟GPIO用于测试
    return std::make_unique<VirtualGpio>();
#endif
}
```

### 5. 配置编译系统

在你的CMakeLists.txt或Makefile中：

```cmake
# 定义平台宏
add_definitions(-DSTM32_PLATFORM)

# 包含平台相关的源文件
if(STM32_PLATFORM)
    target_sources(HAL PRIVATE
        Stm32Gpio.cpp
        Stm32Gpio.h
    )
    # 链接STM32 HAL库
    target_link_libraries(HAL PRIVATE stm32_hal)
endif()
```

## 平台特定示例

### STM32 平台

```cpp
bool Stm32Gpio::init(const GpioConfig &config) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 使能时钟
    __HAL_RCC_GPIOA_CLK_ENABLE(); // 根据端口调整
    
    GPIO_InitStruct.Pin = convertPinNumber(config.pin);
    GPIO_InitStruct.Mode = convertGpioMode(config.mode);
    GPIO_InitStruct.Pull = convertPullMode(config.mode);
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    HAL_GPIO_Init(getGpioPort(config.pin), &GPIO_InitStruct);
    
    if (config.mode == GpioMode::OUTPUT) {
        HAL_GPIO_WritePin(getGpioPort(config.pin), convertPinNumber(config.pin),
                          config.initState == GpioState::HIGH ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
    
    return true;
}
```

### ESP32 平台

```cpp
#include <driver/gpio.h>

bool Esp32Gpio::init(const GpioConfig &config) {
    gpio_config_t io_conf = {};
    
    io_conf.pin_bit_mask = (1ULL << config.pin);
    
    switch (config.mode) {
    case GpioMode::INPUT:
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        break;
    case GpioMode::OUTPUT:
        io_conf.mode = GPIO_MODE_OUTPUT;
        break;
    case GpioMode::INPUT_PULLUP:
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        break;
    case GpioMode::INPUT_PULLDOWN:
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        break;
    }
    
    io_conf.intr_type = GPIO_INTR_DISABLE;
    
    gpio_config(&io_conf);
    
    if (config.mode == GpioMode::OUTPUT) {
        gpio_set_level((gpio_num_t)config.pin, 
                      config.initState == GpioState::HIGH ? 1 : 0);
    }
    
    return true;
}
```

## 使用方式

移植完成后，使用方式保持不变：

```cpp
// 创建GPIO实例
auto gpio = HAL::GpioFactory::createHardwareGpio();  // 使用硬件GPIO
// auto gpio = HAL::GpioFactory::createVirtualGpio();  // 使用虚拟GPIO测试

// 配置和使用
HAL::GpioConfig config(0, HAL::GpioMode::OUTPUT, HAL::GpioState::LOW);
gpio->init(config);
gpio->write(0, HAL::GpioState::HIGH);
```

## 测试建议

1. **先用虚拟GPIO测试**: 确保应用逻辑正确
2. **硬件在环测试**: 在目标硬件上验证GPIO操作
3. **边界条件测试**: 测试无效引脚号、模式切换等
4. **性能测试**: 验证GPIO操作的时序要求

## 注意事项

1. **引脚映射**: 确保逻辑引脚号正确映射到物理引脚
2. **时钟使能**: 在初始化前确保GPIO时钟已使能
3. **中断处理**: 如果需要GPIO中断，可扩展接口
4. **电气特性**: 注意不同平台的电压电平和驱动能力差异
5. **线程安全**: 如果多线程访问，考虑添加互斥保护

## 扩展功能

可以根据需要扩展GPIO接口：

```cpp
class IAdvancedGpio : public IGpio {
public:
    // 中断相关
    virtual bool enableInterrupt(uint8_t pin, InterruptMode mode, InterruptCallback callback) = 0;
    virtual bool disableInterrupt(uint8_t pin) = 0;
    
    // PWM相关
    virtual bool setPwm(uint8_t pin, uint32_t frequency, uint8_t dutyCycle) = 0;
    
    // 模拟功能
    virtual uint16_t readAnalog(uint8_t pin) = 0;
    virtual bool writeAnalog(uint8_t pin, uint16_t value) = 0;
};
```

这个模块化设计让你可以快速适配到任何嵌入式平台，同时保持代码的可测试性和可维护性。 