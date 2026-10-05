# 데이터 오버레이 단계 2 (효과 실측) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 새 게임을 시작한 상태에서, 데이터 파일의 값을 바꾸면 게임이 달라지는지를 잰다: 시작 금화의 출처, `product_count`가 시작 자원을 정하는지, 게임을 시작하면 값이 어디로 옮겨지는지.

**Architecture:** 단계 1의 도구를 그대로 쓴다. 값 열 개를 게임 JSON에 없는 수로 바꾸는 프리셋을 입히고, 모듈이 메뉴에서 한 번·그 뒤 60초마다 덤프하게 한 채 사용자가 새 게임을 시작해 화면의 수치를 읽고 직접 끈다. 게임 안에서 뜬 덤프에서 값의 자리를 찾고, 화면의 수치와 맞춰 본다. 도구에는 요청 파일의 앞머리를 받는 옵션과, 게임 파일에 이미 있는 값을 알려 주는 경고만 더한다.

**Tech Stack:** Python 3.14 (표준 라이브러리, `unittest`), PowerShell 7. C++ 모듈은 바꾸지 않는다(`0.2.0`).

**Spec:** `docs/superpowers/specs/2026-10-04-data-overlay-design.md` §5. 앞 단계의 결과는 `research/02-new-game-state.md`(게임 안을 알아보는 신호), `research/04-overlay-verify.md`.

## Global Constraints

- **추측으로 작업하지 않는다.** 이 계획이 기대는 사실은 스펙 §5.1의 표에 출처가 있다. 거기 없는 사실이 필요해지면 먼저 그것을 확인하고 출처를 원장과 표에 적는다. 확인할 수 없으면 "모른다"로 두고 그 위에 다음 단계를 쌓지 않는다. 결과를 적을 때 본 것과 미루어 본 것을 가른다.
- **Task 0~3은 게임을 켜지 않고 실제 게임 폴더에 쓰지 않는다.** 실제 게임에 대고 돌려도 되는 것은 `overlay.ps1`의 `status`, `check`, `scan`, `keys`, `probe-request`(레포나 임시 폴더에 쓴다)다.
- 게임을 켜는 것은 Task 4의 **1회**다. 예비 1회는 그때 사용자에게 다시 승인받는다. 켜기 전에 사용자가 할 일을 차례대로 알린다(스펙 §5.6).
- 이번에는 사용자가 새 게임을 시작한다. 게임 안에서는 도구가 게임을 끄지 않는다(사용자가 끈다. `probe.ps1`의 되풀이 요청 동작).
- 새 게임을 시작하는 실행이므로 먼저 `tools/saves-backup.ps1`으로 세이브 폴더의 사본을 뜬다. 세이브 폴더에는 쓰지 않는다. 게임이 만든 파일을 지우는 것은 사용자가 정한다.
- 게임 스크립트를 부르지 않는다(요청에 `script=` 줄이 없다). `budget_money_get` 같은 스크립트는 인자의 형을 모른다.
- 판정에는 게임 안에서 뜬 덤프만 쓴다: `present`에 `o_character`가 있고 `o_main_menu`가 없는 것.
- 카탈로그의 `effect`는 스펙 §5.2의 기준으로만 올린다. 설정을 들고 있는 자리에서 본 것은 효과가 아니다.
- 알림음을 내지 않는다(`probe.ps1`에 `-Beep`을 주지 않는다).
- 저작물 경계: `refs/`, `backups/`, `downloads/`, `build/`는 커밋하지 않는다. 커밋 전에 `git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"`가 비어 있어야 한다. 카탈로그와 프리셋에 게임 파일의 값을 옮겨 적지 않는다. 조사 기록에는 바꾼 키의 원래 값만 적는다.
- Python은 `py -3.14`, 스크립트는 `pwsh`. 게임 경로는 `Get-NlGameDir`.
- 서브모듈과 모듈 소스(`src/`)는 고치지 않는다.
- 브랜치: `develop`에서 `feat/overlay-effect`를 만들어 작업하고 `git merge --no-ff`로 합친다. `main`은 건드리지 않는다. git 명령은 `git -C E:\NlToyBox`.
- 커밋 메시지는 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`으로 끝낸다.

**계획 승인에 포함해 달라:**

- `tools/check-load.ps1`을 따로 돌리지 않는다. 모듈 소스를 바꾸지 않는다. Task 4의 실행 로그에서 적재 판정 줄을 확인한다(단계 1과 같다).
- 전체 브랜치 검토는 Task 3이 끝난 뒤, 게임을 켜기 전에 받는다. Task 4~5에서 생기는 것은 문서와 카탈로그의 메모·등급뿐이다.

**이 계획의 코드가 어디서 왔는가:** Task 1의 고침은 2026-10-05에 도구의 사본(스크래치)에 먼저 넣어 봤다. 시험만 넣었을 때 `Ran 101 tests`, `FAILED (failures=1, errors=2)`였고 고침까지 넣자 101개가 통과했다. Task 2의 프리셋과 앞머리로 실제 게임 파일에 `check`와 `probe-request`를 돌려(읽기 전용) 값 열 개가 게임 JSON에 없음을 확인했고, 만들어진 요청 파일이 모듈의 읽기 규칙(`nlcore_tests`)을 통과했다.

## Review Focus

1. **요청 파일의 오타.** 앞머리에 모르는 키나 범위 밖의 값이 있으면 모듈이 `dump failed: bad request`를 적고 아무것도 하지 않는다. 사용자의 수고와 게임 실행 한 번을 버린다. → Task 2 Step 3과 Task 4 Step 3의 `test-native.ps1`(`tools/probes/*.txt`를 모듈의 규칙으로 읽는다)
2. **입혀 둔 값을 "게임에 이미 있는 수"로 세는 것.** 프리셋을 입힌 뒤에 요청을 만들므로, 고친 파일을 세면 모든 값에 경고가 붙어 경고가 쓸모없어진다. → Task 1(`test_probe_request_takes_a_header_file_and_says_which_values_the_game_already_has`가 입힌 뒤에 돌린다)
3. **게임에 이미 있는 수를 경고 없이 쓰는 것.** 그런 값은 런타임에서 우연히 맞는다(단계 1의 검토에서 14개가 그랬다). → Task 1(같은 시험), Task 2 Step 4
4. **메뉴에서 뜬 덤프로 "게임 안"을 판정하는 것.** 사용자가 일찍 끄거나 덤프가 게임 화면보다 먼저 뜨면 생긴다. → Task 4 Step 6(덤프마다 `present`를 보고 고른다. 게임 안의 덤프가 없으면 "확인하지 못함")
5. **양성 대조 없이 "없다"를 쓰는 것, 설정을 들고 있는 자리를 효과로 적는 것.** → Task 4 Step 6(`controls`에 양성 대조를 넘긴다), Task 5 Step 2(스펙 §5.2의 표로만 올린다)

## File Structure

| 파일 | 책임 |
|---|---|
| `tools/overlay/verify.py` | `probe_request`가 앞머리 줄들을 받는다 |
| `tools/overlay/cli.py` | `probe-request --header`, 게임 파일에 이미 있는 값의 경고 |
| `tools/overlay.ps1` | `-Header` |
| `tools/overlay/tests/test_verify.py`, `test_cli.py` | 위의 시험 셋 |
| `presets/effect-stage2.json` | 이번 실측의 프리셋(값 열 개, 양성 대조 포함) |
| `tools/probes/stage2-effect.head.txt` | 요청의 앞머리(덤프의 일정, 깊이, watch, 금화를 찾는 줄) |
| `tools/probes/stage2-effect.txt` | 요청(Task 4에서 `probe-request`가 만든다) |
| `research/05-effects.md` | 결과 |
| `catalog/0.5588.9777.0/keys.json`, 스펙, 로드맵, `CLAUDE.md` | 결과의 반영 |

---

### Task 0: 작업 브랜치

- [ ] **Step 1: 브랜치를 만들고 게임이 바닐라인지 본다**

```powershell
git -C E:\NlToyBox switch develop
git -C E:\NlToyBox switch -c feat/overlay-effect
git -C E:\NlToyBox status --short
pwsh -File E:\NlToyBox\tools\game-status.ps1
pwsh -File E:\NlToyBox\tools\overlay.ps1 status
```

Expected: `status --short`가 비어 있다. `패치 여부  : 바닐라`, `실행 여부  : 꺼져 있음`, `mods\      : (없음)`. `overlay.ps1 status`에 `vanilla: 125`, `프리셋: (없음. 바닐라)`.

---

### Task 1: 요청 파일의 앞머리와 값 경고 (`probe-request`)

**Files:**
- Modify: `tools/overlay/verify.py` (`probe_request`)
- Modify: `tools/overlay/cli.py` (`cmd_probe_request`, 인자 하나)
- Modify: `tools/overlay.ps1` (`-Header`)
- Test: `tools/overlay/tests/test_verify.py`, `tools/overlay/tests/test_cli.py`

**Interfaces:**
- Consumes: `store.plan(dirs, cat, preset, game_version) -> (status, data, edits)`, `store.vanilla_bytes(dirs, cat, rel, status)`, `jsonedit.decode`, `jsonedit.scan`, `jsonedit.value_of`, `paths.show`, `compose.Edit`(`new`, `new_text`, `file`, `path`)
- Produces:
  - `verify.DEFAULT_HEADER = ("delay_seconds=60", "max_hits=5000")`
  - `verify.probe_request(edits, header=None) -> str` — `header`는 줄의 목록. 빈 줄은 버리고 줄 끝의 공백을 뗀다
  - `cli.py probe-request --preset F --out F [--header F]` — 끝줄 `probe-request ok: 값 N개 -> <경로> (게임 파일에 없는 수 M개)`. 게임 파일에 이미 있는 값마다 `주의: <값> 은 게임 파일에 이미 K번 있다(예: <파일>). …` 한 줄. 종료 코드는 경고가 있어도 0
  - `pwsh -File tools/overlay.ps1 probe-request -Preset F -Out F [-Header F]`

고치는 법: 아래의 "찾는 글"은 지금의 파일에 정확히 한 번 나온다. 그 자리를 "바꿀 글"로 바꾼다.

- [ ] **Step 1: 실패하는 시험을 쓴다**

`tools/overlay/tests/test_verify.py`에서 찾는 글:

```python
    def test_probe_request_writes_each_value_once(self):
```

바꿀 글:

```python
    def test_probe_request_puts_the_given_header_in_place_of_the_default_one(self):
        header = ["# 새 게임에서 잰다", "delay_seconds=60", "", "repeat_seconds=60  ", "watch=global.a.b"]
        lines = verify.probe_request(EDITS[:1], header).splitlines()
        self.assertEqual(lines[1:], ["# 새 게임에서 잰다", "delay_seconds=60", "repeat_seconds=60", "watch=global.a.b", "find=2345"])
        self.assertNotIn("max_hits=5000", lines)

    def test_probe_request_writes_each_value_once(self):
```

`tools/overlay/tests/test_cli.py`에서 찾는 글:

```python
    def test_verify_does_not_say_absent_on_a_dump_it_cannot_trust(self):
```

바꿀 글:

```python
    def test_probe_request_takes_a_header_file_and_says_which_values_the_game_already_has(self):
        preset = self.root / "odd.json"
        preset.write_text(json.dumps({"name": "Odd", "game_version": support.VERSION, "changes": [
            {"file": "debug.json", "path": "budget_money", "set": 15},             # 15 는 mine 의 나무 비용으로 이미 있다
            {"file": "debug.json", "path": "production_cost.ale", "set": 4.25}]}), encoding="utf-8")
        header = self.root / "head.txt"
        header.write_text("delay_seconds=60\nrepeat_seconds=60\nwatch=global.a.b\n", encoding="utf-8")
        request = self.root / "req.txt"
        self.run_cli("apply", "--preset", str(preset))                           # 입혀 둔 값을 '이미 있다'로 세면 안 된다
        code, out = self.run_cli("probe-request", "--preset", str(preset), "--out", str(request), "--header", str(header))
        self.assertEqual(code, 0, out)
        self.assertEqual(request.read_text(encoding="utf-8").splitlines()[1:],
                         ["delay_seconds=60", "repeat_seconds=60", "watch=global.a.b", "find=15", "find=4.25"])
        self.assertIn("주의: 15 은 게임 파일에 이미 1번 있다(예: debug.json)", out)
        self.assertNotIn("주의: 4.25", out)
        self.assertIn("(게임 파일에 없는 수 1개)", out)

    def test_probe_request_reports_a_missing_header_file(self):
        code, out = self.run_cli("probe-request", "--preset", str(self.preset), "--out", str(self.root / "r.txt"),
                                 "--header", str(self.root / "none.txt"))
        self.assertEqual(code, 1)
        self.assertIn("FAIL: 앞머리 파일을 읽을 수 없다", out)

    def test_verify_does_not_say_absent_on_a_dump_it_cannot_trust(self):
```

- [ ] **Step 2: 시험이 실패하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests`
Expected: `Ran 101 tests`, `FAILED (failures=1, errors=2)`. 실패는 `test_probe_request_puts_the_given_header_in_place_of_the_default_one`, 오류 둘은 `test_probe_request_takes_a_header_file_and_says_which_values_the_game_already_has`와 `test_probe_request_reports_a_missing_header_file`(`--header`를 모르는 인자라 `SystemExit: 2`).

- [ ] **Step 3: `verify.probe_request`를 고친다**

`tools/overlay/verify.py`에서 찾는 글:

```python
def probe_request(edits, delay_seconds=60):
    """edits 의 새 값을 값으로 찾는 요청 파일의 글(tools/probe.ps1 -Request 로 넘긴다).
    배열 안의 값은 런타임 경로에 이름이 남지 않으므로 가장 가까운 키 이름도 찾는다."""
```

바꿀 글:

```python
DEFAULT_HEADER = ("delay_seconds=60", "max_hits=5000")     # 메인 메뉴에서 한 번 덤프한다


def probe_request(edits, header=None):
    """edits 의 새 값을 값으로 찾는 요청 파일의 글(tools/probe.ps1 -Request 로 넘긴다).
    header: 요청의 앞머리 줄들(덤프의 설정, watch, 더 찾을 것). 주지 않으면 DEFAULT_HEADER 다.
    배열 안의 값은 런타임 경로에 이름이 남지 않으므로 가장 가까운 키 이름도 찾는다."""
```

같은 파일에서 찾는 글:

```python
    lines = ["# tools/overlay/cli.py probe-request 가 만들었다. 프리셋이 쓴 값을 메인 메뉴의 런타임에서 찾는다.",
             f"delay_seconds={delay_seconds}", "max_hits=5000"]
    return "\n".join(lines + [f"find={value}" for value in values] + [f"find_name={name}" for name in names]) + "\n"
```

바꿀 글:

```python
    head = list(DEFAULT_HEADER) if header is None else [line.rstrip() for line in header if line.strip()]
    lines = ["# tools/overlay/cli.py probe-request 가 만들었다. 앞머리 아래의 find 줄은 프리셋이 쓴 값이다."]
    return "\n".join(lines + head + [f"find={value}" for value in values] + [f"find_name={name}" for name in names]) + "\n"
```

- [ ] **Step 4: `cli.py`를 고친다**

`tools/overlay/cli.py`에서 찾는 글:

```python
import argparse
import json
```

바꿀 글:

```python
import argparse
import collections
import json
```

같은 파일에서 찾는 글:

```python
def cmd_probe_request(args):
    cat = catalog.load(args.catalog_dir)
    _, _, edits = store.plan(_dirs(args), cat, presets.load(args.preset), args.game_version)
    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(verify.probe_request(edits), encoding="utf-8", newline="\n")
    print(f"probe-request ok: 값 {len(edits)}개 -> {out}")
```

바꿀 글:

```python
def _vanilla_numbers(dirs, cat, status):
    """게임 폴더의 모든 *.json 에 든 수를 (상대경로, 값)으로 낸다. 카탈로그의 파일은 바닐라의 것을 본다
    (프리셋이 입혀져 있어도 그 프리셋의 값을 세지 않는다)."""
    for path in sorted(dirs.game.rglob("*.json")):
        rel = path.relative_to(dirs.game).as_posix()
        try:
            raw = store.vanilla_bytes(dirs, cat, rel, status) if rel in cat.files else path.read_bytes()
            _, text = jsonedit.decode(raw)
            slots = jsonedit.scan(text)
        except (OverlayError, OSError):
            continue                # 게임의 것이 아니거나 읽을 수 없는 JSON. scan 명령이 따로 알려 준다
        for slot in slots:
            if slot.kind == "number":
                yield rel, jsonedit.value_of(text, slot)


def cmd_probe_request(args):
    cat = catalog.load(args.catalog_dir)
    dirs = _dirs(args)
    status, _, edits = store.plan(dirs, cat, presets.load(args.preset), args.game_version)
    header = None
    if args.header:
        try:
            header = pathlib.Path(args.header).read_text(encoding="utf-8-sig").splitlines()
        except (OSError, UnicodeDecodeError) as error:
            raise OverlayError(f"앞머리 파일을 읽을 수 없다: {args.header}: {error}") from None
    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(verify.probe_request(edits, header), encoding="utf-8", newline="\n")
    # 값이 게임 파일에 이미 있는 수면 런타임에서 우연히 맞을 수 있다(스프라이트 번호, 좌표). 알려 준다.
    wanted = {edit.new for edit in edits}
    seen, first = collections.Counter(), {}
    for rel, value in _vanilla_numbers(dirs, cat, status):
        if value in wanted:
            seen[value] += 1
            first.setdefault(value, rel)
    for edit in edits:
        if seen[edit.new]:
            print(f"주의: {edit.new_text} 은 게임 파일에 이미 {seen[edit.new]}번 있다(예: {first[edit.new]}). "
                  f"런타임에서 우연히 맞을 수 있다 — {edit.file} {paths.show(edit.path)}")
    rare = sum(1 for edit in edits if not seen[edit.new])
    print(f"probe-request ok: 값 {len(edits)}개 -> {out} (게임 파일에 없는 수 {rare}개)")
```

같은 파일에서 찾는 글:

```python
    parser.add_argument("--file")
```

바꿀 글:

```python
    parser.add_argument("--file")
    parser.add_argument("--header")
```

같은 파일의 맨 위 설명에서 `  probe-request  프리셋이 쓰는 값을 런타임에서 찾는 요청 파일을 만든다` 줄을 다음 두 줄로 바꾼다:

```
  probe-request  프리셋이 쓰는 값을 런타임에서 찾는 요청 파일을 만든다. --header 로 앞머리(덤프의 일정, watch,
                 더 찾을 것)를 줄 수 있다. 값이 게임 파일에 이미 있는 수면 알려 준다
```

- [ ] **Step 5: 시험이 통과하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests`
Expected: `Ran 101 tests`, `OK`

- [ ] **Step 6: 래퍼에 `-Header`를 더한다**

`tools/overlay.ps1`에서 찾는 글:

```powershell
    [string]$File,
    [string]$CatalogDir
)
```

바꿀 글:

```powershell
    [string]$File,
    [string]$Header,
    [string]$CatalogDir
)
```

같은 파일에서 찾는 글:

```powershell
foreach ($pair in @(@('--preset', $Preset), @('--out', $Out), @('--dump', $Dump), @('--file', $File))) {
```

바꿀 글:

```powershell
foreach ($pair in @(@('--preset', $Preset), @('--out', $Out), @('--dump', $Dump), @('--file', $File), @('--header', $Header))) {
```

- [ ] **Step 7: 래퍼가 그대로 도는지 본다**

Run: `pwsh -File E:\NlToyBox\tools\tests\safety.tests.ps1`
Expected: 마지막 줄 `safety tests: 24 passed`. (래퍼가 `-Header`를 넘기는 것은 Task 2 Step 4에서 실제 명령으로 본다.)

- [ ] **Step 8: 커밋한다**

```powershell
git -C E:\NlToyBox add tools/overlay/verify.py tools/overlay/cli.py tools/overlay.ps1 tools/overlay/tests/test_verify.py tools/overlay/tests/test_cli.py
git -C E:\NlToyBox commit -m @'
feat(overlay): probe-request 가 앞머리를 받고, 게임 파일에 이미 있는 값을 알려 준다

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 2: 효과 실측의 프리셋과 요청의 앞머리

**Files:**
- Create: `presets/effect-stage2.json`
- Create: `tools/probes/stage2-effect.head.txt`

**Interfaces:**
- Consumes: `tools/overlay.ps1`의 `check`, `probe-request -Header`(Task 1), `tools/test-native.ps1`(요청 파일을 모듈의 규칙으로 읽는다), `tools/overlay/tests/test_repo_data.py`(레포의 프리셋이 읽히는지)
- Produces: `presets/effect-stage2.json`(변경 10개, 파일 3개. 실험 키 없음), `tools/probes/stage2-effect.head.txt`. Task 4가 쓴다

값을 고른 근거는 스펙 §5.4다. 값을 바꾸려면 Step 4의 `probe-request`가 경고 없이 끝나는 것을 다시 본다.

- [ ] **Step 1: 프리셋을 쓴다**

`presets/effect-stage2.json`:

```json
{
  "name": "effect-stage2",
  "game_version": "0.5588.9777.0",
  "changes": [
    { "file": "gameplay_variables.json", "path": "global_map.ai_economy.initial_budget", "set": 1006 },

    { "file": "debug_params.json", "path": "budget_money", "set": 2767 },
    { "file": "debug_params.json", "path": "product_count.wood", "set": 1031 },
    { "file": "debug_params.json", "path": "product_count.carrot", "set": 1057 },

    { "file": "debug_params.json", "path": "production_cost.ale", "set": 2.3719 },
    { "file": "debug_params.json", "path": "fair_trade.fair_trade_purchase.ale", "set": 41.3719 },
    { "file": "debug_params.json", "path": "fair_trade.fair_trade_sale.ale", "set": 31.2917 },
    { "file": "debug_params.json", "path": "building_resources.woodcutter_lvl_1[0][1]", "set": 1101 },
    { "file": "debug_params.json", "path": "building_duration_factor", "set": 0.5719 },
    { "file": "battle_params.json", "path": "battle_dodge_shift_better", "set": 11.4373 }
  ]
}
```

- [ ] **Step 2: 요청의 앞머리를 쓴다**

`tools/probes/stage2-effect.head.txt`:

```
# 단계 2 (효과 실측): 새 게임을 시작한 상태에서 잰다. 사용자가 새 게임을 시작하고, 잠시 둔 뒤 직접 끈다.
# 앞머리다. tools/overlay.ps1 probe-request -Header 가 이 아래에 프리셋의 값을 찾는 줄을 붙여 요청 파일을 만든다.
delay_seconds=60
repeat_seconds=60
keep_last=3
# 시작 금화가 든 변수는 단계 0b 의 덤프(깊이 6)의 한 단계 목록에 없었다. 더 깊이 본다. 방문 한도는 구역마다 따로 센다.
# 걸리는 시간은 모른다(깊이 6 에서 2초였다). 이번에 잰다.
max_depth=8
max_visited=8000000
max_hits=20000
# 새 게임의 흐름 (research/02 의 신호)
watch=global.__new_game_initializer.__is_active
watch=global.__new_game_initializer.__current_step
watch=global.__new_game_initializer.__choosed_province
watch=global.__game_load_operator.__game_is_loading
# 시작 금화의 출처. 화면의 금화는 3000 이었다(research/02). 이름으로도 찾는다(부분 일치).
find=3000
find_name=budget
find_name=money
find_name=difficulty
find_name=game_conditions
# 시작 자원이 옮겨지는 자리 (research/02)
find_name=default_resource_count
```

- [ ] **Step 3: 레포의 시험이 둘을 읽는지 본다**

```powershell
py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests
pwsh -File E:\NlToyBox\tools\test-native.ps1
```

Expected: `Ran 101 tests`, `OK`(프리셋이 읽히고 카탈로그가 아는 파일만 가리킨다). `ok - tools/probes 의 요청 파일은 모두 오류 없이 읽힌다`, `core tests: 18 passed`(앞머리가 모듈의 규칙에 맞는다).

- [ ] **Step 4: 실제 게임 파일에 대고 읽기만 하는 명령을 돌린다**

```powershell
pwsh -File E:\NlToyBox\tools\overlay.ps1 check -Preset E:\NlToyBox\presets\effect-stage2.json
pwsh -File E:\NlToyBox\tools\overlay.ps1 probe-request -Preset E:\NlToyBox\presets\effect-stage2.json -Header E:\NlToyBox\tools\probes\stage2-effect.head.txt -Out $env:TEMP\nl-stage2-preview.txt
Get-Content -LiteralPath $env:TEMP\nl-stage2-preview.txt | Select-String '^find'
Remove-Item -LiteralPath $env:TEMP\nl-stage2-preview.txt
```

Expected:

- `check`: 열 줄, 끝에 `변경 10개, 파일 3개. 런타임 반영을 확인하지 않은 키 1개`(`product_count.carrot`이 `SEEN`이다), `check ok (쓰지 않았다)`. `실험`이라는 말이 든 줄이 없다.
- `probe-request`: `probe-request ok: 값 10개 -> … (게임 파일에 없는 수 10개)`. **`주의:` 줄이 없다.** 있으면 그 값을 스펙 §5.1의 "없는 정수"에서 다시 고르고 스펙 §5.4의 표와 함께 고친다.
- `find` 줄: 앞머리의 `find=3000`과 `find_name=` 다섯 줄, 그 아래 `find=11.4373`, `find=2.3719`, `find=1101`, `find=0.5719`, `find=41.3719`, `find=31.2917`, `find=1057`, `find=1031`, `find=2767`, `find=1006`, 끝에 `find_name=woodcutter_lvl_1`.

게임 폴더와 `backups\`에는 쓰지 않는다.

- [ ] **Step 5: 커밋한다**

```powershell
git -C E:\NlToyBox add presets/effect-stage2.json tools/probes/stage2-effect.head.txt
git -C E:\NlToyBox commit -m @'
feat(overlay): 효과 실측의 프리셋과 요청의 앞머리

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 3: 문서와 실행 전 확인

**Files:**
- Modify: `CLAUDE.md`

**Interfaces:**
- Consumes: Task 1~2의 산출물
- Produces: 없음(문서)

- [ ] **Step 1: `CLAUDE.md`를 고친다**

"데이터 오버레이" 절에서 `- 카탈로그와 프리셋에는 게임 파일의 값을 옮겨 적지 않는다(키 이름, 등급, 해시만).`으로 시작하는 항목 **앞에** 더한다:

```
- 새 게임을 시작한 상태에서 재려면 요청의 앞머리(덤프의 일정, 깊이, `watch`)를 `tools/probes/*.head.txt`에 쓰고
  `overlay.ps1 probe-request -Header`로 요청 파일을 만든다(`tools/probes/stage2-effect.head.txt`가 본이다). `probe-request`가
  `주의:`로 알리는 값은 게임 파일에 이미 있는 수다. 런타임에서 우연히 맞으니 다른 수로 고른다(0~999의 정수는 거의 다 있다).
- 카탈로그의 `effect`를 `TESTED`로 올리는 기준은 스펙 §5.2다. 값이 `o_debug`나 `o_province_controller` 같은, 설정을 들고 있는 자리로
  옮겨진 것은 효과가 아니다.
```

- [ ] **Step 2: 전체 확인을 돌린다**

```powershell
pwsh -File E:\NlToyBox\tools\build.ps1
pwsh -File E:\NlToyBox\tools\test-native.ps1
pwsh -File E:\NlToyBox\tools\tests\safety.tests.ps1
py -3.14 -m unittest discover -s E:\NlToyBox\tools\re\tests
py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"
```

Expected, 차례로: `build ok`가 든 줄, `core tests: 18 passed`, `safety tests: 24 passed`, `Ran 9 tests` + `OK`, `Ran 101 tests` + `OK`, 마지막은 비어 있다.

- [ ] **Step 3: 커밋한다**

```powershell
git -C E:\NlToyBox add CLAUDE.md
git -C E:\NlToyBox commit -m @'
docs: 새 게임 상태에서 재는 법과 효과의 기준

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

- [ ] **Step 4: 게임을 켜기 전에 전체 브랜치 검토를 받는다**

실행 방식이 정한 최종 검토를 여기서 한다(Global Constraints). 범위는 `git -C E:\NlToyBox merge-base develop HEAD`부터 `HEAD`까지다. 검토자에게 이 계획의 Review Focus와 스펙 §5를 주고, 특히 프리셋의 값과 앞머리가 게임을 몇 분 돌리는 데 무리가 없는지, Task 4의 판정 절차에 구멍이 없는지를 묻는다. Critical·Important는 실패하는 시험을 먼저 쓰고(코드일 때) 고친 뒤 Step 2를 다시 돌린다.

---

### Task 4: 실측 실행 — 새 게임에서

**게임을 한 번 켠다. 사용자가 새 게임을 시작하고, 화면의 수치를 읽고, 직접 끈다.**

**Files:**
- Create: `tools/probes/stage2-effect.txt` (`probe-request`가 만든다)

**Interfaces:**
- Consumes: `presets/effect-stage2.json`, `tools/probes/stage2-effect.head.txt`(Task 2), `tools/overlay.ps1`, `tools/probe.ps1`(되풀이 요청: 게임이 꺼질 때까지 기다린다), `tools/saves-backup.ps1`, `tools/re/dump_tool.py`
- Produces: `refs\runtime\stage2-effect.{menu,late0,late1,late2}.json`, `refs\runtime\stage2-effect.probe.txt`(추적 안 함), 사용자가 읽은 수치

- [ ] **Step 1: 사용자에게 알리고 승인을 받는다**

> 게임을 한 번 켭니다. 이번에는 해 주실 일이 있습니다(모두 6~7분).
>
> 1. 메인 메뉴가 뜨면 **1분쯤 기다려** 주세요. 화면이 한 번 멈췄다 풀립니다(메뉴 덤프). 그 뒤에 **새 게임을 시작**해 주세요. 설정 화면에서 고르신 **지역과 난이도**를 기억해 주세요(기본값 그대로면 그대로라고).
> 2. 게임 화면이 나오면 화면의 **금화, 나무, 당근, 약**의 수치를 읽어 주세요. 건설 메뉴에서 벌목꾼 건물의 나무 비용이 보이면 그것도요(선택).
> 3. 게임 화면에서 **3분쯤** 그대로 두세요. 60초마다 화면이 멈췄다 풀립니다. 이번에는 더 깊이 훑어서 한 번에 수십 초까지 멈출 수 있습니다(얼마나 걸리는지도 이번에 잽니다). 알림음은 나지 않습니다.
> 4. **평소처럼 게임을 꺼** 주세요. 그 뒤 읽으신 수치와, 게임 화면에 들어간 때와 끈 때를 대강 알려 주세요.
>
> 데이터 파일의 값 열 개가 바뀐 채로 켜집니다(시작 나무 1031, 당근 1057 등). 끝나면 바닐라로 되돌립니다. 새 게임이 세이브를 만들 수 있어 먼저 세이브 폴더의 사본을 뜹니다.

승인이 없으면 여기서 멈춘다. Task 0~3의 결과는 그대로 `develop`에 합칠 수 있다.

- [ ] **Step 2: 준비한다**

```powershell
pwsh -File E:\NlToyBox\tools\saves-backup.ps1
pwsh -File E:\NlToyBox\tools\build.ps1
pwsh -File E:\NlToyBox\tools\setup-aurie.ps1
pwsh -File E:\NlToyBox\tools\deploy.ps1
pwsh -File E:\NlToyBox\tools\game-status.ps1
```

Expected, 차례로: `saves backup ok (N) -> E:\NlToyBox\backups\saves\<시각>`(이 경로를 Step 7에서 쓴다), `build ok -> …`, `setup ok -> …`, `deployed -> …`, `패치 여부  : 패치됨 (.aurie 섹션 있음)`.

- [ ] **Step 3: 프리셋을 입히고 요청 파일을 만든다**

```powershell
pwsh -File E:\NlToyBox\tools\overlay.ps1 apply -Preset E:\NlToyBox\presets\effect-stage2.json
pwsh -File E:\NlToyBox\tools\overlay.ps1 status
pwsh -File E:\NlToyBox\tools\overlay.ps1 probe-request -Preset E:\NlToyBox\presets\effect-stage2.json -Header E:\NlToyBox\tools\probes\stage2-effect.head.txt -Out E:\NlToyBox\tools\probes\stage2-effect.txt
pwsh -File E:\NlToyBox\tools\test-native.ps1
```

Expected: `apply ok: effect-stage2 (쓴 파일 3개). 게임을 다시 켜야 반영된다`. `status`에 `vanilla: 122`, `applied: 3`, `프리셋: effect-stage2`(`부분 적용`이라고 나오면 멈춘다). `probe-request ok: 값 10개 -> … (게임 파일에 없는 수 10개)`, `주의:` 줄 없음. `core tests: 18 passed`.

- [ ] **Step 4: 게임을 켜고, 사용자가 끌 때까지 기다린다**

사용자에게 "지금 켭니다"라고 알린 뒤:

```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\probe.ps1 -Request E:\NlToyBox\tools\probes\stage2-effect.txt -Out E:\NlToyBox\refs\runtime\stage2-effect.json -TimeoutSec 1500 -GraceSec 30 *>&1 | Tee-Object -FilePath E:\NlToyBox\refs\runtime\stage2-effect.probe.txt; "probe exit=$LASTEXITCODE"
```

Expected: 마지막에 `PASS`, `probe exit=0`. `덤프:` 줄이 `stage2-effect.menu.json`과 `stage2-effect.late0.json` … 을 적는다. `게임 종료: not-running`(사용자가 껐다). 로그에 `NlToyBox 0.2.0 loaded`, `builtin code_is_compiled = true`, `script … = found`, `probe done`이 있다(`check-load.ps1`을 갈음한다).

- `FAIL: dump failed: bad request`면 요청 파일의 잘못이다. 게임은 이미 켜졌다. Step 7로 가서 되돌리고 사용자에게 보고한다(예비 실행을 승인받아야 다시 켠다).
- `FAIL: 되풀이 덤프를 하나도 받지 못했습니다`면 게임이 60초 안에 꺼진 것이다. 같은 처리.
- `주의: 제한 시간이 지나 이 도구가 게임을 껐습니다`가 보이면 25분 동안 게임이 켜져 있었던 것이다. 결과에 적는다.

- [ ] **Step 5: 사용자에게 묻는다**

- 고른 지역과 난이도.
- 화면의 금화, 나무, 당근, 약. (읽었다면) 벌목꾼 건물의 나무 비용.
- 게임 화면에 들어간 때와 끈 때. 화면이 멈춘 길이의 체감.
- 평소와 달라 보인 것(오류 창, 이상한 수치).

답을 그대로 적어 둔다. 읽지 못한 수치는 "읽지 못함"이다.

- [ ] **Step 6: 덤프를 고르고 판정한다**

게임이 꺼진 뒤의 데이터 파일(질문 10):

```powershell
pwsh -File E:\NlToyBox\tools\overlay.ps1 status
```

Expected를 미리 알 수 없다. `applied: 3`, `unknown: 0`이면 게임이 데이터 파일을 다시 쓰지 않은 것이다. `unknown`이 있으면 다시 쓴 것이다(그 파일 이름을 적는다. Step 7에서 `data-restore.ps1`을 쓴다).

덤프마다 어느 상태에서 떴는지:

```powershell
foreach ($n in 'menu', 'late0', 'late1', 'late2') {
    $f = "E:\NlToyBox\refs\runtime\stage2-effect.$n.json"
    if (Test-Path -LiteralPath $f) { "--- $n"; py -3.14 E:\NlToyBox\tools\re\dump_tool.py summary $f | Select-Object -First 2 }
}
Select-String -LiteralPath E:\NlToyBox\refs\runtime\stage2-effect.probe.txt -Pattern '^(state|watch|dump \w+ (start|done)) ' | ForEach-Object { $_.Line }
```

`present:` 줄에 `o_character`가 있고 `o_main_menu`가 없는 덤프가 **게임 안의 덤프**다. `seq`가 가장 큰 것을 판정에 쓴다(아래의 `<덤프>`). 사용자가 알려 준 때와 `dump … start … at HH:MM:SS`가 맞는지 본다. 게임 안의 덤프가 하나도 없으면 질문 7~9의 답은 "확인하지 못함"이다. Step 7로 가고, 예비 실행을 승인받을지 사용자에게 묻는다. `dump … done … took N`의 N이 덤프에 걸린 시간이다(적어 둔다).

덤프를 믿어도 되는지와 값의 자리:

```powershell
$d = 'E:\NlToyBox\refs\runtime\stage2-effect.<덤프>.json'
py -3.14 E:\NlToyBox\tools\re\dump_tool.py controls $d "global.__gameplay_vars.global_map_ai_economy_initial_budget=1006"
pwsh -File E:\NlToyBox\tools\overlay.ps1 verify -Preset E:\NlToyBox\presets\effect-stage2.json -Dump $d | Tee-Object -FilePath E:\NlToyBox\refs\runtime\stage2-effect.verdicts.txt
```

- `controls`: 첫 줄 `ok   expect global.__gameplay_vars.global_map_ai_economy_initial_budget`가 양성 대조다. `FAIL`이 하나라도 있으면(`truncated` 포함) 그 덤프의 "없다"를 쓰지 않는다. 깊이 8에서는 방문 한도에 걸릴 수 있다(`truncated`). 그때 찾은 것은 유효하고, 못 찾은 것은 "확인하지 못함"이다.
- `verify`: 값마다 판정과 경로. 게임 안에서는 변수 이름이 파일의 키와 다른 곳이 많아(`default_budget_money`, `default_resource_count[n]`) `value-only`가 많이 나올 것이다. 값이 게임 JSON에 없는 수이므로 경로의 이름으로 판단한다. `… 모두 N곳`이 붙은 값은 아래 명령으로 모두 본다.

값 하나의 모든 히트와, 금화의 출처:

```powershell
py -3.14 E:\NlToyBox\tools\re\dump_tool.py hits $d | Select-String ' = (1031|1057|2767|1006|1101|2\.3719|41\.3719|31\.2917|0\.5719|11\.4373)$'
py -3.14 E:\NlToyBox\tools\re\dump_tool.py hits $d | Select-String ' = 3000$'
py -3.14 E:\NlToyBox\tools\re\dump_tool.py hits $d budget
py -3.14 E:\NlToyBox\tools\re\dump_tool.py hits $d money
py -3.14 E:\NlToyBox\tools\re\dump_tool.py hits $d difficulty
py -3.14 E:\NlToyBox\tools\re\dump_tool.py hits $d game_conditions
py -3.14 E:\NlToyBox\tools\re\dump_tool.py hits $d default_resource_count
```

출력이 길면 파일로 받는다(`| Set-Content E:\NlToyBox\refs\runtime\stage2-effect.hits-<이름>.txt`). 질문마다 본다:

- **질문 7.** 사용자가 읽은 금화가 X라 하자. 이름에 `budget`이나 `money`가 든 히트 가운데 값이 X인 것이 금화가 든 변수의 후보다. 값 2767(`budget_money`)이 `default_budget_money` 말고 어디에 있는지 본다. X가 2767이면 `budget_money`가 시작 금화를 정한 것이다. X가 3000이고 2767이 `default_*`에만 있으면 "정하지 않는다"이고, `difficulty`·`game_conditions`의 히트와 사용자가 고른 난이도가 단서다. **X의 출처를 한 번의 실행으로 단정하지 않는다.** 본 것(변수의 경로와 값, 고른 난이도)을 적고, 출처는 그 근거가 닿는 데까지만 쓴다.
- **질문 8.** 화면의 나무와 당근이 1031, 1057인가. 약은 바꾸지 않은 값 그대로인가. 덤프에서 1031과 1057이 `default_resource_count`의 원소 말고 어디에 있는가(창고나 도시의 재고처럼 보이는 경로인가).
- **질문 9.** 2.3719, 41.3719, 31.2917, 1101, 0.5719, 11.4373이 게임 오브젝트의 어느 변수에 있는가. 메뉴 덤프에는 없고 게임 안의 덤프에만 있는 경로가 "게임을 시작하면 옮겨지는 자리"다(`verify`를 `stage2-effect.menu.json`에도 돌려 견준다).

- [ ] **Step 7: 되돌린다**

```powershell
pwsh -File E:\NlToyBox\tools\overlay.ps1 restore
pwsh -File E:\NlToyBox\tools\restore-game.ps1
pwsh -File E:\NlToyBox\tools\overlay.ps1 status
pwsh -File E:\NlToyBox\tools\game-status.ps1
pwsh -File E:\NlToyBox\tools\saves-backup.ps1 -Diff <Step 2가 찍은 사본의 경로>
```

Expected: `restore ok (3)`, `restore ok`, `vanilla: 125`와 `프리셋: (없음. 바닐라)`, `패치 여부  : 바닐라`·`mods\      : (없음)`. `saves-backup -Diff`가 달라진 파일을 적는다. `saves\` 아래에 새 파일이 있으면 새 게임이 만든 세이브다. 지우지 않고 사용자에게 알린다.

Step 6에서 `unknown`이 있었으면 첫 명령이 거부한다. 그때는:

```powershell
pwsh -File E:\NlToyBox\tools\data-restore.ps1
Remove-Item -LiteralPath E:\NlToyBox\backups\overlay\0.5588.9777.0\state.json
pwsh -File E:\NlToyBox\tools\overlay.ps1 status
```

Expected: `data restore ok (N)`, 그리고 `vanilla: 125`.

- [ ] **Step 8: 요청 파일을 커밋한다**

```powershell
git -C E:\NlToyBox add tools/probes/stage2-effect.txt
git -C E:\NlToyBox commit -m @'
chore(probe): 효과 실측의 요청 파일

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 5: 결과를 적고 합친다

**Files:**
- Create: `research/05-effects.md`
- Modify: `catalog/0.5588.9777.0/keys.json`, `docs/superpowers/specs/2026-10-04-data-overlay-design.md`, `docs/superpowers/specs/2026-10-04-configurable-rules-roadmap.md`, `CLAUDE.md`(필요하면)

**Interfaces:**
- Consumes: Task 4의 덤프, 판정, 사용자가 읽은 수치
- Produces: 결과 문서와 카탈로그의 메모·등급

- [ ] **Step 1: `research/05-effects.md`를 쓴다**

들어갈 것(차례대로):

1. 머리: 조사일, 게임 버전, 게임을 켠 횟수, 덤프의 자리, 다시 재는 명령(Task 4 Step 3~4와 Step 6).
2. 답: 질문 7~10마다 "그렇다 / 아니다 / 확인하지 못함"과 근거 한두 줄.
3. 어느 덤프를 게임 안의 것으로 봤는가: 사용자가 알려 준 때, `state`·`watch` 기록, 덤프의 `present`.
4. 사용자가 읽은 수치(지역, 난이도, 금화, 나무, 당근, 약, 벌목꾼 비용). 읽지 못한 것은 그렇게 적는다.
5. `controls`의 출력 그대로.
6. 값마다의 표: 키, 바닐라 → 바꾼 값, 메뉴 덤프의 경로, 게임 안 덤프의 경로, 화면의 수치, 스펙 §5.2의 어느 줄에 드는가.
7. 금화: 이름에 `budget`·`money`가 든 히트 가운데 값이 화면의 금화와 같은 것, `difficulty`·`game_conditions`의 히트. 출처에 대해 말할 수 있는 것과 없는 것.
8. 덤프에 걸린 시간(깊이 8), 방문 수, 한도에 걸렸는지.
9. 세이브 폴더에서 달라진 파일.
10. 확인하지 못한 것과, 다음에 잴 것(플레이해야 보이는 효과를 무엇으로 볼 수 있을지: 이번에 찾은 자리).

- [ ] **Step 2: 카탈로그를 스펙 §5.2의 기준으로 고친다**

`catalog/0.5588.9777.0/keys.json`에서:

- 스펙 §5.2의 첫 두 줄에 드는 키: 그 파일의 첫 묶음 앞에 새 묶음을 넣는다. `paths`는 그 키 하나, `runtime`은 지금의 것, `effect`는 `TESTED`, `effect_by`는 `nltoybox`, `note`에 본 것("새 게임의 화면에서 나무가 1031 이었다", 또는 경로)과 `(research/05)`. 키가 지금 속한 묶음에 커뮤니티의 보고가 적혀 있으면 그 말을 새 `note`에 옮긴다.
- 셋째 줄에 드는 키(설정을 들고 있는 자리에서만 봤다): `effect`를 바꾸지 않는다. 그 키가 지금 속한 묶음이 그 키 하나의 묶음이면 `note` 끝에 "게임을 시작하면 <경로> 로 옮겨진다 (research/05)"를 붙인다. 여러 키의 묶음이면 그 키 하나의 묶음을 그 앞에 새로 넣고(등급은 그대로) 거기 적는다.
- 넷째·다섯째 줄에 드는 키: 같은 방법으로 `note`만 고친다.
- 게임 파일의 값을 적지 않는다(바꾼 값과 화면에서 본 수치는 적어도 된다).

고친 뒤:

```powershell
py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests
pwsh -File E:\NlToyBox\tools\overlay.ps1 check -Preset E:\NlToyBox\presets\effect-stage2.json
pwsh -File E:\NlToyBox\tools\overlay.ps1 check -Preset E:\NlToyBox\presets\verify-stage1.json
```

Expected: `Ran 101 tests`, `OK`. 두 `check` 모두 `check ok (쓰지 않았다)`이고, 앞의 것에서 `TESTED`로 올린 키가 `[…/TESTED]`로 보인다.

- [ ] **Step 3: 스펙과 로드맵의 상태를 고친다**

- 스펙의 머리 `상태` 줄에 `단계 2(효과 실측) 완료(research/05-effects.md)`를 더한다. 스펙 §5.7 아래에 "실측 결과" 문단을 넣는다: 질문 7~10의 답을 한 줄씩.
- 로드맵 §7의 표 1번(Gold)의 "미정"을 이번 결과로 고친다: 금화의 출처에 대해 알게 된 것, 또는 "여전히 모른다"와 본 것.
- `CLAUDE.md`의 "모듈을 쓸 때"에 있는 `budget_money`와 시작 금화에 대한 문장이 이번 결과와 어긋나면 고친다.

- [ ] **Step 4: 확인하고 커밋하고 합친다**

```powershell
git -C E:\NlToyBox add research catalog docs CLAUDE.md
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"
git -C E:\NlToyBox commit -m @'
docs(research): 새 게임에서 잰 효과 — 시작 금화, 시작 자원, 값이 옮겨지는 자리

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
pwsh -File E:\NlToyBox\tools\test-native.ps1
pwsh -File E:\NlToyBox\tools\tests\safety.tests.ps1
py -3.14 -m unittest discover -s E:\NlToyBox\tools\re\tests
py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests
git -C E:\NlToyBox switch develop
git -C E:\NlToyBox merge --no-ff feat/overlay-effect -m "Merge branch 'feat/overlay-effect' into develop"
git -C E:\NlToyBox branch -d feat/overlay-effect
git -C E:\NlToyBox status --short
```

Expected: 둘째 명령의 출력이 비어 있다. `core tests: 18 passed`, `safety tests: 24 passed`, `Ran 9 tests` + `OK`, `Ran 101 tests` + `OK`. 머지 뒤 `status`가 비어 있다.

- [ ] **Step 5: 사용자에게 보고한다**

질문 7~10의 답, 사용자가 읽은 수치와 덤프가 맞았는지, 게임이 바닐라라는 것, 세이브 폴더에서 달라진 파일(새 세이브가 있으면 그 경로. 지울지는 사용자가 정한다), 쓴 게임 실행 횟수. 그리고 다음에 잴 것을 제안한다: 이번에 찾은 자리로 볼 수 있게 된 효과(건물 비용, 거래 가격)와 그 방법.
