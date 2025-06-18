# WhtsProtocol - 分布式架构

## 概述

WhtsProtocol项目已经重构为分布式架构，将原来的单一main程序拆分为三个独立的可执行文件：

1. **Master** - 主控制器
2. **Slave** - 从设备
3. **main** - 原始的单体程序（向后兼容）

## 架构说明

### Master 服务器
- **监听端口**: 8888
- **处理的消息类型**:
  - `Backend2Master` - 来自后端的控制命令
  - `Slave2Master` - 来自从设备的响应和状态报告

### Slave 设备
- **监听端口**: 8889
- **设备ID**: 0x12345678 (可配置)
- **处理的消息类型**:
  - `Master2Slave` - 来自主控制器的指令

### 消息流向

```
Backend → Master → Slave
       ← Master ← Slave
```

## 构建说明

```bash
# 配置项目
cmake -B build

# 构建所有目标
cmake --build build

# 或构建特定目标
cmake --build build --target Master
cmake --build build --target Slave
```

## 运行说明

### 启动Master服务器
```bash
./build/src/Master.exe
```

### 启动Slave设备
```bash
./build/src/Slave.exe
```

## 支持的消息类型

### Backend2Master
- SLAVE_CFG_MSG - 从设备配置
- MODE_CFG_MSG - 模式配置
- SLAVE_RST_MSG - 从设备重置
- CTRL_MSG - 控制消息
- PING_CTRL_MSG - Ping控制
- DEVICE_LIST_REQ_MSG - 设备列表请求

### Master2Slave
- SYNC_MSG - 同步消息
- CONDUCTION_CFG_MSG - 导通配置
- RESISTANCE_CFG_MSG - 电阻配置
- CLIP_CFG_MSG - 夹具配置
- READ_COND_DATA_MSG - 读取导通数据
- READ_RES_DATA_MSG - 读取电阻数据
- READ_CLIP_DATA_MSG - 读取夹具数据
- RST_MSG - 重置消息
- PING_REQ_MSG - Ping请求
- SHORT_ID_ASSIGN_MSG - 短ID分配

### Slave2Master
- CONDUCTION_CFG_RSP_MSG - 导通配置响应
- RESISTANCE_CFG_RSP_MSG - 电阻配置响应
- CLIP_CFG_RSP_MSG - 夹具配置响应
- RST_RSP_MSG - 重置响应
- PING_RSP_MSG - Ping响应
- ANNOUNCE_MSG - 设备公告
- SHORT_ID_CONFIRM_MSG - 短ID确认

## 特性

- **模块化设计**: 每个组件职责明确
- **独立部署**: Master和Slave可以在不同的机器上运行
- **向后兼容**: 保留原始main程序
- **GPIO集成**: Slave设备集成了导通数据采集功能
- **实时通信**: 基于UDP的实时消息传输

## 开发说明

### 添加新的消息类型
1. 在相应的消息头文件中定义新的消息类
2. 在Master或Slave的处理函数中添加处理逻辑
3. 更新协议文档

### 扩展功能
- Master可以管理多个Slave设备
- 支持设备发现和自动配置
- 可以添加数据持久化功能
- 支持负载均衡和故障转移 