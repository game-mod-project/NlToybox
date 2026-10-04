$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

Assert-NlGameNotRunning
$repo = Get-NlRepoRoot
$gameDir = Get-NlGameDir
$pins = Get-NlPins

# 1) 받기와 대조. pins.json 과 다르면 지우고 멈춘다. 이 단계에서는 게임을 건드리지 않는다.
$local = @{}
foreach ($f in $pins.files) {
    $dir = Join-Path $repo "downloads\$($f.project)-$($f.tag)"
    $path = Join-Path $dir $f.name
    if (-not (Test-Path -LiteralPath $path)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
        Write-Host "받는 중: $($f.url)"
        Invoke-WebRequest -Uri $f.url -OutFile $path -UseBasicParsing
    }
    $size = (Get-Item -LiteralPath $path).Length
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    if ($size -ne $f.size -or $hash -ne $f.sha256) {
        Remove-Item -LiteralPath $path -Force
        throw "받은 파일이 pins.json 과 다릅니다: $($f.name) (size $size, sha256 $hash). 파일을 지웠습니다."
    }
    $local[$f.name] = $path
}

# 2) 백업. 되돌릴 원본 없이 진행하지 않는다.
$info = Get-NlExeInfo
if ($info.Patched) {
    if (@(Get-NlBackups $info.Version).Count -eq 0) {
        throw "exe 가 이미 패치된 상태인데 버전 $($info.Version) 의 백업이 없습니다. Steam 무결성 검사로 원본을 되살린 뒤 다시 실행하세요."
    }
} else {
    $backupDir = Join-Path $repo 'backups'
    $backupPath = Join-Path $backupDir "Norland.exe.$($info.Version).$($info.Sha256.Substring(0, 12))"
    if (-not (Test-Path -LiteralPath $backupPath)) {
        New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
        Copy-Item -LiteralPath $info.Path -Destination $backupPath
        $copied = (Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash
        if ($copied -ne $info.Sha256) { throw "백업 복사본의 해시가 원본과 다릅니다: $backupPath" }
        Write-Host "백업: $backupPath"
    }
}

# 3) 배치
foreach ($f in $pins.files) {
    if (-not $f.gamePath) { continue }
    $dst = Join-Path $gameDir $f.gamePath
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
    Copy-Item -LiteralPath $local[$f.name] -Destination $dst -Force
}

# 4) 패치. AuriePatcher 는 넘겨준 DLL 경로를 exe 안에 적어 둔다.
$core = Join-Path $gameDir 'mods\Native\AurieCore.dll'
& $local['AuriePatcher.exe'] $info.Path $core install
if ($LASTEXITCODE -ne 0) { throw "AuriePatcher 실패 (종료 코드 $LASTEXITCODE)" }

# 5) 확인
$after = Get-NlExeInfo
if (-not $after.Patched) { throw 'AuriePatcher 는 성공을 보고했지만 exe 에 .aurie 섹션이 없습니다.' }
Write-Host "setup ok -> $($after.Path)  (SHA256 $($after.Sha256))"
