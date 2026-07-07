<#
.SYNOPSIS
    Build & install BricsCAD.Electrical (Windows).

.DESCRIPTION
    Compiles the plugin and copies the artefacts into the BricsCAD bundle
    directory so it is auto-loaded the next time BricsCAD starts.

.PARAMETER BuildDir
    Path to the CMake build directory (default: "build" under the project root).

.PARAMETER InstallDir
    Path to the BricsCAD bundle folder. The default is the roaming AppData
    location for a per-user install.

.PARAMETER Config
    Build configuration: Debug | Release (default: Release).

.PARAMETER Parallel
    Number of parallel build jobs (default: auto).

.EXAMPLE
    .\setup.ps1

.EXAMPLE
    .\setup.ps1 -Config Debug -Parallel 4

.NOTES
    Prerequisites:
      • BRX SDK V26 installed with the BRX_SDK_ROOT environment variable set
        (e.g. C:\BRX_SDK_V26).
      • wxWidgets ≥ 3.2, development files installed and wx-config / CMake
        modules discoverable.
      • Visual Studio 2022 with the v143 toolset and "Desktop development with
        C++" workload.
      • CMake ≥ 3.20 on PATH.
#>

param(
    [string]$BuildDir   = "",
    [string]$InstallDir = "",
    [ValidateSet("Debug", "Release")]
    [string]$Config     = "Release",
    [int]$Parallel      = 0
)

$ErrorActionPreference = "Stop"

# ---- Pre-flight checks ----------------------------------------------------
$cmakePath = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakePath) {
    Write-Host "[setup.ps1] ✘ CMake is not installed or not on PATH." -ForegroundColor Red
    Write-Host ""
    Write-Host "  Install CMake ≥ 3.20 from: https://cmake.org/download/"
    Write-Host "  Or via Chocolatey: choco install cmake"
    Write-Host "  Or via winget:      winget install Kitware.CMake"
    Write-Host ""
    throw "CMake not found. Aborting."
}

$cmakeVersion = & cmake --version | Select-Object -First 1
Write-Host "[setup.ps1] CMake version : $cmakeVersion"

# ---- Resolve paths --------------------------------------------------------
$ScriptPath = Split-Path -Path $PSCommandPath -Parent
$ProjectDir = Split-Path -Path $ScriptPath -Parent

if (-not $BuildDir) {
    $BuildDir = Join-Path $ProjectDir "build"
}
if (-not $InstallDir) {
    $InstallDir = Join-Path $env:LOCALAPPDATA "open-electrical"
}
if ($Parallel -eq 0) {
    $Parallel = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors
}

Write-Host "[setup.ps1] Project root : $ProjectDir"
Write-Host "[setup.ps1] Build  dir   : $BuildDir"
Write-Host "[setup.ps1] Install dir  : $InstallDir"
Write-Host "[setup.ps1] Config       : $Config"
Write-Host "[setup.ps1] Parallel     : $Parallel"

# ---- Step 1 - Configure ---------------------------------------------------
# BRX_SDK_ROOT / BRICSCAD_ROOT fall back to the CMake defaults when the
# environment variables are not set; pass them through when they are.
Write-Host "[setup.ps1] Configuring with CMake ..."
$cmakeArgs = @(
    "-S", $ProjectDir, "-B", $BuildDir,
    "-G", "Visual Studio 17 2022", "-A", "x64", "-T", "v143",
    "-DCMAKE_BUILD_TYPE=$Config"
)
if ($env:BRX_SDK_ROOT)  { $cmakeArgs += "-DBRX_SDK_ROOT=$($env:BRX_SDK_ROOT)" }
if ($env:BRICSCAD_ROOT) { $cmakeArgs += "-DBRICSCAD_ROOT=$($env:BRICSCAD_ROOT)" }
& cmake @cmakeArgs

if ($LASTEXITCODE -ne 0) {
    throw "[setup.ps1] CMake configuration failed."
}

# ---- Step 2 - Build -------------------------------------------------------
Write-Host "[setup.ps1] Building ..."
& cmake --build $BuildDir --config $Config --parallel $Parallel

if ($LASTEXITCODE -ne 0) {
    throw "[setup.ps1] Build failed."
}

# ---- Step 3 - Install (copy bundle) ---------------------------------------
Write-Host "[setup.ps1] Installing into: $InstallDir"

if (-not (Test-Path $InstallDir)) {
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
}

# Copy the .brx itself (it sits inside a Release/Debug subfolder on MSVC).
$BuiltBrx = Join-Path $BuildDir "${Config}\open-electrical.brx"
if (-not (Test-Path $BuiltBrx)) {
    $BuiltBrx = Join-Path $BuildDir "open-electrical.brx"
}
if (-not (Test-Path $BuiltBrx)) {
    throw "[setup.ps1] open-electrical.brx not found under $BuildDir"
}

Copy-Item -Path $BuiltBrx -Destination $InstallDir -Force

# Copy the resources folder.
$BuiltResources = Join-Path $BuildDir "resources"
if (Test-Path $BuiltResources) {
    Copy-Item -Path "$BuiltResources\*" -Destination $InstallDir\resources -Recurse -Force
}

Write-Host "[setup.ps1] Done - open-electrical installed."
Write-Host "[setup.ps1]   Bundle : $InstallDir"
Write-Host "[setup.ps1]   Load in BricsCAD with APPLOAD -> open-electrical.brx, then type EL."
