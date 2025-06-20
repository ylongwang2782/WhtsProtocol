#pragma once

#include "IUdpSocket.h"
#include "NetworkManager.h"
#include <memory>

namespace HAL {
namespace Network {

/**
 * 平台类型枚举
 */
enum class PlatformType {
    WINDOWS,      // Windows平台（原生socket）
    WINDOWS_ASIO, // Windows平台（ASIO）
    LINUX,        // Linux平台（原生socket）
    LINUX_ASIO,   // Linux平台（ASIO）
    EMBEDDED      // 嵌入式平台（使用lwip）
};

/**
 * 网络工厂类
 * 根据平台类型创建相应的网络实现
 */
class NetworkFactory {
  public:
    /**
     * 获取当前平台类型
     * @return 平台类型
     */
    static PlatformType getCurrentPlatform();

    /**
     * 创建UDP套接字工厂
     * @param platform 目标平台类型，默认为当前平台
     * @return UDP套接字工厂智能指针
     */
    static std::unique_ptr<IUdpSocketFactory>
    createUdpSocketFactory(PlatformType platform = getCurrentPlatform());

    /**
     * 创建网络管理器
     * @param platform 目标平台类型，默认为当前平台
     * @return 已初始化的网络管理器智能指针
     */
    static std::unique_ptr<NetworkManager>
    createNetworkManager(PlatformType platform = getCurrentPlatform());

    /**
     * 获取平台名称字符串
     * @param platform 平台类型
     * @return 平台名称
     */
    static std::string getPlatformName(PlatformType platform);

    /**
     * 检查平台是否支持
     * @param platform 平台类型
     * @return 是否支持该平台
     */
    static bool isPlatformSupported(PlatformType platform);
};

} // namespace Network
} // namespace HAL