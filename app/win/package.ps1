# Package the Windows client into a self-contained, zippable folder.
#
#   powershell -ExecutionPolicy Bypass -File app/win/package.ps1
#
# Assumes the Release app has been built (cmake --build build --config Release --target compositor_win)
# and that Qt6 is available (aqtinstall or vcpkg). Deploys Qt and the MSVC runtime beside the exe and
# zips the result under dist.

param(
    [string]$BuildDir = (Resolve-Path "$PSScriptRoot\..\..\build").Path,
    [string]$QtDir = "C:\Qt\6.8.0\msvc2022_64",
    [string]$Config = "Release",
    [string]$Version = "0.1.0",
    [string]$OutDir = (Resolve-Path "$PSScriptRoot\..\.." ).Path + "\dist"
)

$ErrorActionPreference = "Stop"

$exe = Join-Path $BuildDir "app\win\$Config\compositor_win.exe"
if (-not (Test-Path $exe)) {
    throw "Build the Release app first; $exe was not found."
}

$windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "windeployqt not found at $windeployqt; pass -QtDir."
}

$stage = Join-Path $OutDir "Compositor-$Version-win64"
Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item $exe $stage

# The vcpkg dependencies (libpng16, jpeg62, zlib) that CMake copied next to the executable.
Get-ChildItem (Join-Path $BuildDir "app\win\$Config") -Filter *.dll | ForEach-Object {
    Copy-Item $_.FullName $stage
}

& $windeployqt --release --compiler-runtime --no-translations "$stage\compositor_win.exe"

$zip = Join-Path $OutDir "Compositor-$Version-win64.zip"
Remove-Item -Force $zip -ErrorAction SilentlyContinue
Compress-Archive -Path "$stage\*" -DestinationPath $zip
Write-Host "Packaged: $zip"
