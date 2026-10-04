$ErrorActionPreference = 'Stop'

$script:NlAppId          = '1857090'
$script:NlDefaultGameDir = 'E:\SteamLibrary\steamapps\common\Norland Story Generating Strategy'
$script:NlExeName        = 'Norland.exe'
$script:NlProcessName    = 'Norland'
$script:NlAurieSection   = '.aurie'

function Assert-Equal($actual, $expected, [string]$msg) {
    if ($actual -ne $expected) { throw "$msg : expected <$expected> got <$actual>" }
}
function Assert-True($cond, [string]$msg) {
    if (-not $cond) { throw "$msg : condition false" }
}

function Get-NlRepoRoot { Split-Path -Parent $PSScriptRoot }

function Get-NlGameDir {
    $dir = if ($env:NORLAND_GAME_DIR) { $env:NORLAND_GAME_DIR } else { $script:NlDefaultGameDir }
    if (-not (Test-Path -LiteralPath (Join-Path $dir $script:NlExeName))) {
        throw "게임 폴더에 $($script:NlExeName) 가 없습니다: $dir (NORLAND_GAME_DIR 로 지정할 수 있습니다)"
    }
    (Resolve-Path -LiteralPath $dir).Path
}

function Test-NlGameRunning {
    [bool](Get-Process -Name $script:NlProcessName -ErrorAction SilentlyContinue)
}

function Assert-NlGameNotRunning {
    $p = @(Get-Process -Name $script:NlProcessName -ErrorAction SilentlyContinue)
    if ($p.Count) { throw "게임이 실행 중입니다 (PID $($p[0].Id)). 종료 후 다시 시도하세요." }
}

# PE 헤더를 직접 읽어 섹션 이름을 돌려준다. 게임이 exe 를 잡고 있어도 읽을 수 있게 공유 모드로 연다.
function Get-NlPeSectionNames([string]$Path) {
    $fs = [System.IO.File]::Open($Path, 'Open', 'Read', 'ReadWrite')
    try {
        $br = New-Object System.IO.BinaryReader($fs)
        if ($br.ReadUInt16() -ne 0x5A4D) { throw "PE 파일이 아닙니다 (MZ 없음): $Path" }
        $fs.Position = 0x3C
        $peOff = $br.ReadUInt32()
        $fs.Position = $peOff
        if ($br.ReadUInt32() -ne 0x00004550) { throw "PE 파일이 아닙니다 (PE 서명 없음): $Path" }
        $null = $br.ReadUInt16()                    # Machine
        $count = $br.ReadUInt16()                   # NumberOfSections
        $fs.Position = $peOff + 4 + 16
        $optSize = $br.ReadUInt16()                 # SizeOfOptionalHeader
        $fs.Position = $peOff + 4 + 20 + $optSize   # 섹션 표
        $names = @()
        for ($i = 0; $i -lt $count; $i++) {
            $names += [System.Text.Encoding]::ASCII.GetString($br.ReadBytes(8)).TrimEnd([char]0)
            $fs.Position += 32                      # 섹션 헤더는 40 바이트
        }
        $names
    } finally { $fs.Dispose() }
}

function Get-NlExeInfo {
    $exe = Join-Path (Get-NlGameDir) $script:NlExeName
    $item = Get-Item -LiteralPath $exe
    $sections = @(Get-NlPeSectionNames $exe)
    [pscustomobject]@{
        Path     = $exe
        Version  = $item.VersionInfo.FileVersion
        Size     = $item.Length
        Sha256   = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
        Sections = $sections
        Patched  = $sections -contains $script:NlAurieSection
    }
}

# backups\Norland.exe.<버전>.<SHA256 앞 12자>
function Get-NlBackups([string]$Version) {
    $dir = Join-Path (Get-NlRepoRoot) 'backups'
    if (-not (Test-Path -LiteralPath $dir)) { return @() }
    @(Get-ChildItem -LiteralPath $dir -File -Filter "Norland.exe.$Version.*")
}
