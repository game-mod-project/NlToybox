param(
    [Parameter(Mandatory, Position = 0)]
    [ValidateSet('check', 'apply', 'restore', 'status', 'keys', 'pin', 'scan', 'probe-request', 'verify')]
    [string]$Command,
    [string]$Preset,
    [string]$Out,
    [string]$Dump,
    [string]$File,
    [string]$CatalogDir
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 프리셋을 게임의 데이터 파일에 입히고 되돌린다. 경로와 게임 버전을 채워 tools\overlay\cli.py 를 부른다.
#   overlay.ps1 check   -Preset presets\x.json     무엇이 바뀌는지 보여 준다. 쓰지 않는다
#   overlay.ps1 apply   -Preset presets\x.json     바닐라로 되돌린 뒤 프리셋을 입힌다
#   overlay.ps1 restore                            바닐라로 되돌린다
#   overlay.ps1 status
# 나머지 명령(keys, pin, scan, probe-request, verify)은 tools\overlay\cli.py 의 설명을 본다.
$gameDir = Get-NlGameDir
$version = (Get-NlExeInfo).Version
# 게임은 켤 때 데이터 파일을 읽는다. 켜져 있는 동안에는 파일을 바꾸지 않는다.
if ($Command -in 'apply', 'restore') { Assert-NlGameNotRunning }
if (-not $CatalogDir) { $CatalogDir = Join-Path (Get-NlRepoRoot) "catalog\$version" }
if (-not (Test-Path -LiteralPath (Join-Path $CatalogDir 'keys.json'))) {
    throw "이 게임 버전($version)의 카탈로그가 없습니다: $CatalogDir. 게임이 갱신됐으면 새 버전의 카탈로그를 만들어야 합니다."
}

$cli = @((Join-Path $PSScriptRoot 'overlay\cli.py'), $Command,
    '--game-dir', $gameDir, '--game-version', $version, '--catalog-dir', $CatalogDir,
    '--snapshot-dir', (Get-NlDataSnapshotDir), '--state-dir', (Get-NlOverlayStateDir))
foreach ($pair in @(@('--preset', $Preset), @('--out', $Out), @('--dump', $Dump), @('--file', $File))) {
    if ($pair[1]) { $cli += $pair }
}

# cli.py 는 UTF-8 로 쓴다. 받아서 PowerShell 의 글로 바꾼 뒤 내보낸다(콘솔 코드 페이지가 무엇이든 한글이 깨지지 않게).
$savedEncoding = [Console]::OutputEncoding
$savedIo = $env:PYTHONIOENCODING
try {
    [Console]::OutputEncoding = [Text.Encoding]::UTF8
    $env:PYTHONIOENCODING = 'utf-8'
    $lines = & py -3.14 @cli 2>&1
    $code = $LASTEXITCODE
}
finally {
    [Console]::OutputEncoding = $savedEncoding
    $env:PYTHONIOENCODING = $savedIo
}
$lines | ForEach-Object { Write-Host $_ }
exit $code
