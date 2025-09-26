param(
  [string]$BuildType = "Release",
  [switch]$Clean
)

# load config
$cfgPath = Join-Path $PSScriptRoot "build-config.json"
$cfg = Get-Content $cfgPath -Raw | ConvertFrom-Json

$root     = $PSScriptRoot
$buildDir = Join-Path $root "build"

if ($Clean -and (Test-Path $buildDir)) { Remove-Item -Recurse -Force $buildDir }
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

# Avoid PowerShell expanding $... in the generator expression: use single quotes
$msvcRt = 'MultiThreaded$<$<CONFIG:Debug>:Debug>DLL'

# Optional: honor custom module path if you use one
if ($cfg.CMakeModulePath) { $env:CMAKE_MODULE_PATH = $cfg.CMakeModulePath }

# Configure
cmake -S $root -B $buildDir -G "Visual Studio 17 2022" -A x64 `
  -DChrono_DIR="$($cfg.ChronoDir)" `
  -DHDF5_DIR="$($cfg.Hdf5Dir)" `
  -DEIGEN3_INCLUDE_DIR="$($cfg.EigenDir)" `
  -DIrrlicht_ROOT="$($cfg.IrrlichtDir)" `
  -DPython3_ROOT_DIR="$($cfg.PythonRoot)" `
  -DCMAKE_MSVC_RUNTIME_LIBRARY="$msvcRt"

# Build
cmake --build $buildDir --config $BuildType
