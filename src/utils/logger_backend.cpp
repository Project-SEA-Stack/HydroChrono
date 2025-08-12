/**
 * @file logging_backend.cpp
 * @brief Implementation of the LoggerBackend class
 */

#include "logger_backend.h"
#if __has_include(<hydroc/logging.h>)
#  include <hydroc/logging.h>
#else
#  include "../../include/hydroc/logging.h"
#endif
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <sysinfoapi.h>
#include <direct.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#endif

// Version information - these should be defined by the build system
#ifndef HYDROCHRONO_VERSION
    constexpr const char* HYDROCHRONO_VERSION = "unknown";
#endif
#ifndef CHRONO_VERSION
    constexpr const char* CHRONO_VERSION = "unknown";
#endif
#ifndef HYDROCHRONO_BUILD_TYPE
    constexpr const char* HYDROCHRONO_BUILD_TYPE = "unknown";
#endif

namespace hydroc {

//------------------------------------------------------------------------------
// Local path utilities (portable, minimal, no <filesystem> dependency)
//------------------------------------------------------------------------------

namespace {

    inline bool IsPathSeparator(char c) {
        return c == '/' || c == '\\';
    }

    // Return parent directory portion of a file path (empty if none)
    std::string ExtractParentDirectory(const std::string& file_path) {
        if (file_path.empty()) return std::string();
        size_t end = file_path.size();
        // Trim trailing separators
        while (end > 0 && IsPathSeparator(file_path[end - 1])) {
            --end;
        }
        // Find last separator
        size_t pos = std::string::npos;
        for (size_t i = end; i > 0; --i) {
            if (IsPathSeparator(file_path[i - 1])) { pos = i - 1; break; }
        }
        if (pos == std::string::npos) return std::string();
        // Handle Windows drive roots like "C:\\"
        if (pos == 2 && file_path[1] == ':') {
            // Parent of drive root is the root itself (e.g., "C:\\")
            return file_path.substr(0, 3);
        }
        return file_path.substr(0, pos);
    }

    // Return the last path component (file name)
    std::string ExtractFileName(const std::string& file_path) {
        if (file_path.empty()) return std::string();
        size_t start = 0;
        for (size_t i = file_path.size(); i > 0; --i) {
            if (IsPathSeparator(file_path[i - 1])) { start = i; break; }
        }
        return file_path.substr(start);
    }

    // Recursively create directories for the given absolute or relative path.
    bool EnsureDirectoriesExist(const std::string& dir_path) {
        if (dir_path.empty()) return true;

        std::string accum;
        accum.reserve(dir_path.size());

        // On Windows, preserve drive prefix like "C:" if present
        size_t idx = 0;
#ifdef _WIN32
        if (dir_path.size() >= 2 && dir_path[1] == ':') {
            accum += dir_path[0];
            accum += ':';
            idx = 2;
        }
#endif
        for (; idx < dir_path.size(); ++idx) {
            char c = dir_path[idx];
            accum += c;
            if (IsPathSeparator(c)) {
                // Skip creating empty/duplicate separators
                if (accum.size() <= 1) continue;
#ifdef _WIN32
                // Avoid calling CreateDirectory on drive root like "C:\\"
                if (accum.size() == 3 && accum[1] == ':' && IsPathSeparator(accum[2])) continue;
                if (!CreateDirectoryA(accum.c_str(), nullptr)) {
                    DWORD err = GetLastError();
                    if (err != ERROR_ALREADY_EXISTS) return false;
                }
#else
                if (mkdir(accum.c_str(), 0755) != 0) {
                    if (errno != EEXIST) return false;
                }
#endif
            }
        }

        // Create the final directory if the path does not end with separator
        if (!dir_path.empty() && !IsPathSeparator(dir_path.back())) {
#ifdef _WIN32
            if (!(dir_path.size() == 2 && dir_path[1] == ':')) {
                if (!CreateDirectoryA(dir_path.c_str(), nullptr)) {
                    DWORD err = GetLastError();
                    if (err != ERROR_ALREADY_EXISTS) return false;
                }
            }
#else
            if (mkdir(dir_path.c_str(), 0755) != 0) {
                if (errno != EEXIST) return false;
            }
#endif
        }
        return true;
    }

} // namespace

//-----------------------------------------------------------------------------
// LoggerBackend Implementation
//-----------------------------------------------------------------------------

LoggerBackend::LoggerBackend(const LoggingConfig& config)
    : config_(config), file_initialized_(false) {
    
    stats_.start_time = std::chrono::system_clock::now();
    
    // Initialize platform-specific data
    executable_path_ = GetPlatformExecutableInfo();
    executable_name_ = ExtractFileName(executable_path_);
    
    // Initialize log file if file output is enabled
    if (config_.enable_file_output && !config_.log_file_path.empty()) {
        InitializeLogFile();
    }
}

LoggerBackend::~LoggerBackend() {
    if (log_file_.is_open()) {
        log_file_ << CreateLogFooter();
        log_file_.flush();
        log_file_.close();
    }
}

LoggerBackend::LoggerBackend(LoggerBackend&& other) noexcept
    : config_(std::move(other.config_)),
      log_file_(std::move(other.log_file_)),
      stats_(std::move(other.stats_)),
      file_initialized_(other.file_initialized_),
      executable_path_(std::move(other.executable_path_)),
      executable_name_(std::move(other.executable_name_)) {
    other.file_initialized_ = false;
}

LoggerBackend& LoggerBackend::operator=(LoggerBackend&& other) noexcept {
    if (this != &other) {
        if (log_file_.is_open()) {
            log_file_.close();
        }
        
        config_ = std::move(other.config_);
        log_file_ = std::move(other.log_file_);
        stats_ = std::move(other.stats_);
        file_initialized_ = other.file_initialized_;
        executable_path_ = std::move(other.executable_path_);
        executable_name_ = std::move(other.executable_name_);
        /* no per-instance start_time_ field */
        
        other.file_initialized_ = false;
    }
    return *this;
}

void LoggerBackend::Log(LogLevel level, const std::string& message, 
                       const LogContext& context, LogColor color) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    // Update statistics
    stats_.total_messages++;
    if (static_cast<size_t>(level) < kNumLogLevels) {
        stats_.messages_by_level[static_cast<size_t>(level)]++;
    }
    
    // Write to console if enabled and level meets threshold
    if (config_.enable_cli_output && ShouldLog(level, true)) {
        std::string console_message = FormatConsoleMessage(message, level, context, color);
        WriteToConsole(console_message, color);
    }
    
    // Write to file if enabled and level meets threshold
    if (config_.enable_file_output && ShouldLog(level, false) && log_file_.is_open()) {
        std::string file_message = FormatFileMessage(message, level, context);
        WriteToFile(file_message, level, context);
    }
}

bool LoggerBackend::ShouldLog(LogLevel level, bool is_console_output) const noexcept {
    int threshold = static_cast<int>(is_console_output ? config_.console_level : config_.file_level);
    return static_cast<int>(level) >= threshold;
}

const LoggingConfig& LoggerBackend::GetConfig() const noexcept {
    return config_;
}

void LoggerBackend::UpdateConfig(const LoggingConfig& config) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    config_ = config;
    
    // Reinitialize log file if needed
    if (config_.enable_file_output && !config_.log_file_path.empty() && !file_initialized_) {
        InitializeLogFile();
    }
}

bool LoggerBackend::IsFileLoggingEnabled() const noexcept {
    return file_initialized_ && log_file_.is_open();
}

std::string LoggerBackend::GetLogFilePath() const noexcept {
    if (!file_initialized_) {
        return "";
    }
    return config_.log_file_path;
}

void LoggerBackend::Flush() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (log_file_.is_open()) {
        log_file_.flush();
    }
}

bool LoggerBackend::RotateLogFile(const std::string& new_path) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    if (!log_file_.is_open()) {
        return false;
    }
    
    // Write footer to current file
    log_file_ << CreateLogFooter();
    log_file_.flush();
    log_file_.close();
    
    // Update path if provided
    if (!new_path.empty()) {
        config_.log_file_path = new_path;
    }
    
    // Reinitialize with new file
    bool success = InitializeLogFile();
    if (success) {
        stats_.file_rotations++;
    }
    
    return success;
}

std::string LoggerBackend::GetSystemInfo() const {
    return GetPlatformSystemInfo();
}

std::string LoggerBackend::GetExecutableInfo() const {
    return GetPlatformExecutableInfo();
}

void LoggerBackend::WriteSystemInfo() {
    if (log_file_.is_open()) {
        log_file_ << GetSystemInfo() << std::endl;
        log_file_.flush();
    }
}

LoggerBackend::LogStats LoggerBackend::GetStats() const noexcept {
    std::lock_guard<std::mutex> lock(log_mutex_);
    return stats_;
}

void LoggerBackend::ResetStats() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    stats_ = LogStats{};
    stats_.start_time = std::chrono::system_clock::now();
}

//-----------------------------------------------------------------------------
// Private Implementation
//-----------------------------------------------------------------------------

bool LoggerBackend::InitializeLogFile() {
    try {
        // Create parent directory if it doesn't exist (portable, no <filesystem>)
        const std::string log_dir = ExtractParentDirectory(config_.log_file_path);
        if (!log_dir.empty()) {
            if (!EnsureDirectoriesExist(log_dir)) {
                return false;
            }
        }
        
        // Open log file
        log_file_.open(config_.log_file_path, std::ios::out | std::ios::trunc);
        if (!log_file_.is_open()) {
            return false;
        }
        
        // Write header
        log_file_ << CreateLogHeader();
        log_file_.flush();
        
        file_initialized_ = true;
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

void LoggerBackend::WriteToConsole(const std::string& message, LogColor color) {
    if (!config_.enable_colors) {
        std::cout << message << std::endl;
    } else {
        std::string color_code = GetColorCode(color);
        std::string reset_code = "\033[0m";
        std::cout << color_code << message << reset_code << std::endl;
    }
}

void LoggerBackend::WriteToFile(const std::string& message, LogLevel level, 
                               const LogContext& context) {
    if (log_file_.is_open()) {
        log_file_ << message << std::endl;
        stats_.bytes_written += message.length() + 1; // +1 for newline
    }
}

std::string LoggerBackend::FormatConsoleMessage(const std::string& message, LogLevel level,
                                               const LogContext& context, LogColor color) {
    // For CLI output, return the raw message without timestamp/level prefixes.
    // Styling (colors/emojis) is handled by the caller via 'color'.
    return message;
}

std::string LoggerBackend::FormatFileMessage(const std::string& message, LogLevel level,
                                            const LogContext& context) {
    std::stringstream ss;
    
    ss << "[" << GetTimestampISO8601() << "] ";
    ss << "[" << LogLevelToString(level) << "] ";
    
    if (config_.enable_source_location && !context.source_file.empty()) {
        ss << "[" << context.source_file << ":" << context.source_line;
        if (!context.function_name.empty()) {
            ss << ":" << context.function_name;
        }
        ss << "] ";
    }
    // Preserve original message content, including Unicode symbols/emojis and
    // box-drawing characters for better readability in the log file.
    ss << message;
    
    return ss.str();
}

std::string LoggerBackend::CreateLogHeader() {
    std::stringstream ss;
    
    ss << "============================================================\n";
    ss << " HydroChrono Simulation Log\n";
    ss << "============================================================\n";
    ss << " Executable:          " << executable_name_ << "\n";
    ss << " HydroChrono version: " << HYDROCHRONO_VERSION << "\n";
    ss << " Chrono version:      " << CHRONO_VERSION << "\n";
    ss << " Build type:          " << HYDROCHRONO_BUILD_TYPE << "\n";
    ss << " Platform:            ";
#ifdef _WIN32
    ss << "Windows";
#elif defined(__APPLE__)
    ss << "macOS";
#else
    ss << "Linux";
#endif
    ss << "\n";
    
    ss << GetSystemInfo();
    
    ss << " Log started:         " << GetTimestamp() << "\n";
    ss << " Log Levels:          DEBUG, INFO, SUCCESS, WARNING, ERROR\n";
    ss << "============================================================\n";
    
    return ss.str();
}

std::string LoggerBackend::CreateLogFooter() {
    std::stringstream ss;
    ss << "============================================================\n";
    ss << " Log ended:           " << GetTimestamp() << "\n";
    ss << "============================================================\n";
    return ss.str();
}

std::string LoggerBackend::GetPlatformSystemInfo() const {
    std::stringstream ss;
    
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    ::GetSystemInfo(&sysInfo);
    
    std::string arch;
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: arch = "x64 (AMD or Intel)"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: arch = "x86 (Intel)"; break;
        case PROCESSOR_ARCHITECTURE_ARM: arch = "ARM"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: arch = "ARM64"; break;
        case PROCESSOR_ARCHITECTURE_IA64: arch = "Intel Itanium"; break;
        default: arch = "Unknown"; break;
    }
    
    ss << " CPU Architecture:    " << arch << "\n";
    ss << " Number of CPUs:      " << sysInfo.dwNumberOfProcessors << "\n";
    
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        ss << " Total Physical RAM:  " << (memInfo.ullTotalPhys / (1024*1024*1024)) << " GB\n";
        ss << " Available Physical:  " << (memInfo.ullAvailPhys / (1024*1024*1024)) << " GB\n";
    }
#else
    ss << " CPU Architecture:    Linux\n";
    // Add more Linux-specific system info here if needed
#endif
    
    return ss.str();
}

std::string LoggerBackend::GetPlatformExecutableInfo() const {
#ifdef _WIN32
    char path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, path, MAX_PATH) == 0) {
        return "";
    }
    return std::string(path);
#else
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count != -1) {
        path[count] = '\0';
        return std::string(path);
    }
    return "";
#endif
}

} // namespace hydroc 