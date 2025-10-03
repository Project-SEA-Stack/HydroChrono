---
title: Logging Internals
permalink: /developer/logging-internals/
---

## Architecture

Minimal three-part design:
- `include/hydroc/logging.h`: public API (Initialize, Shutdown, IsInitialized, `hydroc::cli` helpers)
- `src/utils/logging.cpp`: coordinator + CLI helpers + stream interception
- `src/utils/logger_backend.{h,cpp}`: thread-safe backend (file I/O, timestamps, thresholds)

### Data flow

1. Apps call `hydroc::Initialize(LoggingConfig)`.
2. Backend is constructed with thresholds:
   - `console_level`: default Info (Debug if `--debug`)
   - `file_level`: Debug (ensures detailed file logs when `--log` is used)
3. CLI helpers format human-friendly messages and call backend.
4. Stream interception (`std::cout`/`std::cerr`) reroutes stray prints through the logger and collects noisy warnings for the unified Warnings block.

## Key behaviors

- Single boxed header; all other sections use flat 60‑char dividers.
- Emoji-aware width calculation (UTF‑8) ensures perfect alignment.
- File logs preserve emojis/symbols and include ISO8601 timestamps and levels.
- `--quiet` disables console output (`enable_cli_output=false`) but does not affect file logging.
- `--debug` increases console verbosity; file verbosity is always Debug when logging is enabled.

## LoggingConfig

```cpp
struct LoggingConfig {
  std::string log_file_path;        // empty => no file
  bool enable_cli_output = true;    // console on/off
  bool enable_file_output = true;   // file on/off
  bool enable_debug_logging = false;// developer diagnostics gate
  enum class Level { Debug, Info, Success, Warning, Error };
  Level console_level = Level::Info;// runtime threshold for console
  Level file_level = Level::Debug;  // runtime threshold for file
  bool enable_colors = true;        // ANSI colors in CLI
  bool enable_timestamps = true;
  bool enable_source_location = false;
};
```

## Stream interception

- Custom `LoggerStreambuf` captures `std::cout`/`std::cerr`.
- Prevents recursion via `thread_local` guard.
- Recognizes known noisy lines (e.g., colormap warnings) and collects them via `cli::CollectWarning` for later display; not printed inline.

## Extensibility

- Add JSON log writer: implement alternative `WriteToFile`/formatter.
- File rotation: call `LoggerBackend::RotateLogFile()` based on size/time.
- Structured events: extend `LogContext` and `FormatFileMessage`.

## Testing

- Build: `./build.ps1 -Clean` then `./build.ps1 -Verbose`
- Regression: `ctest -C Release -L regression`
- Manual checks:
  - `--log`, `--debug`, `--quiet`, `--quiet --log`, `--debug --log`
  - Confirm single boxed header, 60‑char dividers, aligned colons, warnings block only.



