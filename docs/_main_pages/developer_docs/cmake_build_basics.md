# CMake Build Configuration Basics for HydroChrono

This quick guide shows how to pick the right CMake generator and build type so HydroChrono links correctly with Project Chrono.

## Background

In CMake, a build configuration (e.g., `Debug`, `Release`) controls optimization and debug symbols. Choose a generator (single- or multi-config) and ensure HydroChrono and Project Chrono are built with the same configuration to avoid link/runtime issues.

## Generators (how CMake builds your project)

- Common generators: **Unix Makefiles**, **Ninja**, **Visual Studio**, **Xcode**
- Multi-config variants exist: **Visual Studio**, **Xcode**, **Ninja Multi-Config**
- Linux supports multi-config via **Ninja Multi-Config** (cross-platform).
- Pick a generator with `-G` (e.g., `-G Ninja`).

## Single-Config vs Multi-Config

| Type              | Examples                                  | Choose build type                     | Notes |
| ----------------- | ------------------------------------------ | ------------------------------------- | ----- |
| **Single-Config** | Makefiles, Ninja                           | At configure time: `-DCMAKE_BUILD_TYPE=...` | One type per build directory |
| **Multi-Config**  | Visual Studio, Xcode, Ninja Multi-Config   | At build time: `--config ...`         | `-DCMAKE_BUILD_TYPE` is ignored |

> ⚠️ On multi-config generators, pass `--config` for all build/install/test steps.

## Quick examples

### Single-Config (e.g., Makefiles/Ninja)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
Switching type? Reconfigure a fresh build dir (or use a separate one).

### Multi-Config (Visual Studio/Xcode/Ninja Multi-Config)
```bash
cmake -S . -B build
cmake --build build --config Release
cmake --build build --config Debug
```

## Common build types

| Type             | Description                                 |
| ---------------- | ------------------------------------------- |
| `Debug`          | No optimizations, full debug symbols        |
| `Release`        | Full optimizations, typically no debug symbols |
| `RelWithDebInfo` | Optimized, includes debug symbols           |
| `MinSizeRel`     | Optimized for smallest binary size          |

## Keep HydroChrono and Chrono consistent

- Build and link the same configuration across both (e.g., `Release` with `Release`).
- Mixing types can cause linker errors, ODR issues, or runtime crashes.

## Troubleshooting

- Check Chrono's build type in its `CMakeCache.txt`.
- Switching types? For single-config builds, clean and reconfigure. For multi-config, just pass `--config`.

## Best practices

- Prefer out-of-source builds (`mkdir build && cd build`).
- For single-config, always set `-DCMAKE_BUILD_TYPE` explicitly.
- For multi-config, always pass `--config` when building/installing/testing.

## See also

- [CMake: CMAKE\_BUILD\_TYPE](https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html)
- [CMake: CMAKE\_CONFIGURATION\_TYPES](https://cmake.org/cmake/help/latest/variable/CMAKE_CONFIGURATION_TYPES.html)

<p align="center">
  <img src="https://nrel.github.io/HydroChrono/assets/img/wave_animation2.gif" alt="Wave Energy" width="80%" />
</p>