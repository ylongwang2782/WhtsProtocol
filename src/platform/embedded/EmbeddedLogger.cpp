#include "EmbeddedLogger.h"
#include <cstdio>

EmbeddedLogger::EmbeddedLogger()
    : m_currentLogLevel(LogLevel::VERBOSE), m_logCounter(0) {}

void EmbeddedLogger::setLogLevel(LogLevel level) { m_currentLogLevel = level; }

void EmbeddedLogger::enableFileLogging(const std::string &filename) {
    // 嵌入式环境通常不支持文件系统，这里只是示例
    printf("[%lu][INFO][EmbeddedLogger] File logging not supported in embedded "
           "environment: %s\n",
           ++m_logCounter, filename.c_str());
}

void EmbeddedLogger::disableFileLogging() {
    printf("[%lu][INFO][EmbeddedLogger] File logging disabled\n",
           ++m_logCounter);
}

void EmbeddedLogger::log(LogLevel level, const std::string &tag,
                         const std::string &message) {
    if (!shouldLog(level)) {
        return;
    }

    printf("[%lu][%s][%s] %s\n", ++m_logCounter, logLevelToString(level),
           tag.c_str(), message.c_str());
}

void EmbeddedLogger::v(const std::string &tag, const std::string &message) {
    log(LogLevel::VERBOSE, tag, message);
}

void EmbeddedLogger::d(const std::string &tag, const std::string &message) {
    log(LogLevel::DEBUG, tag, message);
}

void EmbeddedLogger::i(const std::string &tag, const std::string &message) {
    log(LogLevel::INFO, tag, message);
}

void EmbeddedLogger::w(const std::string &tag, const std::string &message) {
    log(LogLevel::WARN, tag, message);
}

void EmbeddedLogger::e(const std::string &tag, const std::string &message) {
    log(LogLevel::ERR, tag, message);
}

const char *EmbeddedLogger::logLevelToString(LogLevel level) {
    switch (level) {
    case LogLevel::VERBOSE:
        return "VERB";
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARN:
        return "WARN";
    case LogLevel::ERR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

bool EmbeddedLogger::shouldLog(LogLevel level) {
    return static_cast<int>(level) >= static_cast<int>(m_currentLogLevel);
}