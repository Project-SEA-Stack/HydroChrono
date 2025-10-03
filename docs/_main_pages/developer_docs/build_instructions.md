---
layout: page
title: Building HydroChrono
parent_section: Developer Documentation
---

# Build & Setup (Windows)

A PowerShell script (`build.ps1`) is provided to build HydroChrono from the command line. The steps to configure, build, and test with this script are outlined below. Alternatively, you can use the CMake GUI with Visual Studio; those instructions appear later on this page.

## Prerequisites (download/install these)

- Visual Studio 2022 (Desktop development with C++)
- CMake ≥ 3.18
- Project Chrono (built from source; enable Parsers and Irrlicht modules)
- HDF5 1.10.8 (CMake package)
- Eigen 3.4 (headers)
- Irrlicht 1.8.4 (if GUI)
- Python 3.12 (only for tests/docs)

## Quick build with build.ps1 (recommended)

1) Copy `build-config-example.json` to `build-config.json` and set paths:
```json
{
  "ChronoDir": "C:/path/to/chrono/build/cmake",
  "Hdf5Dir": "C:/path/to/hdf5/share/cmake",
  "EigenDir": "C:/path/to/eigen-3.4.0",
  "IrrlichtDir": "C:/path/to/irrlicht-1.8.4",
  "PythonRoot": "C:/path/to/python/env"
}
```

2) From the repo root, run (first build) - use `-Verbose` for a more detailed output:
```powershell
./build.ps1 -Verbose
```

3) Create a distributable ZIP (installs to `build/install` and zips it):
```powershell
./build.ps1 -Verbose -Package
```

### Useful switches

- `-BuildType Release|Debug|RelWithDebInfo|MinSizeRel` (default: Release)
- `-YamlRunner ON|OFF` to include/exclude the YAML runner target (default: ON)
- `-Clean` remove existing `build/` before configuring
- `-Package` run `cmake --install` and `cpack` (ZIP)
- `-ConfigPath <file>` use a different JSON config

What it does:
- Passes your dependency paths to CMake
- Builds with VS/MSBuild
- For `-Package`: stages a flat install tree and creates a ZIP

Install tree layout:
```
install/
  bin/    # run_hydrochrono.exe + DLLs
  data/   # Chrono visual assets (skybox, colormaps)
  tests/  # public regression suite (run_hydrochrono)
```

## Alternative: CMake GUI + Visual Studio

1. Open CMake (GUI)
    - Source: HydroChrono repo root
    - Build: `<repo>/build`

2. Configure:
    - Set `Chrono_DIR` to `<chrono>/build/cmake`
    - Set `HDF5_DIR`, `Irrlicht_ROOT`, `Python3_ROOT_DIR`, `EIGEN3_INCLUDE_DIR`
    - Ensure your HydroChrono `CMAKE_MSVC_RUNTIME_LIBRARY` matches Chrono

3. Generate → Open in Visual Studio → Build `run_hydrochrono` (choose the matching config, e.g., Release)

4. Runtime assets
    - Copy required DLLs next to the exe, or use the `-Package` flow above to stage `install/bin`


## Run regression tests (from the source tree)

After a local build, you have two regression test suites - **Option A** uses CTest to test the C++ based HydroChrono models, and **Option B** uses Python to test the main HydroChrono app - `run_hydrochrono.exe` :

**Option A** - CTest:
```powershell
ctest -C Release -L regression
```

  - Common CTest options/examples:

```powershell
# Show failing test output
ctest -C Release -L regression --output-on-failure

# Extra verbose output
ctest -C Release -L regression -VV

# Only run specific tests
ctest -C Release -R f3of -L regression
ctest -C Release -R rm3  -L regression

# Run in parallel
ctest -C Release -j 6 -L regression
```

**Option B** — Python-based YAML tests for the main executable:
```powershell
cd .\tests\regression\run_hydrochrono
python .\run_tests.py --all --exe ..\..\..\build\bin\Release\run_hydrochrono.exe
# Run a subset:
# python .\run_tests.py --sphere-decay --exe ..\..\..\build\bin\Release\run_hydrochrono.exe
# Optional GUI: add --gui
```

Outputs:
- Results (HDF5) and plots are written per case under `tests\regression\run_hydrochrono\<case>\<test>\outputs\`.
- PASS/FAIL summary and RMSrel are printed to the console.

Note: For the packaged ZIP, use the included `tests\RUN-TESTS.ps1` or run `run_tests.py` with `--exe ..\..\bin\run_hydrochrono.exe`.

### Generate a PDF regression report

Create a consolidated PDF report of results and comparisons:
```powershell
python .\tests\regression\utilities\generate_report.py --build-dir .\build --pdf
```
This summarizes PASS/FAIL and embeds plots for the standard regression cases.

## Troubleshooting

- Chrono/HydroChrono build types must match (e.g., both Release, same version of Visual Studio, etc.)
- `yaml-cpp.dll` or other dlls missing → ensure they are next to the exe
- GUI skybox missing → ensure `data/skybox/` exists in the install ZIP (packaging now includes it)
- Python tests: install `numpy`, `h5py`, `PyYAML`, `matplotlib`, or run `tests/RUN-TESTS.ps1`