/**
 * @file logging_backend.cpp
 * @brief Implementation of the LoggerBackend class
 */

#include "logger_backend.h"
#include <hydroc/logging.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cassert>

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
    constexpr const char* HYDROCHRONO_VERSION = "unknown";
#endif
#ifndef CHRONO_VERSION
    constexpr const char* CHRONO_VERSION = "unknown";
#endif
#ifndef HYDROCHRONO_BUILD_TYPE
    constexpr const char* HYDROCHRONO_BUILD_TYPE = "unknown";
#endif

namespace hydroc {

//-----------------------------------------------------------------------------
// LoggerBackend Implementation
//-----------------------------------------------------------------------------

LoggerBackend::LoggerBackend(const LoggingConfig& config)
    : config_(config), file_initialized_(false) {
    
    start_time_ = std::chrono::system_clock::now();
    stats_.start_time = start_time_;
    
    // Initialize platform-specific data
    executable_path_ = GetPlatformExecutableInfo();
    executable_name_ = std::filesystem::path(executable_path_).filename().string();
    
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
      executable_name_(std::move(other.executable_name_)),
      start_time_(other.start_time_) {
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
        start_time_ = other.start_time_;
        
        other.file_initialized_ = false;
    }
    return *this;
}

void LoggerBackend::Log(LogLevel level, const std::string& message, 
                       const LogContext& context, LogColor color) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    // Update statistics
    stats_.total_messages++;
    if (static_cast<size_t>(level) < 5) {
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

bool LoggerBackend::ShouldLog(LogLevel level, bool is_console_output) const {
    int threshold = static_cast<int>(is_console_output ? config_.console_level : config_.file_level);
    return static_cast<int>(level) >= threshold;
}

const LoggingConfig& LoggerBackend::GetConfig() const {
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

bool LoggerBackend::IsFileLoggingEnabled() const {
    return file_initialized_ && log_file_.is_open();
}

std::string LoggerBackend::GetLogFilePath() const {
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

LoggerBackend::LogStats LoggerBackend::GetStats() const {
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
        // Create directory if it doesn't exist
        std::filesystem::path log_path(config_.log_file_path);
        std::filesystem::path log_dir = log_path.parent_path();
        
        if (!log_dir.empty() && !std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
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
    ss << " Platform:            "
#ifdef _WIN32
       << "Windows"
#else
       << "Linux"
#endif
       << "\n";
    
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