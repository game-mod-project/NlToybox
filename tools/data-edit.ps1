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

# 바이트로 읽는다. BOM 은 그대로 두고, UTF-8 로 읽히지 않는 파일은 건드리지 않는다(글자가 조용히 바뀌는 것을 막는다).
$bytes = [IO.File]::ReadAllBytes($path)
$offset = if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { 3 } else { 0 }
$strict = New-Object System.Text.UTF8Encoding($false, $true)   # 잘못된 바이트에서 예외
try { $text = $strict.GetString($bytes, $offset, $bytes.Length - $offset) }
catch { throw "UTF-8 로 읽을 수 없는 파일입니다. 건드리지 않습니다: $File" }

$count = [regex]::Matches($text, [regex]::Escape($Find)).Count
if ($count -ne 1) { throw "찾는 글이 정확히 한 번 나와야 합니다. $count 번 나왔습니다: $Find" }

$body = $strict.GetBytes($text.Replace($Find, $Replace))
$out = New-Object byte[] ($offset + $body.Length)
[Array]::Copy($bytes, 0, $out, 0, $offset)
[Array]::Copy($body, 0, $out, $offset, $body.Length)
[IO.File]::WriteAllBytes($path, $out)
Write-Host "수정: $File  ($Find -> $Replace)"
