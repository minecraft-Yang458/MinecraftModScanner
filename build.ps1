<#
.SYNOPSIS
    Build MinecraftModScanner on Windows with Qt 6 + CMake.

.DESCRIPTION
    Automatically locates a Qt 6 kit (or use -QtDir), configures CMake,
    builds the Release/Debug executable, optionally runs CTest, deploys
    Qt runtime files with windeployqt, and optionally starts the app.
    The CMake build also runs windeployqt after linking by default, so the
    EXE in the build directory is directly runnable.

.PARAMETER QtDir
    Qt kit directory, for example D:\Qt\6.11.2\mingw_64.
    If omitted, QTDIR, qmake6, and common C:\Qt / D:\Qt installs are checked.

.PARAMETER BuildDir
    Build directory relative to this script (default: build).

.PARAMETER Configuration
    CMake configuration: Debug, Release, RelWithDebInfo, MinSizeRel.

.PARAMETER Clean
    Delete the build directory before configuring.

.PARAMETER Test
    Run CTest after building.

.PARAMETER Deploy
    Copy the EXE to .\dist and run windeployqt so the result is portable.

.PARAMETER Run
    Start the built (or deployed) EXE after a successful build.

.PARAMETER Jobs
    Parallel build jobs. Defaults to the number of logical processors.

.PARAMETER Generator
    Override the CMake generator. MinGW kits default to "MinGW Makefiles";
    MSVC kits default to "Visual Studio 17 2022".

.EXAMPLE
    .\build.ps1 -Clean -Test

.EXAMPLE
    .\build.ps1 -QtDir "D:\Qt\6.11.2\mingw_64" -Configuration Debug -Deploy -Run

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\build.ps1 -Clean -Test -Deploy
#>

[CmdletBinding()]
param(
    [string]$QtDir = "",
    [string]$BuildDir = "build",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",
    [switch]$Clean,
    [switch]$Test,
    [switch]$Deploy,
    [switch]$Run,
    [ValidateRange(1, 64)]
    [int]$Jobs = [Environment]::ProcessorCount,
    [string]$Generator = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:ProjectRoot = (Split-Path -Parent $MyInvocation.MyCommand.Path).TrimEnd('\')
$script:QtDir = ""
$script:CMake = ""
$script:CTest = ""

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Write-Info {
    param([string]$Message)
    Write-Host "    $Message"
}

function Fail {
    param([string]$Message)
    throw $Message
}

function Test-QtKit {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return $false }
    $config = Join-Path $Path "lib\cmake\Qt6\Qt6Config.cmake"
    return Test-Path -LiteralPath $config
}

function Resolve-KitPath {
    param([string]$Path)
    $item = Get-Item -LiteralPath $Path -ErrorAction Stop
    if (-not $item.PSIsContainer) { Fail "Qt path is not a directory: $Path" }
    return $item.FullName.TrimEnd('\')
}

function Find-QtDir {
    param([string]$Hint)

    if (-not [string]::IsNullOrWhiteSpace($Hint)) {
        $resolved = Resolve-KitPath $Hint
        if (-not (Test-QtKit $resolved)) {
            Fail "Qt6Config.cmake not found under '$resolved'. Pass the Qt kit directory, e.g. D:\Qt\6.11.2\mingw_64."
        }
        return $resolved
    }

    if (-not [string]::IsNullOrWhiteSpace($env:QTDIR)) {
        try {
            $resolved = Resolve-KitPath $env:QTDIR
            if (Test-QtKit $resolved) { return $resolved }
        } catch {
            Write-Warning "QTDIR is set but invalid: $env:QTDIR"
        }
    }

    $qmake = Get-Command "qmake6.exe" -ErrorAction SilentlyContinue
    if ($null -ne $qmake) {
        try {
            $prefix = (& $qmake.Source -query QT_INSTALL_PREFIX 2>$null | Select-Object -First 1)
            if (-not [string]::IsNullOrWhiteSpace($prefix)) {
                $resolved = Resolve-KitPath $prefix.Trim()
                if (Test-QtKit $resolved) { return $resolved }
            }
        } catch {
            Write-Verbose "qmake6 lookup failed: $_"
        }
    }

    $roots = @(
        (Join-Path $env:SystemDrive "Qt"),
        "C:\Qt",
        "D:\Qt"
    ) | Select-Object -Unique

    $candidates = @()
    foreach ($root in $roots) {
        if (-not (Test-Path -LiteralPath $root)) { continue }
        $versions = Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^[0-9]+(\.[0-9]+)+' } |
            Sort-Object Name -Descending
        foreach ($versionDir in $versions) {
            $version = [version]'0.0.0'
            [void][version]::TryParse($versionDir.Name, [ref]$version)
            $kits = Get-ChildItem -LiteralPath $versionDir.FullName -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match '^(mingw|msvc).*_64$' }
            foreach ($kit in $kits) {
                if (-not (Test-QtKit $kit.FullName)) { continue }
                $preference = if ($kit.Name -like "mingw*") { 0 } else { 1 }
                $candidates += [pscustomobject]@{
                    Path          = $kit.FullName.TrimEnd('\')
                    Version       = $version
                    KitPreference = $preference
                }
            }
        }
    }

    if ($candidates.Count -eq 0) {
        Fail "No Qt 6 kit found. Install Qt 6 with a MinGW or MSVC 64-bit kit, or pass -QtDir `"D:\Qt\6.11.2\mingw_64`"."
    }

    $best = $candidates |
        Sort-Object -Property @{ Expression = { $_.Version }; Descending = $true },
                              @{ Expression = { $_.KitPreference }; Descending = $false } |
        Select-Object -First 1
    return $best.Path
}

function Find-ToolRoot {
    param([string]$KitPath)
    $qtInstallRoot = Split-Path -Parent (Split-Path -Parent $KitPath)
    return (Join-Path $qtInstallRoot "Tools")
}

function Find-MinGWCompiler {
    param([string]$ToolRoot)
    $candidates = @()
    if (Test-Path -LiteralPath $ToolRoot) {
        $candidates = @(Get-ChildItem -LiteralPath $ToolRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^mingw.*_64$' } |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName "bin\g++.exe" } |
            Where-Object { Test-Path -LiteralPath $_ })
    }
    if ($candidates.Count -gt 0) {
        return (Get-Item -LiteralPath $candidates[0]).FullName
    }
    $onPath = Get-Command "g++.exe" -ErrorAction SilentlyContinue
    if ($null -ne $onPath) { return $onPath.Source }
    return ""
}

function Find-CMake {
    param([string]$ToolRoot)
    $onPath = Get-Command "cmake.exe" -ErrorAction SilentlyContinue
    if ($null -ne $onPath) { return $onPath.Source }
    if (Test-Path -LiteralPath $ToolRoot) {
        $candidate = Get-ChildItem -LiteralPath $ToolRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like "CMake*" } |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName "bin\cmake.exe" } |
            Where-Object { Test-Path -LiteralPath $_ } |
            Select-Object -First 1
        if ($candidate) { return (Get-Item -LiteralPath $candidate).FullName }
    }
    return ""
}

function Assert-BuildPathSafe {
    param([string]$Path)
    $full = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
    $root = [System.IO.Path]::GetFullPath($script:ProjectRoot).TrimEnd('\')
    if ($full -eq $root) {
        Fail "BuildDir must not be the project root."
    }
    if (-not $full.StartsWith($root + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        Fail "BuildDir must be inside the project directory: $script:ProjectRoot"
    }
    return $full
}

if ($env:OS -ne "Windows_NT") {
    Fail "build.ps1 is intended for Windows. On Linux/macOS use: cmake -S . -B build && cmake --build build"
}

$script:QtDir = Find-QtDir $QtDir
if (-not (Test-QtKit $script:QtDir)) {
    Fail "Invalid Qt kit: $script:QtDir"
}
$toolRoot = Find-ToolRoot $script:QtDir
$isMinGW = $script:QtDir -match '[\\/]mingw'

$ninjaDir = Join-Path $toolRoot "Ninja"
if (Test-Path -LiteralPath (Join-Path $ninjaDir "ninja.exe")) {
    $env:Path = "$ninjaDir;$env:Path"
}

$gpp = ""
$gppDir = ""
if ($isMinGW) {
    $gpp = Find-MinGWCompiler $toolRoot
    if ([string]::IsNullOrWhiteSpace($gpp)) {
        Fail "MinGW Qt kit selected, but g++.exe was not found. Install the matching MinGW toolchain under '$toolRoot' or pass a kit with a toolchain."
    }
    $gppDir = Split-Path -Parent $gpp
    $env:Path = "$gppDir;$env:Path"
}

$script:CMake = Find-CMake $toolRoot
if ([string]::IsNullOrWhiteSpace($script:CMake)) {
    Fail "cmake.exe was not found. Install CMake or add it to PATH."
}
$cmakeDir = Split-Path -Parent $script:CMake
$ctestCandidate = Join-Path $cmakeDir "ctest.exe"
if (Test-Path -LiteralPath $ctestCandidate) {
    $script:CTest = $ctestCandidate
} else {
    $ctestCommand = Get-Command "ctest.exe" -ErrorAction SilentlyContinue
    if ($null -ne $ctestCommand) { $script:CTest = $ctestCommand.Source }
}

$env:Path = (Join-Path $script:QtDir "bin") + ";" + $cmakeDir + ";" + $env:Path

if ([string]::IsNullOrWhiteSpace($Generator)) {
    if ($isMinGW) {
        $Generator = "MinGW Makefiles"
    } else {
        $Generator = "Visual Studio 17 2022"
    }
}

$buildPath = Assert-BuildPathSafe (Join-Path $script:ProjectRoot $BuildDir)

Write-Step "MinecraftModScanner build"
Write-Info "Project : $script:ProjectRoot"
Write-Info "Qt kit  : $script:QtDir"
if ($isMinGW) { Write-Info "Compiler: $gpp" }
Write-Info "CMake   : $script:CMake"
Write-Info "Generator: $Generator"
Write-Info "Config  : $Configuration"
Write-Info "Build   : $buildPath"
Write-Info "Jobs    : $Jobs"

if ($Clean -and (Test-Path -LiteralPath $buildPath)) {
    Write-Step "Cleaning build directory"
    Remove-Item -LiteralPath $buildPath -Recurse -Force
}

$configureArgs = @(
    "-S", $script:ProjectRoot,
    "-B", $buildPath,
    "-G", $Generator,
    "-DCMAKE_PREFIX_PATH=$script:QtDir"
)

if ($isMinGW) {
    $configureArgs += "-DCMAKE_CXX_COMPILER=$gpp"
}

if ($Generator -match 'Visual Studio') {
    $configureArgs += @("-A", "x64")
} else {
    $configureArgs += "-DCMAKE_BUILD_TYPE=$Configuration"
}

Write-Step "Configuring with CMake"
& $script:CMake @configureArgs
if ($LASTEXITCODE -ne 0) {
    Fail "CMake configure failed with exit code $LASTEXITCODE."
}

Write-Step "Building"
& $script:CMake --build $buildPath --config $Configuration --parallel $Jobs
if ($LASTEXITCODE -ne 0) {
    Fail "Build failed with exit code $LASTEXITCODE."
}

$exeCandidates = @(@(
    (Join-Path $buildPath "MinecraftModScanner.exe"),
    (Join-Path (Join-Path $buildPath $Configuration) "MinecraftModScanner.exe")
) | Where-Object { Test-Path -LiteralPath $_ })

if ($exeCandidates.Count -eq 0) {
    Fail "Build finished but MinecraftModScanner.exe was not found under $buildPath."
}

$exe = Get-Item -LiteralPath ($exeCandidates | Sort-Object { (Get-Item -LiteralPath $_).LastWriteTime } -Descending | Select-Object -First 1)
Write-Step "Build succeeded"
Write-Info "EXE: $($exe.FullName)"

if ($Test) {
    Write-Step "Running tests"
    if ([string]::IsNullOrWhiteSpace($script:CTest) -or -not (Test-Path -LiteralPath $script:CTest)) {
        Fail "ctest.exe was not found; cannot run tests."
    }
    & $script:CTest --test-dir $buildPath -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        Fail "Tests failed with exit code $LASTEXITCODE."
    }
    Write-Info "All tests passed."
}

$runTarget = $exe.FullName

if ($Deploy) {
    Write-Step "Deploying Qt runtime"
    $windeployqt = Join-Path $script:QtDir "bin\windeployqt.exe"
    if (-not (Test-Path -LiteralPath $windeployqt)) {
        Fail "windeployqt.exe not found: $windeployqt"
    }
    $distDir = Join-Path $script:ProjectRoot "dist"
    New-Item -ItemType Directory -Path $distDir -Force | Out-Null
    $distExe = Join-Path $distDir "MinecraftModScanner.exe"
    Copy-Item -LiteralPath $exe.FullName -Destination $distExe -Force
    foreach ($doc in @("README.md", "LICENSE")) {
        $docPath = Join-Path $script:ProjectRoot $doc
        if (Test-Path -LiteralPath $docPath) {
            Copy-Item -LiteralPath $docPath -Destination (Join-Path $distDir $doc) -Force
        }
    }

    $deployArgs = @("--no-translations", "--compiler-runtime")
    if ($Configuration -eq "Debug") {
        $deployArgs += "--debug"
    } else {
        $deployArgs += "--release"
    }
    & $windeployqt @deployArgs $distExe
    if ($LASTEXITCODE -ne 0) {
        Fail "windeployqt failed with exit code $LASTEXITCODE."
    }
    $runTarget = $distExe
    Write-Info "Portable output: $distDir"
}

if ($Run) {
    Write-Step "Starting application"
    Write-Info "$runTarget"
    Start-Process -FilePath $runTarget -WorkingDirectory $script:ProjectRoot | Out-Null
}

Write-Host ""
Write-Host "Done. EXE: $runTarget" -ForegroundColor Green
if ($Deploy) {
    Write-Host "The dist folder contains the deployed Qt runtime; you can run the EXE directly." -ForegroundColor DarkGray
}
