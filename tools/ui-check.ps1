param(
    [int]$ShotSeconds = 30,
    [int]$TimeoutSec = 180,
    [int]$GraceSec = 15,
    [string]$Name = (Get-Date -Format 'yyyyMMdd-HHmmss'),
    [switch]$Closed      # 창을 닫은 채로 시작한다(기본은 연 채로)
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 모드창이 제대로 그려지는지 본다. 게임을 켜고, 모듈이 ShotSeconds 초 뒤의 화면(게임이 그린 프레임)을 파일로 뜨면
# 그것을 refs\ui\<Name>.png 로 가져오고 게임을 끈다. 화면을 긁지 않는다. 게임 창이 가려져 있어도 된다.
Assert-NlReadyToLaunch
$modDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
$config = Join-Path $modDir 'NlToyBox.ui.txt'
$bmp = Join-Path $modDir 'NlToyBox.ui.bmp'
$log = Join-Path $modDir 'NlToyBox.log'
$outDir = Join-Path (Get-NlRepoRoot) 'refs\ui'
$png = Join-Path $outDir "$Name.png"
foreach ($f in $bmp, $log) { if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force } }

$result = $null
try {
    Set-Content -LiteralPath $config -Value @("open=$([int](-not $Closed))", "shot_seconds=$ShotSeconds") -Encoding ascii
    Write-Host "게임을 켭니다 (steam://rungameid/$($script:NlAppId)). 게임 창을 누르지 마세요."
    Start-Process "steam://rungameid/$($script:NlAppId)"

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $seen = $false
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        if (Test-NlGameRunning) { $seen = $true }
        elseif ($seen) { $result = '게임 프로세스가 사라졌습니다.'; break }
        $lines = @(if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -ErrorAction SilentlyContinue })
        $hit = @($lines -match '^ui (shot done|shot failed|setup failed|hook failed)')
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
