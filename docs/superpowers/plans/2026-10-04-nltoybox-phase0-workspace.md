# NlToyBox Phase 0 작업 구성 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `E:\NlToyBox`에서 "빌드 → 게임에 배치 → 게임을 켜서 모듈이 붙었는지 확인 → 바닐라로 복원" 한 바퀴가 스크립트만으로 돌게 한다.

**Architecture:** Aurie v2.0.2가 `Norland.exe`를 패치해 붙고, YYToolkit v5.0.0c가 GameMaker 러너에 접근하는 인터페이스를 내준다. 이 레포는 그 위에 올라가는 C++ 모듈 `NlToyBox.dll` 하나와, 게임 폴더를 다루는 PowerShell 도구 여섯 개를 만든다. 모듈은 "붙었다"는 증거만 로그로 남기고, 도구가 그 로그를 읽어 판정한다.

**Tech Stack:** C++ (`/std:c++latest`, MSVC 14.44, CMake + Ninja), PowerShell 7 (`pwsh`), Python 3.14 (조사 도구), git 서브모듈.

**Spec:** `docs/superpowers/specs/2026-10-04-nltoybox-phase0-workspace-design.md`

## Global Constraints

- 게임 경로는 `$env:NORLAND_GAME_DIR` 우선, 없으면 `E:\SteamLibrary\steamapps\common\Norland Story Generating Strategy`. 기본값은 `tools/common.ps1` 한 곳에만 적는다.
- 게임 경로에 공백이 있다. PowerShell에서는 `-LiteralPath`를 쓴다.
- 스크립트는 `pwsh`(PowerShell 7)로 실행한다. 파일은 BOM 없는 UTF-8이다. Windows PowerShell 5.1로 돌리지 않는다.
- git 명령은 모두 `git -C E:\NlToyBox`로 쓴다. 작업 브랜치는 `chore/workspace-setup`이다. `main`·`develop`에 직접 커밋하지 않는다. 커밋 직전에 `git -C E:\NlToyBox branch --show-current`가 `chore/workspace-setup`인지 본다.
- 커밋 메시지는 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` 줄로 끝낸다.
- `refs/`·`backups/`·`downloads/`·`build/` 아래 파일은 커밋하지 않는다.
- 고정 버전: Aurie 릴리스 v2.0.2, YYToolkit 릴리스 v5.0.0c, 서브모듈 `d5cc0078bfbfd5907884ad3fe0b3c6cbaa1c3f54`.
- 기준 exe: 버전 `0.5588.9777.0`, 129,225,728 B, SHA256 `609C17CFD812E90C7D3E84D3EB730EC559EA3976F30A239395922F604516863E`.
- 게임 폴더에서 바뀌는 것은 `Norland.exe`와 `mods\` 아래뿐이다. `%LOCALAPPDATA%\Strategy`는 건드리지 않는다.
- 게임이 실행 중이면 `setup-aurie`·`restore-game`·`deploy`·`check-load`는 거부한다.
- Python은 `py -3.14`로 부른다. `WindowsApps\python.exe`는 스토어 스텁이다.
- 단계 머리의 표시: **[다운로드]** 는 외부에서 파일을 받는다, **[게임 변경]** 은 게임 폴더를 바꾼다, **[게임 실행]** 은 게임을 켠다. 이 셋은 이 계획이 승인된 뒤에만 한다.
- 게임을 켜는 확인은 Task 5의 한 번으로 몰아서 한다. 켜기 전에 사용자에게 게임 창을 누르지 말라고 알린다.

## Review Focus

스펙이 암시하지만 정상 경로만 돌려서는 드러나지 않는 실패. 줄마다 시험을 해당 Task에 넣었다.

1. Steam이 게임을 갱신해 exe 버전이 바뀐 뒤 복원을 돌린다. 옛 버전의 백업으로 덮어쓰면 안 되고 거부해야 한다. → Task 4 Step 8
2. 게임이 켜진 채로 `setup-aurie`·`restore-game`·`deploy`를 돌린다. 셋 다 거부하고 파일을 바꾸지 않아야 한다. → Task 5 Step 7
3. 받은 파일이 `pins.json`과 다르다(손상·변조). 파일을 지우고 중단하며 게임은 그대로여야 한다. → Task 4 Step 5
4. 빌드가 실패했는데 옛 DLL이 남아 있다. `deploy`가 낡은 산출물을 배포하지 않아야 한다. → Task 5 Step 4
5. `NORLAND_GAME_DIR`이 게임이 아닌 폴더를 가리킨다. 도구가 무엇이 틀렸는지 말하고 멈춰야 한다. → Task 1 Step 5

## 파일 구조

| 파일 | 책임 | Task |
|---|---|---|
| `.gitignore` | 저작물·배포물·산출물을 추적에서 뺀다 | 1 |
| `tools/common.ps1` | 게임 경로, 실행 여부, exe 정보, 백업 목록 | 1, 4 |
| `tools/game-status.ps1` | 상태를 한 화면에 찍는다 (읽기 전용) | 1 |
| `tools/re/datawin_probe.py` | `data.win` 청크 표·GEN8·문자열 표 | 2 |
| `tools/re/exe_strings.py` | exe ASCII 문자열 | 2 |
| `external/YYToolkit` | 서브모듈 (헤더) | 3 |
| `CMakeLists.txt` · `src/ModuleMain.cpp` | 모듈 | 3 |
| `tools/build.ps1` | 빌드 | 3 |
| `tools/pins.json` | 받을 파일의 출처·크기·해시 | 4 |
| `tools/setup-aurie.ps1` | 받기 → 백업 → 배치 → 패치 | 4 |
| `tools/restore-game.ps1` | 바닐라 복원 | 4 |
| `tools/deploy.ps1` | 모듈 DLL 배치 | 5 |
| `tools/check-load.ps1` | 게임을 켜서 로그로 판정 | 5 |
| `research/00-game-structure.md` | 조사 기록과 실측 결과 | 6 |
| `CLAUDE.md` · `README.md` | 레포 규칙과 사용법 | 6 |

---

### Task 1: 공용 함수와 상태 점검

**Files:**
- Create: `E:\NlToyBox\.gitignore`
- Create: `E:\NlToyBox\tools\common.ps1`
- Create: `E:\NlToyBox\tools\game-status.ps1`

**Interfaces:**
- Consumes: 없음
- Produces (`tools/common.ps1`을 dot-source 하면 쓸 수 있다):
  - `$script:NlAppId` = `'1857090'`, `$script:NlProcessName` = `'Norland'`
  - `Assert-Equal $actual $expected [string]$msg`, `Assert-True $cond [string]$msg` — 어긋나면 throw
  - `Get-NlRepoRoot` → `[string]` 레포 절대경로
  - `Get-NlGameDir` → `[string]` 게임 폴더 절대경로. `Norland.exe`가 없으면 throw
  - `Test-NlGameRunning` → `[bool]`
  - `Assert-NlGameNotRunning` — 실행 중이면 PID를 담아 throw
  - `Get-NlPeSectionNames [string]$Path` → `[string[]]`. PE가 아니면 throw
  - `Get-NlExeInfo` → 객체 `{ Path, Version, Size, Sha256, Sections, Patched }`
  - `Get-NlBackups [string]$Version` → `FileInfo[]` (`backups\Norland.exe.<Version>.*`)

- [ ] **Step 1: `.gitignore` 작성**

```gitignore
# 게임 저작물과 그 파생물 (Long Jaunt) - 커밋하지 않는다
refs/
backups/

# 제3자 배포물 - tools/pins.json 으로 다시 받는다
downloads/

# 빌드 산출물
build/
dist/
*.user
.vs/
__pycache__/

# 도구 상태
.omc/
**/.omc/
.superpowers/
.semgrep/
```

- [ ] **Step 2: `tools/common.ps1` 작성**

```powershell
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
```

- [ ] **Step 3: `Get-NlExeInfo`가 기준 exe를 읽는지 확인**

Run:
```powershell
pwsh -NoProfile -Command ". E:\NlToyBox\tools\common.ps1; `$i = Get-NlExeInfo; Assert-Equal `$i.Version '0.5588.9777.0' 'exe 버전'; Assert-Equal `$i.Size 129225728 'exe 크기'; Assert-Equal `$i.Sha256 '609C17CFD812E90C7D3E84D3EB730EC559EA3976F30A239395922F604516863E' 'exe 해시'; Assert-Equal `$i.Patched `$false '패치 여부'; Assert-True (`$i.Sections -contains '.text') '.text 섹션'; 'OK ' + (`$i.Sections -join ' ')"
```
Expected: `OK`로 시작하고 섹션 이름이 이어진다(`.text`가 들어 있고 `.aurie`는 없다). throw가 나면 메시지의 `expected <…> got <…>`가 어긋난 값을 알려 준다.

- [ ] **Step 4: PE가 아닌 파일에서 오류가 나는지 확인**

Run:
```powershell
pwsh -NoProfile -Command ". E:\NlToyBox\tools\common.ps1; `$t = Join-Path `$env:TEMP 'nl-notpe.bin'; [IO.File]::WriteAllBytes(`$t, [byte[]](1..64)); try { Get-NlPeSectionNames `$t; 'FAIL: 오류가 나지 않음' } catch { 'OK: ' + `$_.Exception.Message } finally { Remove-Item -LiteralPath `$t }"
```
Expected: `OK: PE 파일이 아닙니다 (MZ 없음): …nl-notpe.bin`

- [ ] **Step 5: `NORLAND_GAME_DIR`이 틀린 폴더일 때 멈추는지 확인 (Review Focus 5)**

Run:
```powershell
pwsh -NoProfile -Command "`$env:NORLAND_GAME_DIR = 'C:\Windows'; . E:\NlToyBox\tools\common.ps1; try { Get-NlGameDir; 'FAIL: 오류가 나지 않음' } catch { 'OK: ' + `$_.Exception.Message }"
```
Expected: `OK: 게임 폴더에 Norland.exe 가 없습니다: C:\Windows (NORLAND_GAME_DIR 로 지정할 수 있습니다)`

- [ ] **Step 6: `tools/game-status.ps1` 작성**

```powershell
. (Join-Path $PSScriptRoot 'common.ps1')

$gameDir = Get-NlGameDir
$info = Get-NlExeInfo

# <라이브러리>\steamapps\common\<게임> 에서 두 단계 위가 steamapps 다.
$manifest = Join-Path (Split-Path -Parent (Split-Path -Parent $gameDir)) "appmanifest_$($script:NlAppId).acf"
$buildId = '(appmanifest 없음)'
if (Test-Path -LiteralPath $manifest) {
    $m = [regex]::Match((Get-Content -LiteralPath $manifest -Raw), '"buildid"\s+"(\d+)"')
    if ($m.Success) { $buildId = $m.Groups[1].Value }
}

$modsDir = Join-Path $gameDir 'mods'
$mods = @()
if (Test-Path -LiteralPath $modsDir) {
    $mods = @(Get-ChildItem -LiteralPath $modsDir -Recurse -File |
        ForEach-Object { "$($_.FullName.Substring($gameDir.Length + 1))  ($($_.Length) B)" })
}

"게임 폴더  : $gameDir"
"buildid    : $buildId"
"exe 버전   : $($info.Version)"
"exe 크기   : $($info.Size)"
"exe SHA256 : $($info.Sha256)"
"패치 여부  : $(if ($info.Patched) { '패치됨 (.aurie 섹션 있음)' } else { '바닐라 (.aurie 섹션 없음)' })"
"실행 여부  : $(if (Test-NlGameRunning) { '실행 중' } else { '꺼져 있음' })"
if ($mods.Count) { 'mods\      :'; $mods | ForEach-Object { "  $_" } } else { 'mods\      : (없음)' }

$backups = @(Get-NlBackups $info.Version)
if ($backups.Count -eq 0) { '백업       : (이 버전의 백업 없음)' }
foreach ($b in $backups) {
    $bh = (Get-FileHash -LiteralPath $b.FullName -Algorithm SHA256).Hash
    $rel = if ($info.Patched) { 'exe 는 패치 상태라 비교하지 않음' }
           elseif ($bh -eq $info.Sha256) { 'exe 와 일치' }
           else { 'exe 와 다름' }
    "백업       : $($b.Name)  SHA256 $bh  ($rel)"
}
```

- [ ] **Step 7: `game-status.ps1` 실행 확인**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\game-status.ps1`

Expected (값이 이것과 같아야 한다):
```
게임 폴더  : E:\SteamLibrary\steamapps\common\Norland Story Generating Strategy
buildid    : 25211575
exe 버전   : 0.5588.9777.0
exe 크기   : 129225728
exe SHA256 : 609C17CFD812E90C7D3E84D3EB730EC559EA3976F30A239395922F604516863E
패치 여부  : 바닐라 (.aurie 섹션 없음)
실행 여부  : 꺼져 있음
mods\      : (없음)
백업       : (이 버전의 백업 없음)
```

- [ ] **Step 8: 커밋**

```powershell
git -C E:\NlToyBox branch --show-current   # chore/workspace-setup 이어야 한다
git -C E:\NlToyBox add -- .gitignore tools/common.ps1 tools/game-status.ps1
git -C E:\NlToyBox status --short          # .omc/ 가 더는 보이지 않아야 한다
git -C E:\NlToyBox commit -m "chore(tools): 공용 함수와 게임 상태 점검" -m "게임 경로·실행 여부·exe 정보(PE 섹션으로 패치 여부 판정)·백업 목록." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: 조사 도구

**Files:**
- Create: `E:\NlToyBox\tools\re\datawin_probe.py`
- Create: `E:\NlToyBox\tools\re\exe_strings.py`

**Interfaces:**
- Consumes: `Get-NlGameDir` (Task 1)
- Produces: `py -3.14 tools/re/datawin_probe.py <data.win> <출력.txt>`, `py -3.14 tools/re/exe_strings.py <exe> <출력.txt>`. 출력은 `refs/` 아래에 둔다(추적 안 함).

이 Task는 exe가 바닐라일 때 돌린다. 패치 뒤에는 문자열 수가 달라진다.

- [ ] **Step 1: `tools/re/datawin_probe.py` 작성**

```python
"""data.win(GameMaker IFF)을 읽기 전용으로 훑어 청크 표, GEN8 정보, STRG 문자열을 뽑는다.

사용: py -3.14 datawin_probe.py <data.win> <문자열 출력 파일>
"""
import datetime
import os
import struct
import sys

src, out_strings = sys.argv[1], sys.argv[2]

with open(src, "rb") as f:
    data = f.read()

assert data[:4] == b"FORM", data[:4]
total = struct.unpack_from("<I", data, 4)[0]
print(f"FORM size={total:,} file={len(data):,}")

chunks = {}
pos = 8
while pos < 8 + total:
    name = data[pos:pos + 4].decode("ascii", "replace")
    size = struct.unpack_from("<I", data, pos + 4)[0]
    chunks[name] = (pos + 8, size)
    print(f"  {name}  off=0x{pos + 8:08X}  size={size:>12,}")
    pos += 8 + size


def cstr(off):
    if off == 0:
        return ""
    end = data.index(b"\0", off)
    return data[off:end].decode("utf-8", "replace")


if "GEN8" in chunks:
    o, _ = chunks["GEN8"]
    debugger_disabled, bytecode = data[o], data[o + 1]
    fn_off, cfg_off = struct.unpack_from("<II", data, o + 4)
    name_off = struct.unpack_from("<I", data, o + 40)[0]
    major, minor, release, build = struct.unpack_from("<IIII", data, o + 44)
    ts = struct.unpack_from("<Q", data, o + 92)[0]
    disp_off = struct.unpack_from("<I", data, o + 100)[0]
    print("--- GEN8 ---")
    print(f"  debuggerDisabled={debugger_disabled} bytecodeVersion={bytecode}")
    print(f"  filename={cstr(fn_off)!r} config={cstr(cfg_off)!r} name={cstr(name_off)!r} display={cstr(disp_off)!r}")
    print(f"  version={major}.{minor}.{release}.{build}")
    print(f"  timestamp={datetime.datetime.fromtimestamp(ts, datetime.timezone.utc).isoformat()}")

# CODE/VARI/FUNC 가 없으면 YYC 다.
for key in ("CODE", "VARI", "FUNC", "SCPT", "OBJT", "SPRT", "TXTR", "AUDO", "EXTN"):
    if key in chunks:
        o, size = chunks[key]
        count = struct.unpack_from("<I", data, o)[0] if size >= 4 else None
        print(f"  {key}: size={size:,} count={count}")
    else:
        print(f"  {key}: (청크 없음)")

if "STRG" in chunks:
    o, size = chunks["STRG"]
    n = struct.unpack_from("<I", data, o)[0]
    offs = struct.unpack_from(f"<{n}I", data, o + 4)
    os.makedirs(os.path.dirname(os.path.abspath(out_strings)), exist_ok=True)
    with open(out_strings, "w", encoding="utf-8", newline="\n") as w:
        for so in offs:
            ln = struct.unpack_from("<I", data, so)[0]
            s = data[so + 4:so + 4 + ln].decode("utf-8", "replace")
            w.write(s.replace("\r", "\\r").replace("\n", "\\n") + "\n")
    print(f"--- STRG --- count={n:,} -> {out_strings}")
```

- [ ] **Step 2: `tools/re/exe_strings.py` 작성**

```python
"""실행 파일에서 출력 가능한 ASCII 문자열(길이 5 이상)을 뽑아 중복 없이 저장한다. 읽기 전용.

사용: py -3.14 exe_strings.py <exe> <출력 파일>
"""
import os
import re
import sys

src, out = sys.argv[1], sys.argv[2]
with open(src, "rb") as f:
    data = f.read()

seen = set()
os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
with open(out, "w", encoding="utf-8", newline="\n") as w:
    for m in re.finditer(rb"[\x20-\x7e]{5,400}", data):
        s = m.group().decode("ascii")
        if s not in seen:
            seen.add(s)
            w.write(s + "\n")
print(f"{src}: {len(data):,} bytes, unique strings={len(seen):,} -> {out}")
```

- [ ] **Step 3: `data.win`에 돌려 스펙 §2.2의 수치가 다시 나오는지 확인**

Run:
```powershell
pwsh -NoProfile -Command ". E:\NlToyBox\tools\common.ps1; py -3.14 E:\NlToyBox\tools\re\datawin_probe.py (Join-Path (Get-NlGameDir) 'data.win') E:\NlToyBox\refs\strg.txt"
```
Expected (해당 줄이 그대로 있어야 한다):
```
  filename='Strategy' config='SteamRelease' name='Strategy' display='Norland'
  CODE: (청크 없음)
  VARI: (청크 없음)
  FUNC: (청크 없음)
  SCPT: size=489,768 count=40813
  OBJT: size=31,288 count=64
--- STRG --- count=49,102 -> E:\NlToyBox\refs\strg.txt
```

- [ ] **Step 4: `Norland.exe`에 돌려 확인**

Run:
```powershell
pwsh -NoProfile -Command ". E:\NlToyBox\tools\common.ps1; py -3.14 E:\NlToyBox\tools\re\exe_strings.py (Join-Path (Get-NlGameDir) 'Norland.exe') E:\NlToyBox\refs\exe_strings.txt; Select-String -LiteralPath E:\NlToyBox\refs\exe_strings.txt -Pattern '^code_is_compiled$','^gml_Script_command_line_parameters_init$','^2023\.4\.0\.113$' | ForEach-Object { `$_.Line }"
```
Expected: 첫 줄이 `… 129,225,728 bytes, unique strings=137,939 -> E:\NlToyBox\refs\exe_strings.txt`이고, 이어서 세 줄 `code_is_compiled`, `gml_Script_command_line_parameters_init`, `2023.4.0.113`이 나온다(순서는 다를 수 있다).

- [ ] **Step 5: 커밋 (`refs/`가 추적되지 않는지 확인)**

```powershell
git -C E:\NlToyBox branch --show-current
git -C E:\NlToyBox add -- tools/re/datawin_probe.py tools/re/exe_strings.py
git -C E:\NlToyBox status --short          # refs/ 가 보이지 않아야 한다
git -C E:\NlToyBox commit -m "chore(tools): data.win·exe 조사 스크립트" -m "청크 표·GEN8·문자열 표, exe ASCII 문자열. 출력은 refs/ (추적 안 함)." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: 모듈과 빌드

**Files:**
- Create: `E:\NlToyBox\external\YYToolkit` (서브모듈), `E:\NlToyBox\.gitmodules`
- Create: `E:\NlToyBox\CMakeLists.txt`
- Create: `E:\NlToyBox\src\ModuleMain.cpp`
- Create: `E:\NlToyBox\tools\build.ps1`

**Interfaces:**
- Consumes: `Get-NlRepoRoot` (Task 1)
- Produces:
  - `pwsh -File tools/build.ps1` → `build\NlToyBox.dll`. 실패하면 throw(종료 코드 1)
  - 모듈이 쓰는 로그 `NlToyBox.log`(모듈 DLL 옆). 줄 형식은 다음과 같고 Task 5의 `check-load.ps1`이 이 문자열을 그대로 찾는다:
    ```
    NlToyBox 0.0.1 loaded
    yytk <major>.<minor>.<patch>
    builtin code_is_compiled = <true|false|error:<상태>>
    script gml_Script_command_line_parameters_init = <found|error:<상태>>
    probe done
    ```

- [ ] **Step 1: [다운로드] 서브모듈 추가와 커밋 고정**

```powershell
git -C E:\NlToyBox submodule add -b experimental https://github.com/AurieFramework/YYToolkit.git external/YYToolkit
git -C E:\NlToyBox\external\YYToolkit checkout d5cc0078bfbfd5907884ad3fe0b3c6cbaa1c3f54
git -C E:\NlToyBox submodule status
```
Expected: 마지막 명령이 `d5cc0078bfbfd5907884ad3fe0b3c6cbaa1c3f54 external/YYToolkit`로 시작하는 줄을 낸다(앞에 `+`가 붙어도 된다. 다음 커밋에서 고정된다).

- [ ] **Step 2: 헤더가 v5인지 확인**

Run:
```powershell
Select-String -LiteralPath E:\NlToyBox\external\YYToolkit\YYToolkit\source\YYTK\Shared\YYTK_Shared_Base.hpp -Pattern 'define YYTK_(MAJOR|MINOR|PATCH) ' | ForEach-Object { $_.Line.Trim() }
```
Expected:
```
#define YYTK_MAJOR 5
#define YYTK_MINOR 0
#define YYTK_PATCH 0
```
`YYTK_MAJOR 4`가 나오면 `stable`을 체크아웃한 것이다. Step 1의 checkout을 다시 한다.

- [ ] **Step 3: `CMakeLists.txt` 작성**

```cmake
cmake_minimum_required(VERSION 3.21)
project(NlToyBox LANGUAGES CXX)

# 헤더는 YYToolkit 본체가 쓰는 정본을 쓴다. ExamplePlugin/include 의 사본은 내용이 다르다.
set(YYTK_ROOT "${CMAKE_SOURCE_DIR}/external/YYToolkit/YYToolkit")
set(YYTK_SHARED "${YYTK_ROOT}/source/YYTK/Shared")

if(NOT EXISTS "${YYTK_SHARED}/YYTK_Shared.hpp")
  message(FATAL_ERROR "external/YYToolkit 이 비어 있습니다. git submodule update --init 을 먼저 실행하세요.")
endif()

add_library(nltoybox SHARED
  src/ModuleMain.cpp
  "${YYTK_SHARED}/YYTK_Shared_Types.cpp"
)

set_target_properties(nltoybox PROPERTIES
  OUTPUT_NAME "NlToyBox"
  PREFIX ""
  MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
)

target_include_directories(nltoybox PRIVATE
  "${YYTK_SHARED}"
  "${YYTK_ROOT}/include"
)

target_compile_options(nltoybox PRIVATE /std:c++latest /W3 /utf-8)
target_compile_definitions(nltoybox PRIVATE UNICODE _UNICODE)
```

- [ ] **Step 4: `src/ModuleMain.cpp` 작성**

```cpp
// NlToyBox Phase 0 모듈.
// 붙었다는 증거만 남긴다. 모듈 옆 NlToyBox.log 에 적재, YYToolkit 버전, 프로브 두 개의 결과를 쓴다.
// 줄 형식은 tools/check-load.ps1 이 그대로 찾는다 (스펙 §4.3). 바꾸면 그쪽도 바꾼다.

#include <YYTK_Shared.hpp>

#include <fstream>
#include <string>

using namespace Aurie;
using namespace YYTK;

namespace
{
	constexpr const char* k_Version = "0.0.1";
	constexpr const char* k_ProbeBuiltin = "code_is_compiled";
	constexpr const char* k_ProbeScript = "gml_Script_command_line_parameters_init";

	YYTKInterface* g_Yytk = nullptr;
	fs::path g_LogPath;
	bool g_Probed = false;

	void LogLine(const std::string& Line, bool Truncate = false)
	{
		std::ofstream out(g_LogPath, Truncate ? std::ios::trunc : std::ios::app);
		out << Line << '\n';
		DbgPrintEx(LOG_SEVERITY_INFO, "[NlToyBox] %s", Line.c_str());
	}

	std::string StatusText(AurieStatus Status)
	{
		return std::string("error:") + AurieStatusToString(Status);
	}

	// 빌트인 호출이 되는지 본다. YYC 빌드이므로 참이 나와야 한다.
	std::string ProbeBuiltin()
	{
		CInstance* global_instance = nullptr;
		AurieStatus status = g_Yytk->GetGlobalInstance(&global_instance);
		if (!AurieSuccess(status))
			return StatusText(status);

		RValue result;
		status = g_Yytk->CallBuiltinEx(result, k_ProbeBuiltin, global_instance, global_instance, {});
		if (!AurieSuccess(status))
			return StatusText(status);

		return result.ToBoolean() ? "true" : "false";
	}

	// 게임 스크립트를 이름으로 찾을 수 있는지 본다.
	std::string ProbeScript()
	{
		PVOID routine = nullptr;
		const AurieStatus status = g_Yytk->GetNamedRoutinePointer(k_ProbeScript, &routine);
		if (!AurieSuccess(status))
			return StatusText(status);

		return routine ? "found" : "error:null pointer";
	}

	void FrameCallback(FWFrame& FrameContext)
	{
		UNREFERENCED_PARAMETER(FrameContext);

		// 첫 프레임에 한 번만 시험한다. 러너가 준비된 뒤여야 빌트인을 부를 수 있다.
		if (g_Probed)
			return;
		g_Probed = true;

		LogLine(std::string("builtin ") + k_ProbeBuiltin + " = " + ProbeBuiltin());
		LogLine(std::string("script ") + k_ProbeScript + " = " + ProbeScript());
		LogLine("probe done");
	}
}

EXPORTED AurieStatus ModuleInitialize(
	IN AurieModule* Module,
	IN const fs::path& ModulePath
)
{
	// ModulePath 가 DLL 경로인지 폴더인지에 기대지 않는다.
	std::error_code ec;
	const fs::path module_dir = fs::is_directory(ModulePath, ec) ? ModulePath : ModulePath.parent_path();
	g_LogPath = module_dir / "NlToyBox.log";

	LogLine(std::string("NlToyBox ") + k_Version + " loaded", true);

	g_Yytk = YYTK::GetInterface();
	if (!g_Yytk)
	{
		LogLine("error:YYTK interface not found");
		return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
	}

	// 헤더(YYTK_MAJOR 5)와 실제 바이너리가 어긋났는지 보는 단서.
	short major = 0, minor = 0, patch = 0;
	g_Yytk->QueryVersion(major, minor, patch);
	LogLine("yytk " + std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch));

	const AurieStatus status = g_Yytk->CreateCallback(Module, EVENT_FRAME, FrameCallback, 0);
	if (!AurieSuccess(status))
	{
		LogLine("error:CreateCallback " + std::string(AurieStatusToString(status)));
		return status;
	}

	return AURIE_SUCCESS;
}
```

- [ ] **Step 5: `tools/build.ps1` 작성**

```powershell
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$repo = Get-NlRepoRoot
if (-not (Test-Path -LiteralPath (Join-Path $repo 'external\YYToolkit\YYToolkit\source\YYTK\Shared\YYTK_Shared.hpp'))) {
    throw "external/YYToolkit 이 비어 있습니다. 먼저 실행: git -C `"$repo`" submodule update --init"
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ 빌드 도구를 찾지 못했습니다.' }

$vcvars   = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
$cmakeDir = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$cmake    = Join-Path $cmakeDir 'CMake\bin\cmake.exe'
$ninja    = Join-Path $cmakeDir 'Ninja\ninja.exe'
$build    = Join-Path $repo 'build'

$cmd = "`"$vcvars`" >nul && `"$cmake`" -S `"$repo`" -B `"$build`" -G Ninja -DCMAKE_MAKE_PROGRAM=`"$ninja`" -DCMAKE_BUILD_TYPE=RelWithDebInfo && `"$cmake`" --build `"$build`""
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "빌드 실패 ($LASTEXITCODE)" }

$dll = Join-Path $build 'NlToyBox.dll'
if (-not (Test-Path -LiteralPath $dll)) { throw "빌드는 끝났지만 산출물이 없습니다: $dll" }
Write-Host "build ok -> $dll"
```

- [ ] **Step 6: 빌드**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1`

Expected: 마지막 줄이 `build ok -> E:\NlToyBox\build\NlToyBox.dll`이고 종료 코드 0.

컴파일 오류가 나면 오류가 가리키는 줄을 `external\YYToolkit\YYToolkit\source\YYTK\Shared\YYTK_Shared_Interface.hpp`의 선언과 대조해 `src/ModuleMain.cpp`를 고친다. 서브모듈 안의 파일은 고치지 않는다.

- [ ] **Step 7: DLL이 Aurie가 찾는 진입점을 내보내는지 확인**

Run:
```powershell
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && dumpbin /nologo /exports E:\NlToyBox\build\NlToyBox.dll" | Select-String 'ModuleInitialize|__AurieFrameworkInit' | ForEach-Object { $_.Line.Trim() }
```
Expected: `ModuleInitialize`와 `__AurieFrameworkInit`이 든 줄이 각각 하나씩 나온다.

- [ ] **Step 8: 커밋 (`build/`가 추적되지 않는지 확인)**

```powershell
git -C E:\NlToyBox branch --show-current
git -C E:\NlToyBox add -- .gitmodules external/YYToolkit CMakeLists.txt src/ModuleMain.cpp tools/build.ps1
git -C E:\NlToyBox status --short          # build/ 가 보이지 않아야 한다
git -C E:\NlToyBox submodule status        # ' d5cc0078…' 로 시작해야 한다 (+ 없음)
git -C E:\NlToyBox commit -m "feat(module): Phase 0 모듈과 빌드" -m "YYToolkit 서브모듈(experimental d5cc0078), CMake+Ninja, 첫 프레임에 빌트인 호출과 스크립트 조회를 시험해 NlToyBox.log 에 남기는 모듈." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: 설치와 복원

**Files:**
- Create: `E:\NlToyBox\tools\pins.json`
- Modify: `E:\NlToyBox\tools\common.ps1` (끝에 `Get-NlPins` 추가)
- Create: `E:\NlToyBox\tools\setup-aurie.ps1`
- Create: `E:\NlToyBox\tools\restore-game.ps1`

**Interfaces:**
- Consumes: `Get-NlRepoRoot`, `Get-NlGameDir`, `Assert-NlGameNotRunning`, `Get-NlExeInfo`, `Get-NlBackups`, `Get-NlPeSectionNames` (Task 1)
- Produces:
  - `Get-NlPins` → 객체 `{ files: [ { name, project, tag, url, size, sha256, gamePath } ] }`
  - `pwsh -File tools/setup-aurie.ps1` → 게임에 `mods\Native\AurieCore.dll`, `mods\Aurie\YYToolkit.dll`을 놓고 exe를 패치한다. 끝줄 `setup ok -> …`. 실패하면 종료 코드 1
  - `pwsh -File tools/restore-game.ps1` → exe를 백업으로 덮어쓰고 `mods\`에서 이 레포가 놓은 파일을 지운다. 끝줄 `restore ok`. 실패하면 종료 코드 1
  - 백업 파일 이름: `backups\Norland.exe.<버전>.<SHA256 앞 12자>`

- [ ] **Step 1: `tools/pins.json` 작성**

```json
{
  "files": [
    {
      "name": "AurieCore.dll",
      "project": "Aurie",
      "tag": "v2.0.2",
      "url": "https://github.com/AurieFramework/Aurie/releases/download/v2.0.2/AurieCore.dll",
      "size": 967680,
      "sha256": "18e3a1de980f487a6b3858b673d2030e96984dd96de3a047b43a263a5ba829ae",
      "gamePath": "mods\\Native\\AurieCore.dll"
    },
    {
      "name": "AuriePatcher.exe",
      "project": "Aurie",
      "tag": "v2.0.2",
      "url": "https://github.com/AurieFramework/Aurie/releases/download/v2.0.2/AuriePatcher.exe",
      "size": 260096,
      "sha256": "4d3aec439dbba5209ad48fb0a40d6324406247fe678541df9031f5b89363d536",
      "gamePath": null
    },
    {
      "name": "YYToolkit.dll",
      "project": "YYToolkit",
      "tag": "v5.0.0c",
      "url": "https://github.com/AurieFramework/YYToolkit/releases/download/v5.0.0c/YYToolkit.dll",
      "size": 860672,
      "sha256": "ae7809f136f9222e5f49393d7ce7c3ad4c375c171308e2414660946d4ba0378a",
      "gamePath": "mods\\Aurie\\YYToolkit.dll"
    }
  ]
}
```

- [ ] **Step 2: `tools/common.ps1` 끝에 `Get-NlPins` 추가**

```powershell

# 받을 파일의 출처·크기·SHA256. downloads\<project>-<tag>\<name> 에 받는다.
function Get-NlPins {
    Get-Content -LiteralPath (Join-Path $PSScriptRoot 'pins.json') -Raw | ConvertFrom-Json
}
```

- [ ] **Step 3: `tools/setup-aurie.ps1` 작성**

```powershell
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
```

- [ ] **Step 4: `tools/restore-game.ps1` 작성**

```powershell
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

Assert-NlGameNotRunning
$gameDir = Get-NlGameDir
$info = Get-NlExeInfo

if ($info.Patched) {
    # 지금 exe 와 같은 버전의 백업만 쓴다. 게임이 갱신됐으면 옛 백업으로 덮어쓰지 않는다.
    $backups = @(Get-NlBackups $info.Version)
    if ($backups.Count -eq 0) {
        throw "버전 $($info.Version) 의 백업이 없습니다. Steam 무결성 검사로 되돌리세요."
    }
    if ($backups.Count -gt 1) {
        throw ("버전 $($info.Version) 의 백업이 둘 이상입니다. 하나만 남기고 다시 실행하세요:`n" +
               (($backups | ForEach-Object { "  $($_.FullName)" }) -join "`n"))
    }
    $backup = $backups[0]
    Copy-Item -LiteralPath $backup.FullName -Destination $info.Path -Force
    $want = (Get-FileHash -LiteralPath $backup.FullName -Algorithm SHA256).Hash
    $got = (Get-NlExeInfo).Sha256
    if ($got -ne $want) { throw "복원 뒤 exe 해시가 백업과 다릅니다 (exe $got, 백업 $want)" }
    Write-Host "exe 복원: $($backup.Name)"
} else {
    Write-Host 'exe 는 패치되지 않은 상태입니다. 그대로 둡니다.'
}

# 이 레포가 놓은 파일만 지운다. 다른 파일이 있으면 남기고 알린다.
$ours = @(
    'mods\Native\AurieCore.dll',
    'mods\Aurie\YYToolkit.dll',
    'mods\Aurie\NlToyBox.dll',
    'mods\Aurie\NlToyBox.log'
)
foreach ($rel in $ours) {
    $p = Join-Path $gameDir $rel
    if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Force }
}
foreach ($rel in 'mods\Native', 'mods\Aurie', 'mods') {
    $d = Join-Path $gameDir $rel
    if (-not (Test-Path -LiteralPath $d)) { continue }
    $left = @(Get-ChildItem -LiteralPath $d -Force)
    if ($left.Count -eq 0) {
        Remove-Item -LiteralPath $d -Force
    } else {
        Write-Host "남김 (이 레포가 놓지 않은 것이 있음): $d"
        $left | ForEach-Object { Write-Host "  $($_.Name)" }
    }
}
Write-Host 'restore ok'
```

- [ ] **Step 5: 받은 파일이 `pins.json`과 다를 때 멈추는지 확인 (Review Focus 3)**

가짜 파일을 받을 자리에 먼저 놓고 돌린다. 네트워크를 쓰지 않고, 게임을 건드리기 전에 멈춰야 한다.

Run:
```powershell
New-Item -ItemType Directory -Force -Path E:\NlToyBox\downloads\Aurie-v2.0.2 | Out-Null
[IO.File]::WriteAllBytes('E:\NlToyBox\downloads\Aurie-v2.0.2\AurieCore.dll', [byte[]](1..10))
pwsh -NoProfile -File E:\NlToyBox\tools\setup-aurie.ps1; "exit=$LASTEXITCODE"
"가짜 파일 남음: " + (Test-Path -LiteralPath E:\NlToyBox\downloads\Aurie-v2.0.2\AurieCore.dll)
pwsh -NoProfile -File E:\NlToyBox\tools\game-status.ps1 | Select-String '패치 여부|mods'
```
Expected:
- `받은 파일이 pins.json 과 다릅니다: AurieCore.dll (size 10, sha256 …). 파일을 지웠습니다.`와 `exit=1`
- `가짜 파일 남음: False`
- `패치 여부  : 바닐라 (.aurie 섹션 없음)`, `mods\      : (없음)`

- [ ] **Step 6: [다운로드] [게임 변경] 설치 실행**

Run:
```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\setup-aurie.ps1; "exit=$LASTEXITCODE"
pwsh -NoProfile -File E:\NlToyBox\tools\game-status.ps1
git -C E:\NlToyBox status --short
```
Expected:
- `받는 중: …` 세 줄, `백업: E:\NlToyBox\backups\Norland.exe.0.5588.9777.0.609C17CFD812`, AuriePatcher의 `File saved successfully. Done.`, `setup ok -> …`, `exit=0`
- `game-status`: `패치 여부  : 패치됨 (.aurie 섹션 있음)`, `mods\Aurie\YYToolkit.dll  (860672 B)`, `mods\Native\AurieCore.dll  (967680 B)`, `백업 … SHA256 609C17CF…863E  (exe 는 패치 상태라 비교하지 않음)`
- `git status --short`에 `downloads/`·`backups/`가 보이지 않는다

- [ ] **Step 7: 다시 돌려도 같은 결과인지 확인**

Run:
```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\setup-aurie.ps1; "exit=$LASTEXITCODE"
(Get-ChildItem -LiteralPath E:\NlToyBox\backups).Name
```
Expected: `받는 중`·`백업:` 줄 없이 `setup ok -> …`, `exit=0`. 백업은 여전히 `Norland.exe.0.5588.9777.0.609C17CFD812` 하나다.

- [ ] **Step 8: 다른 버전의 백업만 있을 때와 백업이 둘일 때 복원이 거부하는지 확인 (Review Focus 1)**

Run:
```powershell
$b = 'E:\NlToyBox\backups\Norland.exe.0.5588.9777.0.609C17CFD812'

# (가) 게임이 갱신된 상황을 흉내 낸다: 백업이 다른 버전의 것만 있다.
Rename-Item -LiteralPath $b -NewName 'Norland.exe.0.0.0.0.609C17CFD812'
pwsh -NoProfile -File E:\NlToyBox\tools\restore-game.ps1; "restore exit=$LASTEXITCODE"
pwsh -NoProfile -File E:\NlToyBox\tools\setup-aurie.ps1;  "setup exit=$LASTEXITCODE"
Rename-Item -LiteralPath 'E:\NlToyBox\backups\Norland.exe.0.0.0.0.609C17CFD812' -NewName 'Norland.exe.0.5588.9777.0.609C17CFD812'

# (나) 같은 버전의 백업이 둘이다.
Copy-Item -LiteralPath $b -Destination 'E:\NlToyBox\backups\Norland.exe.0.5588.9777.0.AAAAAAAAAAAA'
pwsh -NoProfile -File E:\NlToyBox\tools\restore-game.ps1; "restore exit=$LASTEXITCODE"
Remove-Item -LiteralPath 'E:\NlToyBox\backups\Norland.exe.0.5588.9777.0.AAAAAAAAAAAA'

pwsh -NoProfile -File E:\NlToyBox\tools\game-status.ps1 | Select-String '패치 여부'
```
Expected:
- (가) `버전 0.5588.9777.0 의 백업이 없습니다. Steam 무결성 검사로 되돌리세요.` / `restore exit=1`, 그리고 `exe 가 이미 패치된 상태인데 버전 0.5588.9777.0 의 백업이 없습니다. …` / `setup exit=1`
- (나) `버전 0.5588.9777.0 의 백업이 둘 이상입니다. …`와 두 경로 / `restore exit=1`
- 마지막: `패치 여부  : 패치됨 (.aurie 섹션 있음)` — 거부한 동안 exe가 바뀌지 않았다

- [ ] **Step 9: `AuriePatcher remove`가 원본을 되살리는지 사본으로 실측**

게임 exe가 아니라 사본에 돌린다. 결과는 Task 6에서 `research/`에 적는다.

Run:
```powershell
. E:\NlToyBox\tools\common.ps1
$g = Get-NlGameDir
New-Item -ItemType Directory -Force -Path E:\NlToyBox\build\patchtest | Out-Null
Copy-Item -LiteralPath (Join-Path $g 'Norland.exe') -Destination E:\NlToyBox\build\patchtest\Norland.exe -Force
& E:\NlToyBox\downloads\Aurie-v2.0.2\AuriePatcher.exe E:\NlToyBox\build\patchtest\Norland.exe (Join-Path $g 'mods\Native\AurieCore.dll') remove
"patcher exit=$LASTEXITCODE"
$h = (Get-FileHash -LiteralPath E:\NlToyBox\build\patchtest\Norland.exe -Algorithm SHA256).Hash
"remove 뒤 SHA256 = $h"
"원본과 같음 = " + ($h -eq '609C17CFD812E90C7D3E84D3EB730EC559EA3976F30A239395922F604516863E')
"섹션 = " + ((Get-NlPeSectionNames E:\NlToyBox\build\patchtest\Norland.exe) -join ' ')
Remove-Item -LiteralPath E:\NlToyBox\build\patchtest -Recurse -Force
```
Expected: `patcher exit=0`, `.aurie`가 없는 섹션 목록. `원본과 같음`은 `True`든 `False`든 그대로 기록한다. 이 값이 `False`여도 계획은 그대로다. `restore-game.ps1`은 백업을 덮어쓰는 방식이라 영향이 없다.

- [ ] **Step 10: [게임 변경] 복원 실행과 확인**

Run:
```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\restore-game.ps1; "exit=$LASTEXITCODE"
pwsh -NoProfile -File E:\NlToyBox\tools\game-status.ps1
```
Expected:
- `exe 복원: Norland.exe.0.5588.9777.0.609C17CFD812`, `restore ok`, `exit=0`. `남김` 줄이 없다
- `game-status`: `exe SHA256 : 609C17CF…863E`, `패치 여부  : 바닐라 (.aurie 섹션 없음)`, `mods\      : (없음)`, `백업 … (exe 와 일치)`

- [ ] **Step 11: 커밋**

```powershell
git -C E:\NlToyBox branch --show-current
git -C E:\NlToyBox add -- tools/pins.json tools/common.ps1 tools/setup-aurie.ps1 tools/restore-game.ps1
git -C E:\NlToyBox status --short
git -C E:\NlToyBox commit -m "feat(tools): Aurie·YYToolkit 설치와 바닐라 복원" -m "릴리스 파일을 크기·SHA256 으로 고정해 받고, exe 를 백업한 뒤 패치한다. 복원은 같은 버전의 백업 하나로만 덮어쓴다." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: 배포와 적재 확인

**Files:**
- Create: `E:\NlToyBox\tools\deploy.ps1`
- Create: `E:\NlToyBox\tools\check-load.ps1`
- Modify (조건부): `E:\NlToyBox\tools\restore-game.ps1`의 `$ours` 목록 (Step 8)

**Interfaces:**
- Consumes: `Get-NlRepoRoot`, `Get-NlGameDir`, `Assert-NlGameNotRunning`, `Test-NlGameRunning`, `Get-NlExeInfo`, `$script:NlAppId`, `$script:NlProcessName` (Task 1), `build\NlToyBox.dll`과 로그 줄 형식 (Task 3), `setup-aurie.ps1`·`restore-game.ps1` (Task 4)
- Produces:
  - `pwsh -File tools/deploy.ps1` → `build\NlToyBox.dll`을 `<게임>\mods\Aurie\NlToyBox.dll`로 복사. 끝줄 `deployed -> …`
  - `pwsh -File tools/check-load.ps1 [-TimeoutSec 180] [-KeepRunning]` → 게임을 켜고 로그를 판정. 끝줄 `PASS`(종료 코드 0) 또는 `FAIL: …`(종료 코드 1)

- [ ] **Step 1: `tools/deploy.ps1` 작성**

```powershell
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$repo = Get-NlRepoRoot
$src = Join-Path $repo 'build\NlToyBox.dll'
if (-not (Test-Path -LiteralPath $src)) { throw "빌드 산출물이 없습니다: $src" }

# 빌드가 실패해도 이전 산출물이 남는다. 소스보다 오래된 산출물은 거부한다. 게임 재시작 한 번이 비싸다.
$newestSrc = Get-ChildItem -LiteralPath (Join-Path $repo 'src') -Recurse -File |
             Sort-Object LastWriteTime -Descending | Select-Object -First 1
$dll = Get-Item -LiteralPath $src
if ($newestSrc -and $dll.LastWriteTime -lt $newestSrc.LastWriteTime) {
    throw ("산출물이 소스보다 오래되었습니다. 빌드가 실패했을 수 있습니다.`n" +
           "  DLL : $($dll.LastWriteTime)`n" +
           "  소스: $($newestSrc.LastWriteTime)  ($($newestSrc.Name))")
}

Assert-NlGameNotRunning
$info = Get-NlExeInfo
if (-not $info.Patched) { throw 'exe 가 패치되지 않았습니다. 먼저 tools\setup-aurie.ps1 을 실행하세요.' }

$dstDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
if (-not (Test-Path -LiteralPath $dstDir)) { throw "mods\Aurie 폴더가 없습니다. tools\setup-aurie.ps1 을 다시 실행하세요: $dstDir" }
$dst = Join-Path $dstDir 'NlToyBox.dll'
Copy-Item -LiteralPath $src -Destination $dst -Force
Write-Host "deployed -> $dst  ($($dll.LastWriteTime))"
```

- [ ] **Step 2: `tools/check-load.ps1` 작성**

```powershell
param(
    [int]$TimeoutSec = 180,
    [switch]$KeepRunning
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

Assert-NlGameNotRunning
$gameDir = Get-NlGameDir
$log = Join-Path $gameDir 'mods\Aurie\NlToyBox.log'
if (-not (Test-Path -LiteralPath (Join-Path $gameDir 'mods\Aurie\NlToyBox.dll'))) {
    throw 'mods\Aurie\NlToyBox.dll 이 없습니다. 먼저 tools\deploy.ps1 을 실행하세요.'
}
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log -Force }

Write-Host "게임을 켭니다 (steam://rungameid/$($script:NlAppId)). 게임 창을 누르지 마세요."
Start-Process "steam://rungameid/$($script:NlAppId)"

# 모듈이 'probe done' 을 쓸 때까지 기다린다. 게임이 떴다가 사라지면 더 기다리지 않는다.
$deadline = (Get-Date).AddSeconds($TimeoutSec)
$seenProcess = $false
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    if (Test-NlGameRunning) { $seenProcess = $true }
    elseif ($seenProcess) { Write-Host '게임 프로세스가 사라졌습니다.'; break }
    if ((Test-Path -LiteralPath $log) -and (@(Get-Content -LiteralPath $log -ErrorAction SilentlyContinue) -contains 'probe done')) { break }
}

$lines = @()
if (Test-Path -LiteralPath $log) { $lines = @(Get-Content -LiteralPath $log) }

Write-Host '--- NlToyBox.log ---'
if ($lines.Count) { $lines | ForEach-Object { Write-Host $_ } } else { Write-Host '(로그 없음)' }
Write-Host '--------------------'
if (-not $seenProcess) { Write-Host '게임 프로세스를 한 번도 보지 못했습니다.' }

if (-not $KeepRunning) {
    $p = Get-Process -Name $script:NlProcessName -ErrorAction SilentlyContinue
    if ($p) { $p | Stop-Process -Force; Write-Host '게임을 끝냈습니다.' }
}

# 줄 형식은 src/ModuleMain.cpp 가 쓴다 (스펙 §4.3). 'yytk' 줄은 판정에 쓰지 않는다.
$checks = [ordered]@{
    'loaded'  = [bool]($lines -match '^NlToyBox \S+ loaded$')
    'builtin' = $lines -contains 'builtin code_is_compiled = true'
    'script'  = $lines -contains 'script gml_Script_command_line_parameters_init = found'
    'done'    = $lines -contains 'probe done'
}
$failed = @($checks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object { $_.Key })
if ($failed.Count) {
    Write-Host "FAIL: $($failed -join ', ')"
    exit 1
}
Write-Host 'PASS'
exit 0
```

- [ ] **Step 3: 바닐라 상태에서 `deploy`와 `check-load`가 거부하는지 확인**

Task 4가 끝난 상태(바닐라, `mods\` 없음)에서 돌린다. `check-load`는 게임을 켜기 전에 멈춰야 한다.

Run:
```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\deploy.ps1;     "deploy exit=$LASTEXITCODE"
pwsh -NoProfile -File E:\NlToyBox\tools\check-load.ps1; "check exit=$LASTEXITCODE"
"게임 켜짐 = " + [bool](Get-Process Norland -ErrorAction SilentlyContinue)
```
Expected:
- `exe 가 패치되지 않았습니다. 먼저 tools\setup-aurie.ps1 을 실행하세요.`와 `deploy exit=1`
- `mods\Aurie\NlToyBox.dll 이 없습니다. 먼저 tools\deploy.ps1 을 실행하세요.`와 `check exit=1`
- `게임 켜짐 = False`

- [ ] **Step 4: [게임 변경] 설치한 뒤, 낡은 산출물을 `deploy`가 거부하는지 확인 (Review Focus 4)**

Run:
```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\setup-aurie.ps1; "setup exit=$LASTEXITCODE"

# 소스가 산출물보다 새것이 된 상황을 만든다 (빌드가 조용히 실패한 것과 같다).
(Get-Item -LiteralPath E:\NlToyBox\src\ModuleMain.cpp).LastWriteTime = Get-Date
pwsh -NoProfile -File E:\NlToyBox\tools\deploy.ps1; "deploy exit=$LASTEXITCODE"
. E:\NlToyBox\tools\common.ps1
"배포됨: " + (Test-Path -LiteralPath (Join-Path (Get-NlGameDir) 'mods\Aurie\NlToyBox.dll'))
```
Expected: `setup exit=0`, 이어서 `산출물이 소스보다 오래되었습니다. 빌드가 실패했을 수 있습니다.`와 `deploy exit=1`, `배포됨: False`

- [ ] **Step 5: 다시 빌드하고 배포**

Run:
```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1;  "build exit=$LASTEXITCODE"
pwsh -NoProfile -File E:\NlToyBox\tools\deploy.ps1; "deploy exit=$LASTEXITCODE"
git -C E:\NlToyBox status --short     # src/ModuleMain.cpp 가 바뀐 것으로 보이지 않아야 한다 (시각만 바뀌었다)
```
Expected: `build ok -> …`, `build exit=0`, `deployed -> …\mods\Aurie\NlToyBox.dll  (…)`, `deploy exit=0`

- [ ] **Step 6: [게임 실행] 적재 확인 — 이 계획에서 게임을 켜는 유일한 단계**

켜기 전에 사용자에게 알린다: "게임을 한 번 켭니다. 확인이 끝날 때까지 게임 창을 누르지 말아 주세요."

실행 시작 시각을 먼저 적어 둔다(Step 8에서 쓴다).

Run:
```powershell
$started = Get-Date; $started.ToString('o') | Set-Content -LiteralPath E:\NlToyBox\build\check-started.txt
pwsh -NoProfile -File E:\NlToyBox\tools\check-load.ps1 -KeepRunning; "check exit=$LASTEXITCODE"
```
(도구의 시간 제한은 240초 이상으로 준다. 스크립트가 최대 180초를 기다린다.)

Expected:
```
게임을 켭니다 (steam://rungameid/1857090). 게임 창을 누르지 마세요.
--- NlToyBox.log ---
NlToyBox 0.0.1 loaded
yytk 5.0.0
builtin code_is_compiled = true
script gml_Script_command_line_parameters_init = found
probe done
--------------------
PASS
check exit=0
```
`yytk` 줄의 숫자는 판정에 쓰지 않는다. 본 그대로 적어 둔다.

**`FAIL`이거나 `check exit=1`이면 여기서 멈춘다.** 다음을 하고 사용자에게 보고한다. Step 7 이후와 Task 6으로 넘어가지 않는다.
1. 게임이 켜져 있으면 끈다: `Get-Process Norland -ErrorAction SilentlyContinue | Stop-Process -Force`
2. 증거를 모은다: `NlToyBox.log` 전문, `mods\` 아래 파일 목록, 게임 폴더와 `%LOCALAPPDATA%\Strategy`에서 `check-started.txt` 시각 이후 바뀐 파일 목록과 그중 로그 파일의 끝 40줄.
3. `pwsh -NoProfile -File E:\NlToyBox\tools\restore-game.ps1`로 바닐라로 되돌린다.
4. 스펙 §3.2에 따라 보고한다: 무엇이 실패했는지(로그가 없음 / `loaded`만 있음 / 프로브가 `error:…`), 증거, 그리고 다음 순서(B 구성 실측 또는 YYToolkit 소스 빌드)에 대한 제안. 그 뒤의 작업은 새 계획으로 쓴다.

- [ ] **Step 7: 게임이 켜진 채로 도구들이 거부하는지 확인 (Review Focus 2), 그리고 게임을 끈다**

Run:
```powershell
. E:\NlToyBox\tools\common.ps1
$before = (Get-NlExeInfo).Sha256
foreach ($s in 'deploy.ps1', 'restore-game.ps1', 'setup-aurie.ps1', 'check-load.ps1') {
    pwsh -NoProfile -File "E:\NlToyBox\tools\$s" 2>&1 | Select-Object -Last 1
    "$s exit=$LASTEXITCODE"
}
"exe 그대로 = " + ($before -eq (Get-NlExeInfo).Sha256)

Get-Process Norland -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 3
"게임 꺼짐 = " + (-not (Get-Process Norland -ErrorAction SilentlyContinue))
```
Expected: 네 스크립트 모두 `게임이 실행 중입니다 (PID …). 종료 후 다시 시도하세요.`가 든 줄과 `exit=1`. `exe 그대로 = True`, `게임 꺼짐 = True`

- [ ] **Step 8: 실행 중 Aurie·YYToolkit이 만든 파일을 찾아 복원 목록에 넣는다**

Run:
```powershell
. E:\NlToyBox\tools\common.ps1
$g = Get-NlGameDir
$started = [datetime](Get-Content -LiteralPath E:\NlToyBox\build\check-started.txt)
$known = 'imgui.ini', 'Norland_DLL_log.txt', 'mods\Aurie\NlToyBox.log', 'mods\Aurie\NlToyBox.dll', 'mods\Aurie\YYToolkit.dll', 'mods\Native\AurieCore.dll', 'Norland.exe'
Get-ChildItem -LiteralPath $g -Recurse -File |
    Where-Object { $_.LastWriteTime -ge $started -or $_.CreationTime -ge $started } |
    ForEach-Object { $_.FullName.Substring($g.Length + 1) } |
    Where-Object { $known -notcontains $_ }
```
Expected: 아무것도 나오지 않거나, Aurie·YYToolkit이 만든 파일(로그·설정 등)의 상대경로가 나온다.

- 아무것도 나오지 않으면 이 단계는 끝이다.
- 경로가 나오면, 그 경로들을 `tools/restore-game.ps1`의 `$ours` 배열에 한 줄씩 더한다(예: `'mods\Aurie\aurie.log',`). 그 파일이 `mods\`의 새 하위 폴더 안에 있으면, 폴더를 비운 뒤 지우는 `foreach ($rel in 'mods\Native', 'mods\Aurie', 'mods')` 목록의 맨 앞에 그 하위 폴더도 더한다. 같은 경로들을 스펙 §4.2의 배치 그림에 더한다. 나온 경로와 그 파일의 앞 20줄은 Task 6에서 `research/`에 적는다.

`imgui.ini`와 `Norland_DLL_log.txt`는 게임이 원래 쓰는 파일이므로 지우지 않는다.

- [ ] **Step 9: 커밋**

```powershell
git -C E:\NlToyBox branch --show-current
git -C E:\NlToyBox add -- tools/deploy.ps1 tools/check-load.ps1 tools/restore-game.ps1 docs/superpowers/specs/2026-10-04-nltoybox-phase0-workspace-design.md
git -C E:\NlToyBox status --short
git -C E:\NlToyBox commit -m "feat(tools): 모듈 배포와 적재 확인" -m "deploy 는 게임 실행 중·미패치·낡은 산출물을 거부한다. check-load 는 게임을 켜서 NlToyBox.log 로 판정한다." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
(Step 8에서 바꾼 것이 없으면 `restore-game.ps1`과 스펙은 스테이징되지 않는다. 그대로 커밋한다.)

---

### Task 6: 기록과 마무리

**Files:**
- Create: `E:\NlToyBox\research\00-game-structure.md`
- Create: `E:\NlToyBox\CLAUDE.md`
- Create: `E:\NlToyBox\README.md`

**Interfaces:**
- Consumes: Task 1~5의 모든 도구와, 다음 단계에서 본 출력
  - Task 4 Step 9: `원본과 같음 = …`, `섹션 = …`
  - Task 5 Step 6: `NlToyBox.log` 전문 (`yytk` 줄 포함)
  - Task 5 Step 8: 실행 중 생긴 파일 목록
- Produces: 문서 세 편, `develop`에 병합된 `chore/workspace-setup`

- [ ] **Step 1: [게임 변경] 복원이 바닐라를 되살리는지 확인 (스펙 §7의 4번)**

Run:
```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\restore-game.ps1; "exit=$LASTEXITCODE"
pwsh -NoProfile -File E:\NlToyBox\tools\game-status.ps1
```
Expected: `restore ok`, `exit=0`, `남김` 줄 없음. `game-status`가 `exe SHA256 : 609C17CFD812E90C7D3E84D3EB730EC559EA3976F30A239395922F604516863E`, `패치 여부  : 바닐라 (.aurie 섹션 없음)`, `mods\      : (없음)`을 낸다.

`남김` 줄이 나오면 Task 5 Step 8을 다시 한다(목록에서 빠진 파일이 있다).

- [ ] **Step 2: `research/00-game-structure.md` 작성**

아래 내용으로 쓴다. "Phase 0 실측" 표의 값 칸에는 괄호 안에 적힌 단계에서 본 출력을 그대로 옮겨 적는다.

```markdown
# 00 — 게임 구조와 Phase 0 실측

조사일 2026-10-04. 대상: Norland `0.5588.9777.0` (Steam buildid `25211575`).
다시 재려면 `tools/game-status.ps1`과 `tools/re/*.py`를 쓴다.

## 설치

| 항목 | 값 |
|---|---|
| 설치 경로 | `E:\SteamLibrary\steamapps\common\Norland Story Generating Strategy` |
| `Norland.exe` | 129,225,728 B, SHA256 `609C17CFD812E90C7D3E84D3EB730EC559EA3976F30A239395922F604516863E` |
| 빌드 날짜 / 구성 | 2026-09-09 / `SteamRelease` |
| 세이브·설정 | `%LOCALAPPDATA%\Strategy` (GEN8 프로젝트 이름이 `Strategy`) |
| 게임이 실행 때 쓰는 파일 | 게임 폴더의 `imgui.ini`, `Norland_DLL_log.txt` |

## 엔진

- GameMaker **YYC**. `data.win`에 `CODE`·`VARI`·`FUNC` 청크가 없다. GML 디컴파일은 불가능하다.
- `data.win`: `SCPT` 40,813개, `OBJT` 64개, `STRG` 49,102개. 280MB 가운데 270MB가 `TXTR`다.
- 스크립트 이름(`gml_Script_*`)은 `data.win`의 문자열 표와 exe 양쪽에 남아 있다.
- exe에 `2023.4.0.113` 문자열이 있다. 런타임 버전으로 보이나 확인하지 않았다.
- 동봉 확장: `Imguigml_x64.dll`, `Steamworks_x64.dll`, `hardware_info.dll`, `YYRunnerExperiments.dll`.

## 느슨한 데이터 파일

exe가 파일 이름을 직접 참조한다. JSON 약 4,700개와 CSV 9개.

| 위치 | 내용 |
|---|---|
| `building_constructor/` | JSON 4,099개 (건물·에셋·그래프, `NewWorld.json`) |
| `actor_constructor/` | JSON 269개 (배너, 색 모음) |
| `knowledge/technology/` | JSON 121개 |
| `sounds/` | JSON 201개 |
| `localization/` | CSV 8개 (18.9MB), `locale_definition.json` |
| 최상위 | `gameplay_variables.json`, `battle_params.json`, `director_params.json`, `debug_params.json`, `NewWorldParams.csv` |

## 모드 지원

공식 지원이 없다. `mods` 폴더, 워크숍 구독, exe 안의 모드 로더 문자열이 없다.
`data.win` 문자열 표의 `steam_ugc_*`는 Steamworks 확장이 내보내는 함수 이름일 뿐이다.

## 내장 디버그 도구 (Phase 1 후보)

릴리스 빌드에 디버그 스크립트가 들어 있다. 켜는 방법은 확인하지 않았다.

- 창: `imgui_debug_actor`, `imgui_debug_building`, `imgui_debug_province`, `imgui_debug_managers`,
  `imgui_debug_system_window`, `imgui_debug_main_menu_window`, `imgui_debug_prices_setup`,
  `imgui_debug_traits`, `imgui_debug_animal`, `imgui_debug_console_log`
- 동작: `debug_spawn_army`
- 게이트로 보이는 이름: `is_debug_forced`, `current_debug_mode`, `current_debug_mode_on_init`,
  `command_line_parameters_init`, `CommandLineParameter`, `CommandLineParametersOperator`
- 빌드 구성 이름: `SteamRelease`, `SteamReleaseForTesters`, `SteamReleaseForOpenBeta`

## Phase 0 실측

| 항목 | 값 |
|---|---|
| 붙은 구성 | Aurie v2.0.2 + YYToolkit v5.0.0c (헤더: `experimental` `d5cc0078`) |
| `NlToyBox.log` 전문 | (Task 5 Step 6의 로그 다섯 줄) |
| YYToolkit이 보고한 버전 | (그 로그의 `yytk` 줄) |
| Steam으로 켰을 때 패치된 exe가 실행되는가 | (Task 5 Step 6이 PASS면 "실행된다") |
| 실행 중 Aurie·YYToolkit이 만든 파일 | (Task 5 Step 8의 출력. 없으면 "없음") |
| `AuriePatcher remove`가 원본을 바이트 단위로 되살리는가 | (Task 4 Step 9의 `원본과 같음` 값과 섹션 목록) |
| 복원 뒤 exe 해시 | `609C17CF…863E`와 일치 (Task 6 Step 1) |

## 알아 둘 것

- YYToolkit의 git 태그 `v5.0.0c`(와 `v5.0.0b`)는 `stable` 브랜치의 v4 시절 커밋 `86c133dc`에 붙어 있다.
  v5 소스는 `experimental` 브랜치다. 헤더를 태그로 받으면 v4 헤더가 온다.
- `ExamplePlugin/include`의 헤더 사본은 `experimental`에서도 정본(`YYToolkit/source/YYTK/Shared`,
  `YYToolkit/include`)과 내용이 다르다. 정본을 쓴다.
- v5 인터페이스에는 `Print` 계열이 없다. 출력은 Aurie의 `DbgPrintEx`로 한다.
- YYToolkit은 `ModuleEntrypoint` 단계에서 인터페이스(`YYTK_ZeusMain`)를 만든다.
  모듈의 `ModuleInitialize`에서는 적재 순서와 무관하게 얻을 수 있다.
- AuriePatcher는 넘겨준 `AurieCore.dll`의 경로를 exe 안에 적어 둔다. 게임 폴더를 옮기면 다시 패치한다.
```

- [ ] **Step 3: `CLAUDE.md` 작성**

```markdown
# NlToyBox

Norland (Steam appid 1857090) 네이티브 코드 모드 작업 공간. Aurie + YYToolkit 위에
C++ 모듈 `NlToyBox.dll`을 올린다.

- 설계: `docs/superpowers/specs/2026-10-04-nltoybox-phase0-workspace-design.md`
- 조사 기록: `research/00-game-structure.md` (먼저 읽을 것)

## 브랜치 전략

`main`은 보호 브랜치다. 직접 커밋·푸시하지 않는다.

    main ← develop ← feat/* | fix/* | chore/* | docs/*

- 작업 브랜치는 `develop`에서 분기하고 `develop`으로 통합한다.
- `main`에는 릴리스 시점에 `develop`에서만 들어간다.
- 원격 저장소가 없다. PR 대신 로컬 merge commit(`git merge --no-ff`)을 쓴다.
  원격을 만들면 2단계 PR로 바꾸고 이 절을 고친다.
- CI가 없다. 코드가 바뀌면 머지 전에 `tools/build.ps1` 성공과
  `tools/check-load.ps1` 종료 코드 0을 확인한다. 문서만 바뀌면 생략해도 된다.
- 머지한 브랜치는 지운다(`git branch -d`).

## 경로

- 게임 경로는 환경변수 `NORLAND_GAME_DIR` 우선, 없으면 `tools/common.ps1`의 기본값.
  **경로를 스크립트에 직접 적지 않는다.** `Get-NlGameDir`를 쓴다.
- 게임 경로에 공백이 있다. PowerShell에서는 `-LiteralPath`를 쓴다.
- 세이브·설정은 `%LOCALAPPDATA%\Strategy`. 도구는 이 폴더를 건드리지 않는다.
- 작업 디렉토리가 게임 폴더로 열려 있어도 소스와 git은 `E:\NlToyBox`에 있다.
  git 명령은 `git -C E:\NlToyBox`로 쓴다.

## 저작물 경계

`refs/`(게임 파일에서 뽑은 문자열)와 `backups/`(`Norland.exe` 원본)는 Long Jaunt의 저작물이다.
`downloads/`는 제3자 배포물이다. **커밋하지 않는다.** 커밋 전에 확인한다:

    git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"

결과가 비어 있어야 한다. 셋 다 도구로 다시 만든다(`tools/re/*.py`, `tools/setup-aurie.ps1`).

## 개발 루프

    pwsh -File tools/build.ps1         # build\NlToyBox.dll
    pwsh -File tools/deploy.ps1        # → <게임>\mods\Aurie\
    pwsh -File tools/check-load.ps1    # 게임을 켜서 NlToyBox.log 로 판정, 끝나면 끈다

- 스크립트는 `pwsh`(PowerShell 7)로 실행한다. Windows PowerShell 5.1은 BOM 없는 UTF-8의
  한글을 깨뜨린다.
- 모듈 변경은 게임을 다시 켜야 적용된다. 배포는 게임을 끈 상태에서 한다.
- `check-load.ps1`이 찾는 줄 형식은 `src/ModuleMain.cpp`가 쓴다. 한쪽을 바꾸면 다른 쪽도 바꾼다.
- 게임을 켜는 확인은 한 번에 몰아서 하고 끝나면 바로 끈다. 켜기 전에 사용자에게
  게임 창을 누르지 말라고 알린다.

## 게임에 가하는 변경

- `tools/setup-aurie.ps1`: `Norland.exe` 패치(원본은 `backups/`에 보관),
  `mods\Native\AurieCore.dll`, `mods\Aurie\YYToolkit.dll`.
- `tools/restore-game.ps1`: 같은 버전의 백업으로 exe를 덮어쓰고 `mods\`에서 이 레포가 놓은
  파일만 지운다.
- 게임 갱신이나 Steam 무결성 검사 뒤에는 `tools/game-status.ps1`로 패치가 남았는지 보고
  `tools/setup-aurie.ps1`을 다시 돌린다. 그 뒤 `tools/check-load.ps1`로 다시 확인한다.

## 의존

- Aurie v2.0.2, YYToolkit v5.0.0c 릴리스 바이너리. 출처·크기·SHA256은 `tools/pins.json`.
- 헤더는 서브모듈 `external/YYToolkit` (`experimental` 브랜치 `d5cc0078`).
  **git 태그 `v5.0.0c`를 체크아웃하지 않는다.** 그 태그는 v4 헤더를 가리킨다.
  새로 클론했으면 `git submodule update --init` 먼저.
- 서브모듈 안의 파일은 고치지 않는다.
- Aurie와 YYToolkit은 AGPL-3.0이다. 레포 공개나 모듈 배포 전에 다시 검토한다.

## 도구

- Python은 `py -3.14`. `WindowsApps\python.exe`는 스토어 스텁이라 멈춘다.
- cmake·ninja는 PATH에 없다. `tools/build.ps1`이 Build Tools 동봉본을 찾아 쓴다.

## 작업 규칙

- 원인은 실측(로그·해시·덤프)으로 확인한 뒤 보고한다. 추정을 결론처럼 쓰지 않는다.
- 게임 버전이 바뀌면 `research/00-game-structure.md`의 수치를 다시 잰다.
```

- [ ] **Step 4: `README.md` 작성**

```markdown
# NlToyBox

Norland용 네이티브 코드 모드 작업 공간. Aurie + YYToolkit 위에 C++ 모듈을 올린다.

## 필요한 것

- Norland (Steam), Windows x64
- Visual Studio Build Tools 2022 (C++ 도구, 동봉 CMake·Ninja)
- PowerShell 7 (`pwsh`), git
- (조사 도구만) Python 3.14

## 처음 한 번

    git submodule update --init
    pwsh -File tools/setup-aurie.ps1     # Aurie·YYToolkit 을 받아 게임에 놓고 exe 를 패치한다

`setup-aurie.ps1`은 `Norland.exe` 원본을 `backups/`에 보관한 뒤 패치한다.

## 한 바퀴

    pwsh -File tools/build.ps1
    pwsh -File tools/deploy.ps1
    pwsh -File tools/check-load.ps1

`check-load.ps1`은 게임을 켜고 `mods\Aurie\NlToyBox.log`를 읽어 `PASS`/`FAIL`을 낸 뒤 게임을 끈다.

## 바닐라로 되돌리기

    pwsh -File tools/restore-game.ps1

## 상태 보기

    pwsh -File tools/game-status.ps1

게임 경로가 기본값과 다르면 환경변수 `NORLAND_GAME_DIR`로 지정한다.

## 문서

- `docs/superpowers/specs/` — 설계
- `research/00-game-structure.md` — 게임 구조와 실측 기록
- `CLAUDE.md` — 레포 규칙
```

- [ ] **Step 5: 추적 경계 확인 (스펙 §7의 5번)**

Run:
```powershell
git -C E:\NlToyBox add -- research/00-game-structure.md CLAUDE.md README.md
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"
git -C E:\NlToyBox status --short
```
Expected: 둘째 명령의 출력이 비어 있다. 셋째 명령에는 방금 스테이징한 세 파일만 `A`로 보인다.

- [ ] **Step 6: 커밋**

```powershell
git -C E:\NlToyBox branch --show-current
git -C E:\NlToyBox commit -m "docs: 게임 구조 조사 기록, 레포 규칙, 사용법" -m "Phase 0 실측 결과(붙은 구성, YYToolkit 버전, 실행 중 생긴 파일, AuriePatcher remove 결과) 포함." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 7: `develop`에 병합**

병합 전 확인: Task 3 Step 6의 빌드 성공과 Task 5 Step 6의 `check exit=0`을 이 브랜치에서 보았다.

```powershell
git -C E:\NlToyBox status --short                 # 비어 있어야 한다
git -C E:\NlToyBox switch develop
git -C E:\NlToyBox merge --no-ff chore/workspace-setup -m "Merge branch 'chore/workspace-setup' into develop" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git -C E:\NlToyBox branch -d chore/workspace-setup
git -C E:\NlToyBox log --oneline --graph -12
git -C E:\NlToyBox branch -a
```
Expected: 머지 커밋이 `develop`의 맨 위에 있고, 브랜치는 `develop`(현재)과 `main`만 남는다. `main`은 루트 커밋 `531f239`에 그대로 있다.

- [ ] **Step 8: 게임을 어떤 상태로 둘지 사용자에게 묻는다**

지금 게임은 바닐라다(Step 1). 사용자에게 묻는다:

- **개발 가능한 상태로 둔다** → `pwsh -NoProfile -File E:\NlToyBox\tools\setup-aurie.ps1`, 이어서 `pwsh -NoProfile -File E:\NlToyBox\tools\deploy.ps1`. `game-status.ps1`로 `패치됨`과 `mods\Aurie\NlToyBox.dll`을 확인한다. 이 상태에서는 게임을 켤 때마다 Aurie와 모듈이 함께 뜬다.
- **바닐라로 둔다** → 아무것도 하지 않는다. 다음에 작업할 때 `setup-aurie.ps1`부터 돌린다.

`main`으로의 릴리스 병합은 이 계획의 범위가 아니다.
