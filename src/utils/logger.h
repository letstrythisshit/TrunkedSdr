#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <iostream>
#include <fstream>
#include <mutex>
#include <sstream>
#include <ctime>
#include <iomanip>

namespace TrunkSDR {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

class Logger {
public:
    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    void setLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        log_level_ = level;
    }

    void setLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        log_file_.open(filename, std::ios::app);
    }

    void log(LogLevel level, const std::string& message) {
        if (level < log_level_) return;

        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ["
            << levelToString(level) << "] " << message << std::endl;

        std::string log_msg = oss.str();
        std::cout << log_msg;

        if (log_file_.is_open()) {
            log_file_ << log_msg;
            log_file_.flush();
        }
    }

    template<typename... Args>
    void debug(Args&&... args) {
        log(LogLevel::DEBUG, formatMessage(std::forward<Args>(args)...));
    }

    template<typename... Args>
    void info(Args&&... args) {
        log(LogLevel::INFO, formatMessage(std::forward<Args>(args)...));
    }

    template<typename... Args>
    void warning(Args&&... args) {
        log(LogLevel::WARNING, formatMessage(std::forward<Args>(args)...));
    }

    template<typename... Args>
    void error(Args&&... args) {
        log(LogLevel::ERROR, formatMessage(std::forward<Args>(args)...));
    }

    template<typename... Args>
    void critical(Args&&... args) {
        log(LogLevel::CRITICAL, formatMessage(std::forward<Args>(args)...));
    }

private:
    Logger() : log_level_(LogLevel::INFO) {}
    ~Logger() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:    return "DEBUG";
            case LogLevel::INFO:     return "INFO";
            case LogLevel::WARNING:  return "WARN";
            case LogLevel::ERROR:    return "ERROR";
            case LogLevel::CRITICAL: return "CRIT";
            default:                 return "UNKNOWN";
        }
    }

    template<typename T>
    std::string formatMessage(T&& arg) {
        std::ostringstream oss;
        oss << arg;
        return oss.str();
    }

    template<typename T, typename... Args>
    std::string formatMessage(T&& first, Args&&... args) {
        std::ostringstream oss;
        oss << first;
        ((oss << " " << args), ...);
        return oss.str();
    }

    LogLevel log_level_;
    std::ofstream log_file_;
    std::mutex mutex_;
};

// Convenience macros
#define LOG_DEBUG(...) TrunkSDR::Logger::instance().debug(__VA_ARGS__)
#define LOG_INFO(...) TrunkSDR::Logger::instance().info(__VA_ARGS__)
#define LOG_WARNING(...) TrunkSDR::Logger::instance().warning(__VA_ARGS__)
#define LOG_ERROR(...) TrunkSDR::Logger::instance().error(__VA_ARGS__)
#define LOG_CRITICAL(...) TrunkSDR::Logger::instance().critical(__VA_ARGS__)

} // namespace TrunkSDR

#endif // LOGGER_H
