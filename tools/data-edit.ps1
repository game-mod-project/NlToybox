param(
    [Parameter(Mandatory)][string]$File,
    [Parameter(Mandatory)][string]$Find,
    [Parameter(Mandatory)][string]$Replace
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 실측용. 스냅샷이 있는 파일에서 찾는 글이 정확히 한 번 나올 때만 바꾼다. 나머지 바이트는 그대로 둔다.
Assert-NlGameNotRunning
Assert-NlSafeRelPath $File
$path = Join-Path (Get-NlGameDir) $File
if (-not (Test-Path -LiteralPath (Join-Path (Get-NlDataSnapshotDir) $File))) {
    throw "스냅샷이 없습니다: $File. 먼저 tools\data-snapshot.ps1 -Files '$File' 을 실행하세요."
}

$utf8 = New-Object System.Text.UTF8Encoding($false)
$text = [IO.File]::ReadAllText($path, $utf8)
$count = [regex]::Matches($text, [regex]::Escape($Find)).Count
if ($count -ne 1) { throw "찾는 글이 정확히 한 번 나와야 합니다. $count 번 나왔습니다: $Find" }

[IO.File]::WriteAllText($path, $text.Replace($Find, $Replace), $utf8)
Write-Host "수정: $File  ($Find -> $Replace)"
