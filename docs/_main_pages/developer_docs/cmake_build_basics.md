# CMake Build Configuration Basics for HydroChrono

This guide explains how CMake handles build configurations across platforms, and how to correctly configure and build HydroChrono to avoid common issues.

## Overview

HydroChrono uses [CMake](https://cmake.org/) as its build system. CMake generates native build files (like Makefiles or Visual Studio projects) that compile your source code. One key part of this process is choosing a **build configuration**, which determines things like optimization level, debug symbol inclusion, and compatibility with dependencies.

## What Are CMake Generators?

A **generator** is the type of build system CMake creates. Common examples:

- **Unix Makefiles** (Linux/macOS)
- **Ninja** (cross-platform)
- **Visual Studio** (Windows)
- **Xcode** (macOS)

CMake picks a default generator based on your platform, but you can override it using `-G` (e.g., `-G Ninja`).

## Single-Config vs. Multi-Config Generators

CMake generators fall into two categories:

| Type              | Examples             | When You Choose Build Type                           | Typical Platforms |
| ----------------- | -------------------- | ---------------------------------------------------- | ----------------- |
| **Single-Config** | Makefiles, Ninja     | At **configure time** using `-DCMAKE_BUILD_TYPE=...` | Linux, macOS      |
| **Multi-Config**  | Visual Studio, Xcode | At **build time** using `--config ...`               | Windows, macOS    |

### 🧹 What's the difference?

- **Single-config generators** create a build setup for *one* build type (like `Release` or `Debug`) at a time. If you want to switch types, you need to reconfigure the build or use a separate build directory.

  > ✅ Pro: Simple, fast\
  > ❌ Con: Not flexible — can't build both Debug and Release without reconfiguring

- **Multi-config generators** support *multiple* build types in a single configuration. You choose which type to build at build time with `--config`.

  > ✅ Pro: Flexible — build Debug and Release from the same configuration\
  > ❌ Con: Only available with certain IDEs like Visual Studio and Xcode

### Single-Config Example (Linux/macOS)

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

To switch to Debug:

```bash
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

### Multi-Config Example (Windows with Visual Studio)

```powershell
cmake ..
cmake --build . --config Release
cmake --build . --config Debug
```

> ⚠️ On Windows with Visual Studio, `-DCMAKE_BUILD_TYPE` is ignored.

## Common Build Types

| Type             | Description                           |
| ---------------- | ------------------------------------- |
| `Debug`          | No optimizations, full debug symbols  |
| `Release`        | Full optimizations, no debug symbols  |
| `RelWithDebInfo` | Optimized, but includes debug symbols |
| `MinSizeRel`     | Optimized for smallest binary size    |

Choose the **same build type** for both HydroChrono and Project Chrono to avoid linker errors or runtime issues.

## Why Build Type Consistency Matters

Mismatching build types (e.g., HydroChrono in `Release` but Chrono in `RelWithDebInfo`) can lead to:

- Linker errors (e.g., unresolved symbols)
- ODR (One Definition Rule) violations
- Runtime crashes or inconsistent behavior

Always match the build type of your dependencies.

## Troubleshooting

- 🔍 Not sure what build type Chrono was built with? Check `CMakeCache.txt` in its build directory.
- 🧹 Switching build types? Delete your build directory and reconfigure (`rm -rf build/`).

## Best Practices

- Always explicitly specify your build type.
- Use consistent build types across all dependencies.
- Prefer out-of-source builds (`mkdir build && cd build`) to keep files clean.
- Document your build settings to avoid confusion later.

## See Also

- [CMake: CMAKE\_BUILD\_TYPE](https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html)
- [CMake: CMAKE\_CONFIGURATION\_TYPES](https://cmake.org/cmake/help/latest/variable/CMAKE_CONFIGURATION_TYPES.html)

---

By understanding how CMake generators and build types work, you can avoid many common pitfalls when building and working with HydroChrono and its dependencies.

