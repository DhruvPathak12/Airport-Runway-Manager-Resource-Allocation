# =============================================================================
# deploy_runtime.ps1 - SmartATC Qt Runtime Deployment Script
# =============================================================================
# PURPOSE:
#   Copies required Qt 6 runtime DLLs, MinGW runtime DLLs, and Qt plugin
#   directories into the CMake build output directory so that SmartATC.exe
#   can be launched directly from that folder without modifying PATH.
#
# USAGE:
#   From the project root in a PowerShell terminal:
#       .\scripts\deploy_runtime.ps1
#
#   Optionally specify a different build directory:
#       .\scripts\deploy_runtime.ps1 -BuildDir "D:\path\to\SmartATC\build_release"
#
# REQUIREMENTS:
#   - Qt 6.11.2 MinGW 64-bit installed at C:\Qt\6.11.2\mingw_64
#   - MinGW 13.1.0 installed at C:\Qt\Tools\mingw1310_64
#   - SmartATC.exe must already be built before running this script
#
# SAFE TO RUN MULTIPLE TIMES (idempotent - no source or user files are touched)
# =============================================================================
param(
    [string]$BuildDir  = "D:\Projects\Airport Runway Manager &Resource Allocation\SmartATC\build",
    [string]$QtBin     = "C:\Qt\6.11.2\mingw_64\bin",
    [string]$QtPlugins = "C:\Qt\6.11.2\mingw_64\plugins",
    [string]$MinGWBin  = "C:\Qt\Tools\mingw1310_64\bin"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# -- Validate -----------------------------------------------------------------
if (-not (Test-Path (Join-Path $BuildDir "SmartATC.exe"))) {
    Write-Error "SmartATC.exe not found in '$BuildDir'. Build the project first."
    exit 1
}
if (-not (Test-Path $QtBin)) {
    Write-Error "Qt bin directory not found: $QtBin"
    exit 1
}

Write-Host "SmartATC Qt Runtime Deployment" -ForegroundColor Cyan
Write-Host "  Build dir : $BuildDir"
Write-Host "  Qt bin    : $QtBin"
Write-Host "  MinGW bin : $MinGWBin"
Write-Host ""

$copied = 0

function Deploy-File([string]$Src, [string]$DstDir) {
    if (-not (Test-Path $Src)) {
        Write-Warning "Source not found (skipping): $Src"
        return
    }
    $dst = Join-Path $DstDir (Split-Path $Src -Leaf)
    Copy-Item $Src $dst -Force
    Write-Host "  [OK] $(Split-Path $Src -Leaf)" -ForegroundColor Green
    $script:copied++
}

# -- 1. Core Qt DLLs ----------------------------------------------------------
Write-Host "Deploying Qt runtime DLLs..." -ForegroundColor Yellow
$qtDlls = @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Sql.dll", "Qt6Test.dll")
foreach ($dll in $qtDlls) { Deploy-File (Join-Path $QtBin $dll) $BuildDir }

# -- 2. MinGW runtime DLLs ----------------------------------------------------
Write-Host "Deploying MinGW runtime DLLs..." -ForegroundColor Yellow
$mingwDlls = @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")
foreach ($dll in $mingwDlls) { Deploy-File (Join-Path $MinGWBin $dll) $BuildDir }

# -- 3. Qt platform plugin ----------------------------------------------------
Write-Host "Deploying platforms plugin..." -ForegroundColor Yellow
$platformsDir = Join-Path $BuildDir "platforms"
New-Item -ItemType Directory -Force -Path $platformsDir | Out-Null
Deploy-File (Join-Path $QtPlugins "platforms\qwindows.dll") $platformsDir

# -- 4. Qt SQL driver plugins -------------------------------------------------
Write-Host "Deploying sqldrivers plugins..." -ForegroundColor Yellow
$sqlDir = Join-Path $BuildDir "sqldrivers"
New-Item -ItemType Directory -Force -Path $sqlDir | Out-Null
Deploy-File (Join-Path $QtPlugins "sqldrivers\qsqlmysql.dll") $sqlDir
Deploy-File (Join-Path $QtPlugins "sqldrivers\qsqlite.dll")   $sqlDir

# -- 5. MySQL client library (required by qsqlmysql.dll) -----------------------
Write-Host "Deploying MySQL client library..." -ForegroundColor Yellow
$mysqlDllCandidates = @(
    (Join-Path $QtBin "libmysql.dll"),
    "C:\Program Files\MySQL\MySQL Server 8.0\lib\libmysql.dll",
    "C:\Program Files\MySQL\MySQL Router 8.0\lib\libmysql.dll"
)
$mysqlDeployed = $false
foreach ($candidate in $mysqlDllCandidates) {
    if (Test-Path $candidate) {
        Deploy-File $candidate $BuildDir
        $mysqlDeployed = $true
        break
    }
}
if (-not $mysqlDeployed -and -not (Test-Path (Join-Path $BuildDir "libmysql.dll"))) {
    Write-Warning "libmysql.dll not found in Qt bin or MySQL installation paths. QMYSQL plugin may fail to load."
}

# -- Summary ------------------------------------------------------------------
Write-Host ""
Write-Host "Deployment complete: $copied file(s) copied." -ForegroundColor Cyan
Write-Host "Launch SmartATC with:"
Write-Host "  & '$BuildDir\SmartATC.exe'"
