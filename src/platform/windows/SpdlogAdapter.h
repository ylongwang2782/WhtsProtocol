#pragma once
#include "../../interface/ILogger.h"
#include <cstdarg>
#include <memory>
#include <mutex>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

/**
 * @brief 自定义格式化器，用于显示大写的日志级别
 */
class UppercaseLevelFormatter : public spdlog::custom_flag_formatter {
  public:
    void format(const spdlog::details::log_msg &msg, const std::tm &,
                spdlog::memory_buf_t &dest) override;
    std::unique_ptr<custom_flag_formatter> clone() const override;
};

/**
 * @brief spdlog实现的日志适配器
 * 在Windows平台上使用spdlog作为底层日志实现
 */
class SpdlogAdapter : public ILogger {
  public:
    SpdlogAdapter();
    ~SpdlogAdapter() override = default;

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
    void initializeLogger();
    void recreateLogger();
    spdlog::level::level_enum convertLogLevel(LogLevel level);

    std::shared_ptr<spdlog::logger> m_logger;
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> m_consoleSink;
    std::shared_ptr<spdlog::sinks::basic_file_sink_mt> m_fileSink;

    bool m_fileLoggingEnabled;
    std::string m_logFilename;
    LogLevel m_currentLogLevel;
    std::mutex m_mutex;
};