#pragma once
#include "../interface/ILogger.h"
#include "LoggerFactory.h"
#include <string>

// 重新定义LogLevel以保持兼容性（如果需要的话）
using LogLevel = ::LogLevel;

/**
 * @brief 日志管理器类
 * 使用工厂模式创建平台特定的日志实现
 */
class Logger {
  public:
    static Logger &getInstance();

    void setLogLevel(LogLevel level);
    void enableFileLogging(const std::string &filename);
    void disableFileLogging();

    void log(LogLevel level, const std::string &tag,
             const std::string &message);

    // 便利方法 - 字符串版本
    void v(const std::string &tag, const std::string &message);
    void d(const std::string &tag, const std::string &message);
    void i(const std::string &tag, const std::string &message);
    void w(const std::string &tag, const std::string &message);
    void e(const std::string &tag, const std::string &message);

    // 格式化版本
    void v(const std::string &tag, const char *format, ...);
    void d(const std::string &tag, const char *format, ...);
    void i(const std::string &tag, const char *format, ...);
    void w(const std::string &tag, const char *format, ...);
    void e(const std::string &tag, const char *format, ...);

    ILogger &getLoggerImpl();

  private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
};

/**
 * @brief 静态日志接口类
 * 提供与原有Log类相同的静态接口，保持向后兼容性
 */
class Log {
  public:
    static void v(const std::string &tag, const std::string &message);
    static void d(const std::string &tag, const std::string &message);
    static void i(const std::string &tag, const std::string &message);
    static void w(const std::string &tag, const std::string &message);
    static void e(const std::string &tag, const std::string &message);

    // 格式化版本
    static void v(const std::string &tag, const char *format, ...);
    static void d(const std::string &tag, const char *format, ...);
    static void i(const std::string &tag, const char *format, ...);
    static void w(const std::string &tag, const char *format, ...);
    static void e(const std::string &tag, const char *format, ...);

    static void setLogLevel(LogLevel level);
    static void enableFileLogging(const std::string &filename);
    static void disableFileLogging();
};