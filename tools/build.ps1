$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$repo = Get-NlRepoRoot
if (-not (Test-Path -LiteralPath (Join-Path $repo 'external\YYToolkit\YYToolkit\source\YYTK\Shared\YYTK_Shared.hpp'))) {
    throw "external/YYToolkit 이 비어 있습니다. 먼저 실행: git -C `"$repo`" submodule update --init"
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ 빌드 도구를 찾지 못했습니다.' }

$vcvars   = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$cmakeDir = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$cmake    = Join-Path $cmakeDir 'CMake\bin\cmake.exe'
$ninja    = Join-Path $cmakeDir 'Ninja\ninja.exe'
$build    = Join-Path $repo 'build'

$cmd = "`"$vcvars`" >nul && `"$cmake`" -S `"$repo`" -B `"$build`" -G Ninja -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DCMAKE_BUILD_TYPE=RelWithDebInfo && `"$cmake`" --build `"$build`""
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "빌드 실패 ($LASTEXITCODE)" }

$dll = Join-Path $build 'NlToyBox.dll'
if (-not (Test-Path -LiteralPath $dll)) { throw "빌드는 끝났지만 산출물이 없습니다: $dll" }
Write-Host "build ok -> $dll"
