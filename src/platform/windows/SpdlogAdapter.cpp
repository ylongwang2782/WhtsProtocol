#include "SpdlogAdapter.h"
#include <cstdarg>
#include <cstdio>
#include <spdlog/pattern_formatter.h>

// 实现自定义格式化器
void UppercaseLevelFormatter::format(const spdlog::details::log_msg &msg,
                                     const std::tm &,
                                     spdlog::memory_buf_t &dest) {
    std::string level_name;
    switch (msg.level) {
    case spdlog::level::trace:
        level_name = "TRACE";
        break;
    case spdlog::level::debug:
        level_name = "DEBUG";
        break;
    case spdlog::level::info:
        level_name = "INFO";
        break;
    case spdlog::level::warn:
        level_name = "WARN";
        break;
    case spdlog::level::err:
        level_name = "ERROR";
        break;
    case spdlog::level::critical:
        level_name = "CRITICAL";
        break;
    default:
        level_name = "UNKNOWN";
        break;
    }
    dest.append(level_name.data(), level_name.data() + level_name.size());
}

std::unique_ptr<spdlog::custom_flag_formatter>
UppercaseLevelFormatter::clone() const {
    return spdlog::details::make_unique<UppercaseLevelFormatter>();
}

SpdlogAdapter::SpdlogAdapter()
    : m_fileLoggingEnabled(false), m_currentLogLevel(LogLevel::VERBOSE) {
    initializeLogger();
}

void SpdlogAdapter::initializeLogger() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 创建控制台sink
    m_consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    // 创建logger，初始只有控制台输出
    std::vector<spdlog::sink_ptr> sinks = {m_consoleSink};
    m_logger = std::make_shared<spdlog::logger>("WhtsProtocol", sinks.begin(),
                                                sinks.end());

    // 设置日志格式：[时间] [级别] [标签] 消息
    m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

    // 创建自定义格式化器以显示大写的日志级别
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<UppercaseLevelFormatter>('U').set_pattern(
        "[%Y-%m-%d %H:%M:%S.%e] [%^%U%$] [%n] %v");
    m_logger->set_formatter(std::move(formatter));

    // 设置日志级别
    m_logger->set_level(convertLogLevel(m_currentLogLevel));

    // 注册logger
    spdlog::register_logger(m_logger);
}

void SpdlogAdapter::recreateLogger() {
    std::vector<spdlog::sink_ptr> sinks = {m_consoleSink};

    if (m_fileLoggingEnabled && m_fileSink) {
        sinks.push_back(m_fileSink);
    }

    m_logger = std::make_shared<spdlog::logger>("WhtsProtocol", sinks.begin(),
                                                sinks.end());

    // 创建自定义格式化器以显示大写的日志级别
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<UppercaseLevelFormatter>('U').set_pattern(
        "[%Y-%m-%d %H:%M:%S.%e] [%^%U%$] [%n] %v");
    m_logger->set_formatter(std::move(formatter));

    m_logger->set_level(convertLogLevel(m_currentLogLevel));

    spdlog::register_logger(m_logger);
}

void SpdlogAdapter::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentLogLevel = level;
    if (m_logger) {
        m_logger->set_level(convertLogLevel(level));
    }
}

void SpdlogAdapter::enableFileLogging(const std::string &filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        m_logFilename = filename;
        m_fileSink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
        m_fileLoggingEnabled = true;
        recreateLogger();
    } catch (const std::exception &e) {
        // 文件日志启用失败，只使用控制台日志
        m_fileLoggingEnabled = false;
        if (m_logger) {
            m_logger->error("Failed to enable file logging: {}", e.what());
        }
    }
}

void SpdlogAdapter::disableFileLogging() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fileLoggingEnabled = false;
    m_fileSink.reset();
    recreateLogger();
}

void SpdlogAdapter::log(LogLevel level, const std::string &tag,
                        const std::string &message) {
    if (!m_logger)
        return;

    // 创建带标签的logger名称用于这次日志
    auto taggedLogger = m_logger->clone(tag);

    switch (level) {
    case LogLevel::VERBOSE:
        taggedLogger->trace(message);
        break;
    case LogLevel::DEBUG:
        taggedLogger->debug(message);
        break;
    case LogLevel::INFO:
        taggedLogger->info(message);
        break;
    case LogLevel::WARN:
        taggedLogger->warn(message);
        break;
    case LogLevel::ERR:
        taggedLogger->error(message);
        break;
    }
}

void SpdlogAdapter::v(const std::string &tag, const std::string &message) {
    log(LogLevel::VERBOSE, tag, message);
}

void SpdlogAdapter::d(const std::string &tag, const std::string &message) {
    log(LogLevel::DEBUG, tag, message);
}

void SpdlogAdapter::i(const std::string &tag, const std::string &message) {
    log(LogLevel::INFO, tag, message);
}

void SpdlogAdapter::w(const std::string &tag, const std::string &message) {
    log(LogLevel::WARN, tag, message);
}

void SpdlogAdapter::e(const std::string &tag, const std::string &message) {
    log(LogLevel::ERR, tag, message);
}

spdlog::level::level_enum SpdlogAdapter::convertLogLevel(LogLevel level) {
    switch (level) {
    case LogLevel::VERBOSE:
        return spdlog::level::trace;
    case LogLevel::DEBUG:
        return spdlog::level::debug;
    case LogLevel::INFO:
        return spdlog::level::info;
    case LogLevel::WARN:
        return spdlog::level::warn;
    case LogLevel::ERR:
        return spdlog::level::err;
    default:
        return spdlog::level::info;
    }
}