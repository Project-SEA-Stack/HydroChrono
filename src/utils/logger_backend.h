/**
 * @file logging_backend.h
 * @brief Core backend for the HydroChrono logging system
 * 
 * This header provides the LoggerBackend class which handles all file I/O,
 * timestamping, thread safety, and message formatting for the logging system.
 * It is designed to be composable and RAII-safe.
 */

#pragma once

#include <hydroc/logging.h>
#include <chrono>
#include <fstream>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

namespace hydroc {

/**
 * @brief Core backend for the logging system
 * 
 * Handles all low-level logging operations including file I/O, thread safety,
 * message formatting, and timestamp generation. This class is designed to be
 * shared between different logging frontends (CLI, Debug, etc.).
 * 
 * @note This class is thread-safe and RAII-compliant
 * @note All file operations are protected by a mutex
 * @note The backend can be shared across multiple frontend instances
 */
class LoggerBackend {
public:
    /**
     * @brief Constructor
     * @param config Configuration for the backend
     */
    explicit LoggerBackend(const LoggingConfig& config);
    
    /**
     * @brief Destructor
     * 
     * Ensures proper cleanup of file handles and resources
     */
    ~LoggerBackend();
    
    // Non-copyable
    LoggerBackend(const LoggerBackend&) = delete;
    LoggerBackend& operator=(const LoggerBackend&) = delete;
    
    // Movable
    LoggerBackend(LoggerBackend&&) noexcept;
    LoggerBackend& operator=(LoggerBackend&&) noexcept;
    
    //-----------------------------------------------------------------------------
    // Core Logging Interface
    //-----------------------------------------------------------------------------
    
    /**
     * @brief Log a message with the specified level and context
     * @param level Log level for the message
     * @param message Message content
     * @param context Optional context information
     * @param color Console color (ignored for file output)
     */
    void Log(LogLevel level, const std::string& message, 
             const LogContext& context = LogContext{}, 
             LogColor color = LogColor::White);
    
     /**
      * @brief Check if a log level should be output
      * @param level Level to check
      * @param is_console_output If true, applies console threshold; otherwise file threshold
      * @return true if the level should be output
      */
    bool ShouldLog(LogLevel level, bool is_console_output) const;
    
    /**
     * @brief Get the current configuration
     * @return Current logging configuration
     */
    const LoggingConfig& GetConfig() const;
    
    /**
     * @brief Update the logging configuration
     * @param config New configuration
     */
    void UpdateConfig(const LoggingConfig& config);
    
    //-----------------------------------------------------------------------------
    // File Management
    //-----------------------------------------------------------------------------
    
    /**
     * @brief Check if file logging is enabled and active
     * @return true if file logging is available
     */
    bool IsFileLoggingEnabled() const;
    
    /**
     * @brief Get the current log file path
     * @return Path to the current log file, or empty string if not enabled
     */
    std::string GetLogFilePath() const;
    
    /**
     * @brief Flush any pending writes to disk
     */
    void Flush();
    
    /**
     * @brief Rotate the log file (create new file, close old one)
     * @param new_path Optional new file path (uses default if empty)
     * @return true if rotation was successful
     */
    bool RotateLogFile(const std::string& new_path = "");
    
    //-----------------------------------------------------------------------------
    // System Information
    //-----------------------------------------------------------------------------
    
    /**
     * @brief Get system information for logging
     * @return Formatted string with system details
     */
    std::string GetSystemInfo() const;
    
    /**
     * @brief Get executable information
     * @return Formatted string with executable details
     */
    std::string GetExecutableInfo() const;
    
    /**
     * @brief Write system information to the log file
     */
    void WriteSystemInfo();
    
    //-----------------------------------------------------------------------------
    // Statistics and Monitoring
    //-----------------------------------------------------------------------------
    
    /**
     * @brief Get logging statistics
     * @return Statistics structure with counts and timing info
     */
    struct LogStats {
        std::chrono::system_clock::time_point start_time;
        size_t total_messages = 0;
        size_t messages_by_level[5] = {0}; // Indexed by LogLevel enum
        size_t bytes_written = 0;
        size_t file_rotations = 0;
    };
    
    LogStats GetStats() const;
    
    /**
     * @brief Reset logging statistics
     */
    void ResetStats();

private:
    //-----------------------------------------------------------------------------
    // Private Implementation
    //-----------------------------------------------------------------------------
    
    /**
     * @brief Initialize the log file
     * @return true if initialization was successful
     */
    bool InitializeLogFile();
    
    /**
     * @brief Write a message to the console
     * @param message Message to write
     * @param color Color to use
     */
    void WriteToConsole(const std::string& message, LogColor color);
    
    /**
     * @brief Write a message to the log file
     * @param message Message to write
     * @param level Log level
     * @param context Context information
     */
    void WriteToFile(const std::string& message, LogLevel level, 
                    const LogContext& context);
    
    /**
     * @brief Format a message for console output
     * @param message Raw message
     * @param level Log level
     * @param context Context information
     * @param color Color to use
     * @return Formatted message ready for console output
     */
    std::string FormatConsoleMessage(const std::string& message, LogLevel level,
                                   const LogContext& context, LogColor color);
    
    /**
     * @brief Format a message for file output
     * @param message Raw message
     * @param level Log level
     * @param context Context information
     * @return Formatted message ready for file output
     */
    std::string FormatFileMessage(const std::string& message, LogLevel level,
                                const LogContext& context);
    
    /**
     * @brief Create a log file header
     * @return Header content
     */
    std::string CreateLogHeader();
    
    /**
     * @brief Create a log file footer
     * @return Footer content
     */
    std::string CreateLogFooter();
    
    /**
     * @brief Get platform-specific system information
     * @return System information string
     */
    std::string GetPlatformSystemInfo() const;
    
    /**
     * @brief Get platform-specific executable information
     * @return Executable information string
     */
    std::string GetPlatformExecutableInfo() const;

private:
    LoggingConfig config_;
    std::ofstream log_file_;
    mutable std::mutex log_mutex_;
    LogStats stats_;
    bool file_initialized_;
    
    // Platform-specific data
    std::string executable_path_;
    std::string executable_name_;
    std::chrono::system_clock::time_point start_time_;
};

} // namespace hydroc