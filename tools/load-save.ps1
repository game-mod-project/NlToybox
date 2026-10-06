param(
    [string]$Name,              # 세이브 파일 이름의 일부(확장자 없이). 없으면 게임이 "계속하기"로 불러올 세이브(가장 최근 것)
    [switch]$List,              # 불러오지 않고 세이브의 이름만 늘어놓는다
    [switch]$KeepSaving,        # 불러온 뒤 게임의 저장을 끄지 않는다(평소에는 실행 묶음이면 끈다)
    [int]$TimeoutSec = 240
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 켜 둔 게임(tools\session.ps1 -Action start)의 메인 메뉴에서 세이브를 불러온다. 사용자가 누르지 않아도 된다.
# 게임의 GameLoadOperator.load_save(세이브 구조체)를 원격 명령으로 부른다(research/11: 본문이 인자 하나를 구조체로 읽고,
# get_last_game_save() 가 그 구조체를 돌려준다. 메뉴에서 불러 세이브가 불러와지는 것을 봤다).
$operator = 'global.__game_load_operator'
$ask = Join-Path $PSScriptRoot 'ask.ps1'

function Ask-Lines([string[]]$Lines, [int]$Timeout = 40) {
    # 같은 프로세스에서 부른다(줄 여럿을 배열로 넘긴다). ask.ps1 의 exit 는 그 스크립트만 끝낸다. 답은 Write-Host 로 나온다.
    $out = & $ask -Lines $Lines -TimeoutSec $Timeout *>&1 | ForEach-Object { "$_" }
    return @($out)
}

# session.ps1 -Action start 는 기다리지 않고 돌아온다. 게임 프로세스가 뜰 때까지 조금 기다린다.
$up = (Get-Date).AddSeconds(90)
while (-not (Test-NlGameRunning) -and (Get-Date) -lt $up) { Start-Sleep -Seconds 2 }
if (-not (Test-NlGameRunning)) { throw '게임이 켜져 있지 않습니다. 먼저 tools\session.ps1 -Action start 를 실행하세요.' }

# 메인 메뉴가 뜰 때까지 기다린다(게임이 자료를 읽는 동안에는 답이 늦다).
$deadline = (Get-Date).AddSeconds($TimeoutSec)
$menu = $false
while ((Get-Date) -lt $deadline) {
    if (-not (Test-NlGameRunning)) { throw '게임이 꺼졌습니다.' }
    $state = Ask-Lines @('state') 20
    if ($state -match 'in_game 1') { throw '이미 게임 화면입니다. 세이브는 메인 메뉴에서만 불러옵니다.' }
    if ($state -match 'o_main_menu\s+[1-9]') { $menu = $true; break }
    Start-Sleep -Seconds 4
}
if (-not $menu) { throw "메인 메뉴가 뜨지 않았습니다 ($TimeoutSec 초)." }

# 세이브의 이름들. 게임이 목록을 채우기를 조금 기다린다.
$names = @()
for ($try = 0; $try -lt 10 -and -not $names.Count; $try++) {
    $rows = Ask-Lines @("list $operator.__saves max=200")
    $count = @($rows | Where-Object { $_ -match '^\s+\[\d+\]\s+struct' }).Count
    if ($count) {
        $lines = 0..($count - 1) | ForEach-Object { "ask $operator.__saves[$_].__file_name" }
        $names = @(Ask-Lines $lines | ForEach-Object { if ($_ -match '=\s+string\s+"(.*)"\s*$') { $Matches[1] } })
        if ($names.Count -ne $count) { $names = @() }
    }
    if (-not $names.Count) { Start-Sleep -Seconds 2 }
}
if (-not $names.Count) { throw '게임의 세이브 목록을 읽지 못했습니다.' }

if ($List) {
    for ($i = 0; $i -lt $names.Count; $i++) { Write-Host ("[{0}] {1}" -f $i, $names[$i]) }
    exit 0
}

$target = "$operator.get_last_game_save"
$index = -1
if ($Name) {
    $hits = @(0..($names.Count - 1) | Where-Object { $names[$_] -like "*$Name*" })
    if ($hits.Count -ne 1) {
        throw "이름에 '$Name' 이 든 세이브가 $($hits.Count)개입니다. 하나만 맞게 적어 주세요:`n$($names -join "`n")"
    }
    $index = $hits[0]
    $chosen = $names[$index]
} else {
    $last = Ask-Lines @("ask $operator.__last_game_save_name")
    $chosen = if ("$last" -match '=\s+string\s+"(.*)"') { $Matches[1] } else { '' }
    $index = [Array]::IndexOf($names, $chosen)
    if ($index -lt 0) { throw "가장 최근 세이브('$chosen')가 목록에 없습니다. -Name 으로 골라 주세요." }
}

$can = Ask-Lines @("method $operator.is_can_load_save")
if (-not ("$can" -match '->\s+bool\s+true')) { throw "게임이 지금은 세이브를 불러올 수 없다고 답했습니다: $($can -join ' ')" }

Write-Host "세이브를 불러옵니다: [$index] $chosen"
$null = Ask-Lines @("method $operator.load_save p:$operator.__saves[$index]") 60

# 게임 화면이 될 때까지 기다린다.
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 4
    if (-not (Test-NlGameRunning)) { Write-Host 'FAIL: 게임이 꺼졌습니다.'; exit 1 }
    $state = Ask-Lines @('state') 30
    if ($state -match 'in_game 1') {
        $state | Where-Object { $_ -match 'game_time|o_character|o_dummy' } | ForEach-Object { Write-Host $_ }
        # 실행 묶음이면 게임의 저장을 끈다(치트 표의 no_autosave. research/25: 켠 채 자동 저장의 시각을 세 번 넘겨도 파일이 생기지 않았다).
        # 실행 묶음의 설정은 끌 때 버려지므로 사용자의 게임에는 남지 않는다. 묶음이 아니면(사용자의 설정이 제자리에 있다) 건드리지 않는다.
        $modDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
        $inSession = (Test-Path -LiteralPath (Join-Path $modDir 'NlToyBox.session.txt')) -or [bool](Get-ChildItem -LiteralPath $modDir -Filter '*.kept' -ErrorAction SilentlyContinue)
        if ($KeepSaving) { Write-Host '게임의 저장: 그대로 둠 (-KeepSaving)' }
        elseif (-not $inSession) { Write-Host '게임의 저장: 그대로 둠 (실행 묶음이 아닙니다)' }
        else {
            $null = Ask-Lines @('cheat no_autosave on') 30
            Start-Sleep -Seconds 2
            $flag = Ask-Lines @('ask inst:o_debug.is_save_disabled') 30
            if ("$flag" -match '=\s+bool\s+true') { Write-Host '게임의 저장: 끔 (no_autosave)' }
            else { Write-Host '경고: 게임의 저장을 끄지 못했습니다. 시험 값이 든 채 자동 저장의 시각을 넘기지 마세요.' }
        }
        Write-Host 'PASS'
        exit 0
    }
}
Write-Host "FAIL: 게임 화면이 되지 않았습니다 ($TimeoutSec 초)."
exit 1
