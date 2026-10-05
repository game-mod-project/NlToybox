$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# src/core 의 시험을 돌린다. 게임을 켜지 않는다. 먼저 tools/build.ps1 로 빌드한다.
$repo = Get-NlRepoRoot
$exe = Join-Path $repo 'build\nlcore_tests.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "시험 실행 파일이 없습니다: $exe. 먼저 tools\build.ps1 을 실행하세요." }

# 소스보다 오래된 실행 파일로 통과했다고 하지 않는다.
$newest = Get-ChildItem -LiteralPath (Join-Path $repo 'src\core'), (Join-Path $repo 'tests\native') -Recurse -File |
          Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($newest -and (Get-Item -LiteralPath $exe).LastWriteTime -lt $newest.LastWriteTime) {
    throw "시험 실행 파일이 소스보다 오래되었습니다 ($($newest.Name)). tools\build.ps1 을 다시 실행하세요."
}

try { [Console]::OutputEncoding = [Text.Encoding]::UTF8 } catch { }     # 시험 이름이 UTF-8 로 나온다
& $exe (Join-Path $repo 'tools\probes')
exit $LASTEXITCODE
