param(
    [string]$QtVersion = "6.7.3",
    [string]$BuildDir = "build-release",
    [string]$DistDir = "dist\HXPainter",
    [switch]$SkipBuild,
    [switch]$SkipSmokeTest
)

$ErrorActionPreference = "Stop"

function Get-FullPath([string]$Path) {
    return [System.IO.Path]::GetFullPath($Path)
}

function Assert-PathUnderProject([string]$Path, [string]$ProjectRoot) {
    $full = Get-FullPath $Path
    $root = (Get-FullPath $ProjectRoot).TrimEnd('\') + '\'
    if (!$full.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify path outside project root: $full"
    }
    return $full
}

function Get-RelativeDisplayPath([string]$BasePath, [string]$FullPath) {
    $base = (Get-FullPath $BasePath).TrimEnd('\') + '\'
    $full = Get-FullPath $FullPath
    if ($full.StartsWith($base, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $full.Substring($base.Length)
    }
    return $full
}

function Remove-QtPathsFromPath([string]$PathValue, [string]$QtRoot) {
    $qtFull = (Get-FullPath $QtRoot).TrimEnd('\') + '\'
    $kept = New-Object System.Collections.Generic.List[string]
    foreach ($part in ($PathValue -split ';')) {
        if ([string]::IsNullOrWhiteSpace($part)) {
            continue
        }
        try {
            $partFull = (Get-FullPath $part).TrimEnd('\') + '\'
            if ($partFull.StartsWith($qtFull, [System.StringComparison]::OrdinalIgnoreCase)) {
                continue
            }
        } catch {
            # Keep non-filesystem PATH entries unchanged.
        }
        $kept.Add($part)
    }
    return ($kept -join ';')
}

function Copy-ProjectDlls([string]$ProjectRoot, [string]$DistPath) {
    $candidateDirs = @("third_party\bin", "external\bin", "vendor\bin", "dll", "runtime")
    foreach ($candidate in $candidateDirs) {
        $sourceDir = Join-Path $ProjectRoot $candidate
        if (!(Test-Path $sourceDir)) {
            continue
        }

        $sourceFull = Get-FullPath $sourceDir
        Get-ChildItem -LiteralPath $sourceFull -Recurse -Filter *.dll | ForEach-Object {
            $name = $_.Name
            $lowerName = $name.ToLowerInvariant()
            if ($lowerName -match '(^|[_-])debug([_-]|\.dll$)' -or
                $lowerName -match '[_-]d\.dll$' -or
                $lowerName -match '^qt6.*d\.dll$') {
                Write-Host "Skipping likely Debug DLL in Release deployment: $($_.FullName)"
                return
            }

            $relative = Get-RelativeDisplayPath $sourceFull $_.FullName
            $target = Join-Path $DistPath $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
            Copy-Item -LiteralPath $_.FullName -Destination $target -Force
            Write-Host "Copied project DLL: $relative"
        }
    }
}

function Assert-RequiredFile([string]$DistPath, [string]$RelativePath) {
    $path = Join-Path $DistPath $RelativePath
    if (!(Test-Path $path)) {
        throw "Deployment validation failed. Missing required file: $RelativePath"
    }
    return $path
}

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$projectRoot = $projectRoot.Path
$qtRoot = Join-Path $projectRoot ".qt\$QtVersion\mingw_64"
$windeployqt = Join-Path $qtRoot "bin\windeployqt.exe"
$distPath = Assert-PathUnderProject (Join-Path $projectRoot $DistDir) $projectRoot
$buildPath = Join-Path $projectRoot $BuildDir
$buildExe = Join-Path $buildPath "HXPainter.exe"
$distExe = Join-Path $distPath "HXPainter.exe"
$logo = Join-Path $projectRoot "logo.png"

if (!(Test-Path $logo)) {
    throw "Required logo file is missing: $logo"
}
if (!(Test-Path $windeployqt)) {
    throw "windeployqt was not found: $windeployqt. Run .\scripts\setup_qt.ps1 first."
}

if (!$SkipBuild) {
    & (Join-Path $projectRoot "scripts\build.ps1") -QtVersion $QtVersion -BuildDir $BuildDir -BuildType Release
}

if (!(Test-Path $buildExe)) {
    throw "Release executable was not found after build: $buildExe"
}

if (Test-Path $distPath) {
    Remove-Item -LiteralPath $distPath -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $distPath | Out-Null

Copy-Item -LiteralPath $buildExe -Destination $distExe -Force
Copy-Item -LiteralPath $logo -Destination (Join-Path $distPath "logo.png") -Force

& $windeployqt --release --compiler-runtime --no-translations --dir $distPath $distExe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

Copy-ProjectDlls $projectRoot $distPath

Assert-RequiredFile $distPath "HXPainter.exe" | Out-Null
Assert-RequiredFile $distPath "logo.png" | Out-Null
Assert-RequiredFile $distPath "platforms\qwindows.dll" | Out-Null
Assert-RequiredFile $distPath "Qt6Core.dll" | Out-Null
Assert-RequiredFile $distPath "Qt6Gui.dll" | Out-Null
Assert-RequiredFile $distPath "Qt6Widgets.dll" | Out-Null
Assert-RequiredFile $distPath "Qt6OpenGL.dll" | Out-Null
Assert-RequiredFile $distPath "Qt6OpenGLWidgets.dll" | Out-Null
Assert-RequiredFile $distPath "Qt6Svg.dll" | Out-Null
Assert-RequiredFile $distPath "iconengines\qsvgicon.dll" | Out-Null
Assert-RequiredFile $distPath "imageformats\qjpeg.dll" | Out-Null

if (!(Test-Path (Join-Path $distPath "imageformats\qpng.dll"))) {
    Write-Host "Note: qpng.dll is not present in this Qt build; PNG support is provided by QtGui."
}

if (!$SkipSmokeTest) {
    $originalPath = $env:Path
    $originalLocation = Get-Location
    try {
        $env:Path = "$distPath;" + (Remove-QtPathsFromPath $originalPath (Join-Path $projectRoot ".qt"))
        Set-Location $env:TEMP
        & $distExe --smoke-icon-test
        if ($LASTEXITCODE -ne 0) {
            throw "Distribution smoke test failed with exit code $LASTEXITCODE"
        }
        & $distExe --mvp-smoke-test
        if ($LASTEXITCODE -ne 0) {
            throw "Distribution MVP smoke test failed with exit code $LASTEXITCODE"
        }
        & $distExe --theme-smoke-test
        if ($LASTEXITCODE -ne 0) {
            throw "Distribution theme smoke test failed with exit code $LASTEXITCODE"
        }
        & $distExe --functional-regression-smoke-test
        if ($LASTEXITCODE -ne 0) {
            throw "Distribution functional regression smoke test failed with exit code $LASTEXITCODE"
        }
    } finally {
        Set-Location $originalLocation
        $env:Path = $originalPath
    }
}

Write-Host "Deployment validation passed."
Write-Host "Distribution folder: $distPath"
Write-Host "Distribution contents:"
Get-ChildItem -LiteralPath $distPath -Recurse | Sort-Object FullName | ForEach-Object {
    $relative = Get-RelativeDisplayPath $distPath $_.FullName
    if ($_.PSIsContainer) {
        Write-Host "  $relative/"
    } else {
        Write-Host ("  {0} ({1} bytes)" -f $relative, $_.Length)
    }
}
