---
title: Logging and CLI Output
permalink: /logging/
---

## Overview

HydroChrono provides a modern, unified logging system for clean, readable CLI output and optional file logs. By default, the CLI shows a structured summary with aligned sections, and no log file is created unless you ask for one.

## Quick start

```bash
# Normal run: structured CLI output, no file
run_hydrochrono.exe <input_dir>

# Create a detailed log file (timestamps + levels) in <input_dir>/logs/
run_hydrochrono.exe <input_dir> --log

# Verbose CLI output (includes debug-level info); no file unless combined with --log
run_hydrochrono.exe <input_dir> --debug

# Silent CLI, but still write a detailed log file (best for batch runs)
run_hydrochrono.exe <input_dir> --quiet --log

# Verbose CLI and detailed file log
run_hydrochrono.exe <input_dir> --log --debug
```

## CLI flags

- **--log**: Enable file logging. The log is written to `<input_dir>/logs/hydrochrono_<timestamp>.log`.
- **--quiet**: Suppress all console output. Pair with `--log` to capture everything in the log file.
- **--debug**: Increase verbosity to include debug messages in the CLI. When combined with `--log`, debug messages are also captured in the file.
- **--trace**: Alias for the most verbose developer diagnostics (enables `--debug`).

Notes:
- Without `--log`, no file is created.
- With `--quiet --log`, the CLI is silent and the log file contains full details.
- File logs always include ISO8601 timestamps and log levels and preserve emojis/symbols.

## What you’ll see

- A single boxed header at the top (“HydroChrono Simulation”).
- Flat section headers for Hydrodynamic Data Summary, Wave Model, System Configuration, etc.
- All section header dividers are exactly 60 visible characters. Colons are aligned within sections.
- A dedicated “Warnings” block; noisy library messages are captured and summarized there.

## Tips

- Use `--quiet --log` for automated runs and CI.
- Use `--debug` while iterating on setups; add `--log` to persist details.
- Log files are relative to the input directory and designed to be copy‑paste friendly.


