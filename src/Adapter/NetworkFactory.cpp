#include "NetworkFactory.h"
#include "../platform/embedded/LwipUdpSocket.h"
#include "../platform/windows/AsioUdpSocket.h"
#include "../platform/windows/WindowsUdpSocket.h"
#include <iostream>

namespace Adapter {

PlatformType NetworkFactory::getCurrentPlatform() {
#ifdef USE_LWIP
    return PlatformType::EMBEDDED;
#elif defined(USE_ASIO)
#if defined(_WIN32)
    return PlatformType::WINDOWS_ASIO;
#elif defined(__linux__)
    return PlatformType::LINUX_ASIO;
#else
    return PlatformType::WINDOWS_ASIO; // 默认ASIO实现
#endif
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
        return std::make_unique<Platform::Windows::WindowsUdpSocketFactory>();

    case PlatformType::WINDOWS_ASIO:
    case PlatformType::LINUX_ASIO:
#ifdef USE_ASIO
        return std::make_unique<Platform::Windows::AsioUdpSocketFactory>();
#else
        std::cerr << "[ERROR] NetworkFactory: ASIO support not compiled in"
                  << std::endl;
        return nullptr;
#endif

    case PlatformType::EMBEDDED:
#ifdef USE_LWIP
        return std::make_unique<Platform::Embedded::LwipUdpSocketFactory>();
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
        return "Windows (Native Socket)";
    case PlatformType::WINDOWS_ASIO:
        return "Windows (ASIO)";
    case PlatformType::LINUX:
        return "Linux (Native Socket)";
    case PlatformType::LINUX_ASIO:
        return "Linux (ASIO)";
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

    case PlatformType::WINDOWS_ASIO:
    case PlatformType::LINUX_ASIO:
#ifdef USE_ASIO
        return true;
#else
        return false;
#endif

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

} // namespace Adapter