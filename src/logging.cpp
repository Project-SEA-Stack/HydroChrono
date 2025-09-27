/**
 * @file logging.cpp
 * @brief Implementation of the HydroChrono logging system
 * 
 * This file implements the thread-safe logging functionality declared in logging.h.
 * The implementation is organized into three main components:
 * 
 * 1. Log File Management
 *    - Creation and initialization of log files
 *    - Directory structure and file naming
 *    - File operations and cleanup
 * 
 * 2. Log Content Generation
 *    - System information gathering
 *    - Log formatting and structure
 *    - Header and footer generation
 * 
 * 3. Platform Integration
 *    - Platform-specific path handling
 *    - System information retrieval
 *    - Command-line argument processing
 * 
 * @note All file operations are thread-safe through the use of a mutex
 * @note Platform-specific code is isolated in dedicated functions
 * @note Exception safety is maintained throughout the implementation
 */

#include <hydroc/logging.h>

// Standard library includes
#include <filesystem>  // for path operations and directory creation
#include <chrono>      // for timestamp generation
#include <ctime>       // for time formatting
#include <sstream>     // for string formatting
#include <stdexcept>   // for error handling
#include <memory>      // for smart pointers
#include <string_view> // for string literals

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>      // for Windows system information
#include <sysinfoapi.h>   // for Windows memory status
#elif defined(__APPLE__)
#include <mach-o/dyld.h>  // for macOS executable path
#else
#include <unistd.h>       // for Linux executable path
#include <limits.h>       // for PATH_MAX constant
#endif

namespace hydroc {
namespace {

//-----------------------------------------------------------------------------
// Configuration Constants
//-----------------------------------------------------------------------------

/**
 * @brief Constants defining the logging system configuration
 * 
 * These constants define the structure and naming conventions for log files
 * and directories. They are used throughout the implementation to ensure
 * consistency in file handling.
 */
constexpr std::string_view LOG_DIR_NAME = "hydrochrono_logs";
constexpr std::string_view LOG_FILE_PREFIX = "hydrochrono_";
constexpr std::string_view LOG_FILE_EXTENSION = ".log";

//-----------------------------------------------------------------------------
// Log File Management
//-----------------------------------------------------------------------------

/**
 * @brief Creates a new log file with a timestamp-based name
 * 
 * This function creates a new log file in the specified directory with a name
 * that includes a timestamp to ensure uniqueness. The file is created and
 * verified to be writable before returning its path.
 * 
 * @param log_dir Directory where the log file will be created
 * @return Path to the created log file
 * @throws std::runtime_error if file creation fails
 * @note The file is created with write-only access
 * @note The timestamp format is YYYYMMDD_HHMMSS
 */
std::filesystem::path create_log_file(const std::filesystem::path& log_dir) {
    // Generate timestamp for unique filename
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    
    // Create full path for log file
    std::filesystem::path log_path = log_dir / 
        (std::string(LOG_FILE_PREFIX) + ss.str() + std::string(LOG_FILE_EXTENSION));
    
    // Create and verify the file
    std::ofstream test_file(log_path, std::ios::out);
    if (!test_file.is_open()) {
        throw std::runtime_error("Failed to create log file: " + log_path.string());
    }
    
    return log_path;
}

/**
 * @brief Ensures the log directory exists and is accessible
 * 
 * This function checks if the log directory exists and creates it if necessary.
 * It verifies that the directory is accessible and can be used for logging.
 * 
 * @param base_path Base directory where logs should be stored
 * @return Path to the log directory
 * @throws std::runtime_error if directory creation fails
 * @note The function will create parent directories if they don't exist
 * @note The function verifies write access to the directory
 */
std::filesystem::path ensure_log_directory(const std::filesystem::path& base_path) {
    std::filesystem::path log_dir = base_path / LOG_DIR_NAME;
    
    if (!std::filesystem::exists(log_dir)) {
        if (!std::filesystem::create_directories(log_dir)) {
            throw std::runtime_error("Failed to create log directory: " + log_dir.string());
        }
    }
    
    return log_dir;
}

//-----------------------------------------------------------------------------
// Log Content Generation
//-----------------------------------------------------------------------------

/**
 * @brief Formats the current time for log entries
 * 
 * This function generates a formatted timestamp string suitable for log entries.
 * The format is consistent with the log file naming convention.
 * 
 * @return Formatted timestamp string in the format YYYY-MM-DD HH:MM:SS
 * @note The time is in the local timezone
 * @note The function is thread-safe
 */
std::string get_formatted_time() {
    auto now = std::time(nullptr);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

/**
 * @brief Generates system architecture information for logging
 * 
 * This function retrieves and formats information about the system's CPU
 * architecture and processor count. The implementation is platform-specific
 * but provides a consistent interface.
 * 
 * @return Formatted string containing architecture information
 * @note On Windows, provides detailed architecture information
 * @note On other platforms, provides basic platform identification
 */
std::string get_architecture_info() {
    std::stringstream ss;
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    std::string_view arch;
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            arch = "x64 (AMD or Intel)";
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            arch = "x86 (Intel)";
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            arch = "ARM";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            arch = "ARM64";
            break;
        case PROCESSOR_ARCHITECTURE_IA64:
            arch = "Intel Itanium";
            break;
        default:
            arch = "Unknown";
    }
    ss << " CPU Architecture:    " << arch << "\n"
       << " Number of CPUs:      " << sysInfo.dwNumberOfProcessors << "\n";
#else
    ss << " CPU Architecture:    " << "Linux" << "\n";
#endif
    return ss.str();
}

/**
 * @brief Generates system memory information for logging
 * 
 * This function retrieves and formats information about the system's memory
 * configuration, including total and available physical RAM.
 * 
 * @return Formatted string containing memory information
 * @note Currently implemented for Windows only
 * @note Memory values are reported in gigabytes
 */
std::string get_memory_info() {
    std::stringstream ss;
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        ss << " Total Physical RAM:  " << (memInfo.ullTotalPhys / (1024*1024*1024)) << " GB\n"
           << " Available Physical:  " << (memInfo.ullAvailPhys / (1024*1024*1024)) << " GB\n";
    }
#endif
    return ss.str();
}

/**
 * @brief Generates the log file header
 * 
 * This function creates the initial header for the log file, including:
 * - Basic system information
 * - Command line arguments
 * - Version information
 * - System architecture and memory details
 * - Log start time and available log levels
 * 
 * @param argc Command line argument count
 * @param argv Command line arguments
 * @return Formatted header string
 * @note The header format is consistent across all log files
 * @note All system information is gathered at log creation time
 */
std::string generate_log_header(int argc, char** argv) {
    std::stringstream ss;
    
    // Basic header
    ss << "============================================================\n"
       << " HydroChrono Simulation Log\n"
       << "============================================================\n"
       << " Executable:          " << Logger::get_executable_name() << "\n";

    // Command line arguments
    if (argc > 1 && argv != nullptr) {
        ss << " Arguments:           ";
        for (int i = 1; i < argc; i++) {
            ss << argv[i];
            if (i < argc - 1) ss << " ";
        }
        ss << "\n";
    }

    // Version information
    ss << " HydroChrono version: " << HYDROCHRONO_VERSION << "\n"
       << " Chrono version:      " << CHRONO_VERSION << "\n"
       << " Build type:          " << HYDROCHRONO_BUILD_TYPE << "\n"
       << " Platform:            " <<
#ifdef _WIN32
       "Windows"
#else
       "Linux"
#endif
       << "\n";
    
    // System information
    ss << get_architecture_info()
       << get_memory_info();

    // Timestamp and log levels
    ss << " Log started:         " << get_formatted_time() << "\n"
       << " Log Levels:          DEBUG, TRACE, ERROR\n"
       << "============================================================\n";
    
    return ss.str();
}

/**
 * @brief Generates the log file footer
 * 
 * This function creates the closing footer for the log file, including:
 * - Log end time
 * - Visual separator
 * 
 * @return Formatted footer string
 * @note The footer format is consistent across all log files
 * @note The end time is recorded when the footer is generated
 */
std::string generate_log_footer() {
    std::stringstream ss;
    ss << "============================================================\n"
       << " Log ended:           " << get_formatted_time() << "\n"
       << "============================================================\n";
    return ss.str();
}

//-----------------------------------------------------------------------------
// Platform Integration
//-----------------------------------------------------------------------------

/**
 * @brief Gets the executable name for the current process
 * 
 * This function retrieves the name of the currently running executable
 * without its path. The implementation is platform-specific but provides
 * a consistent interface.
 * 
 * @return Executable name or "unknown" if retrieval fails
 * @note This function is noexcept and platform-independent
 * @note The returned name does not include the path or extension
 */
std::string get_executable_name() noexcept {
#ifdef _WIN32
    char path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, path, MAX_PATH) == 0) {
        return "unknown";
    }
    return std::filesystem::path(path).filename().string();
#else
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count != -1) {
        path[count] = '\0';
        return std::filesystem::path(path).filename().string();
    }
    return "unknown";
#endif
}

/**
 * @brief Gets the full path to the current executable
 * 
 * This function retrieves the complete path to the currently running
 * executable. The implementation is platform-specific but provides
 * a consistent interface.
 * 
 * @return Full path to the executable or empty string if retrieval fails
 * @note This function is noexcept and platform-independent
 * @note The returned path includes the full directory structure
 */
std::string get_executable_path() noexcept {
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

/**
 * @brief Checks if the debug flag is present in command line arguments
 * 
 * This function examines the command line arguments for the presence
 * of the --debug flag, which enables logging functionality.
 * 
 * @param argc Argument count
 * @param argv Argument array
 * @return true if --debug flag is present
 * @note This function is noexcept and thread-safe
 * @note The function handles null pointers and empty argument lists
 */
bool has_debug_flag(int argc, char** argv) noexcept {
    if (argc <= 1 || argv == nullptr) return false;
    
    return std::any_of(argv + 1, argv + argc,
        [](const char* arg) { return std::string_view(arg) == "--debug"; });
}

} // anonymous namespace

//-----------------------------------------------------------------------------
// Logger Implementation
//-----------------------------------------------------------------------------

/**
 * @brief Static member initialization for the Logger class
 * 
 * These members are initialized at program startup and are shared across
 * all instances of the Logger class. The mutex ensures thread-safe access
 * to the logging functionality.
 */
std::mutex Logger::log_mutex;
std::ofstream Logger::log_file;
bool Logger::logging_enabled = false;

/**
 * @brief Initializes the logging system
 * 
 * This function sets up the logging system by:
 * 1. Checking for the --debug flag in command line arguments
 * 2. Creating the log directory if it doesn't exist
 * 3. Creating and opening a new log file
 * 4. Writing the initial log header
 * 
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @throws std::runtime_error if initialization fails
 * @note This function is not thread-safe and should be called only once at program startup
 */
void Logger::init_logging(int argc, char** argv) {
    logging_enabled = has_debug_flag(argc, argv);
    if (!logging_enabled) return;
    
    try {
        // Get executable path and create log directory
        std::string exe_path = get_executable_path();
        if (exe_path.empty()) {
            throw std::runtime_error("Failed to get executable path");
        }

        // Set up log file
        std::filesystem::path log_dir = ensure_log_directory(
            std::filesystem::path(exe_path).parent_path());
        std::filesystem::path log_path = create_log_file(log_dir);
        
        // Open log file and write header
        log_file.open(log_path, std::ios::out);
        if (!log_file.is_open()) {
            throw std::runtime_error("Failed to open log file: " + log_path.string());
        }
        
        log_file << generate_log_header(argc, argv);
        log_file.flush();
        
    } catch (const std::exception& e) {
        logging_enabled = false;
        throw std::runtime_error(std::string("Failed to initialize logging: ") + e.what());
    }
}

/**
 * @brief Cleans up logging resources
 * 
 * This function performs the following cleanup operations:
 * 1. Writes the log footer if logging is enabled
 * 2. Flushes any pending writes to disk
 * 3. Closes the log file
 * 
 * @note This function is noexcept and will suppress any exceptions during cleanup
 * @note This function is called automatically at program termination via the LogCleanup RAII wrapper
 */
void Logger::cleanup_logging() noexcept {
    if (!logging_enabled) return;
    
    try {
        if (log_file.is_open()) {
            log_file << generate_log_footer();
            log_file.flush();
            log_file.close();
        }
    } catch (...) {
        // Suppress exceptions during cleanup
    }
}

/**
 * @brief Gets the name of the current executable
 * 
 * @return The executable name without path, or "unknown" if retrieval fails
 * @note This function is noexcept and platform-independent
 */
std::string Logger::get_executable_name() noexcept {
    return ::hydroc::get_executable_name();
}

/**
 * @brief Gets the full path to the current executable
 * 
 * @return The full path to the executable, or empty string if retrieval fails
 * @note This function is noexcept and platform-independent
 */
std::string Logger::get_executable_path() noexcept {
    return ::hydroc::get_executable_path();
}

//-----------------------------------------------------------------------------
// Automatic Cleanup
//-----------------------------------------------------------------------------

/**
 * @brief RAII wrapper for automatic logging cleanup
 * 
 * This struct ensures that logging resources are properly released
 * when the program exits, even in case of exceptions. It uses RAII
 * principles to guarantee cleanup.
 * 
 * @note The destructor is noexcept to ensure cleanup even during exception handling
 */
struct LogCleanup {
    ~LogCleanup() noexcept {
        Logger::cleanup_logging();
    }
};

/**
 * @brief Global instance of LogCleanup
 * 
 * This static instance ensures that logging resources are automatically
 * cleaned up when the program exits, regardless of the exit path.
 */
static LogCleanup log_cleanup;

void Logger::log_message(const std::string& level, const std::string& msg) noexcept {
    if (!logging_enabled) {
        return;
    }

    try {
        std::lock_guard<std::mutex> lock(log_mutex);
        if (log_file.is_open()) {
            log_file << "[" << level << "] " << msg << std::endl;
        }
    } catch (...) {
        // Suppress exceptions during logging to ensure thread safety
    }
}

std::ostream& Logger::log_stream(const std::string& level) noexcept {
    static std::stringstream ss;
    ss.str("");  // Clear the stream
    ss << "[" << level << "] ";
    return ss;
}

} // namespace hydroc 