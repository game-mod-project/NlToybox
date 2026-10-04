param([Parameter(Mandatory)][string[]]$Files)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 게임 데이터 파일의 바닐라 사본을 보관한다. 파일이 지금 바닐라인지는 이 도구가 알 수 없다.
# 처음 뜰 때는 Steam 설치 또는 무결성 검사 직후여야 한다.
Assert-NlGameNotRunning
$gameDir = Get-NlGameDir
$root = Get-NlDataSnapshotDir

# 이 버전의 스냅샷을 처음 뜨는데 다른 버전 것이 있으면 게임이 갱신된 것이다. 갱신 전에 고친 파일이 남아 있을 수 있다.
if (-not (Test-Path -LiteralPath $root)) {
    $others = @(Get-ChildItem -LiteralPath (Split-Path -Parent $root) -Directory -ErrorAction SilentlyContinue)
    if ($others.Count) {
        Write-Warning ("다른 게임 버전의 스냅샷이 있습니다: $(($others | ForEach-Object { $_.Name }) -join ', '). " +
                       '지금 파일이 바닐라인지 확인하세요(Steam 무결성 검사 직후여야 합니다).')
    }
}

foreach ($rel in $Files) {
    Assert-NlSafeRelPath $rel
    $src = Join-Path $gameDir $rel
    if (-not (Test-Path -LiteralPath $src -PathType Leaf)) { throw "게임 폴더에 파일이 없습니다: $rel" }
    $dst = Join-Path $root $rel
    $srcHash = (Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash

    if (Test-Path -LiteralPath $dst) {
        if ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -ne $srcHash) {
            throw "게임 파일이 스냅샷과 다릅니다: $rel. 스냅샷은 바닐라여야 합니다. tools\data-restore.ps1 로 되돌린 뒤 다시 실행하세요."
        }
        Write-Host "이미 있음: $rel"
        continue
    }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
    Copy-Item -LiteralPath $src -Destination $dst
    if ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -ne $srcHash) {
        Remove-Item -LiteralPath $dst -Force
        throw "스냅샷 복사본의 해시가 원본과 다릅니다: $rel"
    }
    Write-Host "스냅샷: $rel"
}
