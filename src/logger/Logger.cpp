#include "Logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

Logger &Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(logMutex);
    currentLogLevel = level;
}

void Logger::enableFileLogging(const std::string &filename) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.close();
    }
    logFile.open(filename, std::ios::app);
    fileLoggingEnabled = logFile.is_open();
}

void Logger::disableFileLogging() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.close();
    }
    fileLoggingEnabled = false;
}

void Logger::log(LogLevel level, const std::string &tag,
                 const std::string &message) {
    if (level < currentLogLevel) {
        return;
    }

    std::lock_guard<std::mutex> lock(logMutex);

    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = levelToString(level);

    std::ostringstream oss;
    oss << "[" << timestamp << "] [" << levelStr << "] [" << tag << "] "
        << message;

    std::string logMessage = oss.str();

    // 输出到控制台
    std::cout << logMessage << std::endl;

    // 输出到文件（如果启用）
    if (fileLoggingEnabled && logFile.is_open()) {
        logFile << logMessage << std::endl;
        logFile.flush();
    }
}

void Logger::v(const std::string &tag, const std::string &message) {
    log(LogLevel::VERBOSE, tag, message);
}

void Logger::d(const std::string &tag, const std::string &message) {
    log(LogLevel::DEBUG, tag, message);
}

void Logger::i(const std::string &tag, const std::string &message) {
    log(LogLevel::INFO, tag, message);
}

void Logger::w(const std::string &tag, const std::string &message) {
    log(LogLevel::WARN, tag, message);
}

void Logger::e(const std::string &tag, const std::string &message) {
    log(LogLevel::ERROR, tag, message);
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
    case LogLevel::VERBOSE:
        return "VERBOSE";
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARN:
        return "WARN";
    case LogLevel::ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

// Log 类的静态方法实现
void Log::v(const std::string &tag, const std::string &message) {
    Logger::getInstance().v(tag, message);
}

void Log::d(const std::string &tag, const std::string &message) {
    Logger::getInstance().d(tag, message);
}

void Log::i(const std::string &tag, const std::string &message) {
    Logger::getInstance().i(tag, message);
}

void Log::w(const std::string &tag, const std::string &message) {
    Logger::getInstance().w(tag, message);
}

void Log::e(const std::string &tag, const std::string &message) {
    Logger::getInstance().e(tag, message);
}

void Log::setLogLevel(LogLevel level) {
    Logger::getInstance().setLogLevel(level);
}

void Log::enableFileLogging(const std::string &filename) {
    Logger::getInstance().enableFileLogging(filename);
}

void Log::disableFileLogging() { Logger::getInstance().disableFileLogging(); }