param(
    [Parameter(Mandatory)][string]$Request,
    [Parameter(Mandatory)][string]$Out,
    [int]$TimeoutSec = 240,
    [int]$GraceSec = 15,
    [switch]$Beep      # 메뉴 덤프가 끝났을 때 알림음을 낸다. 기본은 조용하다
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 요청 파일을 놓고 게임을 켜서 모듈의 덤프를 받아 온다. 끝나면 요청 파일과 덤프를 게임 폴더에서 지운다.
# 덤프는 이름대로 온다: -Out x.json 이면 x.menu.json, x.late0.json …
#  - 되풀이하지 않는 요청: 'dump done' 을 기다렸다가 게임을 끈다.
#  - 되풀이 요청(repeat_seconds): 사용자가 새 게임을 시작하고, 사용자가 게임을 끌 때까지 기다린다.
#    게임 안에서는 이 도구가 끄지 않는다. 제한 시간이 지났을 때만 끈다.
Assert-NlReadyToLaunch
if (-not (Test-Path -LiteralPath $Request -PathType Leaf)) { throw "요청 파일이 없습니다: $Request" }
$Out = [IO.Path]::GetFullPath($Out)
$outBase = Join-Path (Split-Path -Parent $Out) ([IO.Path]::GetFileNameWithoutExtension($Out))

$modDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
$reqDst = Join-Path $modDir 'NlToyBox.probe.txt'
$log = Join-Path $modDir 'NlToyBox.log'
function Get-NlDumps { @(Get-ChildItem -LiteralPath $modDir -File -Filter 'NlToyBox.dump*.json' -ErrorAction SilentlyContinue) }
# 덤프 하나를 -Out 옆으로 복사하고 사본의 경로를 돌려준다. 복사하지 못하면 예외를 낸다.
function Copy-NlDump($Dump) {
    $suffix = $Dump.BaseName.Substring('NlToyBox.dump'.Length)      # ".menu" / ".late0" / 옛 형식이면 빈 글
    $dst = "$outBase$suffix.json"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
    Copy-Item -LiteralPath $Dump.FullName -Destination $dst -Force
    $dst
}
foreach ($f in @(Get-NlDumps | ForEach-Object { $_.FullName }) + $log) { if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force } }

$repeat = [bool](Select-String -LiteralPath $Request -Pattern '^\s*repeat_seconds\s*=\s*[1-9]' -Quiet)

$done = $false
$exited = $false
$failed = $null
$toldMenu = $false
$copied = @()
$kept = @()
try {
    Copy-Item -LiteralPath $Request -Destination $reqDst -Force
    if ($repeat) {
        Write-Host "게임을 켭니다 (steam://rungameid/$($script:NlAppId)). 메인 메뉴가 뜨고 약 1분 뒤 화면이 잠깐 멈췄다 풀리면 새 게임을 시작해 주세요. 게임 화면에서 2분쯤 둔 뒤 평소처럼 게임을 꺼 주세요."
    } else {
        Write-Host "게임을 켭니다 (steam://rungameid/$($script:NlAppId)). 게임 창을 누르지 마세요."
    }
    Start-Process "steam://rungameid/$($script:NlAppId)"

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $seenProcess = $false
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        if (Test-NlGameRunning) { $seenProcess = $true }
        elseif ($seenProcess) { Write-Host "게임 프로세스가 사라졌습니다 ($(Get-Date -Format 'HH:mm:ss'))."; $exited = $true; break }
        $lines = @(if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -ErrorAction SilentlyContinue })
        # 모듈은 요청을 읽은 뒤 스스로 지운다. 지우지 못했을 때를 대비해 여기서도 지운다.
        if (($lines -match '^(dump requested|dump failed)') -and (Test-Path -LiteralPath $reqDst)) { Remove-Item -LiteralPath $reqDst -Force -ErrorAction SilentlyContinue }
        # 모듈이 그만뒀으면 더 기다려도 덤프는 오지 않는다.
        $bad = @($lines -match '^dump failed')
        if ($bad.Count) { $failed = $bad[0]; break }
        if ($repeat -and -not $toldMenu -and ($lines -match '^dump menu done')) {
            $toldMenu = $true
            Write-Host "메뉴 덤프가 끝났습니다 ($(Get-Date -Format 'HH:mm:ss')). 이제 새 게임을 시작해 주세요."
            if ($Beep) { try { [Console]::Beep(880, 300) } catch { } }
        }
        if (-not $repeat -and ($lines -contains 'dump done')) { $done = $true; break }
    }
    if (-not $seenProcess) { Write-Host '게임 프로세스를 한 번도 보지 못했습니다.' }

    Write-Host '--- NlToyBox.log ---'
    if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log | ForEach-Object { Write-Host $_ } } else { Write-Host '(로그 없음)' }
    Write-Host '--------------------'

    # 반쯤 쓰인 덤프도 가져온다. 어디서 멈췄는지가 증거다.
    foreach ($d in Get-NlDumps) {
        $dst = Copy-NlDump $d
        if ($copied -notcontains $dst) { $copied += $dst }
        Write-Host "덤프: $dst ($((Get-Item -LiteralPath $dst).Length) B)"
    }
}
finally {
    # 요청 파일이 남으면 평소 플레이 때도 덤프가 돈다. 게임을 끄는 일보다 먼저 지운다(끄다가 실패해도 남지 않게).
    if (Test-Path -LiteralPath $reqDst) { Remove-Item -LiteralPath $reqDst -Force -ErrorAction SilentlyContinue }
    try { Write-Host "게임 종료: $(Stop-NlGame $GraceSec)" }
    finally {
        if (Test-Path -LiteralPath $reqDst) { Remove-Item -LiteralPath $reqDst -Force -ErrorAction SilentlyContinue }
        # 덤프는 다시 켜야만 얻는다. 게임이 꺼진 뒤의 온전한 파일로 한 번 더 복사하고, 복사한 것만 지운다.
        # 복사하지 못한 덤프는 게임 폴더에 남겨 둔다(위에서 예외로 빠져나왔을 때도 여기서 건진다).
        foreach ($d in Get-NlDumps) {
            try {
                $dst = Copy-NlDump $d
                if ($copied -notcontains $dst) { $copied += $dst }
                Remove-Item -LiteralPath $d.FullName -Force
            }
            catch { $kept += $d.Name }
        }
        if ($kept.Count) {
            Write-Host ("주의: 복사하지 못한 덤프를 게임 폴더에 남겨 둡니다: $($kept -join ', ') ($modDir). " +
                        '원인을 고친 뒤 직접 옮기세요. tools\restore-game.ps1 은 이 파일들을 지웁니다.')
        }
    }
}

if ($kept.Count) { Write-Host 'FAIL: 덤프를 복사하지 못했습니다.'; exit 1 }
if ($failed) { Write-Host "FAIL: $failed"; exit 1 }
if ($repeat) {
    if (-not @($copied | Where-Object { $_ -match '\.late\d+\.json$' }).Count) { Write-Host 'FAIL: 되풀이 덤프를 하나도 받지 못했습니다.'; exit 1 }
    if (-not $exited) { Write-Host '주의: 제한 시간이 지나 이 도구가 게임을 껐습니다.' }
}
elseif (-not $done) { Write-Host 'FAIL: dump done 을 보지 못했습니다.'; exit 1 }
Write-Host 'PASS'
exit 0
