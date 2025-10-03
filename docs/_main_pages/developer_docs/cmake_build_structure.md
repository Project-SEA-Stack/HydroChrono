---
layout: page
title: CMake Build Structure
parent_section: Developer Documentation
---

# HydroChrono CMake Build Structure

This page gives a brief overview of how `CMakeLists.txt` is organized, why it’s structured this way, and what to change when adding features. It reflects the current top‑level `CMakeLists.txt` in this repo.

This is internal developer documentation for contributors working on HydroChrono’s build/packaging.

---

## Overview

The top‑level file is split into clearly marked sections:

1. Prerequisites & Policies
2. Project Definition & Defaults
3. User Options
4. Find Dependencies
5. Core Library Target
6. Auxiliary Targets (GUI, Tests, Demos)
7. Installation & Packaging

Banner comments separate sections so it’s easy to navigate and reason about changes.

---

## 1) Prerequisites & Policies

- `cmake_minimum_required(3.18.2)` (CMP0091, modern target props)
- `CMP0091 NEW` to control MSVC runtime via `CMAKE_MSVC_RUNTIME_LIBRARY`
- Guard against in‑source builds

---

## 2) Project Definition & Defaults

- Project `HydroChrono` (C++)
- Default build type: Release (Debug/MinSizeRel/RelWithDebInfo supported)
- `cmake/` added to `CMAKE_MODULE_PATH` (local modules)
- Generates `version.h` under `build/generated/`

---

## 3) User Options (feature toggles)

- `HYDROCHRONO_ENABLE_TESTS`
- `HYDROCHRONO_ENABLE_IRRLICHT`
- `HYDROCHRONO_ENABLE_DEMOS`
- `HYDROCHRONO_ENABLE_YAML_RUNNER`
- `HYDROCHRONO_ENABLE_LOGGING`, `HYDROCHRONO_ENABLE_USER_DOC`, `HYDROCHRONO_ENABLE_PROG_DOC`

Enable lean builds (e.g., CI) or developer variants.

---

## 4) Find Dependencies

### Chrono

- `find_package(Chrono CONFIG REQUIRED)`
- If Irrlicht is enabled, find `Irrlicht` first so `Chrono::Chrono_irrlicht` exists
- Extend `CMAKE_MODULE_PATH` with `Chrono_DIR` when needed

### HDF5 & Eigen

- HDF5 required: `find_package(HDF5 REQUIRED COMPONENTS CXX)`; link static to avoid DLL issues
- Eigen via config or module mode

### Platform notes

- MSVC: a few warning suppressions; Eigen aligned‑storage workaround

---

## 5) Core Library Target

Libraries:

- `HydroChrono` — core hydrodynamics (HDF5 I/O, YAML, utilities)
- `HydroChronoGUI` — GUI helpers when Irrlicht is enabled

`configure_hydro_target(<tgt>)` centralizes: C++ standard, PIC, include dirs, Chrono links.

Key links:

- `HydroChrono` → `Chrono::Chrono_core` (+ HDF5)
- `HydroChronoGUI` → Chrono (+ `Chrono::Chrono_irrlicht` when enabled)


---

## 6) Auxiliary Targets (GUI, Tests, Demos)

### YAML‑Driven CLI

- Executable: `run_hydrochrono` (built on top of the `HydroChrono` library)
- Links: `HydroChrono`, `HydroChronoGUI`, `Chrono::Chrono_parsers`
- Minimal CLI wrapper: runner sources and small utils


>Why library + CLI split?
> - Clear separation: core in `HydroChrono`; thin CLI wrapper.
> - Reuse & testing: same core for tests/demos/GUI; tests link the lib directly.
> - Packaging & speed: multiple frontends, faster CLI relinks, keep extras out of core.

### Tests & Demos

- If enabled, tests are added via `add_subdirectory(tests)` and `add_subdirectory(tests/regression)`
- A helper `configure_test_environment()` assembles PATH on Windows so Chrono/Irrlicht DLLs are found when tests run from the build tree
- Demos can be included behind `HYDROCHRONO_ENABLE_DEMOS`

Runtime concerns that affect tests:
- Matching build types between Chrono and HydroChrono (Release vs Debug)
- DLL search order on Windows

---

## 7) Installation & Packaging

Two layers:

1) Dev kit (optional)
   - `HC_INSTALL_DEV_KIT` (OFF by default)
   - Installs headers, libs, and `HydroChronoConfig.cmake`/targets for downstream `find_package(HydroChrono)`

2) Runtime (default)
   - Flat layout via `cmake --install` + CPack ZIP
   - Installs `run_hydrochrono.exe`, required DLLs, curated tests
   - Copies Chrono visual assets into `data/`; includes `tests/RUN-TESTS.ps1`

CPack (ZIP): `cpack -C Release` yields a ready‑to‑share artifact.


---

## Tips for Modifying the Build

- Add new features behind options (e.g., `HYDROCHRONO_ENABLE_<FEATURE>`) with sensible defaults
- Update `configure_hydro_target()` instead of duplicating setup
- Prefer imported targets (e.g., `Chrono::Chrono_core`) over manual flags
- Avoid hardcoded paths; expose `CACHE` variables when needed
- Keep section banners to make diffs/reviews faster

---

## Related Docs

- [Build & Setup](build_instructions)
- [CMake Build Basics](cmake_build_basics)
- [Project Chrono Documentation](https://api.projectchrono.org/)

<p align="center">
  <img src="https://nrel.github.io/HydroChrono/assets/img/wave_animation2.gif" alt="Wave Energy" width="80%" />
</p>