#ifndef GPIO_H
#define GPIO_H

#include <cstdint>
#include <memory>
#include <vector>

namespace HAL {

// GPIO引脚状态枚举
enum class GpioState : uint8_t {
    LOW = 0, // 低电平
    HIGH = 1 // 高电平
};

// GPIO引脚模式枚举
enum class GpioMode : uint8_t {
    INPUT = 0,         // 输入模式
    OUTPUT = 1,        // 输出模式
    INPUT_PULLUP = 2,  // 输入上拉模式
    INPUT_PULLDOWN = 3 // 输入下拉模式
};

// GPIO引脚配置结构
struct GpioConfig {
    uint8_t pin;         // 引脚编号
    GpioMode mode;       // 引脚模式
    GpioState initState; // 初始状态（仅对输出模式有效）

    GpioConfig(uint8_t p, GpioMode m, GpioState init = GpioState::LOW)
        : pin(p), mode(m), initState(init) {}
};

// GPIO接口抽象基类
class IGpio {
  public:
    virtual ~IGpio() = default;

    // 初始化GPIO引脚
    virtual bool init(const GpioConfig &config) = 0;

    // 读取GPIO引脚状态
    virtual GpioState read(uint8_t pin) = 0;

    // 写入GPIO引脚状态（仅对输出模式有效）
    virtual bool write(uint8_t pin, GpioState state) = 0;

    // 设置GPIO引脚模式
    virtual bool setMode(uint8_t pin, GpioMode mode) = 0;

    // 批量读取多个GPIO引脚状态
    virtual std::vector<GpioState>
    readMultiple(const std::vector<uint8_t> &pins) = 0;

    // 去初始化GPIO引脚
    virtual bool deinit(uint8_t pin) = 0;
};

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

// GPIO工厂类
class GpioFactory {
  public:
    // 创建虚拟GPIO实例
    static std::unique_ptr<IGpio> createVirtualGpio();

    // 未来可以扩展为创建真实硬件GPIO实例
    // static std::unique_ptr<IGpio> createHardwareGpio();
};

} // namespace HAL

#endif // GPIO_H
