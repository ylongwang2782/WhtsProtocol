#ifndef WHTS_PROTOCOL_H
#define WHTS_PROTOCOL_H

// 包含所有子模块
#include "Common.h"
#include "DeviceStatus.h"
#include "Frame.h"
#include "ProtocolProcessor.h"

// 消息模块
#include "messages/Backend2Master.h"
#include "messages/Master2Backend.h"
#include "messages/Master2Slave.h"
#include "messages/Message.h"
#include "messages/Slave2Backend.h"
#include "messages/Slave2Master.h"


// 工具模块
#include "utils/ByteUtils.h"

// 标准库依赖
#include <map>
#include <memory>
#include <queue>
#include <vector>

namespace WhtsProtocol {
// 所有协议相关的类和函数都在各自的头文件中定义
// 这个文件作为统一的入口点
}

#endif // WHTS_PROTOCOL_H