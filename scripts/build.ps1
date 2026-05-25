param(
    [string]$QtVersion = "6.7.3",
    [string]$BuildDir = "build",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$qtRoot = Join-Path $projectRoot ".qt\$QtVersion\mingw_64"
$mingwRoot = Join-Path $projectRoot ".qt\Tools\mingw1310_64"
$pythonScripts = Join-Path $env:APPDATA "Python\Python312\Scripts"
$cmakeExe = Join-Path $pythonScripts "cmake.exe"
$bundledPython = Join-Path $env:USERPROFILE ".cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"

if (!(Test-Path $qtRoot)) {
    throw "Qt was not found at $qtRoot. Run .\scripts\setup_qt.ps1 first."
}

if (!(Test-Path $cmakeExe)) {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (!$cmake) {
        throw "cmake.exe was not found. Run .\scripts\setup_qt.ps1 first."
    }
    $cmakeExe = $cmake.Source
}

$env:Path = "$mingwRoot\bin;$qtRoot\bin;$pythonScripts;$env:Path"

$cmakeArgs = @(
    "-S", $projectRoot,
    "-B", (Join-Path $projectRoot $BuildDir),
    "-G", "Ninja",
    "-DCMAKE_PREFIX_PATH=$qtRoot",
    "-DCMAKE_CXX_COMPILER=$mingwRoot\bin\g++.exe",
    "-DCMAKE_BUILD_TYPE=$BuildType"
)

if (Test-Path $bundledPython) {
    $cmakeArgs += "-DPython3_EXECUTABLE=$bundledPython"
}

& $cmakeExe @cmakeArgs

& $cmakeExe --build (Join-Path $projectRoot $BuildDir)
