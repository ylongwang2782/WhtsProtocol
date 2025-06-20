#pragma once
#include <string>

enum class LogLevel { VERBOSE = 0, DEBUG = 1, INFO = 2, WARN = 3, ERR = 4 };

/**
 * @brief 抽象日志接口
 * 定义了日志系统的基本接口，可以在不同平台上有不同的实现
 */
class ILogger {
  public:
    virtual ~ILogger() = default;

    // 基本日志功能
    virtual void setLogLevel(LogLevel level) = 0;
    virtual void enableFileLogging(const std::string &filename) = 0;
    virtual void disableFileLogging() = 0;

    // 基础日志方法
    virtual void log(LogLevel level, const std::string &tag,
                     const std::string &message) = 0;

    // 便利方法
    virtual void v(const std::string &tag, const std::string &message) = 0;
    virtual void d(const std::string &tag, const std::string &message) = 0;
    virtual void i(const std::string &tag, const std::string &message) = 0;
    virtual void w(const std::string &tag, const std::string &message) = 0;
    virtual void e(const std::string &tag, const std::string &message) = 0;
};