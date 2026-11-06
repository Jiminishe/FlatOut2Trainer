# FlatOut 2 Trainer Build Script
# Requires: Visual Studio 2022 with C++ workload, CMake 3.20+

param(
    [string]$Configuration = "Release",
    [string]$Architecture = "x86",  # FlatOut 2 is 32-bit only
    [switch]$Clean,
    [switch]$RunLauncher,
    [switch]$RunTests
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$projectDir = Join-Path $scriptDir ".."
$buildDir = Join-Path $projectDir "build"

Write-Host "=== FlatOut 2 Trainer Build ===" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration"
Write-Host "Architecture: $Architecture"

if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path $buildDir) {
        Remove-Item -Recurse -Force $buildDir
    }
}

# Create build directory
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

# Configure with CMake
Write-Host "Configuring with CMake..." -ForegroundColor Green
$cmakeArgs = @(
    "-S", $projectDir,
    "-B", $buildDir,
    "-A", "Win32",  # Force 32-bit
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_CXX_STANDARD=20"
)

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed!"
    exit 1
}

# Build
Write-Host "Building..." -ForegroundColor Green
$buildArgs = @(
    "--build", $buildDir,
    "--config", $Configuration,
    "--parallel"
)

& cmake @buildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed!"
    exit 1
}

Write-Host "Build successful!" -ForegroundColor Green

# Output locations
$binDir = Join-Path $buildDir "bin" $Configuration
$dllPath = Join-Path $binDir "FlatOut2TrainerDLL.dll"
$exePath = Join-Path $binDir "FlatOut2Trainer.exe"

Write-Host "Output files:" -ForegroundColor Cyan
Write-Host "  DLL:  $dllPath"
Write-Host "  EXE:  $exePath"

if (Test-Path $dllPath) {
    $size = (Get-Item $dllPath).Length / 1KB
    Write-Host "  DLL size: $([math]::Round($size, 1)) KB"
}
if (Test-Path $exePath) {
    $size = (Get-Item $exePath).Length / 1KB
    Write-Host "  EXE size: $([math]::Round($size, 1)) KB"
}

# Copy DLL next to EXE for easy distribution
Copy-Item -Force $dllPath (Join-Path $binDir "FlatOut2TrainerDLL.dll") -ErrorAction SilentlyContinue

if ($RunLauncher) {
    Write-Host "Launching trainer..." -ForegroundColor Green
    Set-Location $binDir
    & $exePath
}

if ($RunTests) {
    Write-Host "Running tests..." -ForegroundColor Green
    & ctest --test-dir $buildDir --output-on-failure
}

Write-Host "=== Done ===" -ForegroundColor Cyan