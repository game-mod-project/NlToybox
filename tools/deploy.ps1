$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$repo = Get-NlRepoRoot
$src = Join-Path $repo 'build\NlToyBox.dll'
if (-not (Test-Path -LiteralPath $src)) { throw "빌드 산출물이 없습니다: $src" }

# 빌드가 실패해도 이전 산출물이 남는다. 소스보다 오래된 산출물은 거부한다. 게임 재시작 한 번이 비싸다.
$newestSrc = Get-ChildItem -LiteralPath (Join-Path $repo 'src') -Recurse -File |
             Sort-Object LastWriteTime -Descending | Select-Object -First 1
$dll = Get-Item -LiteralPath $src
if ($newestSrc -and $dll.LastWriteTime -lt $newestSrc.LastWriteTime) {
    throw ("산출물이 소스보다 오래되었습니다. 빌드가 실패했을 수 있습니다.`n" +
           "  DLL : $($dll.LastWriteTime)`n" +
           "  소스: $($newestSrc.LastWriteTime)  ($($newestSrc.Name))")
}

Assert-NlGameNotRunning
$info = Get-NlExeInfo
if (-not $info.Patched) { throw 'exe 가 패치되지 않았습니다. 먼저 tools\setup-aurie.ps1 을 실행하세요.' }

$dstDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
if (-not (Test-Path -LiteralPath $dstDir)) { throw "mods\Aurie 폴더가 없습니다. tools\setup-aurie.ps1 을 다시 실행하세요: $dstDir" }
$dst = Join-Path $dstDir 'NlToyBox.dll'
Copy-Item -LiteralPath $src -Destination $dst -Force
Write-Host "deployed -> $dst  ($($dll.LastWriteTime))"
