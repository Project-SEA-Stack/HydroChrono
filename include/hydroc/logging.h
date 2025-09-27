/**
 * @file logging.h
 * @brief Thread-safe logging system for HydroChrono
 * 
 * This header provides a simple, thread-safe logging system that can be enabled
 * via the --debug command line argument. Logs are written to files in a
 * 'hydrochrono_logs' directory with timestamps and system information.
 * 
 * @note This logging system is designed to be thread-safe and can be used from
 *       multiple threads simultaneously. All logging operations are protected
 *       by a mutex to prevent interleaved output.
 * 
 * @warning If logging is enabled, log files will be created in a 'hydrochrono_logs'
 *          directory next to the executable. Ensure the application has write
 *          permissions in this location.
 */

#pragma once

#include <iostream>
#include <iomanip>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <string>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <sysinfoapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

// Version information - these should be defined by the build system
#ifndef HYDROCHRONO_VERSION
    constexpr const char* HYDROCHRONO_VERSION = "unknown";  ///< HydroChrono version string
#endif
#ifndef CHRONO_VERSION
    constexpr const char* CHRONO_VERSION = "unknown";       ///< Chrono version string
#endif
#ifndef HYDROCHRONO_BUILD_TYPE
    constexpr const char* HYDROCHRONO_BUILD_TYPE = "unknown";  ///< Build type (Debug/Release/etc)
#endif

namespace hydroc {

/**
 * @class Logger
 * @brief Static logging class that provides thread-safe file logging functionality
 * 
 * The Logger class provides a simple interface for logging debug, trace, and error
 * messages to a file. Logging can be enabled via the --debug flag. All logging
 * operations are thread-safe.
 * 
 * @note This is a static class - all members and methods are static.
 * @note Logging is disabled by default and must be enabled via --debug flag.
 * @note All logging operations are protected by a mutex for thread safety.
 * 
 * @throw std::runtime_error if log file cannot be created or written to
 */
class Logger {
public:
    /**
     * @brief Initialize the logging system
     * @param argc Command line argument count
     * @param argv Command line arguments
     * 
     * Checks for --debug flag and initializes logging if present.
     * Creates log directory and file with system information.
     * 
     * @throw std::runtime_error if log directory cannot be created
     * @throw std::runtime_error if log file cannot be opened
     */
    static void init_logging(int argc, char** argv);

    /**
     * @brief Clean up logging resources
     * 
     * Writes footer to log file and closes it.
     * This function is called automatically at program exit.
     */
    static void cleanup_logging() noexcept;

    /**
     * @brief Check if logging is enabled
     * @return true if logging is enabled, false otherwise
     * 
     * @note This function is thread-safe
     */
    static inline bool is_logging_enabled() noexcept { return logging_enabled; }

    /**
     * @brief Get the name of the current executable
     * @return The executable name (e.g., "demo_rm3_reg_waves.exe")
     * 
     * Platform-specific implementation to retrieve the executable name.
     * 
     * @note This function is thread-safe
     */
    static std::string get_executable_name() noexcept;

    /**
     * @brief Log a message with the specified level
     * @param level The log level (e.g., "DEBUG", "ERROR", "TRACE")
     * @param msg The message to log
     * 
     * Thread-safe logging of a message with the specified level.
     * Only logs if logging is enabled.
     * 
     * @note This function is thread-safe
     * @note No-op if logging is disabled
     */
    static void log_message(const std::string& level, const std::string& msg) noexcept;

    /**
     * @brief Get a stream for logging with the specified level
     * @param level The log level (e.g., "DEBUG", "ERROR", "TRACE")
     * @return A stream that can be used to build a log message
     * 
     * Thread-safe logging of a formatted message with the specified level.
     * Only logs if logging is enabled.
     * 
     * @note This function is thread-safe
     * @note No-op if logging is disabled
     */
    static std::ostream& log_stream(const std::string& level) noexcept;

private:
    /// Mutex to ensure thread-safe access to the log file
    static std::mutex log_mutex;
    
    /// Output file stream for writing log messages
    static std::ofstream log_file;
    
    /// Flag indicating whether logging is currently enabled
    static bool logging_enabled;

    /**
     * @brief Check command line arguments for --debug flag
     * @param argc Command line argument count
     * @param argv Command line arguments
     * @return true if --debug flag is present, false otherwise
     * 
     * @note This function is thread-safe
     */
    static inline bool check_debug_flag(int argc, char** argv) noexcept;

    /**
     * @brief Get the full path of the current executable
     * @return The full path to the executable
     * 
     * Platform-specific implementation to retrieve the executable path.
     * Used to determine where to create the log directory.
     * 
     * @note This function is thread-safe
     */
    static std::string get_executable_path() noexcept;

    /**
     * @brief Initialize the log file
     * 
     * Creates the log directory if it doesn't exist and opens a new log file
     * with a timestamp in the filename.
     * 
     * @throw std::runtime_error if log directory cannot be created
     * @throw std::runtime_error if log file cannot be opened
     */
    static void init_log_file();

    /**
     * @brief Write initial log file header
     * @param argc Command line argument count
     * @param argv Command line arguments
     * 
     * Writes system information, version numbers, and command line arguments
     * to the log file header.
     * 
     * @throw std::runtime_error if log file cannot be written to
     */
    static void log_boilerplate(int argc, char** argv);

    /**
     * @brief Write log file footer
     * 
     * Writes the closing timestamp and separator to the log file.
     * 
     * @throw std::runtime_error if log file cannot be written to
     */
    static void log_footer();
};

/**
 * @def LOG_DEBUG(msg)
 * @brief Log a debug message
 * @param msg The message to log
 * 
 * Only logs if logging is enabled. Thread-safe.
 * 
 * @note This macro is thread-safe
 * @note No-op if logging is disabled
 */
#define LOG_DEBUG(msg) \
    if (hydroc::Logger::is_logging_enabled()) { \
        hydroc::Logger::log_stream("DEBUG") << msg; \
    }

/**
 * @def LOG_ERROR(msg)
 * @brief Log an error message
 * @param msg The message to log
 * 
 * Only logs if logging is enabled. Thread-safe.
 * 
 * @note This macro is thread-safe
 * @note No-op if logging is disabled
 */
#define LOG_ERROR(msg) \
    if (hydroc::Logger::is_logging_enabled()) { \
        hydroc::Logger::log_stream("ERROR") << msg; \
    }

/**
 * @def LOG_TRACE(msg)
 * @brief Log a trace message
 * @param msg The message to log
 * 
 * Only logs if logging is enabled. Thread-safe.
 * 
 * @note This macro is thread-safe
 * @note No-op if logging is disabled
 */
#define LOG_TRACE(msg) \
    if (hydroc::Logger::is_logging_enabled()) { \
        hydroc::Logger::log_stream("TRACE") << msg; \
    }

} // namespace hydroc 