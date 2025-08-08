/**
 * @file logging.h
 * @brief Unified logging system for HydroChrono
 * 
 * This header provides the main public interface for the HydroChrono logging system.
 * It includes configuration structures, initialization functions, and namespace-based
 * APIs for CLI and debug logging.
 * 
 * @note This is the primary public interface for logging in HydroChrono
 * @note All logging operations are thread-safe when properly configured
 * @note The system supports both CLI and debug logging modes
 *
 * Quick start:
 *
 * ```cpp
 * #include <hydroc/logging.h>
 *
 * int main(int argc, char** argv) {
 *   hydroc::LoggingConfig cfg;
 *   cfg.log_file_path = "logs/run.log";   // empty = no file
 *   cfg.enable_cli_output = true;          // console on
 *   cfg.enable_file_output = true;         // file on
 *   cfg.enable_debug_logging = false;      // debug off by default
 *   hydroc::Initialize(cfg);
 *
 *   hydroc::cli::ShowBanner();
 *   hydroc::cli::LogInfo("Starting simulation...");
 *   hydroc::debug::LogDebug("internal details");
 *
 *   hydroc::Shutdown();
 * }
 * ```
 */

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <ostream>
#include <sstream>
#include <mutex>
#include <fstream>
#include <chrono>

// Version information - these may be provided by the build system
#ifndef HYDROCHRONO_VERSION
    #define HYDROCHRONO_VERSION "unknown"
#endif
#ifndef CHRONO_VERSION
    #define CHRONO_VERSION "unknown"
#endif
#ifndef HYDROCHRONO_BUILD_TYPE
    #define HYDROCHRONO_BUILD_TYPE "unknown"
#endif

namespace hydroc {

// Minimal shared log types
enum class LogLevel { Debug = 0, Info = 1, Success = 2, Warning = 3, Error = 4 };
enum class LogColor { White, Green, Yellow, Red, Cyan, Blue, Gray, BrightWhite, BrightCyan, BrightGreen };

struct LogContext {
    std::string source_file;
    int source_line = 0;
    std::string function_name;
    std::string thread_id;
    std::string component;
    std::chrono::system_clock::time_point timestamp;
    LogContext() : timestamp(std::chrono::system_clock::now()) {}
};

// Utility helpers
std::string LogLevelToString(LogLevel level);
std::string GetColorCode(LogColor color);
std::string GetTimestamp();
std::string GetTimestampISO8601();
int GetVisibleWidth(const std::string& str);
std::string FormatNumber(double value, int decimal_places = 2);

/**
 * @brief Configuration for the logging system
 * 
 * Controls various aspects of logging behavior including
 * output destinations, verbosity levels, and file management.
 */
struct LoggingConfig {
    std::string log_file_path;           ///< Path for log file (empty = no file logging)
    bool enable_cli_output = true;       ///< Enable console output
    bool enable_file_output = true;      ///< Enable file output
    bool enable_debug_logging = false;   ///< Enable debug-level logging
    // Verbosity thresholds
    enum class Level { Debug = 0, Info = 1, Success = 2, Warning = 3, Error = 4 };
    Level console_level = Level::Info;   ///< Minimum level for console output
    Level file_level = Level::Debug;     ///< Minimum level for file output
    bool enable_colors = true;           ///< Enable ANSI color codes in console
    bool enable_timestamps = true;       ///< Include timestamps in log messages
    bool enable_source_location = false; ///< Include source location in debug logs
    
    LoggingConfig() = default;
};

//-----------------------------------------------------------------------------
// Main Logging Interface
//-----------------------------------------------------------------------------

/**
 * @brief Initialize the logging system
 * @param config Configuration for the logging system
 */
bool Initialize(const LoggingConfig& config);

/**
 * @brief Shutdown the logging system
 * 
 * Performs cleanup of logging resources. This is automatically called
 * when the LoggerSession is destroyed, but can be called explicitly
 * if needed.
 * 
 * @note This function is safe to call multiple times
 */
void Shutdown();

/**
 * @brief Get the current CLI logger instance
 * @return Shared pointer to the CLI logger, or nullptr if not initialized
 * 
 * Returns the current CLI logger instance for user-facing logging.
 * The logger must be initialized via InitializeLogging() first.
 */
// Not exposed publicly: coordinator owns the backend

/**
 * @brief Get the current debug logger instance
 * @return Shared pointer to the debug logger, or nullptr if not initialized
 * 
 * Returns the current debug logger instance for developer-facing logging.
 * The logger must be initialized via InitializeLogging() first.
 */
// Not exposed publicly

/**
 * @brief Check if logging is initialized
 * @return true if the logging system is initialized and ready
 */
bool IsInitialized();

//-----------------------------------------------------------------------------
// CLI Logging Namespace
//-----------------------------------------------------------------------------

/**
 * @brief CLI-focused logging namespace
 * 
 * Provides convenient functions for user-facing logging operations.
 * These functions delegate to the current CLI logger instance.
 */
namespace cli {

/**
 * @brief Log an informational message
 * @param message Message content
 */
void LogInfo(const std::string& message);

/**
 * @brief Log a success message (green)
 * @param message Message content
 */
void LogSuccess(const std::string& message);

/**
 * @brief Log a warning message (yellow)
 * @param message Message content
 */
void LogWarning(const std::string& message);

/**
 * @brief Log an error message (red)
 * @param message Message content
 */
void LogError(const std::string& message);

/**
 * @brief Log a debug message (only if debug logging is enabled)
 * @param message Message content
 */
void LogDebug(const std::string& message);

/**
 * @brief Display the HydroChrono banner
 */
void ShowBanner();

/**
 * @brief Display a section separator
 */
void ShowSectionSeparator();

/**
 * @brief Display an empty line for spacing
 */
void ShowEmptyLine();

 /**
  * @brief Display a flat section header line with normalized width (60 chars)
  * @param title Title text (may include emojis/multibyte)
  */
 void ShowHeader(const std::string& title);

/**
 * @brief Display a section box with title and content
 * @param title Section title
 * @param content_lines Vector of content lines
 */
void ShowSectionBox(const std::string& title, 
                   const std::vector<std::string>& content_lines);

/**
 * @brief Display wave model parameters
 * @param wave_type Type of wave model
 * @param height Wave height in meters
 * @param period Wave period in seconds
 * @param direction Wave direction in degrees (optional)
 * @param phase Wave phase in degrees (optional)
 */
void ShowWaveModel(const std::string& wave_type, double height, 
                  double period, double direction = 0.0, double phase = 0.0);

/**
 * @brief Display simulation completion results
 * @param final_time Final simulation time in seconds
 * @param steps Total number of simulation steps
 * @param wall_time Total wall-clock time in seconds
 */
void ShowSimulationResults(double final_time, int steps, double wall_time);

/**
 * @brief Display log file location
 * @param log_path Path to the log file
 */
void ShowLogFileLocation(const std::string& log_path);

/**
 * @brief Display the application footer
 */
void ShowFooter();

/**
 * @brief Collect a warning for later display
 * @param warning_message Warning to collect
 */
void CollectWarning(const std::string& warning_message);

/**
 * @brief Display all collected warnings
 */
void DisplayWarnings();

// Additional helpers used by apps
void ShowSummaryLine(const std::string& icon, const std::string& label, const std::string& value, LogColor color = LogColor::White);
std::string CreateAlignedLine(const std::string& icon, const std::string& label, const std::string& value);

} // namespace cli

//-----------------------------------------------------------------------------
// Debug Logging Namespace
//-----------------------------------------------------------------------------

/**
 * @brief Debug-focused logging namespace
 * 
 * Provides convenient functions for developer-facing logging operations.
 * These functions delegate to the current debug logger instance.
 */
namespace debug {

/**
 * @brief Log a debug message
 * @param message Debug message content
 */
void LogDebug(const std::string& message);

/**
 * @brief Log a trace message (most verbose level)
 * @param message Trace message content
 */
void LogTrace(const std::string& message);

/**
 * @brief Log an informational message
 * @param message Info message content
 */
void LogInfo(const std::string& message);

/**
 * @brief Log a warning message
 * @param message Warning message content
 */
void LogWarning(const std::string& message);

/**
 * @brief Log an error message
 * @param message Error message content
 */
void LogError(const std::string& message);

/**
 * @brief Log comprehensive system information
 */
void LogSystemInfo();

/**
 * @brief Log executable information
 */
void LogExecutableInfo();

/**
 * @brief Log command line arguments
 * @param argc Argument count
 * @param argv Argument array
 */
void LogCommandLineArgs(int argc, char** argv);

/**
 * @brief Log environment variables
 * @param include_all Whether to include all environment variables (default: false)
 */
void LogEnvironmentVars(bool include_all = false);

/**
 * @brief Log memory usage information
 */
void LogMemoryInfo();

/**
 * @brief Log CPU and architecture information
 */
void LogCPUInfo();

/**
 * @brief Log platform-specific information
 */
void LogPlatformInfo();

/**
 * @brief Start a performance timer
 * @param timer_name Name of the timer
 * @return Timer ID for stopping the timer
 */
size_t StartTimer(const std::string& timer_name);

/**
 * @brief Stop a performance timer and log the duration
 * @param timer_id Timer ID returned from StartTimer
 */
void StopTimer(size_t timer_id);

/**
 * @brief Log a performance measurement
 * @param operation_name Name of the operation
 * @param duration Duration in milliseconds
 */
void LogPerformance(const std::string& operation_name, double duration_ms);

/**
 * @brief Log memory allocation/deallocation
 * @param operation Allocation operation ("alloc", "dealloc", "realloc")
 * @param size Size in bytes
 * @param ptr Pointer address (optional)
 */
void LogMemoryOperation(const std::string& operation, size_t size, void* ptr = nullptr);

/**
 * @brief Log function entry
 * @param function_name Name of the function
 */
void LogFunctionEntry(const std::string& function_name);

/**
 * @brief Log function exit
 * @param function_name Name of the function
 */
void LogFunctionExit(const std::string& function_name);

/**
 * @brief Log variable values
 * @param variable_name Name of the variable
 * @param value Variable value as string
 */
void LogVariable(const std::string& variable_name, const std::string& value);

/**
 * @brief Log a stack trace (if available)
 */
void LogStackTrace();

/**
 * @brief Log thread information
 */
void LogThreadInfo();

/**
 * @brief Check if debug logging is enabled
 * @return true if debug logging is enabled
 */
bool IsDebugEnabled();

} // namespace debug

} // namespace hydroc

//-----------------------------------------------------------------------------
// Convenience Macros
//-----------------------------------------------------------------------------

// Variadic stream-friendly macros that build a string with operator<<

#define HC_BUILD_LOG_STRING(...) \
    std::ostringstream _hc_oss; \
    _hc_oss << __VA_ARGS__; \
    const std::string _hc_msg = _hc_oss.str();

// Debug/trace -> debug logger if available
#define LOG_DEBUG(...) \
    do { \
        if (hydroc::debug::IsDebugEnabled()) { \
            HC_BUILD_LOG_STRING(__VA_ARGS__); \
            hydroc::debug::LogDebug(_hc_msg); \
        } \
    } while (0)

#define LOG_TRACE(...) \
    do { \
        if (hydroc::debug::IsDebugEnabled()) { \
            HC_BUILD_LOG_STRING(__VA_ARGS__); \
            hydroc::debug::LogTrace(_hc_msg); \
        } \
    } while (0)

// Info/Warning/Error/Success helpers for convenience
#define LOG_INFO(...) \
    do { \
        HC_BUILD_LOG_STRING(__VA_ARGS__); \
        hydroc::debug::LogInfo(_hc_msg); \
    } while (0)

#define LOG_WARNING(...) \
    do { \
        HC_BUILD_LOG_STRING(__VA_ARGS__); \
        hydroc::debug::LogWarning(_hc_msg); \
    } while (0)

#define LOG_ERROR(...) \
    do { \
        HC_BUILD_LOG_STRING(__VA_ARGS__); \
        hydroc::debug::LogError(_hc_msg); \
    } while (0)

#define LOG_SUCCESS(...) \
    do { \
        HC_BUILD_LOG_STRING(__VA_ARGS__); \
        hydroc::cli::LogSuccess(_hc_msg); \
    } while (0)

/**
 * @brief Log function entry with automatic source location
 */
#define LOG_FUNCTION_ENTRY() \
    if (hydroc::debug::IsDebugEnabled()) { \
        hydroc::debug::LogFunctionEntry(__FUNCTION__); \
    }
// Named variant
#define LOG_FUNCTION_ENTRY_NAMED(func) \
    if (hydroc::debug::IsDebugEnabled()) { \
        hydroc::debug::LogFunctionEntry(func); \
    }

/**
 * @brief Log function exit with automatic source location
 */
#define LOG_FUNCTION_EXIT() \
    if (hydroc::debug::IsDebugEnabled()) { \
        hydroc::debug::LogFunctionExit(__FUNCTION__); \
    }
// Named variant
#define LOG_FUNCTION_EXIT_NAMED(func) \
    if (hydroc::debug::IsDebugEnabled()) { \
        hydroc::debug::LogFunctionExit(func); \
    }

/**
 * @brief Log a variable value with automatic source location
 * @param var Variable name
 * @param val Variable value
 */
#define LOG_VARIABLE(var, val) \
    if (hydroc::debug::IsDebugEnabled()) { \
        hydroc::debug::LogVariable(#var, std::to_string(val)); \
    } 