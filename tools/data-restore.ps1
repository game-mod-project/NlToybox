$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 스냅샷에 있는 파일을 게임 폴더로 되돌린다. 스냅샷에 없던 파일은 건드리지 않는다.
Assert-NlGameNotRunning
$gameDir = Get-NlGameDir
$root = Get-NlDataSnapshotDir
$count = 0

# 지금 버전의 스냅샷은 없는데 다른 버전 것이 있으면 게임이 갱신된 것이다. 아무것도 안 하고 성공이라고 하지 않는다.
if (-not (Test-Path -LiteralPath $root)) {
    $others = @(Get-ChildItem -LiteralPath (Split-Path -Parent $root) -Directory -ErrorAction SilentlyContinue)
    if ($others.Count) {
        throw ("이 게임 버전의 스냅샷이 없고 다른 버전의 스냅샷만 있습니다: $(($others | ForEach-Object { $_.Name }) -join ', ').`n" +
               '게임이 갱신된 것으로 보입니다. Steam 무결성 검사로 데이터 파일을 되돌린 뒤 tools\data-snapshot.ps1 을 다시 실행하세요.')
    }
}

if (Test-Path -LiteralPath $root) {
    foreach ($f in Get-ChildItem -LiteralPath $root -Recurse -File) {
        $rel = $f.FullName.Substring($root.Length + 1)
        $dst = Join-Path $gameDir $rel
        $want = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
        if ((Test-Path -LiteralPath $dst) -and ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -eq $want)) { continue }

        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
        Copy-Item -LiteralPath $f.FullName -Destination $dst -Force
        if ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -ne $want) { throw "복원 뒤 해시가 스냅샷과 다릅니다: $rel" }
        Write-Host "복원: $rel"
        $count++
    }
}
Write-Host "data restore ok ($count)"
