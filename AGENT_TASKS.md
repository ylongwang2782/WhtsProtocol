## 📄 `AGENT_TASKS.md`

# 🤖 AGENT 编程任务清单 | Agent Task Board

> 本文档用于记录通过 ChatGPT 等 Agent 协助完成的开发任务。每个任务包括背景、具体指令、输出预期及完成情况。

---

## ✅ 任务 1：日志模块重构（Logger 分层）

- **状态**：✅ 已完成
- **背景说明**：
  目前日志逻辑分布零散，需抽象统一接口，并为不同平台提供适配。
- **任务指令**：
```

帮我设计一个跨平台的 Logger 接口 ILogger，要求包括 log(LogLevel level, const std::string& msg) 方法。

* 在 Windows 上使用 spdlog 实现（SpdlogAdapter）
* 在 Embedded 上使用 printf 实现（EmbeddedLogger）
* 工厂类 LoggerFactory 负责根据平台宏返回不同实现

```
- **预期输出**：
- `interface/ILogger.h`
- `platform/windows/SpdlogAdapter.cpp/h`
- `platform/embedded/EmbeddedLogger.cpp/h`
- `adapter/LoggerFactory.cpp/h`

---

## ✅ 任务 2：GPIO 模块迁移与重构

- **状态**：✅ 已完成
- **背景说明**：
Gpio 相关代码分布不合理，需要按职责分层整理。
- **任务指令**：
```

按照 platform/adapter/interface/app 层级，将 Gpio 文件重构并迁移到对应文件夹：

* IGpio.h → interface/
* VirtualGpio → platform/windows/
* HardwareGpio → platform/embedded/
* GpioFactory → adapter/
* Gpio.cpp/h → app/slave\_main/
  要求：slave\_main 编译正常

```
- **预期输出**：
- `interface/IGpio.h`
- `platform/windows/VirtualGpio.cpp/h`
- `platform/embedded/HardwareGpio.cpp/h`
- `adapter/GpioFactory.cpp/h`
- `app/slave_main/Gpio.cpp/h`

---

## ✅ 任务 3：UDP 网络通信模块抽象

- **状态**：✅ 已完成
- **背景说明**：
当前网络通信依赖平台库（如 asio），需封装成统一接口以便切换。
- **任务指令**：
```

定义统一的 IUdpSocket 接口，封装 async\_send, async\_recv 等方法。
分别实现 AsioUdpSocket（Windows）和 LwipUdpSocket（Embedded）。
并添加 NetworkFactory 工厂类用于实例化。

```
- **预期输出**：
- `interface/IUdpSocket.h`
- `platform/windows/AsioUdpSocket.cpp/h`
- `platform/embedded/LwipUdpSocket.cpp/h`
- `adapter/NetworkFactory.cpp/h`

---

## ⏳ 任务 4：NetworkManager 模块统一引用检查（未完成）

- **状态**：🔄 进行中
- **背景说明**：
master_main 和 slave_main 中网络调用存在不一致，需统一使用 NetworkManager。
- **任务指令**：
```

检查 slave\_main 和 master\_main 中是否使用了 Windows 网络库接口。
如果是，请改为使用 src/app/NetworkManager.h 中定义的 NetworkHAL 层封装接口。

```
- **预期输出**：
- slave_main 和 master_main 中不再引用 WindowsSocket，改用 NetworkManager 封装类

---

## 🆕 任务 5：提取 Agent 通用指令模板（可选）

- **状态**：📝 计划中
- **背景说明**：
编写高质量 prompt 可提高 agent 编程效率，建议整理复用模板。
- **任务指令**：
```

整理一套跨平台模块设计 prompt 模板，适用于 interface + adapter + platform 的标准架构。
支持 Logger、GPIO、UDP、Storage、TaskRunner 等模块。

```
- **预期输出**：
- `prompts/logger_refactor.md`
- `prompts/gpio_interface.md`
- `prompts/udp_socket.md`

---

## 📌 使用建议

- 每完成一个任务，勾选 `[x]` 并归档到 ✅ 区域
- 每个任务附带：
- 背景（Why）
- 指令（What）
- 输出结构（Deliverable）

- 可结合 GitHub PR 引用 `AGENT_TASKS.md` 中的任务编号

```

# 示例：PR 描述中引用

This PR implements Task 3 in AGENT\_TASKS.md: UDP 网络通信模块抽象

```

---

是否需要我同时为你生成这些对应的 prompt 文件模板？例如 `prompts/logger_refactor.md`？这能帮助你快速复用任务生成代码。
