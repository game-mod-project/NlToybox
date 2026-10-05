param(
    [int]$ShotSeconds = 30,
    [int]$TimeoutSec = 180,
    [int]$GraceSec = 15,
    [string]$Name = (Get-Date -Format 'yyyyMMdd-HHmmss'),
    [switch]$Closed,     # 창을 닫은 채로 시작한다(기본은 연 채로)
    [string[]]$Set = @(),# 시험용 배율. '이름:배율' (예: building_cost:0.5). 설정 파일에는 저장되지 않는다
    [int]$PressF8 = 0,   # 모드창이 준비되고 8초 뒤에 게임 창에 F8 을 이만큼 보낸다(창 메시지로. 실제 키보드를 건드리지 않는다)
    [string]$Drag,       # 'x1,y1,x2,y2': 그 뒤에 왼쪽 단추를 누른 채 끄는 메시지를 보낸다(게임 창 안의 좌표)
    [string]$TestDrag,   # 'x1,y1,x2,y2': 모듈이 화면을 뜨기 5초 전에 ImGui 에 직접 끌기를 넣는다(실제 마우스를 건드리지 않는다)
    [string[]]$Saved = @(),# 저장된 설정이 있는 것처럼 시작한다. '이름=배율' (예: building_cost=0.5)
    [string]$Page,         # 왼쪽 목록에서 열 영역의 이름(explorer, time, util, …). src/core/CheatTable.cpp 의 Key
    [string]$Path,         # 탐색기가 처음 열 주소(예: inst:o_time_controller)
    [string[]]$Ask = @(),  # 화면을 뜨기 5초 전에 그 주소의 값을 로그에 적는다
    [string[]]$Poke = @()  # '주소=수': 써 넣고, 다시 읽고, 원래 값으로 되돌린다. 결과를 로그에 적는다
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
$Set = @($Set | ForEach-Object { $_ -split ',' } | Where-Object { $_ })      # pwsh -File 은 쉼표로 이은 것을 한 글로 넘긴다
$Ask = @($Ask | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$Poke = @($Poke | ForEach-Object { $_ -split ',' } | Where-Object { $_ })

# 모드창이 제대로 그려지는지 본다. 게임을 켜고, 모듈이 ShotSeconds 초 뒤의 화면(게임이 그린 프레임)을 파일로 뜨면
# 그것을 refs\ui\<Name>.png 로 가져오고 게임을 끈다. 화면을 긁지 않는다. 게임 창이 가려져 있어도 된다.
$modDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
$config = Join-Path $modDir 'NlToyBox.ui.txt'
$bmp = Join-Path $modDir 'NlToyBox.ui.bmp'
$log = Join-Path $modDir 'NlToyBox.log'
$outDir = Join-Path (Get-NlRepoRoot) 'refs\ui'
$png = Join-Path $outDir "$Name.png"
# 시험이 사용자의 설정을 바꾸지 않게 한다: 있던 배율 설정과 치트 상태는 *.kept 로 치워 두었다가 되돌린다.
$settings = Join-Path $modDir 'NlToyBox.settings.txt'
$settingsKept = "$settings.kept"
$cheats = Join-Path $modDir 'NlToyBox.cheats.txt'
$cheatsKept = "$cheats.kept"
$Saved = @($Saved | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
# 앞선 실행이 도중에 죽었으면 *.kept 가 남아 있다. 그것이 사용자의 원본이다. 그 위에 다시 치우면 원본을 잃는다.
foreach ($kept in $settingsKept, $cheatsKept) {
    if (Test-Path -LiteralPath $kept) {
        throw "앞선 실행이 남긴 사본이 있습니다: $kept`n내용을 확인해 원래 이름(.kept 를 뗀 이름)으로 되돌린 뒤 다시 실행하세요."
    }
}
Assert-NlReadyToLaunch

$result = $null
$allLines = @()
$settingsMoved = $false     # 참이면 그 이름의 파일은 시험의 것이고 사용자의 것은 *.kept 에 있다
$cheatsMoved = $false
try {
    if (Test-Path -LiteralPath $settings) { Move-Item -LiteralPath $settings -Destination $settingsKept }
    $settingsMoved = $true
    if ($Saved.Count) { Set-Content -LiteralPath $settings -Value $Saved -Encoding ascii }
    if (Test-Path -LiteralPath $cheats) { Move-Item -LiteralPath $cheats -Destination $cheatsKept }
    $cheatsMoved = $true
    foreach ($f in $bmp, $log) { if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force } }

    $configLines = @("open=$([int](-not $Closed))", "shot_seconds=$ShotSeconds") + @($Set | ForEach-Object { "set=$_" }) +
        @(if ($TestDrag) { "drag=$TestDrag" }) + @(if ($Page) { "page=$Page" }) + @(if ($Path) { "path=$Path" }) +
        @($Ask | ForEach-Object { "ask=$_" }) + @($Poke | ForEach-Object { "poke=$_" })
    Set-Content -LiteralPath $config -Value $configLines -Encoding ascii
    Write-Host "게임을 켭니다 (steam://rungameid/$($script:NlAppId)). 게임 창을 누르지 마세요."
    Start-Process "steam://rungameid/$($script:NlAppId)"

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $seen = $false
    $sent = $false
    $readyAt = $null
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        if (Test-NlGameRunning) { $seen = $true }
        elseif ($seen) { $result = '게임 프로세스가 사라졌습니다.'; break }
        $lines = @(if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -ErrorAction SilentlyContinue })
        $hit = @($lines -match '^ui (shot done|shot failed|setup failed|hook failed)')
        if (-not $sent -and ($lines -match '^ui ready') -and ($PressF8 -or $Drag)) {
            if (-not $readyAt) { $readyAt = Get-Date }
            elseif (((Get-Date) - $readyAt).TotalSeconds -ge 8) {
                $sent = $true
                foreach ($h in Get-NlGameWindows) {
                    for ($i = 0; $i -lt $PressF8; $i++) {
                        [void][NlToyBox.Win]::PostMessageW($h, 0x0100, [IntPtr]0x77, [IntPtr]0x00420001)                  # WM_KEYDOWN VK_F8
                        [void][NlToyBox.Win]::PostMessageW($h, 0x0101, [IntPtr]0x77, [IntPtr]::new(0xC0420001L))          # WM_KEYUP
                        Start-Sleep -Milliseconds 300
                    }
                    if ($Drag) {
                        $c = @($Drag -split ',' | ForEach-Object { [int]$_ })
                        $from = [IntPtr](($c[1] -shl 16) -bor $c[0]); $to = [IntPtr](($c[3] -shl 16) -bor $c[2])
                        [void][NlToyBox.Win]::PostMessageW($h, 0x0200, [IntPtr]0, $from); Start-Sleep -Milliseconds 200       # WM_MOUSEMOVE
                        [void][NlToyBox.Win]::PostMessageW($h, 0x0201, [IntPtr]1, $from); Start-Sleep -Milliseconds 200       # WM_LBUTTONDOWN
                        [void][NlToyBox.Win]::PostMessageW($h, 0x0200, [IntPtr]1, $to); Start-Sleep -Milliseconds 200         # 누른 채 옮긴다
                        [void][NlToyBox.Win]::PostMessageW($h, 0x0202, [IntPtr]0, $to)                                        # WM_LBUTTONUP
                    }
                }
                Write-Host "입력을 보냈습니다: F8 x$PressF8, 끌기 '$Drag'"
            }
        }
        if ($hit.Count) { $result = $hit[0]; break }
    }
    if (-not $result) { $result = '제한 시간 안에 화면을 받지 못했습니다.' }

    $allLines = @(if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log })
    Write-Host '--- NlToyBox.log ---'
    if ($allLines.Count) { $allLines | ForEach-Object { Write-Host $_ } } else { Write-Host '(로그 없음)' }
    Write-Host '--------------------'
}
finally {
    if (Test-Path -LiteralPath $config) { Remove-Item -LiteralPath $config -Force -ErrorAction SilentlyContinue }
    Write-Host "게임 종료: $(Stop-NlGame $GraceSec)"
    # 치워 둔 것만 되돌린다. 치우기 전에 실패했으면 그 이름의 파일은 사용자의 것이다. 건드리지 않는다.
    if ($settingsMoved) {
        if (Test-Path -LiteralPath $settings) {
            Write-Host '--- 시험이 저장한 설정 ---'
            Get-Content -LiteralPath $settings | ForEach-Object { Write-Host $_ }
            Remove-Item -LiteralPath $settings -Force
        }
        if (Test-Path -LiteralPath $settingsKept) { Move-Item -LiteralPath $settingsKept -Destination $settings }
    }
    if ($cheatsMoved) {
        if (Test-Path -LiteralPath $cheats) {
            Write-Host '--- 시험이 저장한 치트 상태 ---'
            Get-Content -LiteralPath $cheats | ForEach-Object { Write-Host $_ }
            Remove-Item -LiteralPath $cheats -Force
        }
        if (Test-Path -LiteralPath $cheatsKept) { Move-Item -LiteralPath $cheatsKept -Destination $cheats }
    }
}

if (Test-Path -LiteralPath $bmp) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    Add-Type -AssemblyName System.Drawing
    $image = [System.Drawing.Image]::FromFile($bmp)
    try { $image.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $image.Dispose() }
    Remove-Item -LiteralPath $bmp -Force
    Write-Host "화면: $png"
}
# 적재 판정도 함께 낸다(check-load.ps1 과 같은 줄). 실행 한 번으로 둘을 본다.
$loadFailed = @(Get-NlLoadFailures $allLines)
if ($loadFailed.Count) { Write-Host "FAIL: 적재 판정 - $($loadFailed -join ', ')"; exit 1 }
Write-Host '적재 판정: 통과'
if ($result -match '^ui shot done' -and (Test-Path -LiteralPath $png)) { Write-Host 'PASS'; exit 0 }
Write-Host "FAIL: $result"
exit 1
