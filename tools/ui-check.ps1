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
    [string[]]$Saved = @() # 저장된 설정이 있는 것처럼 시작한다. '이름=배율' (예: building_cost=0.5)
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')
$Set = @($Set | ForEach-Object { $_ -split ',' } | Where-Object { $_ })      # pwsh -File 은 쉼표로 이은 것을 한 글로 넘긴다

# 모드창이 제대로 그려지는지 본다. 게임을 켜고, 모듈이 ShotSeconds 초 뒤의 화면(게임이 그린 프레임)을 파일로 뜨면
# 그것을 refs\ui\<Name>.png 로 가져오고 게임을 끈다. 화면을 긁지 않는다. 게임 창이 가려져 있어도 된다.
Assert-NlReadyToLaunch
$modDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
$config = Join-Path $modDir 'NlToyBox.ui.txt'
$bmp = Join-Path $modDir 'NlToyBox.ui.bmp'
$log = Join-Path $modDir 'NlToyBox.log'
$outDir = Join-Path (Get-NlRepoRoot) 'refs\ui'
$png = Join-Path $outDir "$Name.png"
# 시험이 사용자의 배율 설정을 바꾸지 않게 한다: 있던 설정 파일은 치워 두었다가 되돌린다.
$settings = Join-Path $modDir 'NlToyBox.settings.txt'
$settingsKept = "$settings.kept"
if (Test-Path -LiteralPath $settings) { Move-Item -LiteralPath $settings -Destination $settingsKept -Force }
$Saved = @($Saved | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
if ($Saved.Count) { Set-Content -LiteralPath $settings -Value $Saved -Encoding ascii }
foreach ($f in $bmp, $log) { if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force } }

$result = $null
try {
    Set-Content -LiteralPath $config -Value (@("open=$([int](-not $Closed))", "shot_seconds=$ShotSeconds") + @($Set | ForEach-Object { "set=$_" }) + @(if ($TestDrag) { "drag=$TestDrag" })) -Encoding ascii
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

    Write-Host '--- NlToyBox.log ---'
    if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log | ForEach-Object { Write-Host $_ } } else { Write-Host '(로그 없음)' }
    Write-Host '--------------------'
}
finally {
    if (Test-Path -LiteralPath $config) { Remove-Item -LiteralPath $config -Force -ErrorAction SilentlyContinue }
    Write-Host "게임 종료: $(Stop-NlGame $GraceSec)"
    if (Test-Path -LiteralPath $settings) {
        Write-Host '--- 시험이 저장한 설정 ---'
        Get-Content -LiteralPath $settings | ForEach-Object { Write-Host $_ }
        Remove-Item -LiteralPath $settings -Force
    }
    if (Test-Path -LiteralPath $settingsKept) { Move-Item -LiteralPath $settingsKept -Destination $settings -Force }
}

if (Test-Path -LiteralPath $bmp) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    Add-Type -AssemblyName System.Drawing
    $image = [System.Drawing.Image]::FromFile($bmp)
    try { $image.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $image.Dispose() }
    Remove-Item -LiteralPath $bmp -Force
    Write-Host "화면: $png"
}
if ($result -match '^ui shot done' -and (Test-Path -LiteralPath $png)) { Write-Host 'PASS'; exit 0 }
Write-Host "FAIL: $result"
exit 1
