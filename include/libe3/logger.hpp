/**
 * @file logger.hpp
 * @brief Logging facility for libe3
 *
 * Provides a lightweight, thread-safe logging mechanism that can be
 * configured at runtime. Supports integration with external logging
 * frameworks through callbacks.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_LOGGER_HPP
#define LIBE3_LOGGER_HPP

#include <string>
#include <functional>
#include <memory>
#include <sstream>
#include <mutex>
#include <cstdio>
#include <ctime>
#include <chrono>

namespace libe3 {

/**
 * @brief Logging levels
 */
enum class LogLevel : int {
    NONE = 0,
    ERROR = 1,
    WARN = 2,
    INFO = 3,
    DEBUG = 4,
    TRACE = 5
};

/**
 * @brief Log callback function type
 *
 * Allows external logging frameworks to receive log messages.
 * @param level Log level
 * @param component Component name (e.g., "E3Agent", "Connector")
 * @param message Log message
 */
using LogCallback = std::function<void(LogLevel level, const std::string& component, const std::string& message)>;

/**
 * @brief Logger singleton class
 */
class Logger {
public:
    /**
     * @brief Get the singleton logger instance
     */
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    /**
     * @brief Set the log level
     */
    void set_level(LogLevel level) noexcept {
        level_ = level;
    }

    /**
     * @brief Set the log level from integer
     */
    void set_level(int level) noexcept {
        level_ = static_cast<LogLevel>(level);
    }

    /**
     * @brief Get current log level
     */
    LogLevel level() const noexcept {
        return level_;
    }

    /**
     * @brief Set external log callback
     *
     * If set, all log messages will be forwarded to this callback
     * instead of being written to stderr.
     */
    void set_callback(LogCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = std::move(callback);
    }

    /**
     * @brief Clear external log callback
     */
    void clear_callback() {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = nullptr;
    }

    /**
     * @brief Set log file path (append mode)
     *
     * If set, logs are written to this file instead of stderr
     * when no callback is configured.
     */
    void set_log_file(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_) {
            std::fclose(file_);
            file_ = nullptr;
        }

        if (!path.empty()) {
            file_ = std::fopen(path.c_str(), "a");
            if (file_) {
                std::setvbuf(file_, nullptr, _IOLBF, 0);
            } else {
                std::fprintf(stderr, "[Logger] Failed to open log file %s\n", path.c_str());
            }
        }
    }

    /**
     * @brief Check if a level should be logged
     */
    bool should_log(LogLevel level) const noexcept {
        return static_cast<int>(level) <= static_cast<int>(level_);
    }

    /**
     * @brief Log a message
     */
    void log(LogLevel level, const std::string& component, const std::string& message) {
        if (!should_log(level)) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        
        if (callback_) {
            callback_(level, component, message);
        } else {
            FILE* out = file_ ? file_ : stderr;
            // Default: write to stderr with timestamp
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
            
            char time_buf[32];
            std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
            
            std::fprintf(out, "[%s.%03d] [%s] [%s] %s\n",
                    time_buf, static_cast<int>(ms.count()),
                    level_to_string(level), component.c_str(), message.c_str());
            std::fflush(out);
        }
    }

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static const char* level_to_string(LogLevel level) noexcept {
        switch (level) {
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::TRACE: return "TRACE";
            default: return "?????";
        }
    }

    LogLevel level_{LogLevel::INFO};
    LogCallback callback_;
    FILE* file_{nullptr};
    std::mutex mutex_;
};

/**
 * @brief Log stream helper for efficient message construction
 */
class LogStream {
public:
    LogStream(LogLevel level, std::string component)
        : level_(level), component_(std::move(component)) {}
    
    ~LogStream() {
        if (!message_.str().empty()) {
            Logger::instance().log(level_, component_, message_.str());
        }
    }

    template<typename T>
    LogStream& operator<<(const T& value) {
        message_ << value;
        return *this;
    }

private:
    LogLevel level_;
    std::string component_;
    std::ostringstream message_;
};

} // namespace libe3

// Convenience macros for logging
#define E3_LOG(level, component) \
    if (::libe3::Logger::instance().should_log(level)) \
        ::libe3::LogStream(level, component)

#define E3_LOG_ERROR(component) E3_LOG(::libe3::LogLevel::ERROR, component)
#define E3_LOG_WARN(component)  E3_LOG(::libe3::LogLevel::WARN, component)
#define E3_LOG_INFO(component)  E3_LOG(::libe3::LogLevel::INFO, component)
#define E3_LOG_DEBUG(component) E3_LOG(::libe3::LogLevel::DEBUG, component)
#define E3_LOG_TRACE(component) E3_LOG(::libe3::LogLevel::TRACE, component)

#endif // LIBE3_LOGGER_HPP
