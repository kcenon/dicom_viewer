#Requires -Version 5.1
<#
.SYNOPSIS
    Build script for dicom_viewer (Windows/PowerShell)

.DESCRIPTION
    Configure and build dicom_viewer using CMake on Windows.
    PowerShell equivalent of build.sh.

.PARAMETER Debug
    Build in Debug mode (default: Release)

.PARAMETER Clean
    Remove build directory and rebuild from scratch

.PARAMETER Test
    Run tests after building

.PARAMETER SkipChecks
    Skip preflight dependency validation

.PARAMETER Jobs
    Set parallel build jobs (default: number of processors)

.EXAMPLE
    .\build.ps1                    # Release build
    .\build.ps1 -Debug -Test      # Debug build + run tests
    .\build.ps1 -Clean -Test      # Clean rebuild + run tests
#>
[CmdletBinding()]
param(
    [switch]$Debug,
    [switch]$Clean,
    [switch]$Test,
    [switch]$SkipChecks,
    [int]$Jobs = [Environment]::ProcessorCount
)

$ErrorActionPreference = 'Stop'

$ProjectDir = $PSScriptRoot
$BuildDir = Join-Path $ProjectDir 'build'
$BuildType = if ($Debug) { 'Debug' } else { 'Release' }

Write-Host "=== dicom_viewer build ==="
Write-Host "  Build type: $BuildType"
Write-Host "  Build dir:  $BuildDir"
Write-Host "  Jobs:       $Jobs"
Write-Host ""

# --- Preflight dependency validation ---

function Test-Preflight {
    $errors = 0

    Write-Host "--- Preflight checks ---"

    # 1. CMake version >= 3.20
    $cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmakeCmd) {
        $versionLine = (& cmake --version) | Select-Object -First 1
        if ($versionLine -match '(\d+)\.(\d+)') {
            $major = [int]$Matches[1]
            $minor = [int]$Matches[2]
            if ($major -lt 3 -or ($major -eq 3 -and $minor -lt 20)) {
                Write-Host "  [FAIL] CMake $major.$minor found, but 3.20+ required"
                Write-Host "         Upgrade: https://cmake.org/download/"
                $errors++
            }
            else {
                Write-Host "  [ok]   CMake $major.$minor"
            }
        }
    }
    else {
        Write-Host "  [FAIL] CMake not found"
        Write-Host "         Install: https://cmake.org/download/"
        $errors++
    }

    # 2. pacs_system build
    $pacsLibDir = Join-Path $ProjectDir '..' 'pacs_system' 'build' 'lib'
    $pacsFound = $false
    foreach ($candidate in @(
            (Join-Path $pacsLibDir 'pacs_core.lib'),
            (Join-Path $pacsLibDir 'Release' 'pacs_core.lib'),
            (Join-Path $pacsLibDir 'Debug' 'pacs_core.lib')
        )) {
        if (Test-Path $candidate) {
            $pacsFound = $true
            break
        }
    }
    if ($pacsFound) {
        Write-Host "  [ok]   pacs_system libraries"
    }
    else {
        Write-Host "  [FAIL] pacs_system not built"
        Write-Host "         Run: .\setup.ps1"
        $errors++
    }

    # 3. Visual Studio / MSVC
    $vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWherePath) {
        $vsInstallPath = & $vsWherePath -latest -property installationPath 2>$null
        if ($vsInstallPath) {
            $vsVersion = & $vsWherePath -latest -property catalog_productLineVersion 2>$null
            Write-Host "  [ok]   Visual Studio $vsVersion"
        }
        else {
            Write-Host "  [FAIL] Visual Studio not found"
            Write-Host "         Install: https://visualstudio.microsoft.com/"
            $errors++
        }
    }
    else {
        if (Get-Command cl -ErrorAction SilentlyContinue) {
            Write-Host "  [ok]   MSVC compiler (cl.exe)"
        }
        else {
            Write-Host "  [warn] Cannot verify Visual Studio / MSVC installation"
        }
    }

    # 4. Optional: vcpkg
    if ($env:VCPKG_ROOT -and (Test-Path (Join-Path $env:VCPKG_ROOT 'vcpkg.exe'))) {
        Write-Host "  [ok]   vcpkg ($env:VCPKG_ROOT)"
    }
    else {
        Write-Host "  [warn] vcpkg not found (set VCPKG_ROOT environment variable)"
    }

    Write-Host ""

    if ($errors -gt 0) {
        Write-Host "Error: Preflight checks failed ($errors error(s))." -ForegroundColor Red
        Write-Host "  Run .\setup.ps1 to install dependencies, or use -SkipChecks to bypass."
        exit 1
    }
}

if (-not $SkipChecks) {
    Test-Preflight
}

# Clean if requested
if ($Clean) {
    Write-Host "--- Cleaning build directory ---"
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
    }
}

# vcpkg toolchain file for dependency discovery
$CmakeExtraArgs = @()
if (-not $env:CMAKE_TOOLCHAIN_FILE -and -not $env:CMAKE_PREFIX_PATH) {
    if ($env:VCPKG_ROOT) {
        $toolchainFile = Join-Path $env:VCPKG_ROOT 'scripts' 'buildsystems' 'vcpkg.cmake'
        if (Test-Path $toolchainFile) {
            $CmakeExtraArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile"
        }
    }
}

# Configure
$cmakeCache = Join-Path $BuildDir 'CMakeCache.txt'
if (-not (Test-Path $cmakeCache)) {
    Write-Host "--- Configuring (CMake) ---"
    $configArgs = @(
        '-S', $ProjectDir
        '-B', $BuildDir
        "-DCMAKE_BUILD_TYPE=$BuildType"
        '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'
    ) + $CmakeExtraArgs

    & cmake @configArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "CMake configuration failed. Common causes:" -ForegroundColor Red
        Write-Host "  - Missing dependencies: run .\setup.ps1"
        Write-Host "  - Stale cache: run .\build.ps1 -Clean"
        Write-Host "  - See README.md for prerequisites"
        exit 1
    }
    Write-Host ""
}

# Build
Write-Host "--- Building ---"
& cmake --build $BuildDir --config $BuildType -j $Jobs
if ($LASTEXITCODE -ne 0) {
    exit 1
}
Write-Host ""

Write-Host "=== Build complete ==="

# Run tests
if ($Test) {
    Write-Host ""
    Write-Host "--- Running tests ---"
    & ctest --test-dir $BuildDir --output-on-failure -C $BuildType
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "=== Some tests failed ===" -ForegroundColor Red
        exit 1
    }
    Write-Host ""
    Write-Host "=== All tests passed ==="
}
