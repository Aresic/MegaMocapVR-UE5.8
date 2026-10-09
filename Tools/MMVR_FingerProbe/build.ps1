param([string]$CMake = "cmake")
$ErrorActionPreference = 'Stop'
if (-not (Get-Command $CMake -ErrorAction SilentlyContinue)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $candidate = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if (Test-Path -LiteralPath $candidate) { $CMake = $candidate }
    }
}
& $CMake -S $PSScriptRoot -B (Join-Path $PSScriptRoot 'build') -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
& $CMake --build (Join-Path $PSScriptRoot 'build') --config Release
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
