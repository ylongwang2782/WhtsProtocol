#pragma once
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>


// 检查是否支持 C++20 的 std::format
#if __cplusplus >= 202002L && __has_include(<format>)
#include <format>
#define HAS_STD_FORMAT 1
#else
#define HAS_STD_FORMAT 0
#include <cstdio>
#include <memory>
#endif

enum class LogLevel { VERBOSE = 0, DEBUG = 1, INFO = 2, WARN = 3, ERROR = 4 };

class Logger {
  public:
    static Logger &getInstance();

    void setLogLevel(LogLevel level);
    void enableFileLogging(const std::string &filename);
    void disableFileLogging();

    void log(LogLevel level, const std::string &tag,
             const std::string &message);

    // 支持参数的日志方法
#if HAS_STD_FORMAT
    template <typename... Args>
    void log(LogLevel level, const std::string &tag, const std::string &format,
             Args &&...args) {
        if (level < currentLogLevel) {
            return;
        }
        std::string message = std::format(format, std::forward<Args>(args)...);
        log(level, tag, message);
    }
#else
    template <typename... Args>
    void log(LogLevel level, const std::string &tag, const std::string &format,
             Args &&...args) {
        if (level < currentLogLevel) {
            return;
        }
        std::string message = formatString(format, std::forward<Args>(args)...);
        log(level, tag, message);
    }
#endif

    // 便捷方法 - 原有的字符串版本
    void v(const std::string &tag, const std::string &message);
    void d(const std::string &tag, const std::string &message);
    void i(const std::string &tag, const std::string &message);
    void w(const std::string &tag, const std::string &message);
    void e(const std::string &tag, const std::string &message);

    // 便捷方法 - 支持参数的版本
    template <typename... Args>
    void v(const std::string &tag, const std::string &format, Args &&...args) {
        log(LogLevel::VERBOSE, tag, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void d(const std::string &tag, const std::string &format, Args &&...args) {
        log(LogLevel::DEBUG, tag, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void i(const std::string &tag, const std::string &format, Args &&...args) {
        log(LogLevel::INFO, tag, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void w(const std::string &tag, const std::string &format, Args &&...args) {
        log(LogLevel::WARN, tag, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void e(const std::string &tag, const std::string &format, Args &&...args) {
        log(LogLevel::ERROR, tag, format, std::forward<Args>(args)...);
    }

  private:
    Logger() = default;
    ~Logger();
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    std::string getCurrentTimestamp();
    std::string levelToString(LogLevel level);

#if !HAS_STD_FORMAT
    template <typename... Args>
    std::string formatString(const std::string &format, Args &&...args) {
        int size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
        if (size <= 0) {
            return format; // 格式化失败，返回原始格式字符串
        }
        std::unique_ptr<char[]> buf(new char[size]);
        std::snprintf(buf.get(), size, format.c_str(), args...);
        return std::string(buf.get(), buf.get() + size - 1);
    }
#endif

    LogLevel currentLogLevel = LogLevel::VERBOSE;
    std::ofstream logFile;
    bool fileLoggingEnabled = false;
    std::mutex logMutex;
};

// 全局便捷访问
class Log {
  public:
    static void v(const std::string &tag, const std::string &message);
    static void d(const std::string &tag, const std::string &message);
    static void i(const std::string &tag, const std::string &message);
    static void w(const std::string &tag, const std::string &message);
    static void e(const std::string &tag, const std::string &message);

    // 支持参数的全局便捷方法
    template <typename... Args>
    static void v(const std::string &tag, const std::string &format,
                  Args &&...args) {
        Logger::getInstance().v(tag, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void d(const std::string &tag, const std::string &format,
                  Args &&...args) {
        Logger::getInstance().d(tag, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void i(const std::string &tag, const std::string &format,
                  Args &&...args) {
        Logger::getInstance().i(tag, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void w(const std::string &tag, const std::string &format,
                  Args &&...args) {
        Logger::getInstance().w(tag, format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void e(const std::string &tag, const std::string &format,
                  Args &&...args) {
        Logger::getInstance().e(tag, format, std::forward<Args>(args)...);
    }

    static void setLogLevel(LogLevel level);
    static void enableFileLogging(const std::string &filename);
    static void disableFileLogging();
};