param(
    [string]$QtVersion = "6.7.3",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$qtRoot = Join-Path $projectRoot ".qt\$QtVersion\mingw_64"
$mingwRoot = Join-Path $projectRoot ".qt\Tools\mingw1310_64"
$exe = Join-Path $projectRoot "$BuildDir\HXPainter.exe"

if (!(Test-Path $exe)) {
    throw "HXPainter.exe was not found. Run .\scripts\build.ps1 first."
}

$env:Path = "$mingwRoot\bin;$qtRoot\bin;$env:Path"
& $exe
