#include "NetworkFactory.h"
#include "LwipUdpSocket.h"
#include "WindowsUdpSocket.h"
#include <iostream>


namespace HAL {
namespace Network {

PlatformType NetworkFactory::getCurrentPlatform() {
#ifdef USE_LWIP
    return PlatformType::EMBEDDED;
#elif defined(_WIN32)
    return PlatformType::WINDOWS;
#elif defined(__linux__)
    return PlatformType::LINUX;
#else
    // 默认使用Windows实现（兼容大多数POSIX系统）
    return PlatformType::WINDOWS;
#endif
}

std::unique_ptr<IUdpSocketFactory>
NetworkFactory::createUdpSocketFactory(PlatformType platform) {
    switch (platform) {
    case PlatformType::WINDOWS:
    case PlatformType::LINUX:
        return std::make_unique<WindowsUdpSocketFactory>();

    case PlatformType::EMBEDDED:
#ifdef USE_LWIP
        return std::make_unique<LwipUdpSocketFactory>();
#else
        std::cerr << "[ERROR] NetworkFactory: lwip support not compiled in"
                  << std::endl;
        return nullptr;
#endif

    default:
        std::cerr << "[ERROR] NetworkFactory: Unsupported platform type"
                  << std::endl;
        return nullptr;
    }
}

std::unique_ptr<NetworkManager>
NetworkFactory::createNetworkManager(PlatformType platform) {
    auto manager = std::make_unique<NetworkManager>();
    auto factory = createUdpSocketFactory(platform);

    if (!factory) {
        std::cerr << "[ERROR] NetworkFactory: Failed to create socket factory "
                     "for platform: "
                  << getPlatformName(platform) << std::endl;
        return nullptr;
    }

    if (!manager->initialize(std::move(factory))) {
        std::cerr << "[ERROR] NetworkFactory: Failed to initialize network "
                     "manager for platform: "
                  << getPlatformName(platform) << std::endl;
        return nullptr;
    }

    std::cout << "[INFO] NetworkFactory: Created network manager for platform: "
              << getPlatformName(platform) << std::endl;

    return manager;
}

std::string NetworkFactory::getPlatformName(PlatformType platform) {
    switch (platform) {
    case PlatformType::WINDOWS:
        return "Windows";
    case PlatformType::LINUX:
        return "Linux";
    case PlatformType::EMBEDDED:
        return "Embedded (lwip)";
    default:
        return "Unknown";
    }
}

bool NetworkFactory::isPlatformSupported(PlatformType platform) {
    switch (platform) {
    case PlatformType::WINDOWS:
    case PlatformType::LINUX:
        return true;

    case PlatformType::EMBEDDED:
#ifdef USE_LWIP
        return true;
#else
        return false;
#endif

    default:
        return false;
    }
}

} // namespace Network
} // namespace HAL