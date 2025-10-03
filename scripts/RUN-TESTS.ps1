Param([string]$Python = "python", [switch]$NoVenv)

# Resolve script directory and install root regardless of current working directory
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
# When installed to tests/, install root is one directory up
$installRoot = Resolve-Path (Join-Path $scriptDir "..")
$tests = Join-Path $installRoot "tests\run_hydrochrono"
$bin   = Join-Path $installRoot "bin"
$exe   = Join-Path $bin  "run_hydrochrono.exe"

if (!(Test-Path $tests)) {
    Write-Error "Tests not found at $tests"
    exit 1
}
if (!(Test-Path $exe)) {
    Write-Error "Executable not found at $exe"
    exit 2
}

# Capture current PATH and prepend bin for this session
$oldPath = $env:PATH
$env:PATH = "$bin;$env:PATH"

Push-Location $tests

$reqs = @()
if (Test-Path ".\requirements.txt") {
    $reqs = Get-Content ".\requirements.txt" | Where-Object { $_ -and -not $_.StartsWith('#') }
}

if (-not $NoVenv) {
    # Friendly summary prompt
    $relTests = (Resolve-Path $tests).Path
    Write-Host ""; Write-Host "HydroChrono Test Environment" -ForegroundColor Cyan
    Write-Host "  Tests folder : $relTests" -ForegroundColor Gray
    Write-Host "  Python       : $Python" -ForegroundColor Gray
    if ($reqs.Count -gt 0) {
        $reqList = ($reqs -join ", ")
        Write-Host "  Will install : $reqList" -ForegroundColor Yellow
        Write-Host "  Source       : PyPI via pip" -ForegroundColor Yellow
    }
    $doSetup = Read-Host "Create local venv at '.venv' and install requirements? [Y]es / [N]o (use current Python)"
    if ($doSetup -match '^(?i)y') {
        if (!(Test-Path ".\.venv")) {
            & $Python -m venv .venv
            if ($LASTEXITCODE) { Pop-Location; $env:PATH = $oldPath; exit $LASTEXITCODE }
        }
        & .\.venv\Scripts\python -m pip install --upgrade pip
        if ($reqs.Count -gt 0) {
            & .\.venv\Scripts\python -m pip install -r .\requirements.txt
        }
        $runner = ".\.venv\Scripts\python"
    } else {
        $runner = $Python
    }
} else {
    $runner = $Python
}

# Pass explicit exe path; also export for default_exe() fallback
$env:HC_RUN_EXE = $exe
& $runner .\run_tests.py --all --exe "$exe"
$code = $LASTEXITCODE

Pop-Location
$env:PATH = $oldPath
exit $code




