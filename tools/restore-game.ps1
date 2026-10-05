$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

Assert-NlGameNotRunning
$gameDir = Get-NlGameDir
$info = Get-NlExeInfo

if ($info.Patched) {
    # 지금 exe 와 같은 버전의 백업만 쓴다. 게임이 갱신됐으면 옛 백업으로 덮어쓰지 않는다.
    $backups = @(Get-NlBackups $info.Version)
    if ($backups.Count -eq 0) {
        throw "버전 $($info.Version) 의 백업이 없습니다. Steam 무결성 검사로 되돌리세요."
    }
    if ($backups.Count -gt 1) {
        throw ("버전 $($info.Version) 의 백업이 둘 이상입니다. 하나만 남기고 다시 실행하세요:`n" +
               (($backups | ForEach-Object { "  $($_.FullName)" }) -join "`n"))
    }
    $backup = $backups[0]
    # 백업 이름의 끝 12자는 원본 SHA256 의 앞부분이다. 내용이 이름과 다르면 그 백업은 원본이 아니다.
    $want = (Get-FileHash -LiteralPath $backup.FullName -Algorithm SHA256).Hash
    $tag = $backup.Name.Substring($backup.Name.LastIndexOf('.') + 1)
    if (-not $want.StartsWith($tag, [StringComparison]::OrdinalIgnoreCase)) {
        throw ("백업의 내용이 이름의 해시($tag)와 다릅니다. 이 백업으로 덮어쓰지 않습니다: $($backup.FullName)`n" +
               'Steam 무결성 검사로 되돌리세요.')
    }
    Copy-Item -LiteralPath $backup.FullName -Destination $info.Path -Force
    $got = (Get-NlExeInfo).Sha256
    if ($got -ne $want) { throw "복원 뒤 exe 해시가 백업과 다릅니다 (exe $got, 백업 $want)" }
    Write-Host "exe 복원: $($backup.Name)"
} else {
    Write-Host 'exe 는 패치되지 않은 상태입니다. 그대로 둡니다.'
}

# 이 레포가 놓은 파일만 지운다. 다른 파일이 있으면 남기고 알린다.
$ours = @(
    'aurie.log',                      # Aurie 가 게임 폴더에 쓴다 (게임을 끌 때 채워진다)
    'mods\Native\AurieCore.dll',
    'mods\Aurie\YYToolkit.dll',
    'mods\Aurie\NlToyBox.dll',
    'mods\Aurie\NlToyBox.log',
    'mods\Aurie\NlToyBox.probe.txt',
    'mods\Aurie\NlToyBox.dump.json'
)
foreach ($rel in $ours) {
    $p = Join-Path $gameDir $rel
    if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Force }
}
# 덤프는 이름이 여럿이다(menu, late0 …). 모듈이 쓰는 이름 꼴로 지운다.
$aurieDir = Join-Path $gameDir 'mods\Aurie'
if (Test-Path -LiteralPath $aurieDir) {
    Get-ChildItem -LiteralPath $aurieDir -File -Filter 'NlToyBox.dump*.json' | ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
}
foreach ($rel in 'mods\Native', 'mods\Aurie', 'mods') {
    $d = Join-Path $gameDir $rel
    if (-not (Test-Path -LiteralPath $d)) { continue }
    $left = @(Get-ChildItem -LiteralPath $d -Force)
    if ($left.Count -eq 0) {
        Remove-Item -LiteralPath $d -Force
    } else {
        Write-Host "남김 (이 레포가 놓지 않은 것이 있음): $d"
        $left | ForEach-Object { Write-Host "  $($_.Name)" }
    }
}
Write-Host 'restore ok'
