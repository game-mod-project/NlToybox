param(
    [int]$TimeoutSec = 180,
    [switch]$KeepRunning
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

Assert-NlReadyToLaunch
$gameDir = Get-NlGameDir
$log = Join-Path $gameDir 'mods\Aurie\NlToyBox.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log -Force }

Write-Host "게임을 켭니다 (steam://rungameid/$($script:NlAppId)). 게임 창을 누르지 마세요."
Start-Process "steam://rungameid/$($script:NlAppId)"

# 모듈이 'probe done' 을 쓸 때까지 기다린다. 게임이 떴다가 사라지면 더 기다리지 않는다.
$deadline = (Get-Date).AddSeconds($TimeoutSec)
$seenProcess = $false
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    [void](Clear-NlConsoleSelect)       # Aurie 콘솔이 선택 모드면 게임이 켜지다 선다(research/06)
    if (Test-NlGameRunning) { $seenProcess = $true }
    elseif ($seenProcess) { Write-Host '게임 프로세스가 사라졌습니다.'; break }
    if ((Test-Path -LiteralPath $log) -and (@(Get-Content -LiteralPath $log -ErrorAction SilentlyContinue) -contains 'probe done')) { break }
}

$lines = @()
if (Test-Path -LiteralPath $log) { $lines = @(Get-Content -LiteralPath $log) }

Write-Host '--- NlToyBox.log ---'
if ($lines.Count) { $lines | ForEach-Object { Write-Host $_ } } else { Write-Host '(로그 없음)' }
Write-Host '--------------------'
if (-not $seenProcess) { Write-Host '게임 프로세스를 한 번도 보지 못했습니다.' }

if (-not $KeepRunning) {
    $how = Stop-NlGame
    if ($how -ne 'not-running') { Write-Host "게임 종료: $how" }
}

# 판정은 common.ps1 의 Get-NlLoadFailures 가 한다(ui-check.ps1 과 같이 쓴다).
$failed = @(Get-NlLoadFailures $lines)
if ($failed.Count) {
    Write-Host "FAIL: $($failed -join ', ')"
    exit 1
}
Write-Host 'PASS'
exit 0
