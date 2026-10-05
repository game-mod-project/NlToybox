param(
    [Parameter(Mandatory)][ValidateSet('start', 'stop', 'status')][string]$Action,
    [string]$Name = (Get-Date -Format 'yyyyMMdd-HHmmss'),   # stop: 답과 로그를 refs\runtime\<Name>.* 로 옮긴다
    [switch]$KeepSettings,                                   # start: 사용자의 배율·치트 설정을 치우지 않는다(걸어 둔 채로 본다)
    [int]$GraceSec = 15
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 실행 묶음: 게임을 켜 둔 채 tools\ask.ps1 로 몇 번이든 묻는다(스펙: 치트 메뉴 §14). start 가 켜고 stop 이 끈다.
# 값을 재는 동안 치트가 걸려 있으면 안 되므로 start 는 사용자의 설정을 *.kept 로 치워 두고, stop 이 되돌린다.
$modDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
$settings = Join-Path $modDir 'NlToyBox.settings.txt'
$cheats = Join-Path $modDir 'NlToyBox.cheats.txt'
$log = Join-Path $modDir 'NlToyBox.log'
$ask = Join-Path $modDir 'NlToyBox.ask.txt'
$answer = Join-Path $modDir 'NlToyBox.answer.txt'

# 치워 둔 사용자의 설정을 되돌린다. 실행 묶음이 그 이름으로 쓴 파일은 버린다.
function Restore-NlKept {
    foreach ($file in $settings, $cheats) {
        $kept = "$file.kept"
        if (Test-Path -LiteralPath $kept) {
            if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force }
            Move-Item -LiteralPath $kept -Destination $file
        }
    }
}

function Get-NlShotFiles {
    if (-not (Test-Path -LiteralPath $modDir)) { return @() }
    @(Get-ChildItem -LiteralPath $modDir -Filter 'NlToyBox.shot.*.bmp' | ForEach-Object { $_.FullName })
}

switch ($Action) {
    'start' {
        foreach ($file in $settings, $cheats) {
            if (Test-Path -LiteralPath "$file.kept") {
                throw "앞선 실행이 남긴 사본이 있습니다: $file.kept`n먼저 tools\session.ps1 -Action stop 을 실행해 되돌리세요."
            }
        }
        Assert-NlReadyToLaunch
        foreach ($f in @($ask, "$ask.tmp", $answer, $log) + (Get-NlShotFiles)) {
            if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force }
        }
        if (-not $KeepSettings) {
            foreach ($file in $settings, $cheats) {
                if (Test-Path -LiteralPath $file) { Move-Item -LiteralPath $file -Destination "$file.kept" }
            }
        }
        try {
            Write-Host "게임을 켭니다 (steam://rungameid/$($script:NlAppId))."
            Start-Process "steam://rungameid/$($script:NlAppId)"
        }
        catch {
            Restore-NlKept
            throw
        }
        Write-Host '켜진 뒤에는 tools\ask.ps1 로 묻고, 끝나면 tools\session.ps1 -Action stop 으로 끕니다.'
    }

    'status' {
        Write-Host "게임 실행 중: $(Test-NlGameRunning)"
        $lines = @(if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log })
        $failed = @(Get-NlLoadFailures $lines)
        Write-Host ('적재 판정: ' + $(if ($failed.Count) { "미통과 - $($failed -join ', ')" } else { '통과' }))
        Write-Host "치워 둔 설정: $(@($settings, $cheats | Where-Object { Test-Path -LiteralPath "$_.kept" }).Count)개"
        $lines | Select-Object -Last 12 | ForEach-Object { Write-Host "  $_" }
        if ($failed.Count) { exit 1 }
    }

    'stop' {
        Write-Host "게임 종료: $(Stop-NlGame $GraceSec)"
        Restore-NlKept
        $outDir = Join-Path (Get-NlRepoRoot) 'refs\runtime'
        if (Test-Path -LiteralPath $answer) {
            New-Item -ItemType Directory -Force -Path $outDir | Out-Null
            Move-Item -LiteralPath $answer -Destination (Join-Path $outDir "$Name.answer.txt") -Force
            Write-Host "답: refs\runtime\$Name.answer.txt"
            if (Test-Path -LiteralPath $log) { Copy-Item -LiteralPath $log -Destination (Join-Path $outDir "$Name.log") -Force }
        }
        foreach ($f in @($ask, "$ask.tmp") + (Get-NlShotFiles)) {
            if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force }
        }
    }
}
