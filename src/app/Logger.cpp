#include "Logger.h"
#include <cstdarg>
#include <cstdio>

namespace {
std::string formatString(const char *format, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    int size = std::vsnprintf(nullptr, 0, format, args_copy) + 1;
    va_end(args_copy);

    if (size <= 0) {
        return std::string(format);
    }

    std::string buffer(size, '\0');
    std::vsnprintf(&buffer[0], size, format, args);
    buffer.pop_back(); // Remove trailing null character
    return buffer;
}
} // namespace

Logger &Logger::getInstance() {
    static Logger instance;
    return instance;
}

ILogger &Logger::getLoggerImpl() { return LoggerFactory::getInstance(); }

void Logger::setLogLevel(LogLevel level) { getLoggerImpl().setLogLevel(level); }

void Logger::enableFileLogging(const std::string &filename) {
    getLoggerImpl().enableFileLogging(filename);
}

void Logger::disableFileLogging() { getLoggerImpl().disableFileLogging(); }

void Logger::log(LogLevel level, const std::string &tag,
                 const std::string &message) {
    getLoggerImpl().log(level, tag, message);
}

void Logger::v(const std::string &tag, const std::string &message) {
    getLoggerImpl().v(tag, message);
}

void Logger::d(const std::string &tag, const std::string &message) {
    getLoggerImpl().d(tag, message);
}

void Logger::i(const std::string &tag, const std::string &message) {
    getLoggerImpl().i(tag, message);
}

void Logger::w(const std::string &tag, const std::string &message) {
    getLoggerImpl().w(tag, message);
}

void Logger::e(const std::string &tag, const std::string &message) {
    getLoggerImpl().e(tag, message);
}

void Logger::v(const std::string &tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    getLoggerImpl().v(tag, formatString(format, args));
    va_end(args);
}

void Logger::d(const std::string &tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    getLoggerImpl().d(tag, formatString(format, args));
    va_end(args);
}

void Logger::i(const std::string &tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    getLoggerImpl().i(tag, formatString(format, args));
    va_end(args);
}

void Logger::w(const std::string &tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    getLoggerImpl().w(tag, formatString(format, args));
    va_end(args);
}

void Logger::e(const std::string &tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    getLoggerImpl().e(tag, formatString(format, args));
    va_end(args);
}

// Log class static method implementations
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

void Log::v(const std::string &tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    Logger::getInstance().v(tag, formatString(format, args));
    va_end(args);
}

void Log::d(const std::string &tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    Logger::getInstance().d(tag, formatString(format, args));
    va_end(args);
}

void Log::i(const std::string &tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    Logger::getInstance().i(tag, formatString(format, args));
    va_end(args);
}

void Log::w(const std::string &tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    Logger::getInstance().w(tag, formatString(format, args));
    va_end(args);
}

void Log::e(const std::string &tag, const char *format, ...) {
    va_list args;
    va_start(args, format);
    Logger::getInstance().e(tag, formatString(format, args));
    va_end(args);
}

void Log::setLogLevel(LogLevel level) {
    Logger::getInstance().setLogLevel(level);
}

void Log::enableFileLogging(const std::string &filename) {
    Logger::getInstance().enableFileLogging(filename);
}

void Log::disableFileLogging() { Logger::getInstance().disableFileLogging(); }