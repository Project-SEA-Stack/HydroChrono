---
layout: page
title: Building HydroChrono
parent_section: Developer Documentation
---

# Building HydroChrono (and demos)

## Introduction

This page provides instructions on how to build HydroChrono and its associated demos on Windows.

## Prerequisites

Before attempting to build HydroChrono, ensure you have installed and built all of the required [prerequisites](/developer_docs/prerequisites). The exact versions listed in the prerequisites are essential for proper functionality.

- CMake 3.16 or higher
- A C++ compiler (Visual Studio 2019 or higher, or GCC through MinGW/MSYS2)
- Project Chrono (built from source, tested with v9.0.0 and v9.0.1)
- HDF5 1.10.8 or higher
- Python 3.8 or higher (with numpy and matplotlib)

### Additional Python Requirements for Documentation
If you plan to build the documentation, you'll need these additional Python packages:
- matplotlib
- sphinx
- sphinxcontrib-bibtex
- breathe
- h5py

## Clean Build Process

To perform a complete clean rebuild of the project, follow these steps:

1. **Clean the Build Directory**
   If you have an existing build directory, you can clean it in one of two ways:

   a. **Remove and recreate the build directory** (recommended for a completely fresh start):
   ```powershell
   # From the project root
   Remove-Item -Recurse -Force build
   mkdir build
   cd build
   ```

   b. **Clean using CMake** (if you want to preserve the build directory):
   ```powershell
   # From the build directory
   cmake --build . --target clean
   ```

2. **Clean CMake Cache** (optional, but recommended for a fresh configuration):
   ```powershell
   # From the build directory
   Remove-Item -Recurse -Force CMakeCache.txt CMakeFiles/
   ```

3. **Verify Clean State**
   The build directory should now be empty (if using method a) or contain only CMake-generated files (if using method b).

## Building from the Command Line

This is the recommended way to build HydroChrono.

### Steps to Build

1. **Create a Build Directory**
   Open PowerShell and navigate to the root of the HydroChrono project. Create a new directory for the build:
   ```powershell
   mkdir build
   cd build
   ```

2. **Configure the Project**
   Run CMake to configure the project with the necessary paths. You'll need to specify the paths to your Chrono and HDF5 installations:
   ```powershell
   cmake .. -DChrono_DIR="<path_to_chrono_build>/cmake" -DHDF5_DIR="<path_to_hdf5_cmake>" -DPython3_ROOT_DIR="<path_to_python>"
   ```
   Note: The Chrono build directory typically contains a `cmake` folder with the Chrono CMake configuration files.

   **⚠️ Important:** The build type (e.g., Release, Debug, RelWithDebInfo) used to build HydroChrono **must match** the build type used when building Project Chrono. On Windows, this is set when running `cmake --build . --config Release`. For more context on build configurations and CMake behavior across platforms, see [CMake Build Configuration Basics](/developer_docs/cmake_build_basics).

3. **Build the Project**
   Compile the project using the following command:
   ```powershell
   cmake --build . --config Release
   ```
   The build output will be in the `Release` folder (or `Debug` if you used that configuration).

4. **Run Tests**
   After building, you can run the tests to ensure everything is working correctly:
   ```powershell
   ctest -C Release --output-on-failure
   ```

## Building with Visual Studio

If you prefer using Visual Studio, you can use the CMake GUI to generate a Visual Studio solution.

1. Open CMake GUI and set the source directory to your HydroChrono directory and the build directory to where you want to build.

2. Configure the project with the following settings:
   - Set `Chrono_DIR` to the Chrono Build location (the directory containing Chrono's CMake files)
   - Set `HDF5_DIR` to your HDF5 build location
   - Enable the following options for additional features: `HYDROCHRONO_ENABLE_DEMOS`, `HYDROCHRONO_ENABLE_IRRLICHT`, and `HYDROCHRONO_ENABLE_TESTS`
   - To build the docs: set `Python3_ROOT_DIR` to your Python environment with required packages

   **⚠️ Important:** The build type (e.g., Release, Debug, RelWithDebInfo) used to build HydroChrono **must match** the build type used when building Project Chrono. On Windows, this is set when running `cmake --build . --config Release`. For more context on build configurations and CMake behavior across platforms, see [CMake Build Configuration Basics](/developer_docs/cmake_build_basics).

3. Click "Generate" to create the Visual Studio solution.

4. Open the generated solution in Visual Studio and build the project.

## Post-Build Steps

1. Copy the `chrono_build/bin/data` folder from the Project Chrono build directory to your build directory's `data` folder to obtain optional shaders and logos.

2. Copy the following DLL files from your Chrono build directory to your build directory's `demos/Release` folder:
   - ChronoEngine.dll
   - ChronoEngine_irrlicht.dll (if using Irrlicht)
   - Irrlicht.dll (if using Irrlicht)

## Running Demos

1. To run the demos, navigate to your build directory's `demos/Release` folder.

2. Demos require a command line argument indicating the location of input files. To specify this:
   - Run demo executables from the command line with an argument pointing to the absolute location of `<path>/HydroChrono/demos`.

## Environment Setup

Before running the tests, set the `HYDROCHRONO_DATA_DIR` environment variable to point to the demos directory:
```powershell
$env:HYDROCHRONO_DATA_DIR = "C:/path/to/HydroChrono/demos"
```
Note: Use the absolute path to the demos directory.

## Troubleshooting

If you encounter any issues during the build process:

1. Make sure all dependencies are properly installed and built
2. Verify that the paths in the CMake command match your local installation directories
3. Check that the `HYDROCHRONO_DATA_DIR` environment variable is set correctly
4. Ensure you have the required Python packages installed (numpy and matplotlib)
5. Verify that all required DLL files are in the correct locations
6. If using Irrlicht, ensure Project Chrono was built with Irrlicht support

## Additional Resources

- [Project Chrono Documentation](https://api.projectchrono.org/)
- [HDF5 Documentation](https://portal.hdfgroup.org/display/HDF5/HDF5)
- [CMake Documentation](https://cmake.org/documentation/)