param(
    [string]$QtVersion = "6.7.3",
    [string]$QtArch = "win64_mingw",
    [string]$MingwToolPackage = "tools_mingw1310",
    [string]$MingwToolId = "qt.tools.win64_mingw1310",
    [string]$InstallRoot = ".qt"
)

$ErrorActionPreference = "Stop"

function Find-Python {
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) {
        return $python.Source
    }

    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($py) {
        return $py.Source
    }

    throw "Python was not found. Install Python first or run from an environment that provides python.exe."
}

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$installPath = Join-Path $projectRoot $InstallRoot
$pythonExe = Find-Python

Write-Host "Using Python: $pythonExe"
Write-Host "Installing Python build helpers if missing..."
& $pythonExe -m pip install --user --upgrade aqtinstall cmake ninja

Write-Host "Installing Qt $QtVersion $QtArch into $installPath"
& $pythonExe -m aqt install-qt windows desktop $QtVersion $QtArch -O $installPath

Write-Host "Installing MinGW toolchain $MingwToolId into $installPath"
& $pythonExe -m aqt install-tool windows desktop $MingwToolPackage $MingwToolId -O $installPath

Write-Host "Qt installation finished."
Write-Host "Build example:"
Write-Host "  .\scripts\build.ps1"
