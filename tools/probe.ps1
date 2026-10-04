param(
    [Parameter(Mandatory)][string]$Request,
    [Parameter(Mandatory)][string]$Out,
    [int]$TimeoutSec = 240
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 요청 파일을 놓고 게임을 켜서 모듈의 덤프를 받아 온다. 끝나면 게임을 끄고 요청 파일과 덤프를 게임 폴더에서 지운다.
Assert-NlReadyToLaunch
if (-not (Test-Path -LiteralPath $Request -PathType Leaf)) { throw "요청 파일이 없습니다: $Request" }
$Out = [IO.Path]::GetFullPath($Out)

$modDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
$reqDst = Join-Path $modDir 'NlToyBox.probe.txt'
$dump = Join-Path $modDir 'NlToyBox.dump.json'
$log = Join-Path $modDir 'NlToyBox.log'
foreach ($f in $dump, $log) { if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force } }

$done = $false
try {
    Copy-Item -LiteralPath $Request -Destination $reqDst -Force
    Write-Host "게임을 켭니다 (steam://rungameid/$($script:NlAppId)). 게임 창을 누르지 마세요."
    Start-Process "steam://rungameid/$($script:NlAppId)"

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $seenProcess = $false
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        if (Test-NlGameRunning) { $seenProcess = $true }
        elseif ($seenProcess) { Write-Host '게임 프로세스가 사라졌습니다.'; break }
        $lines = @(if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -ErrorAction SilentlyContinue })
        # 모듈이 요청을 읽었으면 요청 파일은 더 필요 없다. 이 도구가 도중에 죽어도 남지 않게 바로 지운다.
        if (($lines -match '^dump requested') -and (Test-Path -LiteralPath $reqDst)) { Remove-Item -LiteralPath $reqDst -Force -ErrorAction SilentlyContinue }
        if ($lines -contains 'dump done') { $done = $true; break }
    }
    if (-not $seenProcess) { Write-Host '게임 프로세스를 한 번도 보지 못했습니다.' }

    Write-Host '--- NlToyBox.log ---'
    if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log | ForEach-Object { Write-Host $_ } } else { Write-Host '(로그 없음)' }
    Write-Host '--------------------'

    # 반쯤 쓰인 덤프도 가져온다. 어디서 멈췄는지가 증거다.
    if (Test-Path -LiteralPath $dump) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Out) | Out-Null
        Copy-Item -LiteralPath $dump -Destination $Out -Force
        Write-Host "덤프: $Out ($((Get-Item -LiteralPath $Out).Length) B)"
    }
}
finally {
    # 요청 파일이 남으면 평소 플레이 때도 덤프가 돈다. 게임을 끄는 일보다 먼저 지운다(끄다가 실패해도 남지 않게).
    if (Test-Path -LiteralPath $reqDst) { Remove-Item -LiteralPath $reqDst -Force -ErrorAction SilentlyContinue }
    try { Write-Host "게임 종료: $(Stop-NlGame)" }
    finally {
        # 덤프는 모듈이 쓰는 중이면 잠겨 있다. 게임을 끈 뒤에 지운다.
        foreach ($f in $reqDst, $dump) { if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue } }
    }
}

if (-not $done) { Write-Host 'FAIL: dump done 을 보지 못했습니다.'; exit 1 }
Write-Host 'PASS'
exit 0
