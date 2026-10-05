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

# 받을 파일의 출처·크기·SHA256. downloads\<project>-<tag>\<name> 에 받는다.
function Get-NlPins {
    Get-Content -LiteralPath (Join-Path $PSScriptRoot 'pins.json') -Raw | ConvertFrom-Json
}

# 게임 폴더 안의 상대경로만 받는다.
function Assert-NlSafeRelPath([string]$Rel) {
    if ([IO.Path]::IsPathRooted($Rel) -or (($Rel -split '[\\/]') -contains '..')) {
        throw "게임 폴더 안의 상대경로여야 합니다: $Rel"
    }
}

# 데이터 파일의 바닐라 스냅샷을 두는 곳. 게임 버전별로 나눈다.
function Get-NlDataSnapshotDir {
    Join-Path (Get-NlRepoRoot) "backups\data\$((Get-NlExeInfo).Version)"
}

# 데이터 오버레이의 상태(마지막에 입힌 프리셋과 그때 쓴 파일의 해시)를 두는 곳. 게임 버전별로 나눈다.
# 스냅샷 폴더 안에 두지 않는다: data-restore.ps1 은 스냅샷 폴더의 모든 파일을 게임 폴더로 복사한다.
function Get-NlOverlayStateDir {
    Join-Path (Get-NlRepoRoot) "backups\overlay\$((Get-NlExeInfo).Version)"
}

# 세이브·설정 폴더. 이 레포의 도구는 여기에 쓰지 않는다(읽어서 사본을 뜰 뿐이다). 시험은 NORLAND_SAVES_DIR 로 바꾼다.
function Get-NlSavesDir {
    if ($env:NORLAND_SAVES_DIR) { $env:NORLAND_SAVES_DIR } else { Join-Path $env:LOCALAPPDATA 'Strategy' }
}

# 게임을 켜는 도구가 켜기 전에 부른다. 결과가 정해진 실행에 게임 켜기 한 번을 쓰지 않는다.
function Assert-NlReadyToLaunch {
    Assert-NlGameNotRunning
    $gameDir = Get-NlGameDir
    if (-not (Test-Path -LiteralPath (Join-Path $gameDir 'mods\Aurie\NlToyBox.dll'))) {
        throw 'mods\Aurie\NlToyBox.dll 이 없습니다. 먼저 tools\deploy.ps1 을 실행하세요.'
    }
    if (-not (Get-NlExeInfo).Patched) { throw 'exe 가 패치되지 않았습니다. 먼저 tools\setup-aurie.ps1 을 실행하세요.' }
    foreach ($rel in 'mods\Native\AurieCore.dll', 'mods\Aurie\YYToolkit.dll') {
        if (-not (Test-Path -LiteralPath (Join-Path $gameDir $rel))) { throw "$rel 이 없습니다. tools\setup-aurie.ps1 을 다시 실행하세요." }
    }
}

# 게임 창(클래스 YYGameMakerYY)에 WM_CLOSE 를 보내 정상 종료시킨다. 그래야 aurie.log 가 채워진다.
# 기한 안에 끝나지 않으면 강제 종료한다. 돌려주는 값: 'not-running' | 'closed' | 'killed'
function Stop-NlGame([int]$GraceSec = 15) {
    $procs = @(Get-Process -Name $script:NlProcessName -ErrorAction SilentlyContinue)
    if ($procs.Count -eq 0) { return 'not-running' }

    foreach ($h in Get-NlGameWindows) { [void][NlToyBox.Win]::PostMessageW($h, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) }   # WM_CLOSE

    $deadline = (Get-Date).AddSeconds($GraceSec)
    while ((Get-Date) -lt $deadline -and (Test-NlGameRunning)) { Start-Sleep -Milliseconds 500 }
    if (-not (Test-NlGameRunning)) { return 'closed' }

    Get-Process -Name $script:NlProcessName -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    'killed'
}

# 게임 창(클래스 YYGameMakerYY)의 핸들들. 게임이 꺼져 있으면 비어 있다.
function Get-NlGameWindows {
    $procs = @(Get-Process -Name $script:NlProcessName -ErrorAction SilentlyContinue)
    if ($procs.Count -eq 0) { return @() }

    if (-not ('NlToyBox.Win' -as [type])) {
        Add-Type -Namespace NlToyBox -Name Win -MemberDefinition @'
public delegate bool EnumProc(System.IntPtr h, System.IntPtr l);
[DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, System.IntPtr l);
[DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(System.IntPtr h, out uint pid);
[DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(System.IntPtr h, System.Text.StringBuilder s, int n);
[DllImport("user32.dll")] public static extern bool PostMessageW(System.IntPtr h, uint msg, System.IntPtr w, System.IntPtr l);
'@
    }

    $ids = @($procs.Id)
    $windows = New-Object System.Collections.Generic.List[IntPtr]
    $callback = [NlToyBox.Win+EnumProc]{ param($h, $l)
        $procId = [uint32]0
        [void][NlToyBox.Win]::GetWindowThreadProcessId($h, [ref]$procId)
        if ($ids -contains [int]$procId) {
            $class = New-Object System.Text.StringBuilder 256
            [void][NlToyBox.Win]::GetClassNameW($h, $class, 256)
            if ($class.ToString() -eq 'YYGameMakerYY') { $windows.Add($h) }
        }
        $true
    }
    [void][NlToyBox.Win]::EnumWindows($callback, [IntPtr]::Zero)
    @($windows)
}

# 모듈이 붙었다는 판정. 줄 형식은 src/ModuleMain.cpp 가 쓴다 (Phase 0 스펙 §4.3). 'yytk' 줄은 판정에 쓰지 않는다.
# 돌려주는 값: 빠진 것의 이름들. 비어 있으면 통과다.
function Get-NlLoadFailures([string[]]$Lines) {
    $checks = [ordered]@{
        'loaded'  = [bool]($Lines -match '^NlToyBox \S+ loaded$')
        'builtin' = $Lines -contains 'builtin code_is_compiled = true'
        'script'  = $Lines -contains 'script gml_Script_command_line_parameters_init = found'
        'done'    = $Lines -contains 'probe done'
    }
    @($checks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object { $_.Key })
}
