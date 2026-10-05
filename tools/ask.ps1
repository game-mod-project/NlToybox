param(
    [string[]]$Lines = @(),     # 명령들(src/core/RemoteCommand.hpp). 한 줄에 하나
    [string]$File,              # 명령이 든 파일(줄마다 하나). -Lines 대신
    [int]$TimeoutSec = 30
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 켜져 있는 게임에 묻는다(스펙: 치트 메뉴 §14). 모듈이 0.25초마다 NlToyBox.ask.txt 를 보고 답을 NlToyBox.answer.txt 에 이어 쓴다.
# 공백이나 쉼표가 든 줄을 여럿 넘기려면 pwsh 안에서 부른다:   & tools\ask.ps1 -Lines 'state', 'list inst:o_debug max=20'
if ($File) { $Lines = @(Get-Content -LiteralPath $File) }
$Lines = @($Lines | Where-Object { $_ -and $_.Trim() })
if (-not $Lines.Count) { throw '물을 줄이 없습니다. -Lines 나 -File 을 주세요.' }
if (-not (Test-NlGameRunning)) { throw '게임이 켜져 있지 않습니다. 먼저 tools\session.ps1 -Action start 를 실행하세요.' }

$modDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
$ask = Join-Path $modDir 'NlToyBox.ask.txt'
$answer = Join-Path $modDir 'NlToyBox.answer.txt'
$id = [guid]::NewGuid().ToString('N').Substring(0, 8)

# 앞의 물음을 모듈이 아직 가져가지 않았으면 잠깐 기다린다.
$wait = (Get-Date).AddSeconds(5)
while ((Test-Path -LiteralPath $ask) -and (Get-Date) -lt $wait) { Start-Sleep -Milliseconds 200 }
if (Test-Path -LiteralPath $ask) { throw "앞의 물음이 남아 있습니다(모듈이 읽지 않았습니다): $ask" }

# 임시 파일에 쓴 뒤 이름을 바꾼다. 모듈이 반쯤 쓰인 파일을 읽지 않게.
$tmp = "$ask.tmp"
[IO.File]::WriteAllLines($tmp, [string[]](@("id $id") + $Lines), [Text.UTF8Encoding]::new($false))
Move-Item -LiteralPath $tmp -Destination $ask

$deadline = (Get-Date).AddSeconds($TimeoutSec)
$found = $false
$segment = @()
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 250
    if (Test-Path -LiteralPath $answer) {
        $all = @(Get-Content -LiteralPath $answer -Encoding utf8)
        $from = [Array]::IndexOf($all, "# $id")
        $to = [Array]::IndexOf($all, "# done $id")
        if ($from -ge 0 -and $to -gt $from) {
            $segment = @(if ($to -gt $from + 1) { $all[($from + 1)..($to - 1)] })
            $found = $true
            break
        }
    }
    if (-not (Test-NlGameRunning)) { break }
}

if (-not $found) {
    # 답이 끝나지 않았다. 온 데까지 보여 준다(명령이 게임을 끝냈으면 마지막 줄이 그 명령이다).
    if (Test-Path -LiteralPath $answer) {
        $all = @(Get-Content -LiteralPath $answer -Encoding utf8)
        $from = [Array]::IndexOf($all, "# $id")
        if ($from -ge 0 -and $from + 1 -lt $all.Count) { $all[($from + 1)..($all.Count - 1)] | ForEach-Object { Write-Host $_ } }
    }
    if (Test-Path -LiteralPath $ask) { Remove-Item -LiteralPath $ask -Force -ErrorAction SilentlyContinue }
    Write-Host "FAIL: 답을 받지 못했습니다 ($TimeoutSec 초). 게임 실행 중: $(Test-NlGameRunning)"
    exit 1
}
$segment | ForEach-Object { Write-Host $_ }

# shot <이름> 이 있으면 모듈이 뜬 화면을 refs\ui\<이름>.png 로 가져온다.
$shots = @($Lines | ForEach-Object { if ($_ -match '^\s*shot\s+([A-Za-z0-9_-]+)\s*$') { $Matches[1] } })
foreach ($shot in $shots) {
    $bmp = Join-Path $modDir "NlToyBox.shot.$shot.bmp"
    $shotWait = (Get-Date).AddSeconds(8)
    while (-not (Test-Path -LiteralPath $bmp) -and (Get-Date) -lt $shotWait) { Start-Sleep -Milliseconds 200 }
    if (-not (Test-Path -LiteralPath $bmp)) { Write-Host "화면을 받지 못했습니다: $shot"; continue }
    Start-Sleep -Milliseconds 300       # 다 쓰이기를 기다린다
    $outDir = Join-Path (Get-NlRepoRoot) 'refs\ui'
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    $png = Join-Path $outDir "$shot.png"
    Add-Type -AssemblyName System.Drawing
    $image = [System.Drawing.Image]::FromFile($bmp)
    try { $image.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $image.Dispose() }
    Remove-Item -LiteralPath $bmp -Force
    Write-Host "화면: $png"
}
exit 0
