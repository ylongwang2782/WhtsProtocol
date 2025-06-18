#include "VirtualGpio.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <random>

namespace HAL {

// VirtualGpio实现
VirtualGpio::VirtualGpio() : simulationCounter_(0) {
    // 初始化所有引脚为默认状态
    resetAllPins();
}

VirtualGpio::~VirtualGpio() {
    // 清理资源
}

bool VirtualGpio::init(const GpioConfig &config) {
    if (config.pin >= MAX_PINS) {
        return false;
    }

    pins_[config.pin].mode = config.mode;
    pins_[config.pin].initialized = true;

    // 如果是输出模式，设置初始状态
    if (config.mode == GpioMode::OUTPUT) {
        pins_[config.pin].state = config.initState;
    } else {
        // 输入模式根据上拉下拉设置默认状态
        switch (config.mode) {
        case GpioMode::INPUT_PULLUP:
            pins_[config.pin].state = GpioState::HIGH;
            break;
        case GpioMode::INPUT_PULLDOWN:
            pins_[config.pin].state = GpioState::LOW;
            break;
        default:
            pins_[config.pin].state = GpioState::LOW;
            break;
        }
    }

    return true;
}

GpioState VirtualGpio::read(uint8_t pin) {
    if (pin >= MAX_PINS || !pins_[pin].initialized) {
        return GpioState::LOW;
    }

    // 对于输入模式，模拟一些变化以用于测试
    if (pins_[pin].mode != GpioMode::OUTPUT) {
        simulationCounter_++;

        // 模拟一些随机性，但保持一定的规律性以便测试
        if (simulationCounter_ % 100 == 0) {
            // 每100次读取可能发生状态变化
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<> dis(0, 9);

            if (dis(gen) < 3) { // 30%概率变化
                pins_[pin].state = (pins_[pin].state == GpioState::LOW)
                                       ? GpioState::HIGH
                                       : GpioState::LOW;
            }
        }
    }

    return pins_[pin].state;
}

bool VirtualGpio::write(uint8_t pin, GpioState state) {
    if (pin >= MAX_PINS || !pins_[pin].initialized) {
        return false;
    }

    if (pins_[pin].mode != GpioMode::OUTPUT) {
        return false; // 只有输出模式才能写入
    }

    pins_[pin].state = state;
    return true;
}

bool VirtualGpio::setMode(uint8_t pin, GpioMode mode) {
    if (pin >= MAX_PINS) {
        return false;
    }

    pins_[pin].mode = mode;

    // 根据新模式设置合适的默认状态
    switch (mode) {
    case GpioMode::INPUT_PULLUP:
        pins_[pin].state = GpioState::HIGH;
        break;
    case GpioMode::INPUT_PULLDOWN:
        pins_[pin].state = GpioState::LOW;
        break;
    default:
        // 保持当前状态
        break;
    }

    return true;
}

std::vector<GpioState>
VirtualGpio::readMultiple(const std::vector<uint8_t> &pins) {
    std::vector<GpioState> results;
    results.reserve(pins.size());

    for (uint8_t pin : pins) {
        results.push_back(read(pin));
    }

    return results;
}

bool VirtualGpio::deinit(uint8_t pin) {
    if (pin >= MAX_PINS) {
        return false;
    }

    pins_[pin].initialized = false;
    pins_[pin].mode = GpioMode::INPUT;
    pins_[pin].state = GpioState::LOW;

    return true;
}

void VirtualGpio::setSimulatedState(uint8_t pin, GpioState state) {
    if (pin < MAX_PINS) {
        pins_[pin].state = state;
    }
}

bool VirtualGpio::isPinInitialized(uint8_t pin) const {
    if (pin >= MAX_PINS) {
        return false;
    }
    return pins_[pin].initialized;
}

GpioMode VirtualGpio::getPinMode(uint8_t pin) const {
    if (pin >= MAX_PINS) {
        return GpioMode::INPUT;
    }
    return pins_[pin].mode;
}

void VirtualGpio::resetAllPins() {
    for (int i = 0; i < MAX_PINS; i++) {
        pins_[i] = PinState();
    }
    simulationCounter_ = 0;
}

void VirtualGpio::simulateContinuityPattern(uint8_t numPins, uint32_t pattern) {
    if (numPins > MAX_PINS) {
        numPins = MAX_PINS;
    }

    // 根据模式设置引脚状态
    for (uint8_t i = 0; i < numPins; i++) {
        // 使用位模式来确定引脚状态
        bool pinHigh = (pattern >> (i % 32)) & 1;
        pins_[i].state = pinHigh ? GpioState::HIGH : GpioState::LOW;
    }
}

} // namespace HAL