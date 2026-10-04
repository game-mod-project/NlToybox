. (Join-Path $PSScriptRoot 'common.ps1')

$gameDir = Get-NlGameDir
$info = Get-NlExeInfo

# <라이브러리>\steamapps\common\<게임> 에서 두 단계 위가 steamapps 다.
$manifest = Join-Path (Split-Path -Parent (Split-Path -Parent $gameDir)) "appmanifest_$($script:NlAppId).acf"
$buildId = '(appmanifest 없음)'
if (Test-Path -LiteralPath $manifest) {
    $m = [regex]::Match((Get-Content -LiteralPath $manifest -Raw), '"buildid"\s+"(\d+)"')
    if ($m.Success) { $buildId = $m.Groups[1].Value }
}

$modsDir = Join-Path $gameDir 'mods'
$mods = @()
if (Test-Path -LiteralPath $modsDir) {
    $mods = @(Get-ChildItem -LiteralPath $modsDir -Recurse -File |
        ForEach-Object { "$($_.FullName.Substring($gameDir.Length + 1))  ($($_.Length) B)" })
}

"게임 폴더  : $gameDir"
"buildid    : $buildId"
"exe 버전   : $($info.Version)"
"exe 크기   : $($info.Size)"
"exe SHA256 : $($info.Sha256)"
"패치 여부  : $(if ($info.Patched) { '패치됨 (.aurie 섹션 있음)' } else { '바닐라 (.aurie 섹션 없음)' })"
"실행 여부  : $(if (Test-NlGameRunning) { '실행 중' } else { '꺼져 있음' })"
if ($mods.Count) { 'mods\      :'; $mods | ForEach-Object { "  $_" } } else { 'mods\      : (없음)' }

$backups = @(Get-NlBackups $info.Version)
if ($backups.Count -eq 0) { '백업       : (이 버전의 백업 없음)' }
foreach ($b in $backups) {
    $bh = (Get-FileHash -LiteralPath $b.FullName -Algorithm SHA256).Hash
    $rel = if ($info.Patched) { 'exe 는 패치 상태라 비교하지 않음' }
           elseif ($bh -eq $info.Sha256) { 'exe 와 일치' }
           else { 'exe 와 다름' }
    "백업       : $($b.Name)  SHA256 $bh  ($rel)"
}
