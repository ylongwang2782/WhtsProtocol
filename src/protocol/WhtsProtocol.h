#ifndef WHTS_PROTOCOL_H
#define WHTS_PROTOCOL_H

// 包含所有子模块
#include "Common.h"
#include "DeviceStatus.h"
#include "Frame.h"
#include "messages/Backend2Master.h"
#include "messages/Master2Backend.h"
#include "messages/Master2Slave.h"
#include "messages/Message.h"
#include "messages/Slave2Backend.h"
#include "messages/Slave2Master.h"
#include "utils/ByteUtils.h"

// 其他消息类型头文件将在后续添加
// #include "messages/Slave2Master.h"
// #include "messages/Backend2Master.h"
// #include "messages/Master2Backend.h"
// #include "messages/Slave2Backend.h"

#include <map>
#include <memory>
#include <queue>

namespace WhtsProtocol {

// 包含协议处理器
#include "ProtocolProcessor.h"

} // namespace WhtsProtocol

#endif // WHTS_PROTOCOL_H