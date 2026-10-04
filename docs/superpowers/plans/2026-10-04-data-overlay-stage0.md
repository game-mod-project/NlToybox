# 데이터 오버레이 단계 0 (실측) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 게임 데이터 파일의 값이 런타임에 반영되는지, 반영된다면 메모리 어디에 앉는지, 파일에 없는 키를 넣으면 읽히는지를 게임 실행 세 번으로 잰다.

**Architecture:** `NlToyBox.dll`에 요청 파일이 있을 때만 도는 덤프 기능을 더한다. 덤프는 전역 변수 목록, 스크립트 호출 결과, 값·이름으로 찾은 경로를 JSON으로 쓴다. PowerShell 도구가 데이터 파일을 스냅샷·수정·복원하고, `probe.ps1`이 게임을 켜서 덤프를 받아 온다. 결과는 `research/01-data-overlay.md`에 적는다.

**Tech Stack:** C++ (YYToolkit v5 인터페이스: `EnumInstanceMembers`, `CallGameScriptEx`, `CallBuiltinEx`), PowerShell 7, Python 3.14 (덤프 분석).

**Spec:** `docs/superpowers/specs/2026-10-04-data-overlay-design.md` (§3이 이 계획의 범위다. §4는 범위 밖이다)

## Global Constraints

- 문서 브랜치 `docs/configurable-rules-roadmap`를 먼저 `develop`에 `--no-ff`로 병합한다(문서뿐이다). 구현은 `develop`에서 분기한 `feat/overlay-stage0`에서 한다. `main`·`develop`에 직접 커밋하지 않는다.
- git 명령은 모두 `git -C E:\NlToyBox`. 커밋 직전에 `branch --show-current`를 본다. 커밋 메시지는 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`로 끝낸다.
- 스크립트는 `pwsh`로 실행한다. Python은 `py -3.14`.
- `refs/`·`backups/`·`downloads/`·`build/` 아래는 커밋하지 않는다.
- **게임은 세 번까지 켠다**(Task 4, 5, 6). 더 필요하면 멈추고 사용자에게 묻는다. 켜기 전에 "게임 창을 누르지 말아 주세요"라고 알린다.
- 게임을 끌 때는 `Stop-NlGame`을 쓴다(`WM_CLOSE` 뒤 15초, 그래도 살아 있으면 강제 종료).
- 게임 데이터 파일은 `data-snapshot.ps1`으로 스냅샷을 뜬 뒤에만 고친다. 고치는 파일은 `debug_params.json`, `gameplay_variables.json`, `battle_params.json` 셋뿐이다.
- 덤프 기능은 읽기만 한다. 게임의 값을 쓰지 않는다.
- 끝나면 게임은 바닐라여야 한다: exe SHA256 `609C17CFD812E90C7D3E84D3EB730EC559EA3976F30A239395922F604516863E`, 세 데이터 파일이 스냅샷과 같고, `mods\`와 `aurie.log`가 없다.
- 표시: **[게임 변경]** 게임 폴더를 바꾼다, **[게임 실행]** 게임을 켠다. 이 둘은 계획 승인 뒤에만 한다.

## Review Focus

1. `probe.ps1`이 중간에 실패해 요청 파일이 게임 폴더에 남는다. 그러면 평소 플레이 때도 덤프가 돈다. 어떤 실패 경로에서도 지워야 한다. → Task 3 Step 4
2. 이미 고친 파일에 `data-snapshot`을 다시 돌린다. 고친 내용이 "바닐라"로 보관되면 안 된다. → Task 1 Step 2
3. `data-edit`의 찾는 글이 0번이나 2번 이상 나온다. 거부하고 파일을 그대로 둬야 한다. → Task 1 Step 2
4. 도구에 `..`이 든 경로를 준다. 게임 폴더 밖을 읽거나 쓰면 안 된다. → Task 1 Step 2
5. 덤프 도중 게임이 죽는다. `probe.ps1`은 반쯤 쓰인 덤프를 가져오고, 요청 파일을 지우고, 실패로 끝나야 한다. → 게임 없이는 시험할 수 없다. Task 4에서 실제로 죽으면 그 동작을 확인하고, 죽지 않으면 "확인하지 못함"으로 적는다

---

### Task 1: 데이터 도구와 공용 함수

**Files:**
- Modify: `E:\NlToyBox\tools\common.ps1` (끝에 함수 넷 추가)
- Create: `E:\NlToyBox\tools\data-snapshot.ps1`, `data-restore.ps1`, `data-edit.ps1`
- Modify: `E:\NlToyBox\tools\check-load.ps1` (사전 검사와 종료를 공용 함수로)
- Modify: `E:\NlToyBox\tools\restore-game.ps1` (`$ours`에 두 파일)
- Modify: `E:\NlToyBox\tools\tests\safety.tests.ps1` (시험 넷, 정리 한 줄)

**Interfaces:**
- Consumes: `Get-NlRepoRoot`, `Get-NlGameDir`, `Get-NlExeInfo`, `Assert-NlGameNotRunning`, `Test-NlGameRunning`, `$script:NlProcessName` (기존)
- Produces:
  - `Assert-NlSafeRelPath [string]$Rel` — 절대경로이거나 `..`이 있으면 throw
  - `Get-NlDataSnapshotDir` → `backups\data\<exe 버전>`의 절대경로
  - `Assert-NlReadyToLaunch` — 실행 중, `NlToyBox.dll` 없음, 미패치, Aurie DLL 없음이면 throw
  - `Stop-NlGame [int]$GraceSec = 15` → `'not-running'` | `'closed'` | `'killed'`
  - `pwsh -File tools/data-snapshot.ps1 -Files <상대경로…>`, `tools/data-restore.ps1`(끝줄 `data restore ok (<n>)`), `tools/data-edit.ps1 -File <상대경로> -Find <글> -Replace <글>`

- [ ] **Step 1: 브랜치 준비**

```powershell
git -C E:\NlToyBox switch develop
git -C E:\NlToyBox merge --no-ff docs/configurable-rules-roadmap -m "Merge branch 'docs/configurable-rules-roadmap' into develop" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git -C E:\NlToyBox branch -d docs/configurable-rules-roadmap
git -C E:\NlToyBox switch -c feat/overlay-stage0
```

- [ ] **Step 2: 실패하는 시험을 먼저 쓴다 — `tools/tests/safety.tests.ps1`**

마지막 `Test-Case` 뒤, `Write-Host "safety tests: …"` 줄 앞에 넣는다. 이 시점에 가짜 게임은 패치돼 있고 `mods\Aurie\NlToyBox.dll`(가짜)이 있다.

```powershell
    $x = Join-Path $fake 'x.json'

    Test-Case 'data-snapshot 은 바닐라를 보관하고, 달라진 파일에는 거부한다' {
        [IO.File]::WriteAllText($x, 'AAA=1')
        $r = Invoke-Tool 'data-snapshot.ps1' '-Files x.json'
        Assert-Equal $r.Exit 0 "snapshot 종료 코드`n$($r.Out)"
        [IO.File]::WriteAllText($x, 'AAA=2')                     # 스냅샷을 뜬 뒤 고친 파일
        $r = Invoke-Tool 'data-snapshot.ps1' '-Files x.json'
        Assert-Equal $r.Exit 1 "고친 파일을 바닐라로 보관하면 안 된다`n$($r.Out)"
    }

    Test-Case 'data-edit 은 찾는 글이 정확히 한 번일 때만 바꾼다' {
        [IO.File]::WriteAllText($x, 'AAA=1 BBB=1 BBB=1')
        $r = Invoke-Tool 'data-edit.ps1' '-File x.json -Find BBB=1 -Replace BBB=9'
        Assert-Equal $r.Exit 1 "두 번 나오는 글은 거부해야 한다`n$($r.Out)"
        $r = Invoke-Tool 'data-edit.ps1' '-File x.json -Find CCC=1 -Replace CCC=9'
        Assert-Equal $r.Exit 1 "없는 글은 거부해야 한다`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText($x)) 'AAA=1 BBB=1 BBB=1' '거부했으면 파일은 그대로여야 한다'
        $r = Invoke-Tool 'data-edit.ps1' '-File x.json -Find AAA=1 -Replace AAA=7'
        Assert-Equal $r.Exit 0 "edit 종료 코드`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText($x)) 'AAA=7 BBB=1 BBB=1' '한 번 나오는 글만 바뀌어야 한다'
    }

    Test-Case 'data-restore 는 스냅샷으로 되돌린다' {
        $r = Invoke-Tool 'data-restore.ps1'
        Assert-Equal $r.Exit 0 "restore 종료 코드`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText($x)) 'AAA=1' '스냅샷의 내용으로 돌아와야 한다'
    }

    Test-Case '게임 폴더 밖을 가리키는 경로는 거부한다' {
        $r = Invoke-Tool 'data-snapshot.ps1' '-Files ..\x.json'
        Assert-Equal $r.Exit 1 "snapshot 은 .. 을 거부해야 한다`n$($r.Out)"
        $r = Invoke-Tool 'data-edit.ps1' '-File ..\x.json -Find A -Replace B'
        Assert-Equal $r.Exit 1 "edit 은 .. 을 거부해야 한다`n$($r.Out)"
    }
```

`finally` 블록의 `Get-ChildItem -LiteralPath $backupDir …` 줄 앞에 가짜 게임의 데이터 스냅샷 정리를 넣는다.

```powershell
    $fakeData = Join-Path $backupDir "data\$($orig.Version)"   # 가짜 게임의 버전이다. 진짜 게임의 스냅샷은 버전이 달라 걸리지 않는다.
    if (Test-Path -LiteralPath $fakeData) { Remove-Item -LiteralPath $fakeData -Recurse -Force }
```

- [ ] **Step 3: 시험이 실패하는지 본다**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\tests\safety.tests.ps1 2>&1 | Select-Object -Last 6; "exit=$LASTEXITCODE"`

Expected: 기존 여섯 개는 `ok`, 이어서 `snapshot 종료 코드 … expected <0> got <1>`(스크립트가 없어 "인식되지 않습니다" 오류가 난다)와 `exit=1`.

- [ ] **Step 4: `tools/common.ps1` 끝에 추가**

```powershell

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
    foreach ($h in $windows) { [void][NlToyBox.Win]::PostMessageW($h, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) }   # WM_CLOSE

    $deadline = (Get-Date).AddSeconds($GraceSec)
    while ((Get-Date) -lt $deadline -and (Test-NlGameRunning)) { Start-Sleep -Milliseconds 500 }
    if (-not (Test-NlGameRunning)) { return 'closed' }

    Get-Process -Name $script:NlProcessName -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    'killed'
}
```

- [ ] **Step 5: `tools/data-snapshot.ps1` 작성**

```powershell
param([Parameter(Mandatory)][string[]]$Files)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 게임 데이터 파일의 바닐라 사본을 보관한다. 파일이 지금 바닐라인지는 이 도구가 알 수 없다.
# 처음 뜰 때는 Steam 설치 또는 무결성 검사 직후여야 한다.
Assert-NlGameNotRunning
$gameDir = Get-NlGameDir
$root = Get-NlDataSnapshotDir

foreach ($rel in $Files) {
    Assert-NlSafeRelPath $rel
    $src = Join-Path $gameDir $rel
    if (-not (Test-Path -LiteralPath $src -PathType Leaf)) { throw "게임 폴더에 파일이 없습니다: $rel" }
    $dst = Join-Path $root $rel
    $srcHash = (Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash

    if (Test-Path -LiteralPath $dst) {
        if ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -ne $srcHash) {
            throw "게임 파일이 스냅샷과 다릅니다: $rel. 스냅샷은 바닐라여야 합니다. tools\data-restore.ps1 로 되돌린 뒤 다시 실행하세요."
        }
        Write-Host "이미 있음: $rel"
        continue
    }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
    Copy-Item -LiteralPath $src -Destination $dst
    if ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -ne $srcHash) {
        Remove-Item -LiteralPath $dst -Force
        throw "스냅샷 복사본의 해시가 원본과 다릅니다: $rel"
    }
    Write-Host "스냅샷: $rel"
}
```

- [ ] **Step 6: `tools/data-restore.ps1` 작성**

```powershell
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 스냅샷에 있는 파일을 게임 폴더로 되돌린다. 스냅샷에 없던 파일은 건드리지 않는다.
Assert-NlGameNotRunning
$gameDir = Get-NlGameDir
$root = Get-NlDataSnapshotDir
$count = 0

if (Test-Path -LiteralPath $root) {
    foreach ($f in Get-ChildItem -LiteralPath $root -Recurse -File) {
        $rel = $f.FullName.Substring($root.Length + 1)
        $dst = Join-Path $gameDir $rel
        $want = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
        if ((Test-Path -LiteralPath $dst) -and ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -eq $want)) { continue }

        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
        Copy-Item -LiteralPath $f.FullName -Destination $dst -Force
        if ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -ne $want) { throw "복원 뒤 해시가 스냅샷과 다릅니다: $rel" }
        Write-Host "복원: $rel"
        $count++
    }
}
Write-Host "data restore ok ($count)"
```

- [ ] **Step 7: `tools/data-edit.ps1` 작성**

```powershell
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
```

- [ ] **Step 8: `tools/check-load.ps1`이 공용 함수를 쓰게 고친다**

사전 검사 부분을 다음 네 줄로 바꾼다. 바꾸는 범위는 `Assert-NlGameNotRunning` 줄부터, 로그를 지우는 줄(`if (Test-Path -LiteralPath $log) { Remove-Item … }`)까지다. 그 사이의 `NlToyBox.dll` 검사, `Patched` 검사, `foreach ($rel in 'mods\Native\AurieCore.dll' …)` 블록이 모두 `Assert-NlReadyToLaunch`로 들어간다.

```powershell
Assert-NlReadyToLaunch
$gameDir = Get-NlGameDir
$log = Join-Path $gameDir 'mods\Aurie\NlToyBox.log'
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log -Force }
```

게임을 끄는 부분을 바꾼다.

```powershell
if (-not $KeepRunning) {
    $how = Stop-NlGame
    if ($how -ne 'not-running') { Write-Host "게임 종료: $how" }
}
```

- [ ] **Step 9: `tools/restore-game.ps1`의 `$ours`에 두 줄을 더한다** (`'mods\Aurie\NlToyBox.log'` 줄 뒤, 그 줄 끝에 쉼표를 붙인다)

```powershell
    'mods\Aurie\NlToyBox.probe.txt',
    'mods\Aurie\NlToyBox.dump.json'
```

- [ ] **Step 10: 시험이 통과하는지 본다**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\tests\safety.tests.ps1 2>&1 | Select-Object -Last 12; "exit=$LASTEXITCODE"`

Expected: `ok -` 열 줄, `safety tests: 10 passed`, `exit=0`. 이어서 `pwsh -NoProfile -File E:\NlToyBox\tools\game-status.ps1`이 `바닐라`, `mods\ : (없음)`, 진짜 백업 하나를 보여야 한다(시험이 진짜 게임을 건드리지 않았다). `Get-ChildItem E:\NlToyBox\backups`에 `data` 폴더가 없거나 비어 있어야 한다.

- [ ] **Step 11: 커밋**

```powershell
git -C E:\NlToyBox branch --show-current   # feat/overlay-stage0
git -C E:\NlToyBox add -- tools/common.ps1 tools/data-snapshot.ps1 tools/data-restore.ps1 tools/data-edit.ps1 tools/check-load.ps1 tools/restore-game.ps1 tools/tests/safety.tests.ps1
git -C E:\NlToyBox commit -m "feat(tools): 데이터 파일 스냅샷·수정·복원, 게임 정상 종료" -m "data-snapshot/data-edit/data-restore 와 공용 함수(Assert-NlReadyToLaunch, Stop-NlGame). check-load 는 WM_CLOSE 로 정상 종료시킨 뒤에만 강제 종료한다." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: 모듈 덤프 기능

**Files:**
- Create: `E:\NlToyBox\src\Dump.hpp`, `E:\NlToyBox\src\Dump.cpp`
- Modify: `E:\NlToyBox\src\ModuleMain.cpp`, `E:\NlToyBox\CMakeLists.txt`

**Interfaces:**
- Consumes: YYToolkit v5 인터페이스(`external\YYToolkit\YYToolkit\source\YYTK\Shared\YYTK_Shared_Interface.hpp`): `GetGlobalInstance`, `EnumInstanceMembers(RValue, std::function<bool(const char*, RValue*)>)`(콜백이 거짓을 돌려줘야 다음 멤버로 간다), `CallBuiltinEx`, `CallGameScriptEx(RValue&, std::string_view, CInstance*, CInstance*, const std::vector<RValue>&)`. `RValue`: `IsStruct`, `IsArray`, `IsString`, `IsNumberConvertible`, `ToDouble`, `ToString`, `ToVector`, `ToObject`, `GetKindName`.
- Produces:
  - 요청 파일 `<모듈 폴더>\NlToyBox.probe.txt`: 줄마다 `delay_seconds=<초>`, `script=<이름>[|<인자>…]`, `find=<수>`, `find_name=<멤버 이름>`. `#` 줄은 주석.
  - 덤프 `<모듈 폴더>\NlToyBox.dump.json`:
    ```
    { "module_dump": 1, "delay_seconds": n, "elapsed_seconds": x, "global_status": "AURIE_…",
      "globals": { "<이름>": {"kind": "<형>", "value": …} | {"kind":"struct","members":{…}} | {"kind":"array","length":n} },
      "scripts": [ {"name": "…", "args": […], "status": "AURIE_…", "result": {"kind":…,"value":…}} ],
      "find": { "visited": n, "truncated": false,
                "hits": [ {"path": "…", "why": "value"|"name", "kind": "…", "value": …, "members": {…}} ] } }
    ```
  - 로그 줄 `dump requested: delay <n>s, scripts <n>, find <n>`와 `dump done`

- [ ] **Step 1: `src/Dump.hpp` 작성**

```cpp
#pragma once
// 요청 파일(NlToyBox.probe.txt)이 있을 때만 도는 덤프. 읽기만 한다. 형식은 스펙 §3.2.

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>

namespace NlDump
{
	// 모듈 폴더에서 요청 파일을 읽는다. 없으면 아무 일도 하지 않는다.
	void Init(YYTK::YYTKInterface* Yytk, const Aurie::fs::path& ModuleDir, std::function<void(const std::string&)> Log);

	// 게임 스레드의 콜백에서 매번 부른다. 요청이 있고 기다릴 시간이 지났으면 한 번 덤프한다.
	void Tick();
}
```

- [ ] **Step 2: `src/Dump.cpp` 작성**

```cpp
#include "Dump.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <unordered_set>
#include <vector>

using namespace Aurie;
using namespace YYTK;

namespace
{
	struct ScriptCall
	{
		std::string Name;
		std::vector<std::string> Args;
	};

	constexpr size_t k_MaxString = 200;		// 문자열 값은 이 바이트에서 자른다
	constexpr int k_MaxDepth = 6;			// 찾기가 내려가는 깊이
	constexpr size_t k_MaxVisited = 400000;	// 찾기가 방문하는 값의 수
	constexpr double k_MaxArray = 2048;		// 이보다 긴 배열은 들어가지 않는다
	constexpr size_t k_MaxHits = 300;

	YYTKInterface* g_Yytk = nullptr;
	std::function<void(const std::string&)> g_Log;
	fs::path g_DumpPath;
	std::vector<ScriptCall> g_Scripts;
	std::vector<double> g_FindValues;
	std::vector<std::string> g_FindNames;
	int g_DelaySeconds = 60;
	std::chrono::steady_clock::time_point g_Start;
	std::atomic<bool> g_Active = false;
	std::atomic<bool> g_Done = false;

	std::string Trim(const std::string& Text)
	{
		const size_t begin = Text.find_first_not_of(" \t\r\n");
		if (begin == std::string::npos)
			return "";
		return Text.substr(begin, Text.find_last_not_of(" \t\r\n") - begin + 1);
	}

	// JSON 문자열로 쓴다. 길면 UTF-8 글자 경계에서 자르고, 잘못된 바이트는 '?' 로 바꾼다.
	std::string Quote(std::string Text)
	{
		if (Text.size() > k_MaxString)
		{
			size_t cut = k_MaxString;
			while (cut > 0 && (static_cast<unsigned char>(Text[cut]) & 0xC0) == 0x80)
				cut--;
			Text.resize(cut);
			Text += "...";
		}

		std::string out = "\"";
		for (size_t i = 0; i < Text.size();)
		{
			const unsigned char c = static_cast<unsigned char>(Text[i]);
			const size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
			bool valid = len != 0 && i + len <= Text.size();
			for (size_t k = 1; valid && k < len; k++)
				valid = (static_cast<unsigned char>(Text[i + k]) & 0xC0) == 0x80;

			if (!valid) { out += '?'; i++; continue; }
			if (len > 1) { out.append(Text, i, len); i += len; continue; }

			switch (c)
			{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20) { char buf[8]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
				else out += static_cast<char>(c);
			}
			i++;
		}
		return out + "\"";
	}

	std::string Number(double Value)
	{
		if (!std::isfinite(Value))
			return Value != Value ? "\"nan\"" : (Value > 0 ? "\"inf\"" : "\"-inf\"");
		char buf[40];
		snprintf(buf, sizeof(buf), "%.17g", Value);
		return buf;
	}

	// 배열 길이. 못 얻으면 음수.
	double ArrayLength(const RValue& Value, CInstance* Global)
	{
		RValue length;
		const AurieStatus status = g_Yytk->CallBuiltinEx(length, "array_length", Global, Global, { Value });
		return AurieSuccess(status) ? length.ToDouble() : -1;
	}

	// 값 하나를 "kind":…[,"value":…] 로 적는다(중괄호 없이). 구조체와 배열의 속은 보지 않는다.
	std::string Describe(const RValue& Value, CInstance* Global)
	{
		if (Value.IsStruct())
			return "\"kind\":\"struct\"";
		if (Value.IsArray())
			return "\"kind\":\"array\",\"length\":" + Number(ArrayLength(Value, Global));
		if (Value.IsString())
			return "\"kind\":\"string\",\"value\":" + Quote(Value.ToString());
		if (Value.IsNumberConvertible())
			return "\"kind\":" + Quote(Value.GetKindName()) + ",\"value\":" + Number(Value.ToDouble());
		return "\"kind\":" + Quote(Value.GetKindName());
	}

	// 구조체의 멤버를 "이름":{…},… 로 적는다. Expand 면 구조체 멤버를 한 단계 더 편다.
	void WriteMembers(std::ostream& Out, const RValue& Object, CInstance* Global, bool Expand, bool FlushEach)
	{
		bool first = true;
		g_Yytk->EnumInstanceMembers(Object, [&](const char* Name, RValue* Value) -> bool
		{
			Out << (first ? "" : ",") << "\n" << Quote(Name ? Name : "") << ":{";
			first = false;

			if (!Value)
				Out << "\"kind\":\"error\"";
			else if (Expand && Value->IsStruct())
			{
				Out << "\"kind\":\"struct\",\"members\":{";
				WriteMembers(Out, *Value, Global, false, false);
				Out << "}";
			}
			else
				Out << Describe(*Value, Global);

			Out << "}";
			if (FlushEach)
				Out.flush();	// 도중에 죽어도 어디까지 왔는지 남는다
			return false;		// 거짓을 돌려줘야 다음 멤버로 넘어간다
		});
	}

	struct Finder
	{
		std::ostream& Out;
		CInstance* Global;
		std::unordered_set<const void*> Seen;
		size_t Visited = 0;
		size_t Hits = 0;
		bool Truncated = false;

		void Hit(const std::string& Path, const char* Why, const RValue& Value)
		{
			if (Hits >= k_MaxHits) { Truncated = true; return; }
			Out << (Hits == 0 ? "" : ",") << "\n{\"path\":" << Quote(Path) << ",\"why\":\"" << Why << "\"," << Describe(Value, Global);
			if (Value.IsStruct())
			{
				Out << ",\"members\":{";
				WriteMembers(Out, Value, Global, false, false);
				Out << "}";
			}
			Out << "}";
			Out.flush();
			Hits++;
		}

		void Walk(const RValue& Value, const std::string& Path, const std::string& Name, int Depth)
		{
			if (Truncated)
				return;
			if (++Visited > k_MaxVisited) { Truncated = true; return; }

			for (const std::string& wanted : g_FindNames)
				if (Name == wanted)
					Hit(Path, "name", Value);

			if (Value.IsStruct())
			{
				const void* identity = Value.ToObject();
				if (Depth >= k_MaxDepth || !identity || !Seen.insert(identity).second)
					return;
				g_Yytk->EnumInstanceMembers(Value, [&](const char* MemberName, RValue* Member) -> bool
				{
					const std::string name = MemberName ? MemberName : "";
					if (Member)
						Walk(*Member, Path + "." + name, name, Depth + 1);
					return false;
				});
				return;
			}

			if (Value.IsArray())
			{
				const double length = ArrayLength(Value, Global);
				if (Depth >= k_MaxDepth || length <= 0 || length > k_MaxArray)
					return;
				const std::vector<RValue> items = Value.ToVector();
				for (size_t i = 0; i < items.size(); i++)
					Walk(items[i], Path + "[" + std::to_string(i) + "]", "", Depth + 1);
				return;
			}

			if (Value.IsString() || !Value.IsNumberConvertible())
				return;
			const double number = Value.ToDouble();
			for (const double wanted : g_FindValues)
				if (number == wanted)
					Hit(Path, "value", Value);
		}
	};

	void Run()
	{
		const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_Start).count();

		std::ofstream out(g_DumpPath, std::ios::trunc | std::ios::binary);
		out << "{\"module_dump\":1,\"delay_seconds\":" << g_DelaySeconds << ",\"elapsed_seconds\":" << Number(elapsed);

		CInstance* global_instance = nullptr;
		const AurieStatus global_status = g_Yytk->GetGlobalInstance(&global_instance);
		const bool have_global = AurieSuccess(global_status) && global_instance;
		out << ",\"global_status\":" << Quote(AurieStatusToString(global_status));

		out << ",\"globals\":{";
		if (have_global)
			WriteMembers(out, RValue(global_instance), global_instance, true, true);
		out << "\n}";

		out << ",\"scripts\":[";
		bool first = true;
		for (const ScriptCall& call : g_Scripts)
		{
			std::vector<RValue> args;
			std::string args_json;
			for (const std::string& arg : call.Args)
			{
				char* end = nullptr;
				const double number = std::strtod(arg.c_str(), &end);
				const bool numeric = !arg.empty() && end && *end == '\0';
				args.push_back(numeric ? RValue(number) : RValue(std::string_view(arg)));
				args_json += std::string(args_json.empty() ? "" : ",") + (numeric ? Number(number) : Quote(arg));
			}

			out << (first ? "" : ",") << "\n{\"name\":" << Quote(call.Name) << ",\"args\":[" << args_json << "]";
			out.flush();	// 호출이 게임을 죽이면 어느 스크립트였는지 남는다
			first = false;

			if (!have_global)
			{
				out << ",\"status\":\"no global instance\"}";
				continue;
			}

			RValue result;
			const AurieStatus status = g_Yytk->CallGameScriptEx(result, call.Name, global_instance, global_instance, args);
			out << ",\"status\":" << Quote(AurieStatusToString(status));
			if (AurieSuccess(status))
				out << ",\"result\":{" << Describe(result, global_instance) << "}";
			out << "}";
			out.flush();
		}
		out << "\n]";

		out << ",\"find\":{\"hits\":[";
		Finder finder{ out, global_instance };
		if (have_global && (!g_FindValues.empty() || !g_FindNames.empty()))
			finder.Walk(RValue(global_instance), "global", "", 0);
		out << "\n],\"visited\":" << finder.Visited << ",\"truncated\":" << (finder.Truncated ? "true" : "false") << "}";

		out << "}\n";
		out.close();

		g_Log("dump done");
	}
}

void NlDump::Init(YYTKInterface* Yytk, const fs::path& ModuleDir, std::function<void(const std::string&)> Log)
{
	std::ifstream in(ModuleDir / "NlToyBox.probe.txt");
	if (!in.is_open())
		return;

	g_Yytk = Yytk;
	g_Log = std::move(Log);
	g_DumpPath = ModuleDir / "NlToyBox.dump.json";
	g_Start = std::chrono::steady_clock::now();

	std::string line;
	while (std::getline(in, line))
	{
		line = Trim(line);
		const size_t eq = line.find('=');
		if (line.empty() || line[0] == '#' || eq == std::string::npos)
			continue;

		const std::string key = Trim(line.substr(0, eq));
		const std::string value = Trim(line.substr(eq + 1));

		if (key == "delay_seconds")
		{
			const int parsed = std::atoi(value.c_str());
			if (parsed >= 0)
				g_DelaySeconds = parsed;
		}
		else if (key == "find")
			g_FindValues.push_back(std::strtod(value.c_str(), nullptr));
		else if (key == "find_name" && !value.empty())
			g_FindNames.push_back(value);
		else if (key == "script")
		{
			// 이름|인자|인자…
			ScriptCall call;
			size_t begin = 0;
			while (true)
			{
				const size_t bar = value.find('|', begin);
				const std::string part = Trim(value.substr(begin, bar == std::string::npos ? std::string::npos : bar - begin));
				if (begin == 0)
					call.Name = part;
				else
					call.Args.push_back(part);
				if (bar == std::string::npos)
					break;
				begin = bar + 1;
			}
			if (!call.Name.empty())
				g_Scripts.push_back(std::move(call));
		}
	}

	g_Log("dump requested: delay " + std::to_string(g_DelaySeconds) + "s, scripts " + std::to_string(g_Scripts.size())
		+ ", find " + std::to_string(g_FindValues.size() + g_FindNames.size()));
	g_Active = true;
}

void NlDump::Tick()
{
	if (!g_Active.load(std::memory_order_relaxed) || g_Done.load(std::memory_order_relaxed))
		return;
	if (std::chrono::steady_clock::now() - g_Start < std::chrono::seconds(g_DelaySeconds))
		return;
	if (g_Done.exchange(true))
		return;

	Run();
}
```

- [ ] **Step 3: `src/ModuleMain.cpp` 수정**

`#include <YYTK_Shared.hpp>` 다음 줄에 넣는다.

```cpp
#include "Dump.hpp"
```

버전을 올린다: `constexpr const char* k_Version = "0.0.1";` → `"0.1.0"`.

`CodeCallback`을 바꾼다.

```cpp
	// 오브젝트 이벤트 코드가 실행될 때마다 온다. 래퍼의 내용은 쓰지 않는다.
	void CodeCallback(FWCodeEvent& CodeContext)
	{
		UNREFERENCED_PARAMETER(CodeContext);
		ProbeOnce("object_call");
		NlDump::Tick();
	}
```

`ModuleInitialize`의 마지막 `return AURIE_SUCCESS;` 바로 앞에 넣는다.

```cpp
	// 요청 파일이 있을 때만 덤프를 준비한다 (스펙: 데이터 오버레이 §3.2).
	NlDump::Init(g_Yytk, module_dir, [](const std::string& Line) { LogLine(Line); });
```

- [ ] **Step 4: `CMakeLists.txt`의 `add_library`에 소스를 더한다**

```cmake
add_library(nltoybox SHARED
  src/ModuleMain.cpp
  src/Dump.cpp
  "${YYTK_SHARED}/YYTK_Shared_Types.cpp"
)
```

- [ ] **Step 5: 빌드**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1 2>&1 | Select-Object -Last 6; "exit=$LASTEXITCODE"`

Expected: `build ok -> E:\NlToyBox\build\NlToyBox.dll`, `exit=0`, 경고·오류 줄 없음.

컴파일 오류가 나면 오류가 가리키는 호출을 `external\YYToolkit\YYToolkit\source\YYTK\Shared\YYTK_Shared_Interface.hpp`·`YYTK_Shared_Types.hpp`의 선언과 대조해 `src/Dump.cpp`를 고친다. 서브모듈은 고치지 않는다. 덤프의 출력 형식(Interfaces의 JSON)은 바꾸지 않는다.

- [ ] **Step 6: 커밋**

```powershell
git -C E:\NlToyBox branch --show-current
git -C E:\NlToyBox add -- src/Dump.hpp src/Dump.cpp src/ModuleMain.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "feat(module): 요청 파일이 있을 때 런타임 덤프" -m "전역 변수 목록, 스크립트 호출 결과, 값·이름으로 찾은 경로를 NlToyBox.dump.json 에 쓴다. 읽기만 한다. 요청 파일이 없으면 Phase 0 과 같게 동작한다." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: 덤프를 받아 오는 도구와 분석 스크립트

**Files:**
- Create: `E:\NlToyBox\tools\probe.ps1`
- Create: `E:\NlToyBox\tools\probes\stage0-run1.txt`, `stage0-run2.txt`
- Create: `E:\NlToyBox\tools\re\dump_tool.py`
- Modify: `E:\NlToyBox\tools\tests\safety.tests.ps1` (시험 하나)

**Interfaces:**
- Consumes: `Assert-NlReadyToLaunch`, `Stop-NlGame`, `Get-NlGameDir`, `Test-NlGameRunning`, `$script:NlAppId` (Task 1). 요청 파일 형식과 덤프 형식, 로그 줄 `dump done` (Task 2)
- Produces:
  - `pwsh -File tools/probe.ps1 -Request <파일> -Out <파일> [-TimeoutSec 240]` → 끝줄 `PASS`(종료 코드 0) 또는 `FAIL: …`(1). 끝나면 게임은 꺼져 있고 요청 파일과 덤프는 게임 폴더에 없다
  - `py -3.14 tools/re/dump_tool.py summary <덤프>` / `find <덤프> <낱말 또는 수>…` / `diff <덤프 A> <덤프 B>`

- [ ] **Step 1: 실패하는 시험을 먼저 쓴다** (`safety.tests.ps1`, Task 1에서 넣은 시험들 뒤)

```powershell
    Test-Case 'probe 는 켜는 데 실패해도 요청 파일을 게임 폴더에 남기지 않는다' {
        $req = Join-Path $fake 'req.txt'
        [IO.File]::WriteAllText($req, "delay_seconds=1`n")
        $r = Invoke-Tool 'probe.ps1' "-Request '$req' -Out '$(Join-Path $fake 'out.json')' -TimeoutSec 4" $noLaunch
        Assert-True ($r.Out -match 'LAUNCH-ATTEMPTED') "사전 검사를 지나 켜는 단계에 닿아야 한다`n$($r.Out)"
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $fake 'mods\Aurie\NlToyBox.probe.txt'))) '요청 파일이 남으면 안 된다'
    }
```

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\tests\safety.tests.ps1 2>&1 | Select-Object -Last 5; "exit=$LASTEXITCODE"`
Expected: 열 개 `ok` 뒤에 `사전 검사를 지나 켜는 단계에 닿아야 한다 … condition false`(스크립트가 없어 `LAUNCH-ATTEMPTED`가 나오지 않는다), `exit=1`.

- [ ] **Step 2: `tools/probe.ps1` 작성**

```powershell
param(
    [Parameter(Mandatory)][string]$Request,
    [Parameter(Mandatory)][string]$Out,
    [int]$TimeoutSec = 240
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 요청 파일을 놓고 게임을 켜서 모듈의 덤프를 받아 온다. 끝나면 게임을 끄고 요청 파일과 덤프를 게임 폴더에서 지운다.
Assert-NlReadyToLaunch
if (-not (Test-Path -LiteralPath $Request -PathType Leaf)) { throw "요청 파일이 없습니다: $Request" }
$Out = [IO.Path]::GetFullPath($Out)

$modDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
$reqDst = Join-Path $modDir 'NlToyBox.probe.txt'
$dump = Join-Path $modDir 'NlToyBox.dump.json'
$log = Join-Path $modDir 'NlToyBox.log'
foreach ($f in $dump, $log) { if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force } }

$done = $false
try {
    Copy-Item -LiteralPath $Request -Destination $reqDst -Force
    Write-Host "게임을 켭니다 (steam://rungameid/$($script:NlAppId)). 게임 창을 누르지 마세요."
    Start-Process "steam://rungameid/$($script:NlAppId)"

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $seenProcess = $false
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        if (Test-NlGameRunning) { $seenProcess = $true }
        elseif ($seenProcess) { Write-Host '게임 프로세스가 사라졌습니다.'; break }
        if ((Test-Path -LiteralPath $log) -and (@(Get-Content -LiteralPath $log -ErrorAction SilentlyContinue) -contains 'dump done')) { $done = $true; break }
    }
    if (-not $seenProcess) { Write-Host '게임 프로세스를 한 번도 보지 못했습니다.' }

    Write-Host '--- NlToyBox.log ---'
    if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log | ForEach-Object { Write-Host $_ } } else { Write-Host '(로그 없음)' }
    Write-Host '--------------------'

    # 반쯤 쓰인 덤프도 가져온다. 어디서 멈췄는지가 증거다.
    if (Test-Path -LiteralPath $dump) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Out) | Out-Null
        Copy-Item -LiteralPath $dump -Destination $Out -Force
        Write-Host "덤프: $Out ($((Get-Item -LiteralPath $Out).Length) B)"
    }
}
finally {
    Write-Host "게임 종료: $(Stop-NlGame)"
    # 요청 파일이 남으면 평소 플레이 때도 덤프가 돈다. 어떤 경로로 끝나든 지운다.
    foreach ($f in $reqDst, $dump) { if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force } }
}

if (-not $done) { Write-Host 'FAIL: dump done 을 보지 못했습니다.'; exit 1 }
Write-Host 'PASS'
exit 0
```

- [ ] **Step 3: 요청 파일 작성**

`tools/probes/stage0-run1.txt`:

```
# 단계 0 실행 1: 바닐라. 원래 값과 이름으로 찾는다.
delay_seconds=60
script=gml_Script_budget_default_money_get
script=gml_Script_resource_default_count_get|wood
find=2000
find=700
find_name=budget_money
find_name=default_budget_money
find_name=initial_budget
find_name=ai_economy
find_name=battle_dodge_base
find_name=death_chance_mult
find_name=production_cost
find_name=building_resources
find_name=product_count
find_name=game_speed_slower_div
find_name=gameplay_hour_frames
find_name=farm_pig_grow_duration
find_name=mill_rye_flour_yield
```

`tools/probes/stage0-run2.txt`: 위와 같되 첫 줄 주석을 `# 단계 0 실행 2: 세 값을 바꾼 뒤. 바꾼 값(2345, 745)을 찾는다.`로, `find=2000`과 `find=700`을 `find=2345`와 `find=745`로 바꾼다. 나머지 줄은 그대로 둔다.

- [ ] **Step 4: 시험이 통과하는지 본다 (Review Focus 1)**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\tests\safety.tests.ps1 2>&1 | Select-Object -Last 3; "exit=$LASTEXITCODE"; "게임 켜짐 = " + [bool](Get-Process Norland -ErrorAction SilentlyContinue)`

Expected: `safety tests: 11 passed`, `exit=0`, `게임 켜짐 = False`.

- [ ] **Step 5: `tools/re/dump_tool.py` 작성**

```python
"""모듈 덤프(NlToyBox.dump.json)를 '경로 → 값' 표로 펴서 요약하고, 찾고, 비교한다.

사용:
  py -3.14 dump_tool.py summary <덤프>
  py -3.14 dump_tool.py find <덤프> <낱말 또는 수>...
  py -3.14 dump_tool.py diff <덤프 A> <덤프 B>
"""
import collections
import json
import sys


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def flatten(dump):
    """전역과 스크립트 결과를 {경로: 값} 으로 편다. 값이 없는 것은 '<형>' 으로 적는다."""
    flat = {}

    def put(path, node):
        kind = node.get("kind")
        if kind == "struct":
            flat[path] = "<struct>"
            for key, child in node.get("members", {}).items():
                put(f"{path}.{key}", child)
        elif kind == "array":
            flat[path] = f"<array {node.get('length')}>"
        elif "value" in node:
            flat[path] = node["value"]
        else:
            flat[path] = f"<{kind}>"

    for name, node in dump.get("globals", {}).items():
        put(f"global.{name}", node)
    for call in dump.get("scripts", []):
        key = f"script:{call['name']}({','.join(str(a) for a in call.get('args', []))})"
        if call.get("status") == "AURIE_SUCCESS":
            put(key, call.get("result", {"kind": "none"}))
        else:
            flat[key] = f"<{call.get('status')}>"
    return flat


def summary(dump):
    kinds = collections.Counter(node.get("kind") for node in dump.get("globals", {}).values())
    print(f"elapsed_seconds={dump.get('elapsed_seconds')} global_status={dump.get('global_status')}")
    print(f"globals={len(dump.get('globals', {}))} " + " ".join(f"{k}={n}" for k, n in kinds.most_common()))
    for call in dump.get("scripts", []):
        print(f"script {call['name']}({call.get('args')}) -> {call.get('status')} {call.get('result')}")
    found = dump.get("find", {})
    print(f"find: visited={found.get('visited')} truncated={found.get('truncated')} hits={len(found.get('hits', []))}")
    for hit in found.get("hits", []):
        members = hit.get("members")
        extra = f" members={list(members)[:40]}" if members is not None else ""
        print(f"  [{hit.get('why')}] {hit.get('path')} = {hit.get('value', '<' + str(hit.get('kind')) + '>')}{extra}")


def find(dump, terms):
    flat = flatten(dump)
    for term in terms:
        try:
            number = float(term)
        except ValueError:
            number = None
        print(f"--- {term} ---")
        shown = 0
        for path, value in flat.items():
            by_value = number is not None and isinstance(value, (int, float)) and not isinstance(value, bool) and value == number
            by_name = number is None and term.lower() in path.lower()
            if by_value or by_name:
                print(f"  {path} = {value}")
                shown += 1
                if shown >= 60:
                    print("  ... (60개에서 끊음)")
                    break
        if shown == 0:
            print("  (없음)")


def diff(a, b):
    fa, fb = flatten(a), flatten(b)
    changed = [(p, fa[p], fb[p]) for p in fa if p in fb and fa[p] != fb[p]]
    print(f"changed={len(changed)} only_a={len(fa.keys() - fb.keys())} only_b={len(fb.keys() - fa.keys())}")
    for path, va, vb in changed[:200]:
        print(f"  {path}: {va} -> {vb}")
    for path in sorted(fb.keys() - fa.keys())[:50]:
        print(f"  + {path} = {fb[path]}")
    for path in sorted(fa.keys() - fb.keys())[:50]:
        print(f"  - {path} = {fa[path]}")


def main(argv):
    if len(argv) >= 3 and argv[1] == "summary":
        summary(load(argv[2]))
    elif len(argv) >= 4 and argv[1] == "find":
        find(load(argv[2]), argv[3:])
    elif len(argv) == 4 and argv[1] == "diff":
        diff(load(argv[2]), load(argv[3]))
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
```

- [ ] **Step 6: 분석 스크립트를 손으로 만든 덤프로 확인**

Run:
```powershell
$t = Join-Path $env:TEMP 'nl-dump-a.json'; $u = Join-Path $env:TEMP 'nl-dump-b.json'
'{"elapsed_seconds":61,"global_status":"AURIE_SUCCESS","globals":{"g1":{"kind":"number","value":2000},"s":{"kind":"struct","members":{"m":{"kind":"number","value":700}}},"a":{"kind":"array","length":3}},"scripts":[{"name":"x","args":[],"status":"AURIE_SUCCESS","result":{"kind":"number","value":2000}}],"find":{"hits":[{"path":"global.s.m","why":"value","kind":"number","value":700}],"visited":4,"truncated":false}}' | Set-Content -LiteralPath $t -Encoding utf8
(Get-Content -LiteralPath $t -Raw).Replace('"value":2000}', '"value":2345}') | Set-Content -LiteralPath $u -Encoding utf8
py -3.14 E:\NlToyBox\tools\re\dump_tool.py summary $t
py -3.14 E:\NlToyBox\tools\re\dump_tool.py find $t 700 g1
py -3.14 E:\NlToyBox\tools\re\dump_tool.py diff $t $u
```
Expected:
- summary: `globals=3 number=1 struct=1 array=1`, `script x([]) -> AURIE_SUCCESS …`, `find: visited=4 truncated=False hits=1`, `  [value] global.s.m = 700`
- find: `--- 700 ---` 아래 `  global.s.m = 700`, `--- g1 ---` 아래 `  global.g1 = 2000`
- diff: `changed=2 only_a=0 only_b=0`, `  global.g1: 2000 -> 2345`, `  script:x(): 2000 -> 2345`

확인 뒤 두 임시 파일을 지운다.

- [ ] **Step 7: 커밋**

```powershell
git -C E:\NlToyBox branch --show-current
git -C E:\NlToyBox add -- tools/probe.ps1 tools/probes/stage0-run1.txt tools/probes/stage0-run2.txt tools/re/dump_tool.py tools/tests/safety.tests.ps1
git -C E:\NlToyBox commit -m "feat(tools): 덤프를 받아 오는 probe 와 분석 스크립트" -m "probe.ps1 은 요청 파일을 놓고 게임을 켜서 덤프를 받아 온 뒤 정상 종료시키고, 어떤 경로로 끝나든 요청 파일을 지운다. dump_tool.py 는 덤프를 요약·검색·비교한다." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: 실행 1 — 바닐라 기준 덤프

**Files:** 없음 (산출물은 `refs\runtime\dump-vanilla.json`, 추적 안 함)

**Interfaces:**
- Consumes: Task 1~3의 도구 전부, `setup-aurie.ps1`, `deploy.ps1`
- Produces: `E:\NlToyBox\refs\runtime\dump-vanilla.json`과, Task 5·6·7이 쓰는 관찰 기록(아래 Step 4의 질문에 대한 답)

- [ ] **Step 1: [게임 변경] 스냅샷, 설치, 배포**

배열 인자는 `pwsh -File`로 넘어가지 않으므로 스냅샷은 같은 세션에서 직접 부른다.

```powershell
& E:\NlToyBox\tools\data-snapshot.ps1 -Files debug_params.json, gameplay_variables.json, battle_params.json
pwsh -NoProfile -File E:\NlToyBox\tools\setup-aurie.ps1 | Select-Object -Last 1; "setup exit=$LASTEXITCODE"
pwsh -NoProfile -File E:\NlToyBox\tools\deploy.ps1 | Select-Object -Last 1; "deploy exit=$LASTEXITCODE"
```
Expected: `스냅샷:` 세 줄, `setup ok -> …`, `deployed -> …`, 종료 코드 모두 0.

- [ ] **Step 2: [게임 실행 1/3] 덤프**

켜기 전에 알린다: "게임을 한 번 켭니다(약 2분). 끝날 때까지 게임 창을 누르지 말아 주세요."

```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\probe.ps1 -Request E:\NlToyBox\tools\probes\stage0-run1.txt -Out E:\NlToyBox\refs\runtime\dump-vanilla.json; "probe exit=$LASTEXITCODE"
```
(도구의 시간 제한은 300초 이상으로 준다.)

Expected: 로그에 `NlToyBox 0.1.0 loaded`, `yytk 5.0.0`, `trigger object_call`, `builtin code_is_compiled = true`, `script gml_Script_command_line_parameters_init = found`, `probe done`, `dump requested: delay 60s, scripts 2, find 15`, `dump done`. 이어서 `덤프: …`, `게임 종료: closed`, `PASS`, `probe exit=0`.

**`FAIL`이면**: 로그와 가져온 덤프(반쯤 쓰였을 수 있다)의 끝 20줄을 본다.
- 덤프가 어느 전역에서 멈췄으면(게임이 죽었다) 그 이름을 기록하고 멈춘다. 이것이 Review Focus 5의 실측이다. 요청 파일이 게임 폴더에 남지 않았는지 확인한다.
- `dump requested`가 없으면 요청 파일이 모듈 폴더에 놓이지 않은 것이다.
- `dump done`만 없고 게임이 살아 있었으면 덤프가 오래 걸린 것이다.
어느 경우든 여기서 멈추고 사용자에게 증거와 함께 보고한다. 게임을 더 켜려면 다시 승인받는다.

- [ ] **Step 3: 덤프 요약**

```powershell
py -3.14 E:\NlToyBox\tools\re\dump_tool.py summary E:\NlToyBox\refs\runtime\dump-vanilla.json
```
Expected: `global_status=AURIE_SUCCESS`, `globals=`가 1 이상, 스크립트 두 줄, `find:` 줄과 hit 목록. JSON을 읽지 못하면(덤프가 깨졌다) 깨진 자리의 앞뒤 200바이트를 보고 `src/Dump.cpp`의 `Quote`·`Describe`를 고친 뒤 멈추고 보고한다(다시 켜야 한다).

- [ ] **Step 4: 관찰을 적어 둔다** (Task 7의 조사 기록에 그대로 들어간다)

요약 출력에서 다음 다섯 가지를 읽어 답을 적는다. 답이 "없음"이어도 그대로 적는다.

1. `gml_Script_budget_default_money_get`의 반환값. (2000이면 파일의 `budget_money`와 같다.)
2. `gml_Script_resource_default_count_get("wood")`의 반환값. (300이면 파일의 `product_count.wood`와 같다.)
3. `find=2000`과 `find=700`이 찾은 경로들.
4. `find_name`으로 찾은 이름마다: 찾았는가, 경로, 값(구조체면 멤버 이름들).
5. `find`의 `visited`와 `truncated`. `truncated`가 `true`이면 찾기가 다 돌지 못한 것이므로 "없음"을 "확인하지 못함"으로 읽는다.

---

### Task 5: 실행 2 — 세 값을 바꾼 뒤

**Files:** 없음 (산출물은 `refs\runtime\dump-changed.json`)

**Interfaces:**
- Consumes: `data-edit.ps1`, `probe.ps1`, `dump_tool.py`, Task 4의 덤프
- Produces: `refs\runtime\dump-changed.json`, 질문 2의 답(파일별)

- [ ] **Step 1: [게임 변경] 세 값을 바꾼다**

같은 PowerShell 세션 안에서 스크립트를 직접 부른다(따옴표가 든 인자를 `pwsh -File`로 넘기지 않는다).

```powershell
& E:\NlToyBox\tools\data-edit.ps1 -File debug_params.json       -Find '"budget_money": 2000'     -Replace '"budget_money": 2345'
& E:\NlToyBox\tools\data-edit.ps1 -File gameplay_variables.json -Find '"initial_budget": 700,'   -Replace '"initial_budget": 745,'
& E:\NlToyBox\tools\data-edit.ps1 -File battle_params.json      -Find '"battle_dodge_base": 20,' -Replace '"battle_dodge_base": 23,'
```
Expected: `수정:` 세 줄. "정확히 한 번 나와야 합니다"가 나오면 `Select-String -LiteralPath <게임 폴더>\<파일> -Pattern '<키 이름>'`으로 그 줄의 실제 글자를 보고, 줄에 있는 그대로의 `"키": 값`을 `-Find`에 쓴다.

- [ ] **Step 2: [게임 실행 2/3] 덤프**

```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\probe.ps1 -Request E:\NlToyBox\tools\probes\stage0-run2.txt -Out E:\NlToyBox\refs\runtime\dump-changed.json; "probe exit=$LASTEXITCODE"
```
Expected: `PASS`, `probe exit=0`, `게임 종료: closed`. `FAIL`이면 Task 4 Step 2의 실패 절차를 따른다. 게임이 바뀐 값 때문에 켜지지 않으면(로딩 중 오류) 그것도 결과다: 멈추고 보고한다.

- [ ] **Step 3: 비교**

```powershell
py -3.14 E:\NlToyBox\tools\re\dump_tool.py summary E:\NlToyBox\refs\runtime\dump-changed.json
py -3.14 E:\NlToyBox\tools\re\dump_tool.py diff E:\NlToyBox\refs\runtime\dump-vanilla.json E:\NlToyBox\refs\runtime\dump-changed.json
```

- [ ] **Step 4: 질문 2의 답을 파일별로 정한다**

| 파일 | "반영된다"의 근거 (하나라도 성립하면) | 그렇지 않으면 |
|---|---|---|
| `debug_params.json` | `gml_Script_budget_default_money_get`이 2345를 돌려준다. 또는 `find=2345`가 경로를 찾았다 | 반환값이 2000 그대로이고 2345가 어디에도 없으면 "아니다". 스크립트 호출이 실패했고 찾기가 `truncated`이면 "확인하지 못함" |
| `gameplay_variables.json` | `find=745`가 경로를 찾았다. 또는 실행 1에서 700이던 `initial_budget` 경로의 값이 745다 | 745가 없고 `truncated`가 `false`이면 "아니다". `truncated`가 `true`이면 "확인하지 못함" |
| `battle_params.json` | `find_name=battle_dodge_base`가 찾은 경로의 값이 23이다. 또는 diff에 20 → 23으로 바뀐 경로가 있다 | 이름을 찾았고 값이 20 그대로면 "아니다". 이름을 못 찾았으면 "확인하지 못함"(전투 값은 전투가 시작될 때 읽힐 수 있다) |

값이 앉은 런타임 경로(예: `global.<…>.ai_economy.initial_budget`)를 파일의 키 경로와 나란히 적는다. 이것이 파일 키와 런타임 이름의 대응 규칙이다.

세 파일 모두 "아니다"이면 Task 6을 건너뛰고 Task 7로 간다.

---

### Task 6: 실행 3 — 파일에 없는 키를 추가 (조건부)

**Files:**
- Create (조건부): `E:\NlToyBox\tools\probes\stage0-run3.txt`

**Interfaces:**
- Consumes: Task 5의 덤프와 대응 규칙
- Produces: `refs\runtime\dump-added.json`, 질문 3의 답

- [ ] **Step 1: 추가할 키를 고른다**

Task 5에서 `gameplay_variables.json`이 "반영된다"가 아니었으면 이 Task를 하지 않는다. 질문 3의 답은 "확인하지 못함(파일 값이 런타임에서 보이지 않았다)"이다.

반영됐으면, 실행 2의 덤프에서 `find_name=ai_economy`가 찾은 구조체의 멤버 이름을 본다.

```powershell
py -3.14 E:\NlToyBox\tools\re\dump_tool.py summary E:\NlToyBox\refs\runtime\dump-changed.json | Select-String 'ai_economy'
. E:\NlToyBox\tools\common.ps1
Select-String -LiteralPath (Join-Path (Get-NlGameDir) 'gameplay_variables.json') -Pattern '"ai_economy"' -Context 0,14 | ForEach-Object { $_.Line; $_.Context.PostContext }
```

- 런타임 구조체에 있고 파일의 `ai_economy` 객체에 없는 멤버 가운데 값이 수인 것을 하나 고른다(이름순으로 첫 번째). 그 이름을 `M`, 지금 값을 `V`라 한다.
- 그런 멤버가 없으면: 덤프의 `find` 결과에서 `why`가 `name`이고 구조체인 다른 hit(`production_cost`, `building_resources`, `product_count`)에 대해 같은 비교를 하되 대상 파일은 `debug_params.json`으로 한다.
- 어디에도 없으면 이 Task를 끝낸다. 질문 3의 답은 "런타임 구조체가 파일과 같은 키만 가진다. 파일에 없는 exe 변수는 다른 곳에 산다"이다.

- [ ] **Step 2: [게임 변경] 키를 추가한다**

`ai_economy`의 경우(다른 구조체면 이름과 파일을 바꿔 같은 방법으로):

```powershell
$M = '<Step 1에서 고른 이름>'; $new = <V + 45>
& E:\NlToyBox\tools\data-edit.ps1 -File gameplay_variables.json -Find '"initial_budget": 745,' -Replace ('"initial_budget": 745, "{0}": {1},' -f $M, $new)
```
같은 객체 안, 이미 있는 키 바로 뒤에 넣는다. 그래야 JSON 구조가 유지된다.

요청 파일 `tools/probes/stage0-run3.txt`를 `stage0-run2.txt`를 복사해 만들고, 끝에 두 줄을 더한다: `find=<new>`, `find_name=<M>`.

- [ ] **Step 3: [게임 실행 3/3] 덤프**

```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\probe.ps1 -Request E:\NlToyBox\tools\probes\stage0-run3.txt -Out E:\NlToyBox\refs\runtime\dump-added.json; "probe exit=$LASTEXITCODE"
py -3.14 E:\NlToyBox\tools\re\dump_tool.py summary E:\NlToyBox\refs\runtime\dump-added.json
```

- [ ] **Step 4: 질문 3의 답을 정한다**

- `M`의 런타임 값이 `new`이면 "그렇다": 파일은 코드 기본값 위의 덮어쓰기 층이고, 키를 추가하면 읽힌다.
- `M`의 값이 `V` 그대로면 "아니다(이 구조체에서는)".
- 게임이 켜지지 않거나 로딩에서 오류가 났으면 "아니다. 모르는 키는 로딩을 깨뜨린다"와 그 증거(콘솔·`aurie.log`·`%LOCALAPPDATA%\Strategy\catched_errors_*.txt`의 새 줄).

요청 파일을 만들었으면 커밋한다.

```powershell
git -C E:\NlToyBox add -- tools/probes/stage0-run3.txt
git -C E:\NlToyBox commit -m "chore(tools): 단계 0 실행 3 의 요청 파일" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: 복원, 기록, 병합

**Files:**
- Create: `E:\NlToyBox\research\01-data-overlay.md`
- Modify: `E:\NlToyBox\CLAUDE.md`, `E:\NlToyBox\docs\superpowers\specs\2026-10-04-data-overlay-design.md` (§4 머리의 한 줄)

**Interfaces:**
- Consumes: Task 4 Step 4, Task 5 Step 4, Task 6 Step 4의 답
- Produces: 조사 기록, `develop`에 병합된 `feat/overlay-stage0`

- [ ] **Step 1: [게임 변경] 바닐라로 되돌리고 확인한다 (스펙 §3.6의 4번)**

```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\data-restore.ps1;  "data-restore exit=$LASTEXITCODE"
pwsh -NoProfile -File E:\NlToyBox\tools\restore-game.ps1;  "restore-game exit=$LASTEXITCODE"
pwsh -NoProfile -File E:\NlToyBox\tools\game-status.ps1
. E:\NlToyBox\tools\common.ps1
$g = Get-NlGameDir; $s = Get-NlDataSnapshotDir
foreach ($f in 'debug_params.json', 'gameplay_variables.json', 'battle_params.json') {
    "{0,-26} 스냅샷과 같음 = {1}" -f $f, ((Get-FileHash -LiteralPath (Join-Path $g $f)).Hash -eq (Get-FileHash -LiteralPath (Join-Path $s $f)).Hash)
}
"aurie.log 남음 = " + (Test-Path -LiteralPath (Join-Path $g 'aurie.log'))
```
Expected: `data restore ok (3)`(또는 바꾼 파일 수), `restore ok`, `exe SHA256 : 609C17CF…863E`, `바닐라`, `mods\ : (없음)`, 세 파일 모두 `스냅샷과 같음 = True`, `aurie.log 남음 = False`.

- [ ] **Step 2: `research/01-data-overlay.md` 작성**

아래 뼈대로 쓴다. 각 절의 내용은 괄호에 적힌 단계에서 본 것을 그대로 옮긴다. 본 것과 추정을 섞지 않는다.

```markdown
# 01 — 데이터 오버레이 단계 0 실측

조사일 2026-10-04. 대상: Norland `0.5588.9777.0`. 게임을 켠 횟수: (실제 횟수).
덤프 원본은 `refs/runtime/dump-*.json`에 있다(추적 안 함). 다시 재려면 `tools/probe.ps1`을 쓴다.

## 답

| 질문 | 답 | 근거 |
|---|---|---|
| 1. 런타임 전역 변수는 어떻게 생겼나 | (Task 4 Step 3: 전역 수, 형별 수) | `dump-vanilla.json` |
| 2a. `debug_params.json`의 값이 반영되나 | (Task 5 Step 4) | (스크립트 반환값, 찾은 경로) |
| 2b. `gameplay_variables.json`의 값이 반영되나 | (Task 5 Step 4) | |
| 2c. `battle_params.json`의 값이 반영되나 | (Task 5 Step 4) | |
| 3. 파일에 없는 키를 넣으면 읽히나 | (Task 6 Step 4, 또는 하지 않은 이유) | |

## 파일 키와 런타임 경로의 대응

(Task 5 Step 4에서 적은 대응. 예시 한두 개와 규칙. 규칙이 보이지 않으면 그렇게 적는다.)

## 이름으로 찾은 것

(Task 4 Step 4의 4번: 이름마다 찾았는가, 경로, 값. `game_speed_slower_div` 같은 exe 전용 이름이
전역에서 보였는지는 하위 프로젝트 2의 출발점이다.)

## 덤프 기능에 대해 알게 된 것

- 덤프에 걸린 시간, `visited`, `truncated`
- 켜서 `dump done`까지 걸린 시간
- 게임을 끈 방법(`Stop-NlGame`이 돌려준 값)과 `aurie.log`가 채워졌는지
- 덤프 도중 게임이 죽었는지 (Review Focus 5. 죽지 않았으면 "확인하지 못함")

## 단계 1에 주는 결론

(반영이 확인된 파일만 카탈로그에 올린다. 세이브에 굳는지는 이번에 보지 않았다 — 메인 메뉴에서만 쟀다.)

## 확인하지 못한 것

(메인 메뉴에서만 쟀으므로 새 게임을 시작한 뒤의 값은 모른다. 그 밖에 답이 "확인하지 못함"인 것 전부.)
```

- [ ] **Step 3: `CLAUDE.md`와 스펙을 고친다**

`CLAUDE.md`의 "개발 루프" 절에서 `check-load.ps1`의 종료 방식에 대한 항목을 다음으로 바꾼다.

```markdown
- `check-load.ps1`과 `probe.ps1`은 게임 창에 `WM_CLOSE`를 보내 정상 종료시키고(그래야 `aurie.log`가
  채워진다), 15초 안에 끝나지 않을 때만 강제 종료한다(`Stop-NlGame`).
```

"게임에 가하는 변경" 절 끝에 더한다.

```markdown
- 데이터 파일은 `tools/data-snapshot.ps1`으로 바닐라 사본을 뜬 뒤에만 고친다. 되돌릴 때는
  `tools/data-restore.ps1`. 스냅샷은 `backups\data\<게임 버전>\`에 있다(추적 안 함).
- `tools/probe.ps1`은 요청 파일(`tools/probes/*.txt`)을 놓고 게임을 켜서 런타임 덤프를 받아 온다.
  덤프는 `refs\runtime\`에 둔다(추적 안 함). 분석은 `py -3.14 tools/re/dump_tool.py`.
```

스펙 `2026-10-04-data-overlay-design.md`의 §4 첫 문장("단계 0의 결과로 이 절을 고친 뒤 계획을 쓴다…") 앞에 한 줄을 더한다: `단계 0의 결과: research/01-data-overlay.md.` 질문 2가 세 파일 모두 "아니다"였으면 그 사실과 "단계 1을 시작하지 않는다"를 함께 적는다.

- [ ] **Step 4: 추적 경계 확인과 커밋**

```powershell
git -C E:\NlToyBox add -- research/01-data-overlay.md CLAUDE.md docs/superpowers/specs/2026-10-04-data-overlay-design.md
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"     # 비어 있어야 한다
git -C E:\NlToyBox status --short
git -C E:\NlToyBox commit -m "docs(research): 데이터 오버레이 단계 0 실측 결과" -m "파일 값의 런타임 반영 여부, 파일 키와 런타임 경로의 대응, 덤프 기능에 대해 알게 된 것." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 5: `develop`에 병합**

병합 전 확인: `tools/build.ps1` 성공, `tools/tests/safety.tests.ps1` 통과(11개), 실행 1의 로그에 Phase 0의 판정 줄이 모두 있었다(Task 4 Step 2).

```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1 | Select-Object -Last 1
pwsh -NoProfile -File E:\NlToyBox\tools\tests\safety.tests.ps1 | Select-Object -Last 1
git -C E:\NlToyBox status --short                 # 비어 있어야 한다
git -C E:\NlToyBox switch develop
git -C E:\NlToyBox merge --no-ff feat/overlay-stage0 -m "Merge branch 'feat/overlay-stage0' into develop" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git -C E:\NlToyBox branch -d feat/overlay-stage0
git -C E:\NlToyBox log --oneline --graph -10
```

- [ ] **Step 6: 사용자에게 결과와 다음 단계를 보고한다**

세 질문의 답과, 그 답에 따른 다음 단계를 한 줄로 제안한다.

- 파일 값이 반영된다 → 단계 1(오버레이 도구)의 스펙 §4를 결과에 맞게 고치고 계획을 쓴다.
- 세 파일 모두 반영되지 않는다 → 로드맵으로 돌아가 하위 프로젝트 2(런타임 접근)를 먼저 할지 정한다.
- 일부만 반영된다 → 반영되는 파일만으로 단계 1을 한다.
