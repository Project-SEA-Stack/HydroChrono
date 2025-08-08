/**
 * @file logging.cpp
 * @brief Implementation of the main logging interface
 */

#include "logger_backend.h"
#include <hydroc/logging.h>
#include <memory>
#include <mutex>
#include <iomanip>
#include <thread>
#include <iostream>
#include <chrono>
#include <streambuf>
#include <functional>
#include <filesystem>
#include <cstdint>
#include <unordered_set>

namespace hydroc {

//-----------------------------------------------------------------------------
// Internal CLI Logger
//-----------------------------------------------------------------------------

namespace {
    // Flag to indicate we are currently writing via the logger; used to bypass
    // std::cout/std::cerr interceptors to avoid recursion and suppression
    thread_local bool g_in_logger_write = false;

    struct LoggingWriteGuard {
        LoggingWriteGuard() { g_in_logger_write = true; }
        ~LoggingWriteGuard() { g_in_logger_write = false; }
        LoggingWriteGuard(const LoggingWriteGuard&) = delete;
        LoggingWriteGuard& operator=(const LoggingWriteGuard&) = delete;
    };
}

class CLILogger {
public:
    explicit CLILogger(std::shared_ptr<LoggerBackend> backend)
        : backend_(std::move(backend)), showing_progress_(false), showing_spinner_(false),
          progress_current_(0), progress_total_(0), spinner_frame_(0) {
        last_spinner_update_ = std::chrono::steady_clock::now();
    }

    ~CLILogger() = default;
    CLILogger(const CLILogger&) = delete;
    CLILogger& operator=(const CLILogger&) = delete;
    CLILogger(CLILogger&&) noexcept = default;
    CLILogger& operator=(CLILogger&&) noexcept = default;

    void LogInfo(const std::string& message) { Log(LogLevel::Info, message, LogColor::Cyan); }
    void LogSuccess(const std::string& message) { Log(LogLevel::Success, message, LogColor::Green); }
    void LogWarning(const std::string& message) { Log(LogLevel::Warning, message, LogColor::Yellow); }
    void LogError(const std::string& message) { Log(LogLevel::Error, message, LogColor::Red); }
    void LogDebug(const std::string& message) {
        if (!backend_) return;
        const auto& cfg = backend_->GetConfig();
        if (cfg.enable_debug_logging || cfg.console_level == LoggingConfig::Level::Debug) {
            Log(LogLevel::Debug, message, LogColor::Gray);
        }
    }
    void Log(LogLevel level, const std::string& message, LogColor color = LogColor::White) {
        if (backend_) {
            LoggingWriteGuard guard;
            backend_->Log(level, message, LogContext{}, color);
        }
    }

    void ShowBanner();
    void ShowSectionSeparator() {
        constexpr int kLineWidth = 60;
        std::string sep;
        while (GetVisibleWidth(sep) < kLineWidth) sep += "─";
        // Ensure no overrun from width calc; trim if needed
        while (!sep.empty() && GetVisibleWidth(sep) > kLineWidth) sep.pop_back();
        Log(LogLevel::Success, sep, LogColor::Gray);
    }
    void ShowHeader(const std::string& title) {
        // Render an inline header line with exact visible width of 60 chars.
        constexpr int kLineWidth = 60;
        const std::string prefix = "── ";
        const std::string dash = "─";

        int prefix_width = GetVisibleWidth(prefix);
        int title_width = GetVisibleWidth(title);

        int pad_width = std::max(0, kLineWidth - prefix_width - title_width);
        std::string header = prefix + title;
        // Right pad with dashes
        for (int i = 0; i < pad_width; ++i) header += dash;
        // Trim any excess visible width introduced by width approximation
        while (!header.empty() && GetVisibleWidth(header) > kLineWidth) header.pop_back();

        Log(LogLevel::Success, header, LogColor::BrightCyan);
    }
    void ShowEmptyLine() { Log(LogLevel::Success, "", LogColor::White); }
    void ShowSectionBox(const std::string& title, const std::vector<std::string>& content_lines, LogColor content_color = LogColor::BrightCyan);
    void ShowWaveModel(const std::string& wave_type, double height, double period, double direction = 0.0, double phase = 0.0);
    void ShowSimulationResults(double final_time, int steps, double wall_time);
    void ShowLogFileLocation(const std::string& log_path);
    void ShowFooter();

    void CollectWarning(const std::string& warning_message) {
        // Always write to file if enabled, but suppress immediate console output
        if (backend_ && backend_->IsFileLoggingEnabled()) {
            LoggingConfig cfg = backend_->GetConfig();
            bool old_cli = cfg.enable_cli_output;
            cfg.enable_cli_output = false;
            backend_->UpdateConfig(cfg);
            backend_->Log(LogLevel::Warning, warning_message, LogContext{}, LogColor::Yellow);
            cfg.enable_cli_output = old_cli;
            backend_->UpdateConfig(cfg);
        }
        // Normalize and deduplicate for CLI warnings section
        std::string normalized = NormalizeWarning(warning_message);
        if (warning_set_.insert(normalized).second) {
            collected_warnings_.push_back(normalized);
        }
    }
    void DisplayWarnings();

    void ShowProgress(size_t current, size_t total, const std::string& message = "") {
        showing_progress_ = true;
        progress_current_ = current;
        progress_total_ = total;
        progress_message_ = message;
        UpdateProgressDisplay(current, total, message);
    }
    void ShowSpinner(const std::string& message) {
        showing_spinner_ = true;
        progress_message_ = message;
        spinner_frame_ = 0;
        last_spinner_update_ = std::chrono::steady_clock::now();
        std::string spinner_char = GetSpinnerChar();
        Log(LogLevel::Info, spinner_char + std::string(" ") + message, LogColor::Cyan);
    }
    void StopProgress() {
        if (showing_progress_ || showing_spinner_) {
            Log(LogLevel::Info, "", LogColor::White);
            showing_progress_ = false;
            showing_spinner_ = false;
        }
    }

    void ShowSummaryLine(const std::string& icon, const std::string& label, const std::string& value, LogColor color = LogColor::White) {
        // Align based on label only; icons are not part of alignment width
        constexpr int kLabelTargetWidth = 18; // chars
        int label_width = GetVisibleWidth(label);
        int pad = std::max(0, kLabelTargetWidth - label_width);
        std::string padded_label = label + std::string(pad, ' ');
        const std::string formatted_message = std::string("  ") + icon + " " + padded_label + " : " + value;
        Log(LogLevel::Success, formatted_message, color);
    }
    std::string CreateAlignedLine(const std::string& icon, const std::string& label, const std::string& value) {
        // Align based on label only; icons are not part of alignment width
        constexpr int kLabelTargetWidth = 18; // chars
        int label_width = GetVisibleWidth(label);
        int pad = std::max(0, kLabelTargetWidth - label_width);
        std::string padded_label = label + std::string(pad, ' ');
        std::string prefix = icon.empty() ? std::string("") : icon + std::string(" ");
        return prefix + padded_label + " : " + value;
    }

    std::shared_ptr<LoggerBackend> GetBackend() const { return backend_; }
    bool IsActive() const { return backend_ != nullptr; }

private:
    static void ReplaceAll(std::string& s, const std::string& from, const std::string& to) {
        if (from.empty()) return;
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    }
    static std::string NormalizeWarning(std::string s) {
        // Unify common variants and paths
        ReplaceAll(s, "data file:", "data file");
        // Normalize slashes
        ReplaceAll(s, "\\\\", "/");
        ReplaceAll(s, "\\", "/");
        // Simplify /../ occurrences conservatively
        for (int i = 0; i < 4; ++i) { ReplaceAll(s, "/../", "/"); }
        // Collapse double spaces
        while (s.find("  ") != std::string::npos) ReplaceAll(s, "  ", " ");
        return s;
    }
    void WriteToConsole(const std::string& message, LogColor color) {
        if (backend_ && backend_->GetConfig().enable_colors) {
            std::string color_code = GetColorCode(color);
            std::string reset_code = "\033[0m";
            std::cout << color_code << message << reset_code << std::endl;
        } else {
            std::cout << message << std::endl;
        }
    }
    std::string FormatConsoleMessage(const std::string& message, LogColor /*color*/) { return message; }
    void UpdateProgressDisplay(size_t current, size_t total, const std::string& message) {
        if (total == 0) return;
        const int bar_width = 50;
        const float progress = static_cast<float>(current) / static_cast<float>(total);
        const int filled_width = static_cast<int>(progress * bar_width);
        std::string bar = "[";
        for (int i = 0; i < bar_width; ++i) bar += (i < filled_width ? "=" : (i == filled_width ? ">" : " "));
        bar += "]";
        const int percentage = static_cast<int>(progress * 100);
        std::string progress_text = bar + std::string(" ") + std::to_string(percentage) + "%";
        if (!message.empty()) progress_text += std::string(" - ") + message;
        Log(LogLevel::Info, progress_text, LogColor::Cyan);
    }
    std::string GetSpinnerChar() {
        static const std::string spinner_chars[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_spinner_update_);
        if (elapsed.count() > 100) { spinner_frame_ = (spinner_frame_ + 1) % 10; last_spinner_update_ = now; }
        return spinner_chars[spinner_frame_];
    }

private:
    std::shared_ptr<LoggerBackend> backend_;
    std::vector<std::string> collected_warnings_;
    std::unordered_set<std::string> warning_set_;
    bool showing_progress_;
    bool showing_spinner_;
    size_t progress_current_;
    size_t progress_total_;
    std::string progress_message_;
    size_t spinner_frame_;
    std::chrono::steady_clock::time_point last_spinner_update_;
};

inline void CLILogger::ShowBanner() {
    Log(LogLevel::Success, "", LogColor::White);
    Log(LogLevel::Success, "╭─────────────────────────────────────────────────────────────────────────────────────────────────────╮", LogColor::BrightCyan);
    Log(LogLevel::Success, "│                                                                                                     │", LogColor::BrightCyan);
    Log(LogLevel::Success, "│    ░░   ░░ ░░    ░░ ░░░░░░  ░░░░░░   ░░░░░░   ░░░░░░ ░░   ░░ ░░░░░░   ░░░░░░  ░░░    ░░  ░░░░░░     │", LogColor::BrightCyan);
    Log(LogLevel::Success, "│    ▒▒   ▒▒  ▒▒  ▒▒  ▒▒   ▒▒ ▒▒   ▒▒ ▒▒    ▒▒ ▒▒      ▒▒   ▒▒ ▒▒   ▒▒ ▒▒    ▒▒ ▒▒▒▒   ▒▒ ▒▒    ▒▒    │", LogColor::BrightCyan);
    Log(LogLevel::Success, "│    ▒▒▒▒▒▒▒   ▒▒▒▒   ▒▒   ▒▒ ▒▒▒▒▒▒  ▒▒    ▒▒ ▒▒      ▒▒▒▒▒▒▒ ▒▒▒▒▒▒  ▒▒    ▒▒ ▒▒ ▒▒  ▒▒ ▒▒    ▒▒    │", LogColor::BrightCyan);
    Log(LogLevel::Success, "│    ▓▓   ▓▓    ▓▓    ▓▓   ▓▓ ▓▓   ▓▓ ▓▓    ▓▓ ▓▓      ▓▓   ▓▓ ▓▓   ▓▓ ▓▓    ▓▓ ▓▓  ▓▓ ▓▓ ▓▓    ▓▓    │", LogColor::BrightCyan);
    Log(LogLevel::Success, "│    ██   ██    ██    ██████  ██   ██  ██████   ██████ ██   ██ ██   ██  ██████  ██   ████  ██████     │", LogColor::BrightCyan);
    Log(LogLevel::Success, "│                                                                                                     │", LogColor::BrightCyan);
    Log(LogLevel::Success, "│                                                                                                     │", LogColor::BrightCyan);
    Log(LogLevel::Success, "│                                   Hydrodynamics for Project Chrono                                  │", LogColor::White);
    Log(LogLevel::Success, "│                                                                                                     │", LogColor::BrightCyan);
    Log(LogLevel::Success, "│  Version  : 0.3.0                                                                                   │", LogColor::Gray);
    Log(LogLevel::Success, "│  Status   : Prototype                                                                               │", LogColor::Gray);
    Log(LogLevel::Success, "│  Author   : NREL SEA-Stack Team                                                                     │", LogColor::Gray);
    Log(LogLevel::Success, "│  License  : Apache-2.0                                                                              │", LogColor::Gray);
    Log(LogLevel::Success, "│  URL      : https://github.com/NREL/HydroChrono                                                     │", LogColor::Gray);
    Log(LogLevel::Success, "│                                                                                                     │", LogColor::BrightCyan);
    Log(LogLevel::Success, "╰─────────────────────────────────────────────────────────────────────────────────────────────────────╯", LogColor::BrightCyan);
    Log(LogLevel::Success, "", LogColor::White);
}

inline void CLILogger::ShowSectionBox(const std::string& title, const std::vector<std::string>& content_lines, LogColor content_color) {
    // exactly one blank line above and below
    ShowEmptyLine();
    // Top border: ╭─ <title> ─── … ─╮ (60 chars)
    constexpr int kLineWidth = 60;
    std::string top_mid = "╭─ " + title + " ";
    while (GetVisibleWidth(top_mid) < kLineWidth - 1) top_mid += "─"; // leave 1 for closing
    while (!top_mid.empty() && GetVisibleWidth(top_mid) > kLineWidth - 1) top_mid.pop_back();
    std::string top_border = top_mid + "╮";
    Log(LogLevel::Success, top_border, LogColor::BrightCyan);
    for (const auto& line : content_lines) { Log(LogLevel::Success, std::string("  ") + line, content_color); }
    std::string bottom_mid = "╰";
    while (GetVisibleWidth(bottom_mid) < kLineWidth - 1) bottom_mid += "─"; // leave 1 for closing
    while (!bottom_mid.empty() && GetVisibleWidth(bottom_mid) > kLineWidth - 1) bottom_mid.pop_back();
    std::string bottom_border = bottom_mid + "╯";
    Log(LogLevel::Success, bottom_border, LogColor::BrightCyan);
    ShowEmptyLine();
}

    inline void CLILogger::ShowWaveModel(const std::string& wave_type, double height, double period, double direction, double phase) {
        ShowEmptyLine();
        ShowHeader("🌊 Wave Model");
        const std::string height_str = FormatNumber(height, 1) + " m";
        const std::string period_str = FormatNumber(period, 1) + " s";
        Log(LogLevel::Success, CreateAlignedLine("•", "Type", wave_type), LogColor::White);
        Log(LogLevel::Success, CreateAlignedLine("•", "Height", height_str), LogColor::White);
        Log(LogLevel::Success, CreateAlignedLine("•", "Period", period_str), LogColor::White);
        if (direction != 0.0) Log(LogLevel::Success, CreateAlignedLine("•", "Direction", FormatNumber(direction, 1) + "°"), LogColor::White);
        if (phase != 0.0) Log(LogLevel::Success, CreateAlignedLine("•", "Phase", FormatNumber(phase, 1) + "°"), LogColor::White);
        ShowEmptyLine();
    }

inline void CLILogger::ShowSimulationResults(double final_time, int steps, double wall_time) {
    ShowEmptyLine();
    ShowHeader("✅ Simulation Complete");
    Log(LogLevel::Success, CreateAlignedLine("•", "Final Time", FormatNumber(final_time, 2) + " s"), LogColor::White);
    Log(LogLevel::Success, CreateAlignedLine("•", "Steps", std::to_string(steps)), LogColor::White);
    Log(LogLevel::Success, CreateAlignedLine("•", "Duration", FormatNumber(wall_time, 2) + " s"), LogColor::White);
    Log(LogLevel::Success, CreateAlignedLine("•", "Wall Time", FormatNumber(wall_time, 2) + " s"), LogColor::White);
    ShowEmptyLine();
}

inline void CLILogger::ShowLogFileLocation(const std::string& log_path) {
    if (log_path.empty()) return; // do not show if file logging disabled
    ShowEmptyLine();
    ShowHeader("📄 Log File");
    // Print concise relative path from current working directory if possible
    std::string path_to_show = log_path;
    try {
        std::filesystem::path p = std::filesystem::path(log_path);
        std::filesystem::path cwd = std::filesystem::current_path();
        std::error_code ec;
        std::filesystem::path rel = std::filesystem::relative(p, cwd, ec);
        std::string rel_str = (!ec && !rel.empty()) ? rel.generic_string() : p.generic_string();
        // Prefer showing from 'logs/' onward if present
        auto logs_pos = rel_str.rfind("/logs/");
        if (logs_pos != std::string::npos) {
            path_to_show = rel_str.substr(logs_pos + 1); // keep 'logs/...'
        } else {
            path_to_show = rel_str;
        }
    } catch (...) {
        // fallback to original path
    }
    Log(LogLevel::Success, std::string("📄 Log written to: ") + path_to_show, LogColor::Blue);
    ShowEmptyLine();
}

inline void CLILogger::ShowFooter() {
    ShowEmptyLine();
    ShowHeader("✅ End of Output");
    Log(LogLevel::Success, std::string("💧 Part of Project SEA-Stack • Building the Next Generation of Marine Simulation Software."), LogColor::Gray);
    ShowEmptyLine();
}

inline void CLILogger::DisplayWarnings() {
    if (collected_warnings_.empty()) return;
    ShowEmptyLine();
    ShowHeader("⚠️ Warnings");
    for (const auto& warning : collected_warnings_) {
        Log(LogLevel::Warning, std::string("• ") + warning, LogColor::Yellow);
    }
    ShowEmptyLine();
}

//-----------------------------------------------------------------------------
// Global State Management
//-----------------------------------------------------------------------------

namespace {
    std::shared_ptr<LoggerBackend> g_backend;
    std::shared_ptr<CLILogger> g_cli_logger;
    std::mutex g_logging_mutex;
    bool g_initialized = false;

    // Stream capture machinery to route stray std::cout/std::cerr to our logger
    struct LoggerStreambuf : public std::streambuf {
        enum class StreamKind { StdOut, StdErr };

        LoggerStreambuf(std::streambuf* original, StreamKind kind)
            : original_(original), kind_(kind) {}

      protected:
        int overflow(int ch) override {
            if (ch == EOF) {
                return 0;
            }
            char c = static_cast<char>(ch);
            // If logger is actively writing, bypass interception entirely
            if (g_in_logger_write) {
                if (original_) {
                    original_->sputc(c);
                }
                return ch;
            }
            if (c == '\n') {
                FlushBuffer();
            } else {
                buffer_.push_back(c);
            }
            return ch;
        }

        int sync() override {
            if (!g_in_logger_write) {
                FlushBuffer();
            }
            return 0;
        }

      private:
        static bool StartsWith(const std::string& s, const char* prefix) {
            return s.rfind(prefix, 0) == 0;
        }

        void FlushBuffer() {
            if (buffer_.empty()) {
                return;
            }

            // If logger is actively writing, do not emit buffered external text now
            if (g_in_logger_write) {
                return;
            }

            // If line looks like our own logger output (e.g., starts with '[' or begins with ANSI then '['), pass through
            if (!buffer_.empty() && (buffer_[0] == '[' || (buffer_[0] == '\033' && buffer_.find("[") != std::string::npos))) {
                if (original_) {
                    original_->sputn(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
                    original_->sputc('\n');
                }
                buffer_.clear();
                return;
            }

            // Route or suppress external prints
            if (kind_ == StreamKind::StdOut) {
                if (StartsWith(buffer_, "File: ")) {
                    // Treat noisy OBJ path echoes as debug-only
                    hydroc::debug::LogDebug(buffer_);
                } else if (buffer_.find("Cannot open colormap data file") != std::string::npos) {
                    // Collect for Warnings section only; avoid inline duplication
                    hydroc::cli::CollectWarning(buffer_);
                } else if (buffer_.find("Mesh file has non-standard units") != std::string::npos) {
                    // Collect for Warnings section only; avoid inline duplication
                    hydroc::cli::CollectWarning(buffer_);
                } else {
                    hydroc::cli::LogInfo(buffer_);
                }
            } else { // StdErr
                if (buffer_.find("Cannot open colormap data file") != std::string::npos) {
                    // Collect for Warnings section only; avoid inline duplication
                    hydroc::cli::CollectWarning(buffer_);
                } else if (buffer_.find("Mesh file has non-standard units") != std::string::npos) {
                    // Collect for Warnings section only; avoid inline duplication
                    hydroc::cli::CollectWarning(buffer_);
                } else {
                    hydroc::cli::LogWarning(buffer_);
                }
            }

            buffer_.clear();
        }

        std::string buffer_;
        std::streambuf* original_;
        StreamKind kind_;
    };

    std::unique_ptr<LoggerStreambuf> g_cout_capture;
    std::unique_ptr<LoggerStreambuf> g_cerr_capture;
    std::streambuf* g_orig_cout = nullptr;
    std::streambuf* g_orig_cerr = nullptr;
}

//-----------------------------------------------------------------------------
// Main Logging Interface Implementation
//-----------------------------------------------------------------------------

bool Initialize(const LoggingConfig& config) {
    std::lock_guard<std::mutex> lock(g_logging_mutex);
    
    // Shutdown existing logging if any
    if (g_initialized) {
        Shutdown();
    }
    
    g_backend = std::make_shared<LoggerBackend>(config);
    // Always create the CLI logger to coordinate console and file output.
    // Console emission is still controlled by backend config.enable_cli_output.
    g_cli_logger = std::make_shared<CLILogger>(g_backend);

    // Capture stray std::cout / std::cerr from third-party libs and legacy code
    if (!g_orig_cout) {
        g_orig_cout = std::cout.rdbuf();
    }
    if (!g_orig_cerr) {
        g_orig_cerr = std::cerr.rdbuf();
    }
    g_cout_capture = std::make_unique<LoggerStreambuf>(g_orig_cout, LoggerStreambuf::StreamKind::StdOut);
    g_cerr_capture = std::make_unique<LoggerStreambuf>(g_orig_cerr, LoggerStreambuf::StreamKind::StdErr);
    std::cout.rdbuf(g_cout_capture.get());
    std::cerr.rdbuf(g_cerr_capture.get());
    g_initialized = true;
    return true;
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(g_logging_mutex);
    
    // Restore original stream buffers
    if (g_orig_cout) {
        std::cout.rdbuf(g_orig_cout);
    }
    if (g_orig_cerr) {
        std::cerr.rdbuf(g_orig_cerr);
    }
    g_cout_capture.reset();
    g_cerr_capture.reset();

    g_cli_logger.reset();
    g_backend.reset();
    g_initialized = false;
}

static std::shared_ptr<CLILogger> GetCLILogger() {
    std::lock_guard<std::mutex> lock(g_logging_mutex);
    return g_cli_logger;
}

bool IsInitialized() {
    std::lock_guard<std::mutex> lock(g_logging_mutex);
    return g_initialized && g_backend != nullptr;
}

//-----------------------------------------------------------------------------
// CLI Logging Namespace Implementation
//-----------------------------------------------------------------------------

namespace cli {

void LogInfo(const std::string& message) {
    auto logger = GetCLILogger();
    if (logger) {
        logger->LogInfo(message);
    }
}

void LogSuccess(const std::string& message) {
    auto logger = GetCLILogger();
    if (logger) {
        logger->LogSuccess(message);
    }
}

void LogWarning(const std::string& message) {
    auto logger = GetCLILogger();
    if (logger) {
        logger->LogWarning(message);
    }
}

void LogError(const std::string& message) {
    auto logger = GetCLILogger();
    if (logger) {
        logger->LogError(message);
    }
}

void LogDebug(const std::string& message) {
    auto logger = GetCLILogger();
    if (logger) {
        logger->LogDebug(message);
    }
}

void ShowBanner() {
    auto logger = GetCLILogger();
    if (logger) {
        logger->ShowBanner();
    }
}

void ShowHeader(const std::string& title) {
    auto logger = GetCLILogger();
    if (logger) {
        logger->ShowHeader(title);
    }
}

void ShowSectionSeparator() {
    auto logger = GetCLILogger();
    if (logger) {
        logger->ShowSectionSeparator();
    }
}

void ShowEmptyLine() {
    auto logger = GetCLILogger();
    if (logger) {
        logger->ShowEmptyLine();
    }
}

void ShowSectionBox(const std::string& title, 
                   const std::vector<std::string>& content_lines) {
    auto logger = GetCLILogger();
    if (logger) {
        logger->ShowSectionBox(title, content_lines);
    }
}

void ShowWaveModel(const std::string& wave_type, double height, 
                  double period, double direction, double phase) {
    auto logger = GetCLILogger();
    if (logger) {
        logger->ShowWaveModel(wave_type, height, period, direction, phase);
    }
}

void ShowSimulationResults(double final_time, int steps, double wall_time) {
    auto logger = GetCLILogger();
    if (logger) {
        logger->ShowSimulationResults(final_time, steps, wall_time);
    }
}

void ShowLogFileLocation(const std::string& log_path) {
    auto logger = GetCLILogger();
    if (logger) {
        logger->ShowLogFileLocation(log_path);
    }
}

void ShowFooter() {
    auto logger = GetCLILogger();
    if (logger) {
        logger->ShowFooter();
    }
}

void CollectWarning(const std::string& warning_message) {
    auto logger = GetCLILogger();
    if (logger) {
        logger->CollectWarning(warning_message);
    }
}

void DisplayWarnings() {
    auto logger = GetCLILogger();
    if (logger) {
        logger->DisplayWarnings();
    }
}

void ShowSummaryLine(const std::string& icon, const std::string& label,
                     const std::string& value, LogColor color) {
    auto logger = GetCLILogger();
    if (logger) {
        logger->ShowSummaryLine(icon, label, value, color);
    }
}

std::string CreateAlignedLine(const std::string& icon, const std::string& label,
                              const std::string& value) {
    auto logger = GetCLILogger();
    if (logger) {
        return logger->CreateAlignedLine(icon, label, value);
    }
    return icon + " " + label + " : " + value;
}

} // namespace cli

//-----------------------------------------------------------------------------
// Debug Logging Namespace Implementation
//-----------------------------------------------------------------------------

namespace debug {

void LogDebug(const std::string& message) {
    auto cli = GetCLILogger();
    if (cli) cli->LogDebug(message);
}

void LogTrace(const std::string& message) {
    auto cli = GetCLILogger();
    if (cli) cli->LogDebug(std::string("[TRACE] ") + message);
}

void LogInfo(const std::string& message) {
    auto cli = GetCLILogger();
    if (cli) cli->LogInfo(message);
}

void LogWarning(const std::string& message) {
    hydroc::cli::LogWarning(message);
}

void LogError(const std::string& message) {
    hydroc::cli::LogError(message);
}

void LogSystemInfo() {
    // Minimal stub - could be expanded in backend as needed
}

void LogExecutableInfo() {
    // Minimal stub
}

void LogCommandLineArgs(int argc, char** argv) {
    // Minimal stub
}

void LogEnvironmentVars(bool include_all) {
    // Minimal stub
}

void LogMemoryInfo() {
    // Minimal stub
}

void LogCPUInfo() {
    // Minimal stub
}

void LogPlatformInfo() {
    // Minimal stub
}

size_t StartTimer(const std::string& timer_name) {
    // Minimal stub
    return 0;
}

void StopTimer(size_t timer_id) {
    // Minimal stub
}

void LogPerformance(const std::string& operation_name, double duration_ms) {
    // Minimal stub
}

void LogMemoryOperation(const std::string& operation, size_t size, void* ptr) {
    // Minimal stub
}

void LogFunctionEntry(const std::string& function_name) {
    hydroc::debug::LogDebug(std::string("→ Entering: ") + function_name);
}

void LogFunctionExit(const std::string& function_name) {
    hydroc::debug::LogDebug(std::string("← Exiting: ") + function_name);
}

void LogVariable(const std::string& variable_name, const std::string& value) {
    hydroc::debug::LogDebug(std::string("Variable: ") + variable_name + " = " + value);
}

void LogStackTrace() {
    // Minimal stub
}

void LogThreadInfo() {
    // Minimal stub
}

bool IsDebugEnabled() {
    std::lock_guard<std::mutex> lock(g_logging_mutex);
    return g_initialized;
}

} // namespace debug

//-----------------------------------------------------------------------------
// Shared helpers implementation
//-----------------------------------------------------------------------------

std::string LogLevelToString(LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Success: return "SUCCESS";
    case LogLevel::Warning: return "WARNING";
    case LogLevel::Error: return "ERROR";
    }
    return "INFO";
}

std::string GetColorCode(LogColor color) {
    switch (color) {
    case LogColor::White: return "\033[37m";
    case LogColor::Green: return "\033[32m";
    case LogColor::Yellow: return "\033[33m";
    case LogColor::Red: return "\033[31m";
    case LogColor::Cyan: return "\033[36m";
    case LogColor::Blue: return "\033[34m";
    case LogColor::Gray: return "\033[90m";
    case LogColor::BrightWhite: return "\033[97m";
    case LogColor::BrightCyan: return "\033[96m";
    case LogColor::BrightGreen: return "\033[92m";
    }
    return "\033[0m";
}

std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string GetTimestampISO8601() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    gmtime_s(&tm_buf, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// Minimal UTF-8 decoder: returns codepoint and advances index
static uint32_t DecodeUtf8CodePoint(const std::string& s, size_t& index) {
    unsigned char c0 = static_cast<unsigned char>(s[index]);
    if ((c0 & 0x80u) == 0) { // ASCII
        ++index;
        return c0;
    }
    // Determine sequence length
    size_t remaining = s.size() - index;
    if ((c0 & 0xE0u) == 0xC0u && remaining >= 2) {
        uint32_t cp = (c0 & 0x1Fu) << 6;
        unsigned char c1 = static_cast<unsigned char>(s[index + 1]);
        cp |= (c1 & 0x3Fu);
        index += 2;
        return cp;
    }
    if ((c0 & 0xF0u) == 0xE0u && remaining >= 3) {
        uint32_t cp = (c0 & 0x0Fu) << 12;
        unsigned char c1 = static_cast<unsigned char>(s[index + 1]);
        unsigned char c2 = static_cast<unsigned char>(s[index + 2]);
        cp |= (c1 & 0x3Fu) << 6;
        cp |= (c2 & 0x3Fu);
        index += 3;
        return cp;
    }
    if ((c0 & 0xF8u) == 0xF0u && remaining >= 4) {
        uint32_t cp = (c0 & 0x07u) << 18;
        unsigned char c1 = static_cast<unsigned char>(s[index + 1]);
        unsigned char c2 = static_cast<unsigned char>(s[index + 2]);
        unsigned char c3 = static_cast<unsigned char>(s[index + 3]);
        cp |= (c1 & 0x3Fu) << 12;
        cp |= (c2 & 0x3Fu) << 6;
        cp |= (c3 & 0x3Fu);
        index += 4;
        return cp;
    }
    // Invalid sequence: consume one byte
    ++index;
    return 0xFFFDu; // replacement char
}

static bool IsEmojiDoubleWidth(uint32_t cp) {
    // Approximate: treat emoji blocks as width 2
    // Emoticons, Misc Symbols and Pictographs, Supplemental Symbols and Pictographs, Symbols & Pictographs
    return (cp >= 0x1F300u && cp <= 0x1FAFFu);
}

int GetVisibleWidth(const std::string& str) {
    int width = 0;
    bool in_escape = false;
    for (size_t i = 0; i < str.size();) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c == '\033') { in_escape = true; ++i; continue; }
        if (in_escape) {
            if (c == 'm') in_escape = false;
            ++i;
            continue;
        }
        if ((c & 0x80u) == 0) {
            // ASCII
            ++width;
            ++i;
        } else {
            size_t before = i;
            uint32_t cp = DecodeUtf8CodePoint(str, i);
            // If decode failed, ensure progress
            if (i == before) { ++i; ++width; continue; }
            width += IsEmojiDoubleWidth(cp) ? 2 : 1;
        }
    }
    return width;
}

std::string FormatNumber(double value, int decimal_places) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(decimal_places) << value;
    return oss.str();
}

} // namespace hydroc 