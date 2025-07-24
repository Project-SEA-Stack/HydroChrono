# HydroChrono Build Script
# Usage: .\build.ps1 [OPTIONS]
# Options:
#   -YAML_RUNNER ON|OFF    Enable/disable YAML-based CLI runner (default: ON)
#   -BUILD_TYPE Debug|Release|RelWithDebInfo|MinSizeRel  Build type (default: Release)
#   -CLEAN                 Clean build (remove build directory before building)
#   -HELP                  Show this help message
# Examples:
#   .\build.ps1                           # Default build (Release, YAML_RUNNER=ON)
#   .\build.ps1 -YAML_RUNNER OFF          # Build without YAML runner
#   .\build.ps1 -BUILD_TYPE Debug         # Debug build
#   .\build.ps1 -CLEAN -BUILD_TYPE Debug  # Clean debug build

# Parse command line arguments
param(
    [string]$YAML_RUNNER = "ON",
    [string]$BUILD_TYPE = "Release",
    [switch]$CLEAN,
    [switch]$HELP
)

# Show help if requested
if ($HELP) {
    Get-Help $MyInvocation.MyCommand.Path -Full
    exit 0
}

# Validate YAML_RUNNER parameter
if ($YAML_RUNNER -ne "ON" -and $YAML_RUNNER -ne "OFF") {
    Write-Host "Error: YAML_RUNNER must be 'ON' or 'OFF'. Got: $YAML_RUNNER" -ForegroundColor Red
    Write-Host "Use -HELP for usage information" -ForegroundColor Yellow
    exit 1
}

# Validate BUILD_TYPE parameter
$VALID_BUILD_TYPES = @("Debug", "Release", "RelWithDebInfo", "MinSizeRel")
if ($VALID_BUILD_TYPES -notcontains $BUILD_TYPE) {
    Write-Host "Error: BUILD_TYPE must be one of: $($VALID_BUILD_TYPES -join ', '). Got: $BUILD_TYPE" -ForegroundColor Red
    Write-Host "Use -HELP for usage information" -ForegroundColor Yellow
    exit 1
}

# Show build configuration
Write-Host "=== HydroChrono Build Configuration ===" -ForegroundColor Cyan
Write-Host "YAML_RUNNER: $YAML_RUNNER" -ForegroundColor Green
Write-Host "BUILD_TYPE:  $BUILD_TYPE" -ForegroundColor Green
Write-Host "CLEAN:       $CLEAN" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host ""

# Check if build directory exists and handle clean option
if (Test-Path ".\build") {
    if ($CLEAN) {
        Write-Host "Cleaning build directory..." -ForegroundColor Yellow
        Remove-Item -Recurse -Force .\build -ErrorAction Ignore
        Write-Host "Build directory cleaned." -ForegroundColor Green
    } else {
        Write-Host "Build directory exists. Use -CLEAN to remove it before building." -ForegroundColor Yellow
    }
} else {
    Write-Host "Creating build directory..." -ForegroundColor Yellow
}

# Create build directory if it doesn't exist
if (-not (Test-Path ".\build")) {
    New-Item -ItemType Directory -Path build | Out-Null
    Write-Host "Build directory created." -ForegroundColor Green
}

# Enter build directory
Set-Location -Path build

# Tell CMake where to find FindIrrlicht.cmake
$env:CMAKE_MODULE_PATH = "C:/code/SEA-Stack/chrono/build/cmake"

Write-Host "Configuring CMake..." -ForegroundColor Yellow

# Run cmake configure
$cmakeResult = cmake .. -DChrono_DIR="C:/code/SEA-Stack/chrono/build/cmake" `
         -DHDF5_DIR="C:/libs/CMake-hdf5-1.10.8/build/HDF5-1.10.8-win64/HDF5-1.10.8-win64/share/cmake" `
         -DPython3_ROOT_DIR="C:/Users/david/.conda/envs/sphinx_docs" `
         -DEIGEN3_INCLUDE_DIR="C:/libs/eigen-3.4.0" `
         -DIrrlicht_ROOT="C:/libs/irrlicht-1.8.4" `
         -DHYDROCHRONO_ENABLE_YAML_RUNNER="$YAML_RUNNER" `
         -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "CMake configuration successful!" -ForegroundColor Green
Write-Host "Building HydroChrono..." -ForegroundColor Yellow

# Run cmake build
$buildResult = cmake --build . --config $BUILD_TYPE

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build completed successfully!" -ForegroundColor Green
    Write-Host "Binaries are located in: $(Resolve-Path .\bin)" -ForegroundColor Cyan
} else {
    Write-Host "Build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}
