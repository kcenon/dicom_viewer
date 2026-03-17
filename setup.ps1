#Requires -Version 5.1
<#
.SYNOPSIS
    Bootstrap script for dicom_viewer development environment (Windows/PowerShell)

.DESCRIPTION
    Set up the complete dicom_viewer development environment on Windows.
    Installs dependencies via vcpkg, clones ecosystem repositories, and builds pacs_system.
    PowerShell equivalent of setup.sh.

.PARAMETER SkipDeps
    Skip system dependency installation via vcpkg

.PARAMETER SkipEcosystem
    Skip ecosystem repository cloning and building

.PARAMETER Jobs
    Set parallel build jobs (default: number of processors)

.EXAMPLE
    .\setup.ps1                        # Full setup (first time)
    .\setup.ps1 -SkipDeps              # Only ecosystem clone + build
    .\setup.ps1 -SkipEcosystem         # Only system dependencies

.NOTES
    After setup completes, build with: .\build.ps1 -Test
#>
[CmdletBinding()]
param(
    [switch]$SkipDeps,
    [switch]$SkipEcosystem,
    [int]$Jobs = [Environment]::ProcessorCount
)

$ErrorActionPreference = 'Stop'

$ProjectDir = $PSScriptRoot
$ParentDir = Split-Path $ProjectDir -Parent

# Ecosystem repositories (clone order matters for build dependencies)
$EcosystemOrg = 'kcenon'
$EcosystemRepos = @(
    'common_system'
    'container_system'
    'thread_system'
    'logger_system'
    'network_system'
    'pacs_system'
)

# vcpkg packages (equivalent to Homebrew/apt packages in setup.sh)
$VcpkgPackages = @(
    'qtbase'
    'vtk'
    'itk'
    'gdcm'
    'fftw3'
    'spdlog'
    'fmt'
    'nlohmann-json'
    'gtest'
    'expat'
)

# --- Platform detection ---

$Arch = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { 'arm64' }
        elseif ([Environment]::Is64BitOperatingSystem) { 'x64' }
        else { 'x86' }
$Triplet = "$Arch-windows"

Write-Host "=== dicom_viewer setup ==="
Write-Host "  Platform: Windows ($Arch)"
Write-Host "  Triplet:  $Triplet"
Write-Host "  Jobs:     $Jobs"
Write-Host ""

# --- Prerequisite checks ---

Write-Host "--- Checking prerequisites ---"

function Assert-Command {
    param(
        [string]$Name,
        [string]$InstallHint
    )
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        Write-Host "Error: $Name is not installed. $InstallHint" -ForegroundColor Red
        exit 1
    }
}

Assert-Command 'git' 'Install from https://git-scm.com'
Assert-Command 'cmake' 'Install CMake 3.20+: https://cmake.org/download/'

$gitVersion = (& git --version) -replace 'git version ', ''
$cmakeVersion = ((& cmake --version) | Select-Object -First 1) -replace 'cmake version ', ''
Write-Host "  git:   $gitVersion"
Write-Host "  cmake: $cmakeVersion"

# Check vcpkg
$vcpkgExe = $null
if ($env:VCPKG_ROOT) {
    $vcpkgExe = Join-Path $env:VCPKG_ROOT 'vcpkg.exe'
}

if ($vcpkgExe -and (Test-Path $vcpkgExe)) {
    Write-Host "  vcpkg: $env:VCPKG_ROOT"
}
else {
    Write-Host "  vcpkg: not found" -ForegroundColor Yellow
    if (-not $SkipDeps) {
        Write-Host ""
        Write-Host "Error: vcpkg is required for dependency installation." -ForegroundColor Red
        Write-Host "  Install vcpkg: https://vcpkg.io/en/getting-started"
        Write-Host "  Then set VCPKG_ROOT environment variable."
        Write-Host "  Or use -SkipDeps to skip dependency installation."
        exit 1
    }
}

# Check Visual Studio
$vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWherePath) {
    $vsInstallPath = & $vsWherePath -latest -property installationPath 2>$null
    if ($vsInstallPath) {
        $vsVersion = & $vsWherePath -latest -property catalog_productLineVersion 2>$null
        Write-Host "  VS:    $vsVersion"
    }
    else {
        Write-Host "  [warn] Visual Studio not found" -ForegroundColor Yellow
        Write-Host "         Install: https://visualstudio.microsoft.com/"
    }
}
else {
    Write-Host "  [warn] Cannot verify Visual Studio installation" -ForegroundColor Yellow
}

Write-Host ""

# --- Step 1: System dependencies ---

if (-not $SkipDeps) {
    Write-Host "--- Installing system dependencies via vcpkg ---"
    Write-Host "  Note: First-time build of VTK and ITK may take a very long time."
    Write-Host ""

    foreach ($pkg in $VcpkgPackages) {
        $pkgSpec = "${pkg}:${Triplet}"
        Write-Host "  Installing $pkgSpec..."
        & $vcpkgExe install $pkgSpec
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  [FAIL] Failed to install $pkgSpec" -ForegroundColor Red
        }
        else {
            Write-Host "  [ok]   $pkgSpec"
        }
    }

    Write-Host ""
    Write-Host "  System dependencies ready."
    Write-Host ""
}
else {
    Write-Host "--- Skipping system dependencies (-SkipDeps) ---"
    Write-Host ""
}

# --- Step 2: Ecosystem repositories ---

function Install-Ecosystem {
    Write-Host "--- Cloning ecosystem repositories ---"

    foreach ($repo in $EcosystemRepos) {
        $repoPath = Join-Path $ParentDir $repo
        if (Test-Path $repoPath) {
            Write-Host "  [skip] $repo (already exists at $repoPath)"
        }
        else {
            Write-Host "  Cloning $repo..."
            & git clone --depth 1 "https://github.com/$EcosystemOrg/$repo.git" $repoPath
            if ($LASTEXITCODE -ne 0) {
                Write-Host "Error: Failed to clone $repo" -ForegroundColor Red
                exit 1
            }
        }
    }

    Write-Host ""
    Write-Host "  Ecosystem repositories ready."
    Write-Host ""
}

function Build-PacsSystem {
    Write-Host "--- Building pacs_system ---"

    $pacsDir = Join-Path $ParentDir 'pacs_system'
    $pacsBuild = Join-Path $pacsDir 'build'

    if (-not (Test-Path $pacsDir)) {
        Write-Host "Error: pacs_system not found at $pacsDir" -ForegroundColor Red
        Write-Host "Hint: Run without -SkipEcosystem to clone it first."
        exit 1
    }

    # Skip rebuild if libraries already exist (check both single-config and multi-config paths)
    foreach ($candidate in @(
            (Join-Path $pacsBuild 'lib' 'pacs_core.lib'),
            (Join-Path $pacsBuild 'lib' 'Release' 'pacs_core.lib')
        )) {
        if (Test-Path $candidate) {
            Write-Host "  [skip] pacs_system already built ($candidate exists)"
            Write-Host ""
            return
        }
    }

    # vcpkg toolchain integration
    $extraArgs = @()
    if ($env:VCPKG_ROOT) {
        $toolchainFile = Join-Path $env:VCPKG_ROOT 'scripts' 'buildsystems' 'vcpkg.cmake'
        if (Test-Path $toolchainFile) {
            $extraArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile"
        }
    }

    Write-Host "  Configuring..."
    $configArgs = @(
        '-S', $pacsDir
        '-B', $pacsBuild
        '-DCMAKE_BUILD_TYPE=Release'
        '-DPACS_BUILD_TESTS=OFF'
        '-DPACS_BUILD_EXAMPLES=OFF'
        '-DPACS_BUILD_BENCHMARKS=OFF'
        '-DPACS_BUILD_SAMPLES=OFF'
        '-DPACS_WARNINGS_AS_ERRORS=OFF'
    ) + $extraArgs

    & cmake @configArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: pacs_system CMake configuration failed." -ForegroundColor Red
        exit 1
    }

    Write-Host "  Building (jobs: $Jobs)..."
    & cmake --build $pacsBuild --config Release -j $Jobs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: pacs_system build failed." -ForegroundColor Red
        exit 1
    }

    # Verify build output
    $verified = $false
    foreach ($candidate in @(
            (Join-Path $pacsBuild 'lib' 'pacs_core.lib'),
            (Join-Path $pacsBuild 'lib' 'Release' 'pacs_core.lib')
        )) {
        if (Test-Path $candidate) {
            $verified = $true
            break
        }
    }

    if (-not $verified) {
        Write-Host ""
        Write-Host "Error: pacs_system build completed but pacs_core.lib not found." -ForegroundColor Red
        Write-Host "Hint: Check the build output above for errors."
        exit 1
    }

    Write-Host ""
    Write-Host "  pacs_system build complete."
    Write-Host ""
}

if (-not $SkipEcosystem) {
    Install-Ecosystem
    Build-PacsSystem
}
else {
    Write-Host "--- Skipping ecosystem setup (-SkipEcosystem) ---"
    Write-Host ""
}

# --- Step 3: Validation ---

Write-Host "--- Validating setup ---"

$script:validationPassed = $true

function Test-ValidateDir {
    param([string]$Path, [string]$Label)
    if (Test-Path $Path -PathType Container) {
        Write-Host "  [ok]   $Label"
    }
    else {
        Write-Host "  [FAIL] $Label (not found: $Path)" -ForegroundColor Red
        $script:validationPassed = $false
    }
}

# Validate ecosystem repos
if (-not $SkipEcosystem) {
    foreach ($repo in $EcosystemRepos) {
        Test-ValidateDir (Join-Path $ParentDir $repo) $repo
    }

    # Check pacs_system libraries (handle both single-config and multi-config)
    $pacsLibFound = $false
    foreach ($candidate in @(
            (Join-Path $ParentDir 'pacs_system' 'build' 'lib' 'pacs_core.lib'),
            (Join-Path $ParentDir 'pacs_system' 'build' 'lib' 'Release' 'pacs_core.lib')
        )) {
        if (Test-Path $candidate) {
            $pacsLibFound = $true
            break
        }
    }
    if ($pacsLibFound) {
        Write-Host "  [ok]   pacs_system libraries"
    }
    else {
        Write-Host "  [FAIL] pacs_system libraries (not found)" -ForegroundColor Red
        $script:validationPassed = $false
    }
}

# Validate vcpkg packages
if (-not $SkipDeps -and $vcpkgExe -and (Test-Path $vcpkgExe)) {
    foreach ($pkg in $VcpkgPackages) {
        $pkgSpec = "${pkg}:${Triplet}"
        $listOutput = & $vcpkgExe list $pkgSpec 2>$null
        if ($listOutput -match [regex]::Escape($pkg)) {
            Write-Host "  [ok]   $pkgSpec"
        }
        else {
            Write-Host "  [FAIL] $pkgSpec (not installed)" -ForegroundColor Red
            $script:validationPassed = $false
        }
    }
}

Write-Host ""

if (-not $script:validationPassed) {
    Write-Host "=== Setup completed with errors ===" -ForegroundColor Red
    Write-Host "Some validation checks failed. Review the output above."
    exit 1
}

Write-Host "=== Setup complete ==="
Write-Host ""
Write-Host "Next steps:"
Write-Host "  .\build.ps1 -Test    # Build and run tests"
