/**
 * @file logging.cpp
 * @brief Implementation of the main logging interface
 */

#if __has_include(<hydroc/logging.h>)
#  include <hydroc/logging.h>
#else
#  include "../../include/hydroc/logging.h"
#endif
#include "logger_backend.h"
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <streambuf>
#include <string>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace hydroc {

//-----------------------------------------------------------------------------
// Internal CLI Logger
//-----------------------------------------------------------------------------

namespace {
    // Flag to indicate we are currently writing via the logger; used to bypass
    // std::cout/std::cerr interceptors to avoid recursion and suppression.
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
    /**
     * @brief Coordinating CLI logger used by HydroChrono executables.
     *
     * Bridges the public logging API to the low-level backend, and provides
     * user-facing helpers (headers, boxes, progress). This class is internal
     * to the implementation; the public surface is declared in
     * `include/hydroc/logging.h` under the `hydroc::cli` namespace.
     */
    explicit CLILogger(std::shared_ptr<LoggerBackend> backend)
        : backend_(std::move(backend)), showing_progress_(false),
          progress_last_width_(0), progress_completed_(false) {}

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
        if (cfg.enable_debug_logging || cfg.console_level == LogLevel::Debug) {
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
    /**
     * @brief Output a thin visual separator spanning the standard header width.
     */
    void ShowSectionSeparator() noexcept {
        std::string sep;
        while (GetVisibleWidth(sep) < kHeaderWidth) sep += "─";
        // Ensure no overrun from width calc; trim if needed
        while (!sep.empty() && GetVisibleWidth(sep) > kHeaderWidth) sep.pop_back();
        Log(LogLevel::Success, sep, LogColor::Gray);
    }
    void ShowHeader(const std::string& title) {
        // Render an inline header line with exact visible width of kHeaderWidth.
        const std::string prefix = "── ";
        const std::string dash = "─";

        const int prefix_width = GetVisibleWidth(prefix);
        const int title_width = GetVisibleWidth(title);

        const int pad_width = std::max(0, kHeaderWidth - prefix_width - title_width);
        std::string header = prefix + title;
        // Right pad with dashes
        for (int i = 0; i < pad_width; ++i) header += dash;
        // Trim any excess visible width introduced by width approximation.
        while (!header.empty() && GetVisibleWidth(header) > kHeaderWidth) header.pop_back();

        Log(LogLevel::Success, header, LogColor::BrightCyan);
    }
    void ShowEmptyLine() noexcept { Log(LogLevel::Success, "", LogColor::White); }
    /**
     * @brief Render a boxed section with a title and content lines.
     * @param title Heading displayed in the box border.
     * @param content_lines Lines rendered inside the box body.
     * @param content_color Color used for content lines.
     */
    void ShowSectionBox(const std::string& title, const std::vector<std::string>& content_lines, LogColor content_color = LogColor::BrightCyan);
    void ShowWaveModel(const std::string& wave_type, double height, double period, double direction = 0.0, double phase = 0.0);
    void ShowSimulationResults(double final_time, int steps, double wall_time);
    /**
     * @brief Show a concise path to the active log file (if enabled).
     */
    void ShowLogFileLocation(const std::string& log_path);
    void ShowFooter();

    /**
     * @brief Record a warning for later display and persist it to file if enabled.
     *        CLI output is suppressed at collection time to avoid duplication.
     */
    void CollectWarning(const std::string& warning_message) {
        // Persist to file only (no CLI) without mutating global config.
        // If LoggerBackend doesn't expose such an API, this becomes a no-op on file.
        if (backend_ && backend_->IsFileLoggingEnabled()) {
            // Optional future: backend_->LogToFile(...)
        }
        // Normalize and deduplicate for CLI warnings section.
        std::string normalized = NormalizeWarning(warning_message);
        if (warning_set_.insert(normalized).second) {
            collected_warnings_.push_back(normalized);
        }
    }
    void DisplayWarnings();

    /**
     * @brief Render or update an in-place textual progress bar on stderr.
     * @param current Current progress value (0..total)
     * @param total   Total work units (must be > 0)
     * @param message Optional short status message to append
     */
    void ShowProgress(size_t current, size_t total, const std::string& message = "") {
        showing_progress_ = true;
        progress_completed_ = false;
        UpdateProgressDisplay(current, total, message);
    }
    /**
     * @brief Emit a one-shot spinner indicator with a message.
     *
     * Note: This does not animate by itself; callers who want animation should
     * call this periodically (or drive updates externally) to advance frames.
     */
    // Removed spinner API (unused). Add back if periodic animation is needed.
    /**
     * @brief Clear any active progress/spinner line from the console.
     *
     * Uses stderr for in-place line management. Interleaving with stdout
     * from other threads may still affect presentation.
     */
    void StopProgress() noexcept {
        if (showing_progress_) {
            LoggingWriteGuard guard;
            if (!progress_completed_) {
                // Clear the in-place progress line only if not completed
                std::cerr << "\r";
                for (int i = 0; i < progress_last_width_; ++i) std::cerr << ' ';
                std::cerr << "\r" << std::endl;
            }
            showing_progress_ = false;
            progress_last_width_ = 0;
            progress_completed_ = false;
        }
    }

    void ShowSummaryLine(const std::string& icon, const std::string& label, const std::string& value, LogColor color = LogColor::White) {
        // Align based on label only; icons are not part of alignment width
        int label_width = GetVisibleWidth(label);
        int pad = std::max(0, kLabelTargetWidth - label_width);
        std::string padded_label = label + std::string(pad, ' ');
        const std::string formatted_message = std::string("  ") + icon + " " + padded_label + " : " + value;
        Log(LogLevel::Success, formatted_message, color);
    }
    std::string CreateAlignedLine(const std::string& icon, const std::string& label, const std::string& value) {
        // Align based on label only; icons are not part of alignment width
        int label_width = GetVisibleWidth(label);
        int pad = std::max(0, kLabelTargetWidth - label_width);
        std::string padded_label = label + std::string(pad, ' ');
        std::string prefix = icon.empty() ? std::string("") : icon + std::string(" ");
        return prefix + padded_label + " : " + value;
    }

    /**
     * @brief Access the underlying backend (shared across frontends).
     */
    std::shared_ptr<LoggerBackend> GetBackend() const { return backend_; }
    /**
     * @brief True if the logger is connected to a backend.
     */
    bool IsActive() const { return backend_ != nullptr; }

private:
    // Constants for progress rendering and spinner cadence
    static constexpr int kProgressBarWidth = 50;      // characters inside [ ... ]
    static constexpr int kHeaderWidth = 60;           // visible character target for headers/boxes
    static constexpr int kLabelTargetWidth = 18;      // alignment width for labels

    /**
     * @brief Replace all occurrences of a substring in-place.
     */
    static void ReplaceAll(std::string& s, const std::string& from, const std::string& to) {
        if (from.empty()) return;
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    }
    /**
     * @brief Normalize warning text to reduce duplicates from minor variations.
     */
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
    /**
     * @brief Internal helper to render an in-place progress line to stderr.
     *
     * Preserves visual cleanliness by blanking residual characters when the
     * updated line is shorter than the previous one.
     */
    void UpdateProgressDisplay(size_t current, size_t total, const std::string& message) {
        if (total == 0) return;
        const float progress = static_cast<float>(current) / static_cast<float>(total);
        const int filled_width = static_cast<int>(progress * kProgressBarWidth);
        std::string bar = "[";
        for (int i = 0; i < kProgressBarWidth; ++i) bar += (i < filled_width ? "=" : (i == filled_width ? ">" : " "));
        bar += "]";
        const int percentage = static_cast<int>(progress * 100);
        std::string progress_text = bar + std::string(" ") + std::to_string(percentage) + "%";
        if (!message.empty()) progress_text += std::string(" - ") + message;
        // Write in-place on the same console line using stderr, guarded to bypass interception
        LoggingWriteGuard guard;
        std::cerr << "\r" << progress_text;
        // Clear any remnants from a longer previous line
        int pad = std::max(0, progress_last_width_ - static_cast<int>(progress_text.size()));
        for (int i = 0; i < pad; ++i) std::cerr << ' ';
        std::cerr << std::flush;
        progress_last_width_ = static_cast<int>(progress_text.size());

        // If we've reached or exceeded total, finalize the line with a newline and mark complete
        if (current >= total) {
            std::cerr << std::endl;
            showing_progress_ = false;
            progress_last_width_ = 0;
            progress_completed_ = true;
        } else {
            progress_completed_ = false;
        }
    }
    /**
     * @brief Compute the current spinner glyph based on elapsed time.
     */
    // Spinner support removed

private:
    std::shared_ptr<LoggerBackend> backend_;
    std::vector<std::string> collected_warnings_;
    std::unordered_set<std::string> warning_set_;
    bool showing_progress_;
    int progress_last_width_;
    bool progress_completed_;
    // Spinner timing removed
};

void CLILogger::ShowBanner() {
    // NOLINTBEGIN(readability/line_length)
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
    Log(LogLevel::Success, "│  Version        : 0.3.0                                                                             │", LogColor::Gray);
    Log(LogLevel::Success, "│  Status         : Prototype                                                                         │", LogColor::Gray);
    Log(LogLevel::Success, "│  Author         : SEA-Stack Development Team                                                        │", LogColor::Gray);
    Log(LogLevel::Success, "│  Lead Developer : David Ogden                                                                       │", LogColor::Gray);
    Log(LogLevel::Success, "│  License        : Apache-2.0                                                                        │", LogColor::Gray);
    Log(LogLevel::Success, "│  URL            : https://github.com/NREL/HydroChrono                                               │", LogColor::Gray);
    Log(LogLevel::Success, "│                                                                                                     │", LogColor::BrightCyan);
    Log(LogLevel::Success, "╰─────────────────────────────────────────────────────────────────────────────────────────────────────╯", LogColor::BrightCyan);
    Log(LogLevel::Success, "", LogColor::White);
    // NOLINTEND(readability/line_length)
}

void CLILogger::ShowSectionBox(const std::string& title, const std::vector<std::string>& content_lines, LogColor content_color) {
    // exactly one blank line above and below
    ShowEmptyLine();
    // Top border: ╭─ <title> ─── … ─╮ (kHeaderWidth chars)
    std::string top_mid = "╭─ " + title + " ";
    while (GetVisibleWidth(top_mid) < kHeaderWidth - 1) top_mid += "─"; // leave 1 for closing
    while (!top_mid.empty() && GetVisibleWidth(top_mid) > kHeaderWidth - 1) top_mid.pop_back();
    std::string top_border = top_mid + "╮";
    Log(LogLevel::Success, top_border, LogColor::BrightCyan);
    for (const auto& line : content_lines) { Log(LogLevel::Success, std::string("  ") + line, content_color); }
    std::string bottom_mid = "╰";
    while (GetVisibleWidth(bottom_mid) < kHeaderWidth - 1) bottom_mid += "─"; // leave 1 for closing
    while (!bottom_mid.empty() && GetVisibleWidth(bottom_mid) > kHeaderWidth - 1) bottom_mid.pop_back();
    std::string bottom_border = bottom_mid + "╯";
    Log(LogLevel::Success, bottom_border, LogColor::BrightCyan);
    ShowEmptyLine();
}

void CLILogger::ShowWaveModel(const std::string& wave_type, double height, double period, double direction, double phase) {
    ShowEmptyLine();
    ShowHeader("🌊 Wave Model");
    const std::string height_str = FormatNumber(height, 3) + " m";
    const std::string period_str = FormatNumber(period, 3) + " s";
    Log(LogLevel::Success, CreateAlignedLine("•", "Type", wave_type), LogColor::White);
    Log(LogLevel::Success, CreateAlignedLine("•", "Height", height_str), LogColor::White);
    Log(LogLevel::Success, CreateAlignedLine("•", "Period", period_str), LogColor::White);
    if (direction != 0.0) Log(LogLevel::Success, CreateAlignedLine("•", "Direction", FormatNumber(direction, 1) + "°"), LogColor::White);
    if (phase != 0.0) Log(LogLevel::Success, CreateAlignedLine("•", "Phase", FormatNumber(phase, 1) + "°"), LogColor::White);
    ShowEmptyLine();
}

void CLILogger::ShowSimulationResults(double final_time, int steps, double wall_time) {
    ShowEmptyLine();
    ShowHeader("✅ Simulation Complete");
    Log(LogLevel::Success, CreateAlignedLine("•", "Final Time", FormatNumber(final_time, 2) + " s"), LogColor::White);
    Log(LogLevel::Success, CreateAlignedLine("•", "Steps", std::to_string(steps)), LogColor::White);
    // Note: Duration and Wall Time are currently equivalent in this context.
    Log(LogLevel::Success, CreateAlignedLine("•", "Duration", FormatNumber(wall_time, 2) + " s"), LogColor::White);
    Log(LogLevel::Success, CreateAlignedLine("•", "Wall Time", FormatNumber(wall_time, 2) + " s"), LogColor::White);
    ShowEmptyLine();
}

void CLILogger::ShowLogFileLocation(const std::string& log_path) {
    if (log_path.empty()) return; // do not show if file logging disabled
    ShowEmptyLine();
    ShowHeader("📄 Log File");
    // Prefer concise display starting from 'logs/' if present; avoid filesystem deps
    std::string normalized = log_path;
    ReplaceAll(normalized, "\\\\", "/");
    ReplaceAll(normalized, "\\", "/");
    std::string path_to_show = normalized;
    auto pos = normalized.rfind("/logs/");
    if (pos != std::string::npos && pos + 1 < normalized.size()) {
        path_to_show = normalized.substr(pos + 1); // keep 'logs/...'
    }
    Log(LogLevel::Success, std::string("📄 Log written to: ") + path_to_show, LogColor::Blue);
    ShowEmptyLine();
}

void CLILogger::ShowFooter() {
    ShowEmptyLine();
    ShowHeader("✅ End of Output");
    Log(LogLevel::Success, std::string("💧 Part of Project SEA-Stack • Building the Next Generation of Marine Simulation Software."), LogColor::Gray);
    ShowEmptyLine();
}

void CLILogger::DisplayWarnings() {
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
    // Stream buffer wrapper that mirrors characters to the original stream while
    // routing complete lines into our logger. It avoids recursion by consulting
    // `g_in_logger_write` and preserves carriage-return based progress updates.
    struct LoggerStreambuf : public std::streambuf {
        enum class StreamKind { StdOut, StdErr };

        LoggerStreambuf(std::streambuf* original, StreamKind kind)
            : original_(original), kind_(kind) {}

      protected:
        int_type overflow(int_type ch) override {
            if (traits_type::eq_int_type(ch, traits_type::eof())) {
                return traits_type::not_eof(ch);
            }
            char c = traits_type::to_char_type(ch);
            // If logger is actively writing, bypass interception entirely
            if (g_in_logger_write) {
                if (original_) {
                    if (traits_type::eq_int_type(original_->sputc(c), traits_type::eof())) {
                        return traits_type::eof();
                    }
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
            return original_ ? original_->pubsync() : 0;
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

            // If buffer contains a carriage return, forward as-is without appending
            // a newline (commonly used for in-place progress updates coming from
            // external libraries). We bypass the logger to preserve terminal state.
            if (buffer_.find('\r') != std::string::npos) {
                if (original_) {
                    original_->sputn(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
                }
                buffer_.clear();
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
    // Ensure restoration happens even if user code forgets to call Shutdown.
    std::atexit([](){ Shutdown(); });
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

bool IsInitialized() noexcept {
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

void ShowProgress(size_t current, size_t total, const std::string& message) {
    auto logger = GetCLILogger();
    if (logger) {
        logger->ShowProgress(current, total, message);
    }
}

void StopProgress() {
    auto logger = GetCLILogger();
    if (logger) {
        logger->StopProgress();
    }
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

bool IsDebugEnabled() noexcept {
    std::lock_guard<std::mutex> lock(g_logging_mutex);
    if (!g_initialized || !g_backend) return false;
    const auto& cfg = g_backend->GetConfig();
    // Enable if explicitly requested or if either sink is set to Debug threshold
    return cfg.enable_debug_logging ||
           cfg.console_level == LogLevel::Debug ||
           cfg.file_level == LogLevel::Debug;
}

} // namespace debug

//-----------------------------------------------------------------------------
// Shared helpers implementation
//-----------------------------------------------------------------------------

std::string LogLevelToString(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Success: return "SUCCESS";
    case LogLevel::Warning: return "WARNING";
    case LogLevel::Error: return "ERROR";
    }
    return "INFO";
}

std::string GetColorCode(LogColor color) noexcept {
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