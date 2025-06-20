#pragma once
#include "../Adapter/LoggerFactory.h"
#include "../interface/ILogger.h"
#include <string>

// Redefine LogLevel for compatibility
using LogLevel = ::LogLevel;

/**
 * @brief Logger manager class
 * Uses factory pattern to create platform-specific logger implementations
 */
class Logger {
  public:
    static Logger &getInstance();

    void setLogLevel(LogLevel level);
    void enableFileLogging(const std::string &filename);
    void disableFileLogging();

    void log(LogLevel level, const std::string &tag,
             const std::string &message);

    // Convenience methods - string version
    void v(const std::string &tag, const std::string &message);
    void d(const std::string &tag, const std::string &message);
    void i(const std::string &tag, const std::string &message);
    void w(const std::string &tag, const std::string &message);
    void e(const std::string &tag, const std::string &message);

    // Formatted version
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
 * @brief Static logger interface class
 * Provides the same static interface as the original Log class for backward
 * compatibility
 */
class Log {
  public:
    static void v(const std::string &tag, const std::string &message);
    static void d(const std::string &tag, const std::string &message);
    static void i(const std::string &tag, const std::string &message);
    static void w(const std::string &tag, const std::string &message);
    static void e(const std::string &tag, const std::string &message);

    // Formatted version
    static void v(const std::string &tag, const char *format, ...);
    static void d(const std::string &tag, const char *format, ...);
    static void i(const std::string &tag, const char *format, ...);
    static void w(const std::string &tag, const char *format, ...);
    static void e(const std::string &tag, const char *format, ...);

    static void setLogLevel(LogLevel level);
    static void enableFileLogging(const std::string &filename);
    static void disableFileLogging();
};