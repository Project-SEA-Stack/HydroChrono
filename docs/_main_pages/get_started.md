---
layout: page
title: Get Started
---

## Running HydroChrono (YAML Runner)

HydroChrono ships a YAML-driven CLI runner, `run_hydrochrono.exe`, which loads a case from a directory or a `*.setup.yaml` file and prints a clean, structured summary to the console.

### Usage

```bash
run_hydrochrono.exe [options] <input_directory>
run_hydrochrono.exe [options] <model.setup.yaml>
```

### Common options

- `-h, --help`: Show help and exit
- `-v, --version`: Show HydroChrono version and exit
- `-i, --info`: Show brief project info
- `--nogui`: Disable GUI visualization
- `--log`: Write a detailed log file to `<input_dir>/logs/`
- `--quiet`: Suppress console output (pair with `--log`)
- `--debug`: Increase verbosity (CLI shows debug details)
- `--trace`: Most verbose developer diagnostics (implies `--debug`)

### Examples

```bash
# Normal run: structured CLI output, no file
run_hydrochrono.exe demos/yaml/rm3

# Create a detailed log file (timestamps + levels) in <input_dir>/logs/
run_hydrochrono.exe demos/yaml/rm3 --log

# Verbose CLI output (includes debug-level info); no file unless combined with --log
run_hydrochrono.exe demos/yaml/rm3 --debug

# Silent CLI, but still write a detailed log file (best for batch runs)
run_hydrochrono.exe demos/yaml/rm3 --quiet --log

# Verbose CLI and detailed file log
run_hydrochrono.exe demos/yaml/rm3 --log --debug
```

### Notes

- Without `--log`, no file is created.
- With `--quiet --log`, the CLI is silent and the log file contains full details.
- File logs always include ISO8601 timestamps and log levels and preserve emojis/symbols.
- CLI headers and dividers render to exactly 60 visible characters for consistent layout.
