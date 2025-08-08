<#
    HydroChrono Build Script
    ========================
    
    This PowerShell script configures, builds, and verifies the HydroChrono project
    using CMake and MSVC on Windows.
    
    Key Features:
    - Configurable YAML runner toggle
    - Multi-configuration CMake builds
    - Dependency checks for CMake, Visual Studio, Chrono
    - Clean ASCII output formatting
    - External configuration file support
    
    Usage:
        .\build.ps1 [OPTIONS]
        
    Examples:
        .\build.ps1 -Help                    # Show help
        .\build.ps1 -BuildType Release       # Build with options
        .\build.ps1 -Verbose -Clean          # Verbose clean build
        
    Configuration:
        This script requires a build-config.json file with your local paths.
        Copy build-config.example.json to build-config.json and customize it.
#>

param(
    [Alias("y")][string]$YamlRunner = "ON",
    [Alias("t")][string]$BuildType = "Release",
    [Alias("c")][switch]$Clean,
    [Alias("h")][switch]$Help,
    [Alias("v")][switch]$Verbose,
    [Alias("log")][string]$LogFile = "",
    [Alias("config")][string]$ConfigPath = ".\build-config.json"
)

# =============================================================================
# Configuration Loading
# =============================================================================

function Load-Configuration {
    param([string]$ConfigPath)
    
    # Non-sensitive default values only
    $defaultConfig = @{
        ScriptVersion = "2.1"
        DefaultBuildType = "Release"
        DefaultYamlRunner = "ON"
        RuntimeLibrary = "MultiThreaded`$<`$<CONFIG:Debug>:Debug>DLL"
    }
    
    # Required configuration keys (must be provided by user)
    $requiredKeys = @(
        "ChronoDir",
        "Hdf5Dir", 
        "EigenDir",
        "IrrlichtDir",
        "PythonRoot",
        "VisualStudioPath",
        "CMakeModulePath"
    )
    
    try {
        if (-not (Test-Path $ConfigPath)) {
            Write-Host "ERROR: Configuration file not found: $ConfigPath" -ForegroundColor Red
            Write-Host ""
            Write-Host "To set up your configuration:" -ForegroundColor Yellow
            Write-Host "  1. Copy build-config.example.json to build-config.json" -ForegroundColor White
            Write-Host "  2. Edit build-config.json with your local paths" -ForegroundColor White
            Write-Host "  3. Run the script again" -ForegroundColor White
            Write-Host ""
            Write-Host "For help: .\build.ps1 -Help" -ForegroundColor Cyan
            exit 1
        }
        
        Write-Host "Loading configuration from: $ConfigPath" -ForegroundColor Blue
        $configContent = Get-Content -Path $ConfigPath -Raw
        $configObj = $configContent | ConvertFrom-Json
        $config = @{}
        $configObj.PSObject.Properties | ForEach-Object { $config[$_.Name] = $_.Value }
        
        # Check for missing required keys
        $missingKeys = @()
        foreach ($key in $requiredKeys) {
            if (-not $config.ContainsKey($key) -or [string]::IsNullOrWhiteSpace($config[$key])) {
                $missingKeys += $key
            }
        }
        
        if ($missingKeys.Count -gt 0) {
            Write-Host "ERROR: Missing required configuration keys: $($missingKeys -join ', ')" -ForegroundColor Red
            Write-Host ""
            Write-Host "Please update your build-config.json with the missing values:" -ForegroundColor Yellow
            foreach ($key in $missingKeys) {
                Write-Host "  - $key`: <path-to-your-$key>" -ForegroundColor White
            }
            Write-Host ""
            Write-Host "Use build-config.example.json as a starting point" -ForegroundColor Cyan
            exit 1
        }
        
        # Merge with non-sensitive defaults for any missing optional keys
        foreach ($key in $defaultConfig.Keys) {
            if (-not $config.ContainsKey($key)) {
                $config[$key] = $defaultConfig[$key]
                Write-Host "  INFO: Using default for optional key: $key" -ForegroundColor Blue
            }
        }
        
        return $config
        
    } catch {
        Write-Host "ERROR: Failed to load configuration: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host ""
        Write-Host "Please check your build-config.json file format" -ForegroundColor Yellow
        Write-Host "Use build-config.example.json as a reference" -ForegroundColor Cyan
        exit 1
    }
}

# Load configuration
$Config = Load-Configuration -ConfigPath $ConfigPath

# Extract commonly used values for backward compatibility
$CHRONO_DIR = $Config.ChronoDir
$SCRIPT_VERSION = $Config.ScriptVersion

# =============================================================================
# Output Utilities
# =============================================================================

function Write-Header {
    param([string]$Title)
    Write-Host ""
    Write-Host "=== $Title ===" -ForegroundColor Cyan
    Write-Host ("=" * ($Title.Length + 7)) -ForegroundColor Cyan
}

function Write-Section {
    param([string]$Title)
    Write-Host ""
    Write-Host "--> $Title" -ForegroundColor Blue
    Write-Host ("-" * ($Title.Length + 4)) -ForegroundColor Blue
}

function Write-Subsection {
    param([string]$Title)
    Write-Host ""
    Write-Host "  * $Title" -ForegroundColor Magenta
}

function Write-Info {
    param([string]$Message)
    Write-Host "    [INFO] $Message" -ForegroundColor Blue
}

function Write-Success {
    param([string]$Message)
    Write-Host "    [OK] $Message" -ForegroundColor Green
}

function Write-Warning {
    param([string]$Message)
    Write-Host "    [WARN] $Message" -ForegroundColor Yellow
}

function Write-Error {
    param([string]$Message)
    Write-Host "    [FAIL] $Message" -ForegroundColor Red
}

function Write-Progress {
    param([string]$Message)
    Write-Host "    [BUILD] $Message" -ForegroundColor Cyan
}

# =============================================================================
# Configuration Validation
# =============================================================================

function Test-ConfigurationPaths {
    Write-Section "Validating Configuration"
    
    $requiredPaths = @{
        "Chrono" = $Config.ChronoDir
        "HDF5" = $Config.Hdf5Dir
        "Eigen" = $Config.EigenDir
        "Irrlicht" = $Config.IrrlichtDir
        "Python" = $Config.PythonRoot
    }
    
    $validPaths = 0
    $totalPaths = $requiredPaths.Count
    
    foreach ($pathName in $requiredPaths.Keys) {
        $path = $requiredPaths[$pathName]
        if (Test-Path $path) {
            Write-Host "    [OK] $pathName`: Found at $path" -ForegroundColor Green
            $validPaths++
        } else {
            Write-Host "    [WARN] $pathName`: Not found at $path" -ForegroundColor Yellow
        }
    }
    
    Write-Host ""
    Write-Host "    [INFO] Configuration validation: $validPaths/$totalPaths paths found" -ForegroundColor Blue
    
    if ($validPaths -eq 0) {
        Write-Host "    [WARN] No configuration paths found. Check build-config.json" -ForegroundColor Yellow
        Write-Host "    [INFO] Use build-config.example.json as a starting point" -ForegroundColor Cyan
    } elseif ($validPaths -lt $totalPaths) {
        Write-Host "    [WARN] Some paths missing. Build may fail if dependencies not found" -ForegroundColor Yellow
    } else {
        Write-Host "    [OK] All configuration paths validated" -ForegroundColor Green
    }
    
    return $validPaths
}

# =============================================================================
# Help and Documentation
# =============================================================================

function Show-Help {
    Write-Header "HydroChrono Build System v$SCRIPT_VERSION"
    Write-Host ""
    Write-Host "Modern, fast, reliable build system for HydroChrono" -ForegroundColor Green
    Write-Host "[Multibody-Hydrodynamics simulation framework]" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage: .\build.ps1 [OPTIONS]" -ForegroundColor White
    
    Write-Section "Build Options"
    Write-Host "  -YamlRunner ON|OFF     Enable/disable YAML-based CLI runner (default: ON)" -ForegroundColor White
    Write-Host "  -BuildType TYPE        Build type: Debug|Release|RelWithDebInfo|MinSizeRel (default: Release)" -ForegroundColor White
    Write-Host "  -Clean                 Clean build (remove build directory before building)" -ForegroundColor White
    Write-Host "  -Verbose               Enable verbose output" -ForegroundColor White
    Write-Host "  -Help                  Show this help message" -ForegroundColor White
    Write-Host "  -LogFile PATH          Write build log to specified file (alias: -log)" -ForegroundColor White
    Write-Host "  -ConfigPath PATH       Use custom configuration file (alias: -config)" -ForegroundColor White
    
    Write-Section "Quick Examples"
    Write-Host "  .\build.ps1                           # Default build (Release, YamlRunner=ON)" -ForegroundColor Gray
    Write-Host "  .\build.ps1 -YamlRunner OFF          # Build without YAML runner" -ForegroundColor Gray
    Write-Host "  .\build.ps1 -BuildType Debug         # Debug build" -ForegroundColor Gray
    Write-Host "  .\build.ps1 -Clean -BuildType Debug  # Clean debug build" -ForegroundColor Gray
    Write-Host "  .\build.ps1 -Verbose                 # Verbose build with detailed output" -ForegroundColor Gray
    Write-Host "  .\build.ps1 -ConfigPath custom.json  # Use custom config file" -ForegroundColor Gray
    
    Write-Section "Build Types Explained"
    Write-Host "  Release        # Optimized build for production (default)" -ForegroundColor Gray
    Write-Host "  Debug          # Debug build with symbols" -ForegroundColor Gray
    Write-Host "  RelWithDebInfo # Release with debug info" -ForegroundColor Gray
    Write-Host "  MinSizeRel     # Release optimized for size" -ForegroundColor Gray
    
    Write-Host ""
    Write-Host "[LINK] For more information, visit the HydroChrono documentation" -ForegroundColor Blue
    Write-Host "[OK] Build system ready" -ForegroundColor Cyan
    
    Write-Section "Configuration"
    Write-Host "  Configuration file: build-config.json" -ForegroundColor White
    Write-Host "  Copy build-config.example.json to build-config.json" -ForegroundColor White
    Write-Host "  Customize paths and settings for your local environment" -ForegroundColor White
    Write-Host "  Use -ConfigPath to specify a different config file" -ForegroundColor White
    
    Write-Section "Exit Codes"
    Write-Host "  0  - Success" -ForegroundColor White
    Write-Host "  1  - Parameter validation failed" -ForegroundColor White
    Write-Host "  2  - Dependency check failed" -ForegroundColor White
    Write-Host "  3  - CMake configuration failed" -ForegroundColor White
    Write-Host "  4  - Build failed" -ForegroundColor White
    Write-Host "  5  - Unexpected error" -ForegroundColor White
    
    exit 0
}

# =============================================================================
# Dependency Checking
# =============================================================================

function Test-Dependencies {
    Write-Section "Checking Dependencies"
    
    # Check CMake
    try {
        $cmakeVersion = (cmake --version 2>$null | Select-Object -First 1)
        if ($cmakeVersion) {
            Write-Success "CMake: $cmakeVersion"
        } else {
            Write-Error "CMake not found or not in PATH"
            return $false
        }
    } catch {
        Write-Error "CMake not found or not in PATH"
        return $false
    }
    
    # Check Visual Studio
    $vsPath = $Config.VisualStudioPath
    if (Test-Path $vsPath) {
        Write-Success "Visual Studio 2022: Found"
    } else {
        Write-Warning "Visual Studio 2022 not found in expected location"
    }
    
    # Check Chrono
    if (Test-Path $Config.ChronoDir) {
        Write-Success "Chrono: Found at $($Config.ChronoDir)"
    } else {
        Write-Error "Chrono not found at $($Config.ChronoDir)"
        return $false
    }
    
    Write-Host ""
    Write-Host "    [OK] All dependencies verified" -ForegroundColor Green
    return $true
}

# =============================================================================
# Build Phases
# =============================================================================

function Clean-BuildDirectory {
    Write-Section "Cleaning Build Directory"
    
    if (Test-Path ".\build") {
        Write-Progress "Removing existing build directory..."
        try {
            Remove-Item -Recurse -Force .\build -ErrorAction Stop
            Write-Success "Build directory cleaned successfully"
        } catch {
            Write-Error "Failed to clean build directory: $($_.Exception.Message)"
            return $false
        }
    } else {
        Write-Info "No existing build directory found"
    }
    
    return $true
}

function Initialize-BuildDirectory {
    Write-Section "Initializing Build Directory"
    
    if (-not (Test-Path ".\build")) {
        Write-Progress "Creating build directory..."
        try {
            New-Item -ItemType Directory -Path build | Out-Null
            Write-Success "Build directory created"
        } catch {
            Write-Error "Failed to create build directory: $($_.Exception.Message)"
            return $false
        }
    } else {
        Write-Info "Build directory already exists"
    }
    
    Set-Location -Path build
    return $true
}

function Configure-CMake {
    Write-Section "Configuring CMake"
    
    $env:CMAKE_MODULE_PATH = $Config.CMakeModulePath
    
    Write-Subsection "Build Configuration"
    Write-Info "Runtime library: MultiThreaded DLL (for Chrono compatibility)"
    Write-Info "Build type: $BuildType"
    Write-Info "YAML runner: $YamlRunner"
    
    # Display key paths for reproducibility
    Write-Info "Chrono DIR:   $($Config.ChronoDir)"
    Write-Info "HDF5 DIR:     $($Config.Hdf5Dir)"
    Write-Info "Eigen DIR:    $($Config.EigenDir)"
    Write-Info "Irrlicht DIR: $($Config.IrrlichtDir)"
    Write-Info "Python ROOT:  $($Config.PythonRoot)"
    
    $cmakeArgs = Get-BuildArguments -YamlRunner $YamlRunner -BuildType $BuildType
    
    Write-Subsection "Running CMake Configure"
    Write-Progress "Setting up project configuration..."
    
    if ($Verbose) {
        $cmakeResult = cmake $cmakeArgs
    } else {
        $cmakeResult = cmake $cmakeArgs 2>&1
    }
    
    if ($LASTEXITCODE -eq 0) {
        Write-Success "CMake configuration completed successfully"
        return $true
    } else {
        Write-Error "CMake configuration failed"
        if ($Verbose) {
            Write-Host ""
            Write-Host "    [DEBUG] CMake output:" -ForegroundColor Yellow
            Write-Host $cmakeResult -ForegroundColor Red
        }
        return $false
    }
}

function Build-Project {
    Write-Section "Building HydroChrono"
    
    Write-Subsection "Build Information"
    Write-Info "Build type: $BuildType"
    Write-Info "Using: cmake --build . --config $BuildType"
    
    Write-Subsection "Compilation Progress"
    $buildStart = Get-Date
    
    if ($Verbose) {
        $buildResult = cmake --build . --config $BuildType
    } else {
        $buildResult = cmake --build . --config $BuildType 2>&1
    }
    
    $buildEnd = Get-Date
    $buildDuration = $buildEnd - $buildStart
    
    if ($LASTEXITCODE -eq 0) {
        Write-Success "Build completed successfully"
        Write-Info "Build time: $($buildDuration.TotalSeconds.ToString('F1')) seconds"
        return $true
    } else {
        Write-Error "Build failed"
        if ($Verbose) {
            Write-Host ""
            Write-Host "    [DEBUG] Build output:" -ForegroundColor Yellow
            Write-Host $buildResult -ForegroundColor Red
        }
        return $false
    }
}

function Copy-ChronoDLLs {
    Write-Section "Copying Runtime Dependencies"
    
    try {
        $chronoDllSource = Split-Path -Path $Config.ChronoDir -Parent | Split-Path -Parent | Join-Path -ChildPath "build\bin\$BuildType"
        $hydrochronoBinDest = Join-Path (Get-Location) "bin\$BuildType"
        
        if (-not (Test-Path $chronoDllSource)) {
            Write-Warning "Chrono DLL source directory not found: $chronoDllSource"
            Write-Info "Skipping DLL copy"
            return $true
        }
        
        Write-Progress "Copying Chrono DLLs..."
        
        if (-not (Test-Path $hydrochronoBinDest)) {
            New-Item -ItemType Directory -Path $hydrochronoBinDest -Force | Out-Null
            Write-Info "Created destination directory: $hydrochronoBinDest"
        }
        
        $dllFiles = Get-ChronoDllList -BuildType $BuildType
        
        if ($dllFiles.Count -eq 0) {
            Write-Warning "No DLL files found in: $chronoDllSource"
            return $true
        }
        
        $copiedCount = 0
        foreach ($dll in $dllFiles) {
            $destPath = Join-Path $hydrochronoBinDest $dll.Name
            Copy-Item -Path $dll.FullName -Destination $destPath -Force -ErrorAction SilentlyContinue
            if (Test-Path $destPath) {
                $copiedCount++
                if ($Verbose) {
                    Write-Info "Copied: $($dll.Name)"
                }
            }
        }
        
        if ($copiedCount -gt 0) {
            Write-Success "Successfully copied $copiedCount DLL file(s)"
            Write-Info "Destination: $hydrochronoBinDest"
        } else {
            Write-Warning "Failed to copy any DLL files"
        }
        
        return $true
    } catch {
        Write-Warning "Error during DLL copy: $($_.Exception.Message)"
        Write-Info "Continuing without DLL copy"
        return $true
    }
}

function Show-BuildSummary {
    Write-Section "Build Summary"
    
    $binPath = Join-Path (Get-Location) "bin\$BuildType"
    
    if (Test-Path $binPath) {
        Write-Success "Binaries are located in: $binPath"
        
        # Check for key executables
        $keyExecutables = @("run_hydrochrono.exe")
        foreach ($exe in $keyExecutables) {
            $exePath = Join-Path $binPath $exe
            if (Test-Path $exePath) {
                Write-Success "$exe built successfully"
            } else {
                Write-Warning "$exe not found"
            }
        }
        
        # Show file sizes
        if ($Verbose) {
            Write-Info ""
            Write-Info "Executable sizes:"
            Get-ChildItem -Path $binPath -Filter "*.exe" | ForEach-Object {
                $sizeKB = [math]::Round($_.Length / 1KB, 1)
                Write-Info "  $($_.Name): $sizeKB KB"
            }
        }
    } else {
        Write-Error "Binary directory not found: $binPath"
    }
}

function Show-SuccessSummary {
    param([string]$BuildType)
    
    Write-Header "Build Complete"
    
    Write-Host ""
    Write-Host "[OK] HydroChrono build completed successfully" -ForegroundColor Green
    Write-Host "[BUILD] Simulation framework ready for use" -ForegroundColor Cyan
    
    # Build artifacts summary
    $binPath = Join-Path (Get-Location) "bin\$BuildType"
    Write-Host ""
    Write-Host "Build Artifacts:" -ForegroundColor Cyan
    Write-Host "  Executables: $binPath\run_hydrochrono.exe" -ForegroundColor White
    Write-Host "  Libraries:   (copied Chrono DLLs to bin\$BuildType)" -ForegroundColor White
    Write-Host "  Log files:   build/CMakeFiles/..." -ForegroundColor White
    
    Write-Host ""
    Write-Host "[OK] HydroChrono ready for simulation" -ForegroundColor Magenta

    # Suggested next steps
    Write-Host "" 
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "  Run regression tests:" -ForegroundColor White
    Write-Host "    ctest -C $BuildType -L regression" -ForegroundColor Gray
}

# =============================================================================
# Helper Functions
# =============================================================================

function Get-BuildArguments {
    param([string]$YamlRunner, [string]$BuildType)
    
    return @(
        "..",
        "-DChrono_DIR=`"$($Config.ChronoDir)`"",
        "-DHDF5_DIR=`"$($Config.Hdf5Dir)`"",
        "-DPython3_ROOT_DIR=`"$($Config.PythonRoot)`"",
        "-DEIGEN3_INCLUDE_DIR=`"$($Config.EigenDir)`"",
        "-DIrrlicht_ROOT=`"$($Config.IrrlichtDir)`"",
        "-DHYDROCHRONO_ENABLE_YAML_RUNNER=`"$YamlRunner`"",
        "-DCMAKE_BUILD_TYPE=`"$BuildType`"",
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=`"$($Config.RuntimeLibrary)`""
    )
}

function Validate-Parameters {
    param([string]$YamlRunner, [string]$BuildType)
    
    if ($YamlRunner -ne "ON" -and $YamlRunner -ne "OFF") {
        Write-Error "YamlRunner must be 'ON' or 'OFF'. Got: $YamlRunner"
        return $false
    }
    
    $VALID_BUILD_TYPES = @("Debug", "Release", "RelWithDebInfo", "MinSizeRel")
    if ($VALID_BUILD_TYPES -notcontains $BuildType) {
        Write-Error "BuildType must be one of: $($VALID_BUILD_TYPES -join ', '). Got: $BuildType"
        return $false
    }
    
    return $true
}

function Get-ChronoDllList {
    param([string]$BuildType)
    
    $chronoRoot = Split-Path -Path $Config.ChronoDir -Parent | Split-Path -Parent
    $chronoDllSource = Join-Path $chronoRoot "build\bin\$BuildType"
    
    if (-not (Test-Path $chronoDllSource)) {
        return @()
    }
    
    return Get-ChildItem -Path $chronoDllSource -Filter "*.dll" -ErrorAction SilentlyContinue
}

function Show-ErrorHelp {
    Write-Section "Troubleshooting Guide"
    Write-Host "[WRENCH] Common build issues and solutions:" -ForegroundColor Yellow
    
    Write-Host ""
    Write-Host "[WARN] Runtime library mismatch:" -ForegroundColor Yellow
    Write-Host "   - Ensure Chrono and HydroChrono use the same runtime library" -ForegroundColor White
    Write-Host "   - Try rebuilding Chrono with the same settings" -ForegroundColor White
    
    Write-Host ""
    Write-Host "[WARN] Missing dependencies:" -ForegroundColor Yellow
    Write-Host "   - Check that HDF5, Eigen3, and Irrlicht are properly installed" -ForegroundColor White
    Write-Host "   - Verify all paths in the build script are correct" -ForegroundColor White
    
    Write-Host ""
    Write-Host "[WARN] Visual Studio issues:" -ForegroundColor Yellow
    Write-Host "   - Ensure Visual Studio 2022 is installed with C++ tools" -ForegroundColor White
    Write-Host "   - Try running from a Developer Command Prompt" -ForegroundColor White
    
    Write-Host ""
    Write-Host "[INFO] Try these commands:" -ForegroundColor Blue
    Write-Host "   .\build.ps1 -Clean                    # Clean rebuild" -ForegroundColor White
    Write-Host "   .\build.ps1 -Verbose                  # Detailed output" -ForegroundColor White
    Write-Host "   .\build.ps1 -BuildType Debug         # Debug build" -ForegroundColor White
    
    Write-Host ""
    Write-Host "[LINK] For additional support, check the documentation or open an issue" -ForegroundColor Blue
}

# =============================================================================
# Main Execution
# =============================================================================

try {
    # Start logging if requested
    if ($LogFile -ne "") {
        Start-Transcript -Path $LogFile -Append
        Write-Host "[FILE] Build log: $LogFile" -ForegroundColor Blue
    }
    
    if ($Help) {
        Show-Help
    }
    
    Write-Header "HydroChrono Build System v$SCRIPT_VERSION"
    Write-Host "[BUILD] Initializing hydrodynamics simulation framework" -ForegroundColor Cyan
    
    # Validate parameters
    if (-not (Validate-Parameters -YamlRunner $YamlRunner -BuildType $BuildType)) {
        Write-Host "Use -Help for usage information" -ForegroundColor Yellow
        exit 1
    }
    
    # Validate configuration paths
    Test-ConfigurationPaths | Out-Null
    
    # Show configuration
    Write-Section "Build Configuration"
    Write-Info "YAML Runner: $YamlRunner"
    Write-Info "Build Type:  $BuildType"
    Write-Info "Clean Build: $Clean"
    Write-Info "Verbose:     $Verbose"
    
    # Check dependencies
    if (-not (Test-Dependencies)) {
        Write-Error "Dependency check failed"
        exit 2
    }
    
    # Clean if requested
    if ($Clean) {
        if (-not (Clean-BuildDirectory)) {
            Write-Error "Failed to clean build directory"
            exit 1
        }
    }
    
    # Initialize build directory
    if (-not (Initialize-BuildDirectory)) {
        Write-Error "Failed to initialize build directory"
        exit 1
    }
    
    # Configure CMake
    if (-not (Configure-CMake)) {
        Write-Error "CMake configuration failed"
        Show-ErrorHelp
        exit 3
    }
    
    # Build project
    if (-not (Build-Project)) {
        Write-Error "Build failed"
        Show-ErrorHelp
        exit 4
    }
    
    # Copy dependencies
    Copy-ChronoDLLs | Out-Null
    
    # Show summary
    Show-BuildSummary
    
    # Show success summary
    Show-SuccessSummary -BuildType $BuildType
    
} catch {
    Write-Error "Unexpected error: $($_.Exception.Message)"
    Show-ErrorHelp
    exit 5
} finally {
    # Stop logging if it was started
    if ($LogFile -ne "") {
        Stop-Transcript
    }
} 