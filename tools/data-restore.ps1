$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 스냅샷에 있는 파일을 게임 폴더로 되돌린다. 스냅샷에 없던 파일은 건드리지 않는다.
Assert-NlGameNotRunning
$gameDir = Get-NlGameDir
$root = Get-NlDataSnapshotDir
$count = 0

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
