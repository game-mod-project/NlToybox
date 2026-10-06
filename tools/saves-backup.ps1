param(
    [string]$To,
    [string]$Diff
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 세이브·설정 폴더의 사본을 뜬다(기본: backups\saves\<시각>\). -Diff <사본 폴더> 를 주면 사본과 지금의 차이만 보여 준다.
# 세이브 폴더에는 쓰지 않는다.
$src = Get-NlSavesDir
if (-not (Test-Path -LiteralPath $src -PathType Container)) { throw "세이브 폴더가 없습니다: $src" }
$src = (Resolve-Path -LiteralPath $src).Path

# 상대경로 → SHA256
function Get-NlTree([string]$Root) {
    $map = @{}
    foreach ($f in Get-ChildItem -LiteralPath $Root -Recurse -File -Force) {
        $map[$f.FullName.Substring($Root.Length + 1)] = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
    }
    $map
}

if ($Diff) {
    if (-not (Test-Path -LiteralPath $Diff -PathType Container)) { throw "사본 폴더가 없습니다: $Diff" }
    $old = Get-NlTree (Resolve-Path -LiteralPath $Diff).Path
    $new = Get-NlTree $src
    $n = 0
    foreach ($rel in ($new.Keys | Sort-Object)) {
        if (-not $old.ContainsKey($rel)) { Write-Host "새로 생김: $rel"; $n++ }
        elseif ($old[$rel] -ne $new[$rel]) { Write-Host "바뀜: $rel"; $n++ }
    }
    foreach ($rel in ($old.Keys | Sort-Object)) {
        if (-not $new.ContainsKey($rel)) { Write-Host "없어짐: $rel"; $n++ }
    }
    Write-Host "saves diff ($n)"
    exit 0
}

Assert-NlGameNotRunning      # 게임이 쓰는 도중의 파일을 사본으로 뜨지 않는다
if (-not $To) { $To = Join-Path (Get-NlRepoRoot) "backups\saves\$(Get-Date -Format 'yyyyMMdd-HHmmss')" }
$To = [IO.Path]::GetFullPath($To)
if ($To -eq $src -or $To.StartsWith("$src\", [StringComparison]::OrdinalIgnoreCase)) { throw "사본을 세이브 폴더 안에 둘 수 없습니다: $To" }
if ((Test-Path -LiteralPath $To) -and @(Get-ChildItem -LiteralPath $To -Force).Count) { throw "사본 폴더가 비어 있지 않습니다: $To" }

$tree = Get-NlTree $src
foreach ($rel in $tree.Keys) {
    $dst = Join-Path $To $rel
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
    Copy-Item -LiteralPath (Join-Path $src $rel) -Destination $dst
    if ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -ne $tree[$rel]) { throw "사본이 원본과 다릅니다: $rel" }
}
New-Item -ItemType Directory -Force -Path $To | Out-Null      # 세이브 폴더가 비어 있어도 사본 폴더는 남긴다
Write-Host "saves backup ok ($($tree.Count)) -> $To"
