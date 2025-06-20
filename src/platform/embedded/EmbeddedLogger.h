#pragma once
#include "../../interface/ILogger.h"

/**
 * @brief 嵌入式平台的简单日志实现
 * 使用printf输出，适用于资源受限的嵌入式环境
 */
class EmbeddedLogger : public ILogger {
  public:
    EmbeddedLogger();
    ~EmbeddedLogger() override = default;

    // ILogger接口实现
    void setLogLevel(LogLevel level) override;
    void enableFileLogging(const std::string &filename) override;
    void disableFileLogging() override;
    void log(LogLevel level, const std::string &tag,
             const std::string &message) override;

    void v(const std::string &tag, const std::string &message) override;
    void d(const std::string &tag, const std::string &message) override;
    void i(const std::string &tag, const std::string &message) override;
    void w(const std::string &tag, const std::string &message) override;
    void e(const std::string &tag, const std::string &message) override;

  private:
    const char *logLevelToString(LogLevel level);
    bool shouldLog(LogLevel level);

    LogLevel m_currentLogLevel;
    unsigned long m_logCounter;
};