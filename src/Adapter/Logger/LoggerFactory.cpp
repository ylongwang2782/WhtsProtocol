#include "LoggerFactory.h"

// 根据平台选择不同的实现
#ifdef _WIN32
#include "../../platform/windows/SpdlogAdapter.h"
#define USE_SPDLOG 1
#elif defined(__linux__) || defined(__APPLE__)
// 在Linux/Mac平台可以选择使用spdlog或其他实现
#include "../../platform/windows/SpdlogAdapter.h"
#define USE_SPDLOG 1
#elif defined(EMBEDDED_PLATFORM)
// 嵌入式平台使用自定义实现
#include "../../platform/embedded/EmbeddedLogger.h"
#define USE_EMBEDDED 1
#else
// 默认使用spdlog
#include "../../platform/windows/SpdlogAdapter.h"
#define USE_SPDLOG 1
#endif

std::unique_ptr<ILogger> LoggerFactory::s_instance = nullptr;

std::unique_ptr<ILogger> LoggerFactory::createLogger() {
#if defined(USE_SPDLOG)
    return std::make_unique<SpdlogAdapter>();
#elif defined(USE_EMBEDDED)
    return std::make_unique<EmbeddedLogger>();
#else
    return std::make_unique<SpdlogAdapter>();
#endif
}

ILogger &LoggerFactory::getInstance() {
    if (!s_instance) {
        s_instance = createLogger();
    }
    return *s_instance;
}