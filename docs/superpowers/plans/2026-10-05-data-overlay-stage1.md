# 데이터 오버레이 단계 1 (오버레이 도구) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 프리셋에 적은 수를 게임의 데이터 파일(JSON)에 값의 글자만 바꿔 입히고, 언제든 바이트까지 바닐라로 되돌리는 도구를 만든다. 키마다 무엇을 실측했는지를 카탈로그로 관리하고, 게임을 한 번 켜서 프리셋의 값이 런타임에 올라오는지 확인한다.

**Architecture:** Python 도구 `tools/overlay/`가 일을 한다. `jsonedit`가 값의 자리를 찾아 그 글자만 바꾸고(파싱해서 다시 쓰지 않는다), `compose`가 프리셋·카탈로그·바닐라 글로 새 글을 계산하며, 게임 폴더에 쓰는 것은 `store` 하나다. 바닐라인지는 카탈로그의 SHA256으로 판정하고, 적용은 항상 바닐라에서 출발한다(되돌리기 → 상태 적기 → 새 내용 쓰기). PowerShell 래퍼 `tools/overlay.ps1`이 게임이 꺼져 있는지 보고 경로와 exe 버전을 채워 부른다. 런타임 확인은 단계 0의 모듈과 `tools/probe.ps1`을 그대로 쓴다.

**Tech Stack:** Python 3.14 (표준 라이브러리, `unittest`), PowerShell 7. C++ 모듈은 바꾸지 않는다(Task 10의 측정에 `0.2.0`을 그대로 쓴다).

**Spec:** `docs/superpowers/specs/2026-10-04-data-overlay-design.md` §4. 설계가 기대는 실측은 `research/03-data-files.md`, 앞 단계의 결과는 `research/01-data-overlay.md`, `research/02-new-game-state.md`.

## Global Constraints

- **추측으로 작업하지 않는다.** 이 계획이 기대는 사실은 스펙 §4.0의 표와 `research/03-data-files.md`에 출처가 있다. 실행 중에 거기 없는 사실이 필요해지면 코드보다 먼저 그것을 확인하고(게임 파일, 덤프, 공식 문서, 인터넷 리서치) 출처를 원장과 스펙의 표에 적는다. 확인할 수 없으면 "모른다"로 두고 그 위에 다음 단계를 쌓지 않는다.
- **Task 0~9는 게임을 켜지 않는다.** 게임을 켜는 것은 Task 10의 **1회**다. 예비 1회는 그때 사용자에게 다시 승인받는다. 켜기 전에 사용자에게 알린다(이번에는 사용자가 할 일이 없다. 게임 창을 누르지 않는다).
- **Task 0~9는 실제 게임 폴더에 쓰지 않는다.** 실제 게임에 대고 돌려도 되는 명령은 읽기만 하는 것이다: `overlay.ps1`의 `status`, `scan`, `check`, `keys`, `pin`(레포에 쓴다), `tools/data-snapshot.ps1`(이미 있는 스냅샷과 같은지 확인), `tools/game-status.ps1`. 실제 게임에 `apply`를 하는 것은 Task 10뿐이다. 시험은 임시 폴더와 가짜 게임(`NORLAND_GAME_DIR`)으로 돈다.
- 게임의 데이터 파일을 쓰는 코드는 `tools/overlay/store.py` 하나다. 다른 모듈에 파일 쓰기를 넣지 않는다.
- 게임 경로를 스크립트에 직접 적지 않는다. PowerShell은 `Get-NlGameDir`, Python은 인자로 받는다.
- Python은 `py -3.14`, 표준 라이브러리만 쓴다. 스크립트는 `pwsh`(PowerShell 7).
- 저작물 경계: `refs/`, `backups/`, `downloads/`, `build/`는 커밋하지 않는다. 커밋 전에 `git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"`가 비어 있어야 한다. 카탈로그와 프리셋에는 키 이름, 등급, 해시만 두고 **게임 파일의 값을 옮겨 적지 않는다.** `keys` 명령의 출력(값이 들어 있다)은 `refs\registry\`에만 둔다.
- 세이브 폴더(`%LOCALAPPDATA%\Strategy`)에는 쓰지 않는다.
- 서브모듈 `external/YYToolkit`과 모듈 소스(`src/`, `tests/native/`, `CMakeLists.txt`)는 고치지 않는다.
- 카탈로그의 `runtime` 등급은 이 레포가 이 빌드에서 잰 것만 올린다. 커뮤니티의 보고는 `effect_by: community`와 출처로 따로 적는다.
- 브랜치: `develop`에서 `feat/overlay-stage1`을 만들어 작업하고 `git merge --no-ff`로 `develop`에 합친다. `main`은 건드리지 않는다. git 명령은 `git -C E:\NlToyBox`.
- 커밋 메시지는 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`으로 끝낸다.

**계획 승인에 포함해 달라 (CLAUDE.md의 머지 전 확인과 다른 점):**

- `tools/check-load.ps1`을 따로 돌리지 않는다. 모듈 소스를 바꾸지 않고, 돌리려면 게임을 한 번 더 켜야 한다. 대신 Task 10의 `probe.ps1` 실행이 찍는 `NlToyBox.log`에서 적재 판정 줄(`loaded`, `builtin … = true`, `script … = found`, `probe done`)을 확인한다.
- 전체 브랜치 검토(리뷰어)는 Task 9가 끝난 뒤, **게임을 켜기 전에** 받는다. 켠 뒤에 고칠 것이 나오면 다시 켜야 하기 때문이다. Task 10에서 생기는 것은 문서와 카탈로그의 등급뿐이다.

**이 계획의 코드가 어디서 왔는가:** 아래의 코드 블록은 2026-10-05에 스크래치 폴더에서 먼저 돌려 본 것이다. 단위 시험 82개가 통과했고, 실제 게임 폴더의 JSON 4,701개에 `scan`이 실패 0으로 끝났으며, 카탈로그 파일 125개의 사본에 검증 프리셋을 입혔다 되돌려 바이트가 원본과 같았다(`research/03-data-files.md`). 그래도 Task마다 시험을 먼저 쓰고 실패를 본 뒤 구현한다. 계획의 코드를 옮기다 생기는 실수를 그 순서가 잡는다.

**실행 전 검토에서 고친 것 (2026-10-05):** Task 0~9를 마친 뒤, 게임을 켜기 전에 전체 검토를 받고 고쳤다(커밋 `fix(review): …`). 아래 Task 1~8의 코드 블록은 처음 쓴 그대로이고 **지금의 소스가 정본이다.** 달라진 것: (1) `verify`의 `anchored`는 값이 있고 런타임 경로의 이름 조각 하나가 키 이름과 같을 때다(더 긴 이름의 일부는 치지 않는다). 믿을 수 없는 덤프에서는 `absent` 대신 `unknown`을 적고, `anchored`가 하나도 없으면 알린다. (2) 검증 프리셋의 값을 게임 JSON 어디에도 없는 수로 바꾸고, 이미 `VERIFIED`인 키 하나를 양성 대조로 넣었다(변경 30개). (3) `apply`가 쓰는 도중에 실패하면 무엇을 할지 알리고, `status`는 부분 적용을 알리며 종료 코드 1을 낸다. (4) `pin`은 프리셋이 입혀져 있으면(이 버전이든 옛 버전이든) 거부한다. (5) 스냅샷은 계산이 끝난 뒤, 실제로 바뀌는 파일만 뜬다. 게임 파일에서 읽은 바닐라는 해시를 다시 확인한다. (6) 스냅샷과 복원이 원본의 수정 시각도 옮긴다. (7) `mul`은 1을 곱하면 그대로이고 정수끼리의 곱은 정확하며, 그 밖에는 유효숫자 15자리로 다듬는다. (8) `store.plan`과 `store.vanilla_bytes`의 마지막 인자가 없어졌고 `store.ensure_snapshot`이 생겼다. 시험은 82개에서 98개가 됐다. 이 아래의 `Ran 82 tests`는 `Ran 98 tests`로 읽는다.

## Review Focus

1. **프리셋의 오타.** 없는 키, 틀린 경로, 모르는 항목이 조용히 무시되면 사용자는 적용됐다고 믿는다. 아무 값에도 닿지 않는 변경과 모르는 항목은 오류여야 하고, 오류가 하나라도 있으면 아무것도 쓰지 않아야 한다. → Task 4(`test_a_mistyped_preset_is_an_error`, `test_what_cannot_be_applied_is_an_error`), Task 5(`test_a_preset_that_cannot_be_applied_writes_nothing`)
2. **변경이 쌓이거나 복원이 덜 되는 경우.** 프리셋을 바꿔 다시 입혔을 때 앞 프리셋의 값이 남으면 안 되고, 복원 뒤에는 바이트가 원본과 같아야 한다. → Task 5(`test_applying_another_preset_starts_from_vanilla`, `test_restore_brings_back_every_byte_and_clears_the_state`), Task 7(래퍼의 왕복 시험)
3. **바닐라가 아닌 것을 바닐라로 믿는 경우.** 게임 갱신, Steam 무결성 검사, 손 편집 뒤에 옛 스냅샷으로 덮어쓰거나, 고친 파일에서 스냅샷을 뜨면 되돌릴 길이 없어진다. → Task 5(`test_a_file_changed_outside_the_tool_stops_everything`, `test_a_snapshot_that_is_not_vanilla_is_not_trusted`, `test_an_applied_file_without_its_snapshot_cannot_be_restored`), Task 7
4. **값 바깥의 바이트가 바뀌는 경우.** 닫는 괄호 앞의 쉼표, CRLF, BOM, 긴 소수 표기, 키의 뒤 공백. 게임이 무엇에 민감한지 모르므로 하나도 바뀌면 안 된다. → Task 1(`test_apply_edits_changes_only_the_value_text`, `test_writing_the_same_text_back_leaves_every_byte_alone`), Task 4(`test_a_long_float_is_rewritten_only_when_its_value_changes`), Task 5(`test_a_bom_and_crlf_survive_apply_and_restore`), Task 8(실제 파일 4,701개에 `scan`)
5. **도중에 죽거나, 게임이 켜진 채 쓰는 경우.** 반쯤 쓰인 파일이나 "모르는 것"이 된 파일이 남으면 도구가 스스로 풀지 못한다. → Task 5(`test_a_crash_at_any_write_leaves_files_the_tool_still_recognises`, `test_no_temporary_files_are_left_behind`), Task 7(`overlay apply 는 게임이 켜져 있으면 쓰지 않는다`)

## File Structure

| 파일 | 책임 |
|---|---|
| `tools/overlay/jsonedit.py` | 값의 자리(`Slot`) 찾기, 그 자리의 글자만 바꾸기, 수의 표기, BOM·UTF-8. 오류의 뿌리 `OverlayError` |
| `tools/overlay/paths.py` | 경로 식 읽기(`parse`), 맞춰 보기(`matches`), 적기(`show`) |
| `tools/overlay/catalog.py` | `keys.json`·`files.json` 읽기와 검증, 키의 묶음 찾기, 상대경로 검사, 이 도구의 JSON 읽기 |
| `tools/overlay/preset.py` | 프리셋 읽기와 검증 |
| `tools/overlay/compose.py` | 프리셋 + 카탈로그 + 바닐라 글 → 새 글과 변경 목록. 파일을 읽고 쓰지 않는다 |
| `tools/overlay/store.py` | 게임 폴더에 쓰기: 상태 판정, 스냅샷, 적용, 복원 |
| `tools/overlay/verify.py` | 런타임 확인: 요청 파일의 글, 덤프 판정 |
| `tools/overlay/cli.py` | 명령 아홉 개 |
| `tools/overlay/tests/support.py` | 시험이 함께 쓰는 가짜 게임과 카탈로그 |
| `tools/overlay/tests/test_*.py` | 모듈마다의 시험과, 레포의 카탈로그·프리셋이 읽히는지 |
| `tools/overlay.ps1` | 게임이 꺼져 있는지 보고 경로와 버전을 채워 `cli.py`를 부른다 |
| `tools/common.ps1` | `Get-NlOverlayStateDir` |
| `tools/tests/safety.tests.ps1` | 래퍼의 안전 동작 시험 다섯 개 |
| `catalog/0.5588.9777.0/keys.json` | 키의 묶음과 등급 (손으로 쓴다) |
| `catalog/0.5588.9777.0/files.json` | 파일 125개의 바닐라 SHA256 (`pin`이 만든다) |
| `presets/example.json` | 형식을 보여 주는 예 |
| `presets/verify-stage1.json` | Task 10의 실측에 쓰는 프리셋 |
| `tools/probes/stage1-verify.txt` | Task 10의 요청 (`probe-request`가 만든다) |
| `research/04-overlay-verify.md` | Task 10의 결과 |
| `CLAUDE.md`, `README.md`, 스펙 | 쓰는 법과 규칙, 상태 |

---

### Task 0: 작업 브랜치

- [ ] **Step 1: 브랜치를 만든다**

```powershell
git -C E:\NlToyBox switch develop
git -C E:\NlToyBox switch -c feat/overlay-stage1
git -C E:\NlToyBox status --short
```

Expected: 마지막 명령의 출력이 비어 있다.

- [ ] **Step 2: 게임이 바닐라인지 본다**

```powershell
pwsh -File E:\NlToyBox\tools\game-status.ps1
```

Expected: `패치 여부  : 바닐라`, `실행 여부  : 꺼져 있음`, `mods\      : (없음)`.

---

### Task 1: 값의 자리만 고치는 수정기 (`jsonedit`)

**Files:**
- Create: `tools/overlay/jsonedit.py`
- Test: `tools/overlay/tests/test_jsonedit.py`

**Interfaces:**
- Consumes: 없음
- Produces:
  - `class OverlayError(Exception)` — 이 도구의 모든 오류의 뿌리. `class JsonError(OverlayError)`
  - `BOM: bytes`
  - `@dataclass(frozen=True) class Slot: path: tuple; start: int; end: int; kind: str` — `kind`는 `number`, `string`, `true`, `false`, `null`, `object`, `array`. `path`의 키는 `str`, 배열 첨자는 `int`
  - `decode(raw: bytes) -> tuple[bytes, str]` — (BOM, 글). `encode(bom: bytes, text: str) -> bytes`
  - `scan(text: str) -> list[Slot]` — 문서 순서, 컨테이너가 안의 값보다 먼저
  - `value_of(text: str, slot: Slot)` — 스칼라의 값. 수는 `int` 또는 `float`
  - `format_number(value, like: str) -> str` — `like`는 그 자리에 있던 글
  - `apply_edits(text: str, edits: list[tuple[Slot, str]]) -> str`

- [ ] **Step 1: 실패하는 시험을 쓴다**

`tools/overlay/tests/test_jsonedit.py`:

```python
"""jsonedit.py 의 시험. 사용: py -3.14 -m unittest discover -s tools/overlay/tests -v"""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import jsonedit
from jsonedit import JsonError

# 게임 파일의 모양을 줄인 것: 탭 들여쓰기, CRLF, 닫는 괄호 앞의 쉼표, 뒤 공백이 붙은 키, 긴 소수.
SAMPLE = ('{\r\n\t"production_cost": {\r\n\t\t"ale": 2,\r\n\t\t"coal": 0.90000000000000002,\r\n\t},\r\n'
          '\t"building_resources": {\r\n\t\t"hut": [["wood", 10], ["iron", 5.0]],\r\n\t},\r\n'
          '\t"paper": {"messenger_cost ": 1},\r\n\t"on": true, "off": false, "none": null, "neg": -6\r\n}')


def find(text, *path):
    return next(slot for slot in jsonedit.scan(text) if slot.path == path)


class ScanTests(unittest.TestCase):
    def test_scan_lists_every_value_in_document_order_with_its_path(self):
        slots = jsonedit.scan('{"a": {"b": [1, "x"]}, "c": null}')
        self.assertEqual([(slot.path, slot.kind) for slot in slots],
                         [((), "object"), (("a",), "object"), (("a", "b"), "array"), (("a", "b", 0), "number"),
                          (("a", "b", 1), "string"), (("c",), "null")])

    def test_slot_positions_cover_exactly_the_value_text(self):
        text = '{"a": -12.5 , "b": "q\\"x"}'
        self.assertEqual(text[find(text, "a").start:find(text, "a").end], "-12.5")
        self.assertEqual(text[find(text, "b").start:find(text, "b").end], '"q\\"x"')

    def test_scan_reads_the_shapes_found_in_game_files(self):
        self.assertEqual(jsonedit.value_of(SAMPLE, find(SAMPLE, "production_cost", "coal")), 0.9)
        self.assertEqual(jsonedit.value_of(SAMPLE, find(SAMPLE, "building_resources", "hut", 1, 1)), 5.0)
        self.assertEqual(jsonedit.value_of(SAMPLE, find(SAMPLE, "paper", "messenger_cost ")), 1)
        self.assertEqual(jsonedit.value_of(SAMPLE, find(SAMPLE, "neg")), -6)

    def test_trailing_comma_is_accepted_in_objects_and_arrays(self):
        self.assertEqual(len(jsonedit.scan('{"a": [1, 2, ], }')), 4)

    def test_what_game_files_never_contain_is_an_error_not_a_guess(self):
        for bad in ('{"a": 1} x', '{"a": 1, "a": 2}', "{'a': 1}", "{a: 1}", '{"a": NaN}', '{"a": 1 // c\n}',
                    '{"a": 01}', '{"a": }', '{"a": [1 2]}', '{"a": "x', '{"a": 1', '{,}', '{"a": 1,,}', ""):
            with self.subTest(bad=bad):
                with self.assertRaises(JsonError):
                    jsonedit.scan(bad)

    def test_errors_say_where(self):
        with self.assertRaises(JsonError) as caught:
            jsonedit.scan('{\n  "a": 1,\n  "a": 2\n}')
        self.assertIn("3행 3열", str(caught.exception))


class ValueTests(unittest.TestCase):
    def test_value_of_keeps_integers_integers(self):
        text = '{"i": 7, "f": 7.0, "e": 1e2, "s": "a\\nb", "t": true}'
        self.assertIs(type(jsonedit.value_of(text, find(text, "i"))), int)
        self.assertIs(type(jsonedit.value_of(text, find(text, "f"))), float)
        self.assertEqual(jsonedit.value_of(text, find(text, "e")), 100.0)
        self.assertEqual(jsonedit.value_of(text, find(text, "s")), "a\nb")
        self.assertIs(jsonedit.value_of(text, find(text, "t")), True)

    def test_format_number_writes_integers_and_shortest_floats(self):
        self.assertEqual(jsonedit.format_number(5000, "2000"), "5000")
        self.assertEqual(jsonedit.format_number(5000.0, "2000"), "5000")
        self.assertEqual(jsonedit.format_number(0.1 + 0.2, "1"), "0.30000000000000004")
        self.assertEqual(jsonedit.format_number(-2.5, "1"), "-2.5")

    def test_format_number_keeps_the_decimal_point_of_the_old_text(self):
        self.assertEqual(jsonedit.format_number(3, "2.0"), "3.0")
        self.assertEqual(jsonedit.format_number(3, "0.5"), "3.0")
        self.assertEqual(jsonedit.format_number(3.5, "2.0"), "3.5")

    def test_format_number_never_writes_an_exponent(self):
        self.assertEqual(jsonedit.format_number(0.00001, "1"), "0.00001")
        self.assertEqual(jsonedit.format_number(1.5e-7, "1"), "0.00000015")
        self.assertEqual(jsonedit.format_number(1e22, "1"), "10000000000000000000000")

    def test_format_number_refuses_what_is_not_a_finite_number(self):
        for bad in (float("inf"), float("nan"), True, "5", None):
            with self.subTest(bad=bad):
                with self.assertRaises(JsonError):
                    jsonedit.format_number(bad, "1")


class EditTests(unittest.TestCase):
    def test_apply_edits_changes_only_the_value_text(self):
        out = jsonedit.apply_edits(SAMPLE, [(find(SAMPLE, "production_cost", "ale"), "7"),
                                            (find(SAMPLE, "building_resources", "hut", 0, 1), "5")])
        self.assertEqual(out, SAMPLE.replace('"ale": 2,', '"ale": 7,').replace('["wood", 10]', '["wood", 5]'))

    def test_writing_the_same_text_back_leaves_every_byte_alone(self):
        numbers = [slot for slot in jsonedit.scan(SAMPLE) if slot.kind == "number"]
        self.assertEqual(jsonedit.apply_edits(SAMPLE, [(slot, SAMPLE[slot.start:slot.end]) for slot in numbers]), SAMPLE)
        self.assertEqual(jsonedit.apply_edits(SAMPLE, []), SAMPLE)

    def test_overlapping_edits_are_refused(self):
        text = '{"a": [1, 2]}'
        with self.assertRaises(JsonError):
            jsonedit.apply_edits(text, [(find(text, "a"), "[]"), (find(text, "a", 0), "9")])

    def test_decode_keeps_the_bom_and_the_line_endings(self):
        raw = jsonedit.BOM + SAMPLE.encode("utf-8")
        bom, text = jsonedit.decode(raw)
        self.assertEqual((bom, text), (jsonedit.BOM, SAMPLE))
        self.assertEqual(jsonedit.encode(bom, text), raw)
        self.assertEqual(jsonedit.decode(SAMPLE.encode("utf-8"))[0], b"")

    def test_decode_refuses_bytes_that_are_not_utf8(self):
        with self.assertRaises(JsonError):
            jsonedit.decode(b'{"a": "\xff"}')


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 시험이 실패하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests -p test_jsonedit.py`
Expected: FAIL. `ModuleNotFoundError: No module named 'jsonedit'`

- [ ] **Step 3: 구현한다**

`tools/overlay/jsonedit.py`:

```python
"""게임의 JSON 에서 값의 자리를 찾고 그 자리의 글자만 바꾼다. 파싱해서 다시 쓰지 않는다.

받는 문법은 게임 폴더의 JSON 4,701개에서 실제로 본 것이다(research/03-data-files.md): 표준 JSON 과,
닫는 괄호 앞의 쉼표. 주석, 작은따옴표, 따옴표 없는 키, NaN, 한 객체 안의 같은 키는 한 번도 없었다.
그런 것을 만나면 게임이 어떻게 읽는지 모르므로 오류를 낸다.
"""
import dataclasses
import decimal
import json
import math
import re

BOM = b"\xef\xbb\xbf"


class OverlayError(Exception):
    """이 도구가 사용자에게 그대로 보여 주는 오류."""


class JsonError(OverlayError):
    """읽을 수 없는 JSON, 또는 쓸 수 없는 값."""


@dataclasses.dataclass(frozen=True)
class Slot:
    path: tuple     # 뿌리에서 이 값까지. 객체의 키는 str, 배열의 첨자는 int
    start: int      # 값이 시작하는 글자 위치
    end: int        # 값이 끝난 다음 글자 위치
    kind: str       # number, string, true, false, null, object, array


_WS = " \t\r\n"
_NUMBER = re.compile(r"-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][-+]?[0-9]+)?")
_LITERALS = ("true", "false", "null")


def decode(raw):
    """바이트를 (BOM, 글)로 나눈다. UTF-8 로 읽히지 않으면 오류다(글자가 조용히 바뀌는 것을 막는다)."""
    bom = BOM if raw.startswith(BOM) else b""
    try:
        return bom, raw[len(bom):].decode("utf-8")
    except UnicodeDecodeError as error:
        raise JsonError(f"UTF-8 로 읽을 수 없다: {error}") from None


def encode(bom, text):
    return bom + text.encode("utf-8")


def _fail(text, pos, what):
    line = text.count("\n", 0, pos) + 1
    column = pos - (text.rfind("\n", 0, pos) + 1) + 1
    raise JsonError(f"{what} ({line}행 {column}열)")


def _skip(text, pos):
    while pos < len(text) and text[pos] in _WS:
        pos += 1
    return pos


def _string_end(text, pos):
    """pos 는 여는 따옴표의 위치. 닫는 따옴표의 다음 위치를 돌려준다."""
    i = pos + 1
    while i < len(text):
        if text[i] == "\\":
            i += 2
        elif text[i] == '"':
            return i + 1
        else:
            i += 1
    _fail(text, pos, "문자열이 닫히지 않았다")


def _value(text, pos, path, slots):
    if pos >= len(text):
        _fail(text, pos, "값이 와야 할 자리에서 글이 끝났다")
    c = text[pos]
    if c == "{":
        return _container(text, pos, path, slots, "object", "}")
    if c == "[":
        return _container(text, pos, path, slots, "array", "]")
    if c == '"':
        end = _string_end(text, pos)
        slots.append(Slot(path, pos, end, "string"))
        return end
    match = _NUMBER.match(text, pos)
    if match:
        slots.append(Slot(path, pos, match.end(), "number"))
        return match.end()
    for word in _LITERALS:
        if text.startswith(word, pos):
            slots.append(Slot(path, pos, pos + len(word), word))
            return pos + len(word)
    _fail(text, pos, "읽을 수 없는 값")


def _container(text, pos, path, slots, kind, close):
    index = len(slots)
    slots.append(None)          # 컨테이너의 자리는 끝을 안 뒤에 채운다. 순서는 안의 값보다 앞이다
    seen = set()
    count = 0
    i = _skip(text, pos + 1)
    while True:
        if i >= len(text):
            _fail(text, pos, "괄호가 닫히지 않았다")
        if text[i] == close:
            break
        if kind == "object":
            if text[i] != '"':
                _fail(text, i, "키는 큰따옴표로 시작해야 한다")
            key_end = _string_end(text, i)
            try:
                key = json.loads(text[i:key_end], strict=False)
            except ValueError:
                _fail(text, i, "키를 읽을 수 없다")
            if key in seen:
                _fail(text, i, f"한 객체 안에 같은 키가 두 번 있다: {key!r}")
            seen.add(key)
            i = _skip(text, key_end)
            if i >= len(text) or text[i] != ":":
                _fail(text, i, "키 뒤에 쌍점이 없다")
            i = _value(text, _skip(text, i + 1), path + (key,), slots)
        else:
            i = _value(text, i, path + (count,), slots)
        count += 1
        i = _skip(text, i)
        if i < len(text) and text[i] == ",":
            i = _skip(text, i + 1)          # 닫는 괄호 앞의 쉼표도 받는다(게임 파일 셋에 있다)
        elif i < len(text) and text[i] != close:
            _fail(text, i, f"쉼표나 {close} 가 와야 한다")
    slots[index] = Slot(path, pos, i + 1, kind)
    return i + 1


def scan(text):
    """글 안의 모든 값의 자리를 문서 순서로 돌려준다. 컨테이너가 그 안의 값보다 먼저 나온다."""
    slots = []
    pos = _skip(text, _value(text, _skip(text, 0), (), slots))
    if pos != len(text):
        _fail(text, pos, "값이 끝난 뒤에 글이 더 있다")
    return slots


def value_of(text, slot):
    """스칼라 자리의 값. 수는 소수점이나 지수가 없으면 int, 있으면 float 다."""
    token = text[slot.start:slot.end]
    if slot.kind == "number":
        return float(token) if any(c in token for c in ".eE") else int(token)
    if slot.kind == "string":
        return json.loads(token, strict=False)
    if slot.kind in _LITERALS:
        return {"true": True, "false": False, "null": None}[slot.kind]
    raise JsonError(f"스칼라가 아니다: {slot.kind}")


def format_number(value, like):
    """새 수를 글로 쓴다. 정수는 정수로, 실수는 가장 짧은 왕복 표기로. 지수 표기는 쓰지 않는다
    (게임 파일의 수 254,251개에 지수 표기가 없다. 게임이 읽는지 모른다).
    like 는 그 자리에 있던 글이다. 거기 소수점이 있으면 정수에도 '.0' 을 붙인다."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise JsonError(f"수가 아니다: {value!r}")
    if isinstance(value, float):
        if not math.isfinite(value):
            raise JsonError(f"유한하지 않은 수는 쓰지 않는다: {value!r}")
        if not value.is_integer():
            text = repr(value)
            return format(decimal.Decimal(text), "f") if "e" in text else text
        value = int(value)
    return f"{value}.0" if "." in like else str(value)


def apply_edits(text, edits):
    """edits: (Slot, 새 글)의 목록. 그 자리의 글자만 바뀐 글을 돌려준다. 자리가 겹치면 오류다."""
    ordered = sorted(edits, key=lambda edit: edit[0].start)
    for (a, _), (b, _) in zip(ordered, ordered[1:]):
        if b.start < a.end:
            raise JsonError(f"바꿀 자리가 겹친다: {a.path} 와 {b.path}")
    parts, cursor = [], 0
    for slot, new in ordered:
        parts += [text[cursor:slot.start], new]
        cursor = slot.end
    parts.append(text[cursor:])
    return "".join(parts)
```

- [ ] **Step 4: 시험이 통과하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests -p test_jsonedit.py`
Expected: `Ran 16 tests`, `OK`

- [ ] **Step 5: 커밋한다**

```powershell
git -C E:\NlToyBox add tools/overlay/jsonedit.py tools/overlay/tests/test_jsonedit.py
git -C E:\NlToyBox commit -m @'
feat(overlay): 값의 자리만 고치는 JSON 수정기

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 2: 경로 식 (`paths`)

**Files:**
- Create: `tools/overlay/paths.py`
- Test: `tools/overlay/tests/test_paths.py`

**Interfaces:**
- Consumes: `jsonedit.OverlayError`
- Produces:
  - `class PathError(OverlayError)`
  - `KEY`, `ANY_KEY`, `INDEX`, `ANY_INDEX` — 조각의 종류(글)
  - `parse(expr: str) -> tuple` — `(종류, 값)`의 튜플. 예: `parse("a.*[0]")` → `((KEY, "a"), (ANY_KEY, None), (INDEX, 0))`
  - `matches(pattern: tuple, path: tuple) -> bool` — `path`는 `Slot.path`와 같은 꼴
  - `show(path: tuple) -> str` — 구체 경로를 식으로 적는다

- [ ] **Step 1: 실패하는 시험을 쓴다**

`tools/overlay/tests/test_paths.py`:

```python
"""paths.py 의 시험."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import paths
from paths import ANY_INDEX, ANY_KEY, INDEX, KEY, PathError


class PathTests(unittest.TestCase):
    def test_parse_reads_keys_wildcards_and_indexes(self):
        self.assertEqual(paths.parse("budget_money"), ((KEY, "budget_money"),))
        self.assertEqual(paths.parse("building_resources.*[*][1]"),
                         ((KEY, "building_resources"), (ANY_KEY, None), (ANY_INDEX, None), (INDEX, 1)))
        self.assertEqual(paths.parse("available_parameters[0].population"),
                         ((KEY, "available_parameters"), (INDEX, 0), (KEY, "population")))

    def test_a_number_after_a_dot_is_a_key_not_an_index(self):
        self.assertEqual(paths.parse("unit_skill_stage.1"), ((KEY, "unit_skill_stage"), (KEY, "1")))

    def test_odd_keys_are_written_as_json_strings(self):
        self.assertEqual(paths.parse('paper["messenger_cost "]'), ((KEY, "paper"), (KEY, "messenger_cost ")))
        self.assertEqual(paths.parse('["a.b"]["*"]'), ((KEY, "a.b"), (KEY, "*")))

    def test_malformed_paths_are_errors(self):
        for bad in ("", ".a", "a.", "a..b", "a b", "a[", "a[1", "a[x]", "a[-1]", 'a["x]', "a*", "a.*b", "a.[0]", "a]", None, 5):
            with self.subTest(bad=bad):
                with self.assertRaises(PathError):
                    paths.parse(bad)

    def test_matches_distinguishes_keys_from_indexes(self):
        pattern = paths.parse("building_resources.*[*][1]")
        self.assertTrue(paths.matches(pattern, ("building_resources", "hut", 0, 1)))
        self.assertFalse(paths.matches(pattern, ("building_resources", "hut", 0, 0)))
        self.assertFalse(paths.matches(pattern, ("building_resources", "hut", "0", 1)))
        self.assertFalse(paths.matches(pattern, ("building_resources", "hut", 0)))
        self.assertTrue(paths.matches(paths.parse("unit_skill_stage.1"), ("unit_skill_stage", "1")))
        self.assertFalse(paths.matches(paths.parse("unit_skill_stage.1"), ("unit_skill_stage", 1)))
        self.assertFalse(paths.matches(paths.parse("a.*"), ("a", 0)))

    def test_show_writes_a_path_that_parses_back_to_the_same_place(self):
        for path in (("budget_money",), ("building_resources", "hut", 0, 1), ("paper", "messenger_cost "),
                     ("unit_skill_stage", "1"), ("x", "<unknown built-in variable>"), (0, "a"), ("a.b", "*", 'q"')):
            with self.subTest(path=path):
                self.assertTrue(paths.matches(paths.parse(paths.show(path)), path))
        self.assertEqual(paths.show(("building_resources", "hut", 0, 1)), "building_resources.hut[0][1]")
        self.assertEqual(paths.show(("paper", "messenger_cost ")), 'paper["messenger_cost "]')


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 시험이 실패하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests -p test_paths.py`
Expected: FAIL. `ModuleNotFoundError: No module named 'paths'`

- [ ] **Step 3: 구현한다**

`tools/overlay/paths.py`:

```python
"""경로 식: 프리셋과 카탈로그가 JSON 안의 값을 가리키는 방법.

  budget_money                      키
  fair_trade.fair_trade_sale.ale    점으로 잇는다
  production_cost.*                 * 는 객체의 모든 키
  building_resources.*[*][1]        [n] 은 배열의 첨자, [*] 는 모든 첨자
  paper["messenger_cost "]          점·공백·괄호·별표·따옴표가 든 키는 JSON 문자열로 적는다

점 뒤의 수는 키다(unit_skill_stage.1 의 "1"). 배열의 첨자는 대괄호로만 적는다.
"""
import json
import re

from jsonedit import OverlayError

KEY, ANY_KEY, INDEX, ANY_INDEX = "key", "any_key", "index", "any_index"
_BARE = re.compile(r'[^.\[\]*"\s]+')
_DIGITS = re.compile(r"[0-9]+")
_DECODER = json.JSONDecoder()


class PathError(OverlayError):
    pass


def parse(expr):
    """경로 식을 (종류, 값)의 튜플로 바꾼다."""
    if not isinstance(expr, str) or not expr:
        raise PathError(f"경로는 비어 있지 않은 글이어야 한다: {expr!r}")
    out = []
    i = 0
    while i < len(expr):
        if expr[i] == "[":
            i = _bracket(expr, i, out)
            continue
        if out:
            if expr[i] != ".":
                raise PathError(f"{i + 1}번째 글자에 점이나 대괄호가 와야 한다: {expr!r}")
            i += 1
        if expr.startswith("*", i):
            out.append((ANY_KEY, None))
            i += 1
            continue
        match = _BARE.match(expr, i)
        if not match:
            raise PathError(f"{i + 1}번째 글자에서 키를 읽을 수 없다: {expr!r}")
        out.append((KEY, match.group(0)))
        i = match.end()
    return tuple(out)


def _bracket(expr, i, out):
    j = i + 1
    if expr.startswith("*", j):
        out.append((ANY_INDEX, None))
        j += 1
    elif expr.startswith('"', j):
        try:
            key, j = _DECODER.raw_decode(expr, j)
        except ValueError:
            raise PathError(f"대괄호 안의 문자열을 읽을 수 없다: {expr!r}") from None
        out.append((KEY, key))
    else:
        match = _DIGITS.match(expr, j)
        if not match:
            raise PathError(f"대괄호 안에는 수, *, 문자열이 와야 한다: {expr!r}")
        out.append((INDEX, int(match.group(0))))
        j = match.end()
    if not expr.startswith("]", j):
        raise PathError(f"대괄호가 닫히지 않았다: {expr!r}")
    return j + 1


def matches(pattern, path):
    """parse 한 식이 구체 경로(키는 str, 첨자는 int 인 튜플)에 맞는가."""
    if len(pattern) != len(path):
        return False
    for (kind, value), part in zip(pattern, path):
        if kind in (KEY, ANY_KEY):
            if not isinstance(part, str) or (kind == KEY and part != value):
                return False
        elif not isinstance(part, int) or (kind == INDEX and part != value):
            return False
    return True


def show(path):
    """구체 경로를 식으로 적는다. parse(show(p)) 는 p 에만 맞는다."""
    out = []
    for part in path:
        if isinstance(part, int):
            out.append(f"[{part}]")
        elif _BARE.fullmatch(part):
            out.append(("." if out else "") + part)
        else:
            out.append("[" + json.dumps(part, ensure_ascii=False) + "]")
    return "".join(out)
```

- [ ] **Step 4: 시험이 통과하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests`
Expected: `Ran 22 tests`, `OK`

- [ ] **Step 5: 커밋한다**

```powershell
git -C E:\NlToyBox add tools/overlay/paths.py tools/overlay/tests/test_paths.py
git -C E:\NlToyBox commit -m @'
feat(overlay): 경로 식 — 키, 와일드카드, 배열 첨자

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 3: 카탈로그 (`catalog`)

**Files:**
- Create: `tools/overlay/catalog.py`
- Create: `tools/overlay/tests/support.py`
- Test: `tools/overlay/tests/test_catalog.py`

**Interfaces:**
- Consumes: `paths.parse`, `paths.matches`, `paths.PathError`, `jsonedit.OverlayError`
- Produces:
  - `class CatalogError(OverlayError)`, `RUNTIME = ("VERIFIED", "SEEN", "UNSEEN")`, `EFFECT = ("OFFICIAL", "TESTED", "INFERRED", "UNKNOWN")`
  - `@dataclass(frozen=True) class Group: file: str; paths: tuple; runtime: str; effect: str; effect_by: str; experimental: bool; min; max; note: str` — `experimental`은 적어 두었거나 `runtime`이 `UNSEEN`이면 참
  - `@dataclass(frozen=True) class Catalog: game_version: str; files: dict; groups: tuple` — `files`는 `{상대경로: SHA256}`. `files_matching(pattern: str) -> list[str]`, `find(file: str, path: tuple) -> Group | None`
  - `read_json(path)` — 엄격한 JSON, 같은 키가 두 번이면 `CatalogError`
  - `check_rel(rel) -> str`, `file_matches(pattern: str, rel: str) -> bool`, `number(value, what: str)` — 유한한 수가 아니면 `CatalogError`
  - `parse_keys(data) -> tuple[str, tuple]`, `parse_files(data) -> tuple[str, dict]`, `build(keys_data, files_data) -> Catalog`, `load(directory) -> Catalog`
  - 시험 지원 `support`: `VERSION`, `FILES`(상대경로 → 글), `GROUPS`, `sha(text)`, `keys_data(groups=None)`, `files_data(files=None)`, `make_catalog(groups=None, files=None) -> Catalog`

- [ ] **Step 1: 시험 지원 모듈과 실패하는 시험을 쓴다**

`tools/overlay/tests/support.py`:

```python
"""시험이 함께 쓰는 것: 작은 가짜 게임과 그 카탈로그."""
import hashlib
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import catalog

VERSION = "1.2.3.4"

# 게임 파일의 모양을 줄인 것. debug.json 에는 CRLF, 탭, 닫는 괄호 앞의 쉼표, 긴 소수, '.0' 꼴이 있다.
FILES = {
    "debug.json": '{\r\n\t"budget_money": 2000,\r\n\t"factor": 0.5,\r\n\t"production_cost": {\r\n\t\t"ale": 2,\r\n'
                  '\t\t"coal": 0.90000000000000002,\r\n\t},\r\n\t"building_resources": {\r\n'
                  '\t\t"hut": [["wood", 10], ["iron", 5]],\r\n\t\t"mine": [["wood", 15]],\r\n\t},\r\n'
                  '\t"name": "x",\r\n\t"slave": {"cost": 50.0}\r\n}',
    "books/a.json": '{"upgrade_skill":[{"value":16,"name":"combat"}],"tag":0}',
    "books/b.json": '{"upgrade_skill":[],"tag":0}',
}

GROUPS = [
    {"file": "debug.json", "paths": ["budget_money"], "runtime": "VERIFIED", "effect": "UNKNOWN", "min": 0, "max": 100000},
    {"file": "debug.json", "paths": ["factor", "production_cost.*"], "runtime": "SEEN", "effect": "UNKNOWN"},
    {"file": "debug.json", "paths": ["building_resources.*[*][1]"], "runtime": "UNSEEN", "effect": "UNKNOWN"},
    {"file": "debug.json", "paths": ["slave.*"], "runtime": "SEEN", "effect": "UNKNOWN", "experimental": True},
    {"file": "books/*.json", "paths": ["upgrade_skill[*].value"], "runtime": "SEEN", "effect": "TESTED", "effect_by": "community"},
]


def sha(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest().upper()


def keys_data(groups=None):
    return {"game_version": VERSION, "groups": GROUPS if groups is None else groups}


def files_data(files=None):
    return {"game_version": VERSION, "files": {rel: sha(text) for rel, text in (FILES if files is None else files).items()}}


def make_catalog(groups=None, files=None):
    return catalog.build(keys_data(groups), files_data(files))
```

`tools/overlay/tests/test_catalog.py`:

```python
"""catalog.py 의 시험."""
import copy
import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import support
import catalog
import paths
from catalog import CatalogError


class CatalogTests(unittest.TestCase):
    def test_find_returns_the_first_group_that_matches_file_and_path(self):
        cat = support.make_catalog()
        self.assertEqual(cat.find("debug.json", ("budget_money",)).runtime, "VERIFIED")
        self.assertEqual(cat.find("debug.json", ("production_cost", "ale")).runtime, "SEEN")
        self.assertEqual(cat.find("books/a.json", ("upgrade_skill", 0, "value")).effect_by, "community")
        self.assertIsNone(cat.find("debug.json", ("name",)))
        self.assertIsNone(cat.find("books/a.json", ("budget_money",)))

    def test_unseen_keys_and_marked_keys_are_experimental(self):
        cat = support.make_catalog()
        self.assertFalse(cat.find("debug.json", ("budget_money",)).experimental)
        self.assertTrue(cat.find("debug.json", ("building_resources", "hut", 0, 1)).experimental)
        self.assertTrue(cat.find("debug.json", ("slave", "cost")).experimental)

    def test_file_patterns_match_within_one_path_segment(self):
        self.assertTrue(catalog.file_matches("books/*.json", "books/a.json"))
        self.assertFalse(catalog.file_matches("books/*.json", "books/sub/a.json"))
        self.assertFalse(catalog.file_matches("books/*.json", "books/a.jsonx"))
        self.assertFalse(catalog.file_matches("debug.json", "debugXjson"))
        self.assertEqual(support.make_catalog().files_matching("books/*.json"), ["books/a.json", "books/b.json"])

    def test_paths_outside_the_game_folder_are_refused(self):
        for bad in ("", "..", "../x.json", "a/../x.json", "/x.json", "C:/x.json", "a\\x.json", "a//x.json", "./x.json", None):
            with self.subTest(bad=bad):
                with self.assertRaises(CatalogError):
                    catalog.check_rel(bad)

    def test_a_mistyped_keys_file_is_an_error(self):
        def broken(change):
            groups = copy.deepcopy(support.GROUPS)
            change(groups)
            return groups

        cases = {
            "모르는 항목": lambda g: g[0].update(runtme="SEEN"),
            "runtime": lambda g: g[0].update(runtime="MAYBE"),
            "effect": lambda g: g[0].update(effect="PROVEN"),
            "effect_by": lambda g: g[4].pop("effect_by"),
            "paths": lambda g: g[0].update(paths=[]),
            "점이나 대괄호": lambda g: g[0].update(paths=["a b"]),
            "min 이 max": lambda g: g[0].update(min=5, max=1),
            "유한한 수": lambda g: g[0].update(min="0"),
            "맞는 파일이 없다": lambda g: g[0].update(file="nope.json"),
            "상대경로": lambda g: g[0].update(file="../debug.json"),
        }
        for message, change in cases.items():
            with self.subTest(message=message):
                with self.assertRaises(CatalogError) as caught:
                    support.make_catalog(broken(change))
                self.assertIn(message, str(caught.exception))

    def test_keys_and_files_must_be_for_the_same_game_version(self):
        files = support.files_data()
        files["game_version"] = "9.9.9.9"
        with self.assertRaises(CatalogError):
            catalog.build(support.keys_data(), files)

    def test_file_hashes_must_be_uppercase_sha256(self):
        files = support.files_data()
        files["files"]["debug.json"] = files["files"]["debug.json"].lower()
        with self.assertRaises(CatalogError):
            catalog.build(support.keys_data(), files)

    def test_load_reads_the_two_files_and_rejects_duplicate_keys(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = pathlib.Path(tmp)
            (tmp / "keys.json").write_text(json.dumps(support.keys_data()), encoding="utf-8")
            with self.assertRaises(CatalogError) as caught:
                catalog.load(tmp)
            self.assertIn("files.json", str(caught.exception))
            (tmp / "files.json").write_text(json.dumps(support.files_data()), encoding="utf-8")
            self.assertEqual(len(catalog.load(tmp).files), 3)
            (tmp / "files.json").write_text('{"game_version": "1", "game_version": "2", "files": {}}', encoding="utf-8")
            with self.assertRaises(CatalogError) as caught:
                catalog.load(tmp)
            self.assertIn("두 번", str(caught.exception))

    def test_group_paths_are_parsed(self):
        group = support.make_catalog().groups[2]
        self.assertEqual(group.paths, (paths.parse("building_resources.*[*][1]"),))


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 시험이 실패하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests -p test_catalog.py`
Expected: FAIL. `ModuleNotFoundError: No module named 'catalog'`

- [ ] **Step 3: 구현한다**

`tools/overlay/catalog.py`:

```python
"""카탈로그: 이 도구가 바꿀 수 있는 키와, 키마다 무엇을 실측했는가.

  catalog/<게임 버전>/keys.json    손으로 쓴다. 키의 묶음과 등급
  catalog/<게임 버전>/files.json   `cli.py pin` 이 만든다. 파일마다 바닐라의 SHA256

runtime — 이 레포가 이 빌드에서 잰 것:
  VERIFIED  파일의 값을 바꿔서 런타임에서 그 값을 봤다
  SEEN      바닐라 값이 같은 이름으로 런타임에 있는 것을 봤다
  UNSEEN    런타임에서 본 적이 없다 (찾지 않았거나, 찾았는데 없었다)
effect — 값이 게임을 바꾸는가:
  OFFICIAL  개발진이나 공식 문서가 말했다
  TESTED    게임에서 바꿔 보고 달라진 것을 봤다 (effect_by 에 누가 봤는지 적는다)
  INFERRED  이름이나 구조로 미루어 본 것
  UNKNOWN   모른다
"""
import dataclasses
import json
import math
import pathlib
import re

import paths
from jsonedit import OverlayError

RUNTIME = ("VERIFIED", "SEEN", "UNSEEN")
EFFECT = ("OFFICIAL", "TESTED", "INFERRED", "UNKNOWN")
_GROUP_FIELDS = {"file", "paths", "runtime", "effect", "effect_by", "experimental", "min", "max", "note"}
_SHA256 = re.compile(r"[0-9A-F]{64}")


class CatalogError(OverlayError):
    pass


@dataclasses.dataclass(frozen=True)
class Group:
    file: str               # 게임 폴더 기준 상대경로의 패턴
    paths: tuple            # paths.parse 한 식들
    runtime: str
    effect: str
    effect_by: str
    experimental: bool      # 적어 두었거나 runtime 이 UNSEEN 이면 참
    min: object             # 수 또는 None
    max: object
    note: str


@dataclasses.dataclass(frozen=True)
class Catalog:
    game_version: str
    files: dict             # 상대경로('/' 로 나눈다) → 바닐라의 SHA256(대문자)
    groups: tuple

    def files_matching(self, pattern):
        return sorted(rel for rel in self.files if file_matches(pattern, rel))

    def find(self, file, path):
        """그 파일의 그 값에 맞는 첫 묶음. 없으면 None."""
        for group in self.groups:
            if file_matches(group.file, file) and any(paths.matches(pattern, path) for pattern in group.paths):
                return group
        return None


def read_json(path):
    """이 도구의 파일(카탈로그, 프리셋)을 읽는다. 엄격한 JSON 이고, 한 객체에 같은 키가 두 번 나오면 오류다."""
    def pairs(items):
        keys = [key for key, _ in items]
        twice = sorted({key for key in keys if keys.count(key) > 1})
        if twice:
            raise CatalogError(f"같은 키가 두 번 나온다: {twice} ({path})")
        return dict(items)

    try:
        with open(path, encoding="utf-8-sig") as f:
            return json.load(f, object_pairs_hook=pairs)
    except FileNotFoundError:
        raise CatalogError(f"파일이 없다: {path}") from None
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise CatalogError(f"읽을 수 없다: {path}: {error}") from None


def check_rel(rel):
    """게임 폴더 안의 상대경로(또는 그 패턴)인지 본다. 조각은 '/' 로 나눈다."""
    if not isinstance(rel, str) or not rel or "\\" in rel or ":" in rel or rel.startswith("/"):
        raise CatalogError(f"게임 폴더 안의 상대경로여야 한다('/' 로 나눈다): {rel!r}")
    if any(part in ("", ".", "..") for part in rel.split("/")):
        raise CatalogError(f"게임 폴더 안의 상대경로여야 한다: {rel!r}")
    return rel


def file_matches(pattern, rel):
    """패턴의 '*' 는 한 조각 안의 아무 글자다. 대소문자를 가린다."""
    regex = "/".join("[^/]*".join(re.escape(piece) for piece in part.split("*")) for part in pattern.split("/"))
    return re.fullmatch(regex, rel) is not None


def number(value, what):
    """유한한 수인지 본다. 불리언은 수로 치지 않는다."""
    if isinstance(value, bool) or not isinstance(value, (int, float)) or (isinstance(value, float) and not math.isfinite(value)):
        raise CatalogError(f"{what} 는 유한한 수여야 한다: {value!r}")
    return value


def parse_keys(data):
    """keys.json 의 내용을 (게임 버전, 묶음들)로 바꾼다."""
    if not isinstance(data, dict) or set(data) != {"game_version", "groups"}:
        raise CatalogError("keys.json 은 game_version 과 groups 만 가진 객체여야 한다")
    if not isinstance(data["game_version"], str) or not isinstance(data["groups"], list):
        raise CatalogError("keys.json: game_version 은 글, groups 는 배열이어야 한다")
    groups = []
    for n, raw in enumerate(data["groups"], 1):
        where = f"keys.json 의 묶음 #{n}"
        if not isinstance(raw, dict):
            raise CatalogError(f"{where}: 객체여야 한다")
        unknown = sorted(set(raw) - _GROUP_FIELDS)
        if unknown:
            raise CatalogError(f"{where}: 모르는 항목 {unknown}")
        for field in ("file", "paths", "runtime", "effect"):
            if field not in raw:
                raise CatalogError(f"{where}: {field} 가 없다")
        if not isinstance(raw["paths"], list) or not raw["paths"]:
            raise CatalogError(f"{where}: paths 는 비어 있지 않은 배열이어야 한다")
        if raw["runtime"] not in RUNTIME:
            raise CatalogError(f"{where}: runtime 은 {RUNTIME} 가운데 하나여야 한다: {raw['runtime']!r}")
        if raw["effect"] not in EFFECT:
            raise CatalogError(f"{where}: effect 는 {EFFECT} 가운데 하나여야 한다: {raw['effect']!r}")
        effect_by = raw.get("effect_by", "")
        if not isinstance(effect_by, str) or (raw["effect"] == "TESTED" and not effect_by):
            raise CatalogError(f"{where}: effect 가 TESTED 면 effect_by 에 누가 봤는지 적는다")
        if not isinstance(raw.get("experimental", False), bool) or not isinstance(raw.get("note", ""), str):
            raise CatalogError(f"{where}: experimental 은 불리언, note 는 글이어야 한다")
        low = None if raw.get("min") is None else number(raw["min"], f"{where}: min")
        high = None if raw.get("max") is None else number(raw["max"], f"{where}: max")
        if low is not None and high is not None and low > high:
            raise CatalogError(f"{where}: min 이 max 보다 크다")
        try:
            parsed = tuple(paths.parse(expr) for expr in raw["paths"])
        except paths.PathError as error:
            raise CatalogError(f"{where}: {error}") from None
        groups.append(Group(check_rel(raw["file"]), parsed, raw["runtime"], raw["effect"], effect_by,
                            raw.get("experimental", False) or raw["runtime"] == "UNSEEN", low, high, raw.get("note", "")))
    return data["game_version"], tuple(groups)


def parse_files(data):
    """files.json 의 내용을 (게임 버전, {상대경로: SHA256})로 바꾼다."""
    if not isinstance(data, dict) or set(data) != {"game_version", "files"} or not isinstance(data["files"], dict):
        raise CatalogError("files.json 은 game_version 과 files(객체)만 가진 객체여야 한다")
    for rel, sha in data["files"].items():
        check_rel(rel)
        if "*" in rel or not isinstance(sha, str) or not _SHA256.fullmatch(sha):
            raise CatalogError(f"files.json: {rel!r} 의 값은 대문자 16진 SHA256 이어야 하고, 이름에 * 를 쓸 수 없다")
    return data["game_version"], dict(data["files"])


def build(keys_data, files_data):
    version, groups = parse_keys(keys_data)
    files_version, files = parse_files(files_data)
    if version != files_version:
        raise CatalogError(f"keys.json({version})과 files.json({files_version})의 게임 버전이 다르다")
    for n, group in enumerate(groups, 1):
        if not any(file_matches(group.file, rel) for rel in files):
            raise CatalogError(f"keys.json 의 묶음 #{n}: files.json 에 {group.file!r} 에 맞는 파일이 없다 (cli.py pin 으로 다시 만든다)")
    return Catalog(version, files, groups)


def load(directory):
    directory = pathlib.Path(directory)
    return build(read_json(directory / "keys.json"), read_json(directory / "files.json"))
```

- [ ] **Step 4: 시험이 통과하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests`
Expected: `Ran 31 tests`, `OK`

- [ ] **Step 5: 커밋한다**

```powershell
git -C E:\NlToyBox add tools/overlay/catalog.py tools/overlay/tests/support.py tools/overlay/tests/test_catalog.py
git -C E:\NlToyBox commit -m @'
feat(overlay): 카탈로그 — 키의 묶음, 실측 등급, 바닐라 해시

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 4: 프리셋과 계산 (`preset`, `compose`)

**Files:**
- Create: `tools/overlay/preset.py`, `tools/overlay/compose.py`
- Test: `tools/overlay/tests/test_preset.py`, `tools/overlay/tests/test_compose.py`

**Interfaces:**
- Consumes: `catalog.read_json`, `catalog.check_rel`, `catalog.number`, `catalog.Catalog.files_matching`, `catalog.Catalog.find`, `paths.parse`, `paths.matches`, `paths.show`, `jsonedit.scan`, `jsonedit.value_of`, `jsonedit.format_number`, `jsonedit.apply_edits`, `support.make_catalog`, `support.FILES`, `support.VERSION`
- Produces:
  - `class PresetError(OverlayError)`, `preset.ROUNDINGS = ("ceil", "floor", "nearest")`
  - `@dataclass(frozen=True) class Change: file: str; path: tuple; path_text: str; op: str; value; rounding: str` — `op`는 `set` 또는 `mul`
  - `@dataclass(frozen=True) class Preset: name: str; game_version: str; allow_experimental: bool; changes: tuple`
  - `preset.parse(data) -> Preset`, `preset.load(path) -> Preset`
  - `class ComposeError(OverlayError)`
  - `@dataclass(frozen=True) class Edit: file: str; path: tuple; old; new; old_text: str; new_text: str; group: Group; slot: Slot`
  - `compose.targets(preset, cat) -> list[str]` — 프리셋이 닿는 파일들
  - `compose.multiply(old, factor, rounding: str)` — 곱하고 다듬고 올림·내림한 값
  - `compose.compose(preset, cat, vanilla: dict[str, str]) -> tuple[dict[str, str], list[Edit]]` — (바뀐 파일의 새 글, 변경 목록)

- [ ] **Step 1: 프리셋의 실패하는 시험을 쓴다**

`tools/overlay/tests/test_preset.py`:

```python
"""preset.py 의 시험."""
import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import support
import paths
import preset
from preset import PresetError


def data(*changes, **extra):
    return {"name": "T", "game_version": support.VERSION, "changes": list(changes), **extra}


class PresetTests(unittest.TestCase):
    def test_parse_reads_set_and_mul(self):
        p = preset.parse(data({"file": "debug.json", "path": "budget_money", "set": 5000},
                              {"file": "debug.json", "path": "building_resources.*[*][1]", "mul": 0.5, "round": "ceil"}))
        self.assertEqual((p.name, p.game_version, p.allow_experimental), ("T", support.VERSION, False))
        self.assertEqual((p.changes[0].op, p.changes[0].value, p.changes[0].rounding), ("set", 5000, ""))
        self.assertEqual((p.changes[1].op, p.changes[1].value, p.changes[1].rounding), ("mul", 0.5, "ceil"))
        self.assertEqual(p.changes[1].path, paths.parse("building_resources.*[*][1]"))
        self.assertEqual(p.changes[1].path_text, "building_resources.*[*][1]")

    def test_a_preset_may_have_no_changes(self):
        self.assertEqual(preset.parse(data()).changes, ())

    def test_allow_experimental_is_read(self):
        self.assertTrue(preset.parse(data(allow_experimental=True)).allow_experimental)

    def test_a_mistyped_preset_is_an_error(self):
        ok = {"file": "debug.json", "path": "budget_money", "set": 1}
        cases = {
            "모르는 항목": data(dict(ok, sett=2)),
            "하나만": data({"file": "debug.json", "path": "budget_money"}),
            "하나만 ": data(dict(ok, mul=2)),
            "round": data(dict(ok, round="ceil")),
            "round ": data({"file": "debug.json", "path": "budget_money", "mul": 2, "round": "up"}),
            "유한한 수": data(dict(ok, set="5")),
            "유한한 수 ": data(dict(ok, set=True)),
            "경로": data(dict(ok, path="")),
            "상대경로": data(dict(ok, file="../debug.json")),
            "name": {"game_version": "1", "changes": []},
            "changes": {"name": "T", "game_version": "1"},
            "allow_experimental": data(allow_experimental="yes"),
            "모르는 항목 ": data(note="x"),
        }
        for message, bad in cases.items():
            with self.subTest(message=message):
                with self.assertRaises(PresetError) as caught:
                    preset.parse(bad)
                self.assertIn(message.strip(), str(caught.exception))

    def test_errors_name_the_change(self):
        with self.assertRaises(PresetError) as caught:
            preset.parse(data({"file": "debug.json", "path": "budget_money", "set": 1}, {"file": "debug.json", "path": "a..b", "set": 1}))
        self.assertIn("변경 #2", str(caught.exception))

    def test_load_reads_a_file_and_reports_bad_json_as_a_preset_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / "p.json"
            path.write_text(json.dumps(data({"file": "debug.json", "path": "budget_money", "set": 1})), encoding="utf-8")
            self.assertEqual(preset.load(path).changes[0].value, 1)
            path.write_text('{"name": "T", }', encoding="utf-8")
            with self.assertRaises(PresetError):
                preset.load(path)
            with self.assertRaises(PresetError):
                preset.load(pathlib.Path(tmp) / "none.json")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 시험이 실패하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests -p test_preset.py`
Expected: FAIL. `ModuleNotFoundError: No module named 'preset'`

- [ ] **Step 3: 프리셋을 구현한다**

`tools/overlay/preset.py`:

```python
"""프리셋: 바닐라 위에 입힐 변경의 목록.

  { "name": "Easy", "game_version": "0.5588.9777.0", "allow_experimental": false,
    "changes": [
      { "file": "debug_params.json", "path": "budget_money", "set": 5000 },
      { "file": "debug_params.json", "path": "building_resources.*[*][1]", "mul": 0.5, "round": "ceil" } ] }

- 연산은 set(수를 지정)과 mul(바닐라 값에 곱함) 둘이다. 수만 바꾼다.
- 한 값에 변경이 둘 닿으면 뒤의 것이 이긴다. 둘 다 바닐라 값에서 계산한다(쌓이지 않는다).
"""
import dataclasses

import catalog
import paths
from jsonedit import OverlayError

ROUNDINGS = ("ceil", "floor", "nearest")
_FIELDS = {"name", "game_version", "allow_experimental", "changes"}
_CHANGE_FIELDS = {"file", "path", "set", "mul", "round"}


class PresetError(OverlayError):
    pass


@dataclasses.dataclass(frozen=True)
class Change:
    file: str           # 게임 폴더 기준 상대경로의 패턴
    path: tuple         # paths.parse 한 식
    path_text: str
    op: str             # set 또는 mul
    value: object       # 수
    rounding: str       # mul 일 때만: ceil, floor, nearest, 또는 빈 글


@dataclasses.dataclass(frozen=True)
class Preset:
    name: str
    game_version: str
    allow_experimental: bool
    changes: tuple


def parse(data):
    if not isinstance(data, dict):
        raise PresetError("프리셋은 객체여야 한다")
    unknown = sorted(set(data) - _FIELDS)
    if unknown:
        raise PresetError(f"프리셋에 모르는 항목이 있다: {unknown}")
    for field in ("name", "game_version"):
        if not isinstance(data.get(field), str) or not data[field]:
            raise PresetError(f"프리셋의 {field} 는 비어 있지 않은 글이어야 한다")
    if not isinstance(data.get("allow_experimental", False), bool):
        raise PresetError("프리셋의 allow_experimental 은 불리언이어야 한다")
    if not isinstance(data.get("changes"), list):
        raise PresetError("프리셋의 changes 는 배열이어야 한다")
    changes = []
    for n, raw in enumerate(data["changes"], 1):
        where = f"변경 #{n}"
        if not isinstance(raw, dict):
            raise PresetError(f"{where}: 객체여야 한다")
        unknown = sorted(set(raw) - _CHANGE_FIELDS)
        if unknown:
            raise PresetError(f"{where}: 모르는 항목 {unknown}")
        ops = [op for op in ("set", "mul") if op in raw]
        if len(ops) != 1:
            raise PresetError(f"{where}: set 과 mul 가운데 하나만 있어야 한다")
        rounding = raw.get("round", "")
        if rounding and (ops[0] != "mul" or rounding not in ROUNDINGS):
            raise PresetError(f"{where}: round 는 mul 과 함께만 쓰고 {ROUNDINGS} 가운데 하나여야 한다")
        try:
            file = catalog.check_rel(raw.get("file"))
            value = catalog.number(raw[ops[0]], ops[0])
            path = paths.parse(raw.get("path"))
        except OverlayError as error:
            raise PresetError(f"{where}: {error}") from None
        changes.append(Change(file, path, raw["path"], ops[0], value, rounding))
    return Preset(data["name"], data["game_version"], data.get("allow_experimental", False), tuple(changes))


def load(path):
    try:
        return parse(catalog.read_json(path))
    except catalog.CatalogError as error:
        raise PresetError(str(error)) from None
```

- [ ] **Step 4: 시험이 통과하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests -p test_preset.py`
Expected: `Ran 6 tests`, `OK`

- [ ] **Step 5: 계산의 실패하는 시험을 쓴다**

`tools/overlay/tests/test_compose.py`:

```python
"""compose.py 의 시험."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import support
import compose
import preset
from compose import ComposeError

DEBUG = support.FILES["debug.json"]


def run(*changes, allow_experimental=False, cat=None):
    p = preset.parse({"name": "T", "game_version": support.VERSION, "allow_experimental": allow_experimental,
                      "changes": list(changes)})
    return compose.compose(p, cat or support.make_catalog(), dict(support.FILES))


def change(path, file="debug.json", **op):
    return {"file": file, "path": path, **op}


class ComposeTests(unittest.TestCase):
    def test_set_changes_only_the_text_of_that_value(self):
        texts, edits = run(change("budget_money", set=5000))
        self.assertEqual(texts, {"debug.json": DEBUG.replace('"budget_money": 2000', '"budget_money": 5000')})
        self.assertEqual([(e.file, e.path, e.old, e.new, e.old_text, e.new_text) for e in edits],
                         [("debug.json", ("budget_money",), 2000, 5000, "2000", "5000")])
        self.assertEqual(edits[0].group.runtime, "VERIFIED")

    def test_mul_multiplies_the_vanilla_value_and_rounds_as_asked(self):
        texts, _ = run(change("building_resources.*[*][1]", mul=0.5, round="ceil"), allow_experimental=True)
        self.assertEqual(texts["debug.json"], DEBUG.replace('["wood", 10], ["iron", 5]', '["wood", 5], ["iron", 3]')
                         .replace('["wood", 15]', '["wood", 8]'))

    def test_rounding_modes(self):
        self.assertEqual(compose.multiply(15, 0.5, "ceil"), 8)
        self.assertEqual(compose.multiply(15, 0.5, "floor"), 7)
        self.assertEqual(compose.multiply(15, 0.5, "nearest"), 8)      # 반은 올린다
        self.assertEqual(compose.multiply(5, 0.5, "nearest"), 3)
        self.assertEqual(compose.multiply(15, 0.5, ""), 7.5)

    def test_mul_trims_float_noise_before_rounding(self):
        self.assertEqual(compose.multiply(0.1, 3, ""), 0.3)             # 0.30000000000000004 가 아니다
        self.assertEqual(compose.multiply(0.1, 30, "ceil"), 3)          # 3.0000000000000004 를 4 로 올리지 않는다

    def test_a_long_float_is_rewritten_only_when_its_value_changes(self):
        texts, edits = run(change("production_cost.coal", set=0.9))     # 파일에는 0.90000000000000002 로 적혀 있다
        self.assertEqual((texts, edits), ({}, []))
        texts, _ = run(change("production_cost.coal", mul=2))
        self.assertIn('"coal": 1.8,', texts["debug.json"])

    def test_the_decimal_point_of_the_old_text_is_kept(self):
        texts, _ = run(change("slave.cost", set=60), allow_experimental=True)
        self.assertIn('"slave": {"cost": 60.0}', texts["debug.json"])

    def test_a_later_change_wins_and_both_start_from_vanilla(self):
        texts, edits = run(change("production_cost.*", mul=10), change("production_cost.ale", mul=3))
        self.assertIn('"ale": 6,', texts["debug.json"])                 # 2×3 이다. 2×10×3 이 아니다
        self.assertIn('"coal": 9.0,', texts["debug.json"])             # 소수점으로 적혀 있던 자리다
        self.assertEqual(len(edits), 2)

    def test_a_later_change_back_to_vanilla_cancels_the_earlier_one(self):
        self.assertEqual(run(change("budget_money", set=5000), change("budget_money", set=2000)), ({}, []))

    def test_a_preset_with_no_changes_changes_nothing(self):
        self.assertEqual(run(), ({}, []))

    def test_a_file_pattern_reaches_every_file_that_has_the_path(self):
        texts, edits = run(change("upgrade_skill[*].value", file="books/*.json", mul=2))
        self.assertEqual(texts, {"books/a.json": '{"upgrade_skill":[{"value":32,"name":"combat"}],"tag":0}'})
        self.assertEqual(edits[0].file, "books/a.json")

    def test_targets_lists_the_files_a_preset_touches(self):
        p = preset.parse({"name": "T", "game_version": support.VERSION, "changes": [
            change("upgrade_skill[*].value", file="books/*.json", mul=2), change("budget_money", set=1)]})
        self.assertEqual(compose.targets(p, support.make_catalog()), ["books/a.json", "books/b.json", "debug.json"])

    def test_what_cannot_be_applied_is_an_error(self):
        cases = {
            "아무 값에도 닿지 않는다": [change("budget_mony", set=1)],
            "카탈로그에 없는 파일": [change("budget_money", file="other.json", set=1)],
            "카탈로그에 없는 키": [change("tag", file="books/a.json", set=1)],
            "수가 아닌 값": [change("name", set=1)],
            "수가 아닌 값 ": [change("production_cost", set=1)],
            "실험 키": [change("building_resources.hut[0][1]", set=1)],
            "실험 키 ": [change("slave.cost", set=1)],
            "허용 범위": [change("budget_money", set=-1)],
            "허용 범위 ": [change("budget_money", mul=1000)],
        }
        for message, changes in cases.items():
            with self.subTest(message=message):
                with self.assertRaises(ComposeError) as caught:
                    run(*changes)
                self.assertIn(message.strip(), str(caught.exception))
                self.assertIn("변경 #1", str(caught.exception))

    def test_an_unreadable_game_file_is_reported_with_its_name(self):
        cat = support.make_catalog()
        p = preset.parse({"name": "T", "game_version": support.VERSION, "changes": [change("budget_money", set=1)]})
        with self.assertRaises(ComposeError) as caught:
            compose.compose(p, cat, {"debug.json": '{"budget_money": 2000 // 주석\n}'})
        self.assertIn("debug.json", str(caught.exception))


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 6: 시험이 실패하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests -p test_compose.py`
Expected: FAIL. `ModuleNotFoundError: No module named 'compose'`

- [ ] **Step 7: 계산을 구현한다**

`tools/overlay/compose.py`:

```python
"""프리셋과 카탈로그와 바닐라 글로 새 글을 계산한다. 파일을 읽거나 쓰지 않는다."""
import dataclasses
import math

import jsonedit
import paths
from jsonedit import OverlayError


class ComposeError(OverlayError):
    pass


@dataclasses.dataclass(frozen=True)
class Edit:
    file: str
    path: tuple
    old: object         # 바닐라 값 (int 또는 float)
    new: object
    old_text: str       # 바닐라 파일에 적힌 글
    new_text: str
    group: object       # catalog.Group
    slot: object        # jsonedit.Slot (바닐라 글에서의 자리)


def targets(preset, cat):
    """프리셋이 닿는 파일의 상대경로들."""
    out = set()
    for n, change in enumerate(preset.changes, 1):
        files = cat.files_matching(change.file)
        if not files:
            raise ComposeError(f"변경 #{n}: 카탈로그에 없는 파일이다: {change.file}")
        out.update(files)
    return sorted(out)


def multiply(old, factor, rounding):
    """곱한 값. 유효숫자 12자리로 다듬은 뒤(0.1×3 = 0.30000000000000004 를 0.3 으로) 올림·내림한다."""
    value = float(format(old * factor, ".12g"))
    if rounding == "ceil":
        return math.ceil(value)
    if rounding == "floor":
        return math.floor(value)
    if rounding == "nearest":
        return math.floor(value + 0.5)
    return value


def compose(preset, cat, vanilla):
    """vanilla: {상대경로: 바닐라 글}. targets() 가 말한 파일이 모두 들어 있어야 한다.
    ({상대경로: 새 글}, [Edit])를 돌려준다. 새 글에는 바뀐 파일만 있다. 바닐라와 같은 값은 건드리지 않는다."""
    slots = {}
    chosen = {}         # (파일, 자리의 시작) → Edit. 뒤의 변경이 앞의 것을 덮는다
    for n, change in enumerate(preset.changes, 1):
        where = f"변경 #{n} ({change.file} {change.path_text})"
        files = cat.files_matching(change.file)
        if not files:
            raise ComposeError(f"변경 #{n}: 카탈로그에 없는 파일이다: {change.file}")
        reached = 0
        for rel in files:
            text = vanilla[rel]
            if rel not in slots:
                try:
                    slots[rel] = jsonedit.scan(text)
                except jsonedit.JsonError as error:
                    raise ComposeError(f"{rel}: {error}") from None
            for slot in slots[rel]:
                if not paths.matches(change.path, slot.path):
                    continue
                reached += 1
                shown = f"{rel} {paths.show(slot.path)}"
                if slot.kind != "number":
                    raise ComposeError(f"{where}: 수가 아닌 값에 닿는다: {shown} ({slot.kind})")
                group = cat.find(rel, slot.path)
                if group is None:
                    raise ComposeError(f"{where}: 카탈로그에 없는 키다: {shown}")
                if group.experimental and not preset.allow_experimental:
                    raise ComposeError(f"{where}: 실험 키다(runtime {group.runtime}): {shown}. "
                                       "바꾸려면 프리셋에 \"allow_experimental\": true 를 적는다")
                old = jsonedit.value_of(text, slot)
                new = change.value if change.op == "set" else multiply(old, change.value, change.rounding)
                if (group.min is not None and new < group.min) or (group.max is not None and new > group.max):
                    raise ComposeError(f"{where}: 허용 범위 [{group.min}, {group.max}] 를 벗어난다: {shown} = {new}")
                key = (rel, slot.start)
                if new == old:
                    chosen.pop(key, None)
                    continue
                old_text = text[slot.start:slot.end]
                chosen[key] = Edit(rel, slot.path, old, new, old_text, jsonedit.format_number(new, old_text), group, slot)
        if reached == 0:
            raise ComposeError(f"{where}: 아무 값에도 닿지 않는다")
    edits = [chosen[key] for key in sorted(chosen)]
    texts = {}
    for rel in sorted({edit.file for edit in edits}):
        texts[rel] = jsonedit.apply_edits(vanilla[rel], [(edit.slot, edit.new_text) for edit in edits if edit.file == rel])
    return texts, edits
```

- [ ] **Step 8: 시험이 통과하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests`
Expected: `Ran 50 tests`, `OK`

- [ ] **Step 9: 커밋한다**

```powershell
git -C E:\NlToyBox add tools/overlay/preset.py tools/overlay/compose.py tools/overlay/tests/test_preset.py tools/overlay/tests/test_compose.py
git -C E:\NlToyBox commit -m @'
feat(overlay): 프리셋 읽기와 새 글 계산

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 5: 적용과 복원 (`store`)

**Files:**
- Create: `tools/overlay/store.py`
- Modify: `tools/overlay/tests/support.py` (파일 끝에 더한다)
- Test: `tools/overlay/tests/test_store.py`

**Interfaces:**
- Consumes: `compose.targets`, `compose.compose`, `jsonedit.decode`, `jsonedit.encode`, `catalog.Catalog`, `preset.Preset`, `support.*`
- Produces:
  - `class StoreError(OverlayError)`, `TMP_SUFFIX = ".nltoybox-tmp"`, `STATE_NAME = "state.json"`
  - `@dataclass(frozen=True) class Dirs: game: Path; snapshot: Path; state: Path`
  - `digest(data: bytes) -> str` — 대문자 SHA256. `at(root, rel: str) -> Path`. `write(path: Path, data: bytes)` — 임시 파일에 쓰고 바꿔치기
  - `read_state(dirs) -> dict` — `{"preset": …, "files": {상대경로: SHA256}}`
  - `classify(dirs, cat) -> dict[str, str]` — 파일마다 `vanilla`, `applied`, `unknown`, `missing`
  - `vanilla_bytes(dirs, cat, rel: str, status: dict, create: bool) -> bytes`
  - `plan(dirs, cat, preset, game_version: str, create_snapshots=False) -> tuple[dict, dict[str, bytes], list[Edit]]` — (상태, 새 바이트, 변경 목록). `create_snapshots`가 거짓이면 아무것도 쓰지 않는다
  - `apply(dirs, cat, preset, game_version: str) -> tuple[list[Edit], list[str], list[str]]` — (변경 목록, 쓴 파일, 바닐라로 되돌린 파일)
  - `restore(dirs, cat, game_version: str) -> list[str]`
  - 시험 지원 `support.make_game(root, files=None) -> Dirs`, `support.game_bytes(dirs) -> dict[str, bytes]`

- [ ] **Step 1: 시험 지원 모듈에 가짜 게임을 더하고, 실패하는 시험을 쓴다**

`tools/overlay/tests/support.py`의 끝에, 빈 줄 둘을 두고 더한다(`import store`가 파일 가운데에 오는 것은 뜻한 것이다. 이 아래는 `store.py`가 있어야 돈다):

```python
# --- 가짜 게임 폴더 (store.py 의 시험부터 쓴다) ---
import store


def make_game(root, files=None):
    """root 아래에 가짜 게임 폴더를 만들고 store.Dirs 를 돌려준다. 스냅샷·상태 폴더는 만들지 않는다."""
    root = pathlib.Path(root)
    dirs = store.Dirs(root / "game", root / "backups" / "data" / VERSION, root / "backups" / "overlay" / VERSION)
    for rel, text in (FILES if files is None else files).items():
        path = store.at(dirs.game, rel)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(text.encode("utf-8"))
    return dirs


def game_bytes(dirs):
    return {path.relative_to(dirs.game).as_posix(): path.read_bytes() for path in sorted(dirs.game.rglob("*")) if path.is_file()}
```

`tools/overlay/tests/test_store.py`:

```python
"""store.py 의 시험. 임시 폴더의 가짜 게임으로 돈다."""
import json
import pathlib
import sys
import tempfile
import unittest
import unittest.mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import support
import catalog
import preset
import store
from store import StoreError


def make(*changes, name="T", version=support.VERSION, allow_experimental=False):
    return preset.parse({"name": name, "game_version": version, "allow_experimental": allow_experimental,
                         "changes": [{"file": "debug.json", **c} if "file" not in c else c for c in changes]})


GOLD = make({"path": "budget_money", "set": 5000}, name="Gold")
BOOKS = make({"file": "books/*.json", "path": "upgrade_skill[*].value", "mul": 2}, name="Books")


class StoreTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.dirs = support.make_game(self.tmp.name)
        self.cat = support.make_catalog()
        self.vanilla = support.game_bytes(self.dirs)

    def apply(self, p):
        return store.apply(self.dirs, self.cat, p, support.VERSION)

    def test_apply_writes_the_value_keeps_a_snapshot_and_records_the_hash(self):
        edits, written, restored = self.apply(GOLD)
        self.assertEqual((len(edits), written, restored), (1, ["debug.json"], []))
        now = support.game_bytes(self.dirs)
        self.assertEqual(now["debug.json"], self.vanilla["debug.json"].replace(b'"budget_money": 2000', b'"budget_money": 5000'))
        self.assertEqual(now["books/a.json"], self.vanilla["books/a.json"])
        self.assertEqual((self.dirs.snapshot / "debug.json").read_bytes(), self.vanilla["debug.json"])
        self.assertFalse((self.dirs.snapshot / "books").exists())          # 건드리지 않은 파일은 스냅샷도 뜨지 않는다
        state = json.loads((self.dirs.state / "state.json").read_text(encoding="utf-8"))
        self.assertEqual(state["preset"], "Gold")
        self.assertEqual(state["files"], {"debug.json": store.digest(now["debug.json"])})
        self.assertEqual(store.classify(self.dirs, self.cat)["debug.json"], "applied")

    def test_restore_brings_back_every_byte_and_clears_the_state(self):
        self.apply(GOLD)
        self.assertEqual(store.restore(self.dirs, self.cat, support.VERSION), ["debug.json"])
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)
        self.assertFalse((self.dirs.state / "state.json").exists())
        self.assertEqual(store.restore(self.dirs, self.cat, support.VERSION), [])

    def test_applying_another_preset_starts_from_vanilla(self):
        self.apply(GOLD)
        edits, written, restored = self.apply(BOOKS)
        self.assertEqual((written, restored), (["books/a.json"], ["debug.json"]))
        now = support.game_bytes(self.dirs)
        self.assertEqual(now["debug.json"], self.vanilla["debug.json"])     # 앞 프리셋의 변경이 남지 않는다
        self.assertIn(b'"value":32', now["books/a.json"])
        self.assertEqual(set(store.classify(self.dirs, self.cat).values()), {"vanilla", "applied"})

    def test_applying_the_same_preset_twice_gives_the_same_bytes(self):
        self.apply(GOLD)
        first = support.game_bytes(self.dirs)
        self.apply(GOLD)
        self.assertEqual(support.game_bytes(self.dirs), first)

    def test_a_preset_that_changes_nothing_leaves_every_byte_alone(self):
        edits, written, restored = self.apply(make({"path": "budget_money", "set": 2000}))
        self.assertEqual((edits, written, restored), ([], [], []))
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)

    def test_a_file_changed_outside_the_tool_stops_everything(self):
        self.apply(GOLD)
        applied = support.game_bytes(self.dirs)
        target = self.dirs.game / "books" / "b.json"
        target.write_bytes(b'{"upgrade_skill":[],"tag":1}')                # 게임 갱신이나 손으로 고친 것
        changed = support.game_bytes(self.dirs)
        for action in (lambda: self.apply(BOOKS), lambda: store.restore(self.dirs, self.cat, support.VERSION),
                       lambda: store.plan(self.dirs, self.cat, GOLD, support.VERSION)):
            with self.assertRaises(StoreError) as caught:
                action()
            self.assertIn("books/b.json", str(caught.exception))
        self.assertEqual(support.game_bytes(self.dirs), changed)           # 아무것도 쓰지 않았다
        self.assertNotEqual(changed, applied)

    def test_a_missing_game_file_stops_everything(self):
        (self.dirs.game / "books" / "b.json").unlink()
        with self.assertRaises(StoreError):
            self.apply(GOLD)
        self.assertFalse(self.dirs.snapshot.exists())

    def test_the_game_version_must_match_catalog_and_preset(self):
        with self.assertRaises(StoreError) as caught:
            store.apply(self.dirs, self.cat, GOLD, "9.9.9.9")
        self.assertIn("카탈로그", str(caught.exception))
        with self.assertRaises(StoreError) as caught:
            self.apply(make({"path": "budget_money", "set": 1}, version="9.9.9.9"))
        self.assertIn("프리셋", str(caught.exception))
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)

    def test_a_snapshot_that_is_not_vanilla_is_not_trusted(self):
        snap = self.dirs.snapshot / "debug.json"
        snap.parent.mkdir(parents=True)
        snap.write_bytes(b'{"budget_money": 1}')                           # 고친 파일에서 뜬 스냅샷
        with self.assertRaises(StoreError) as caught:
            self.apply(GOLD)
        self.assertIn("스냅샷", str(caught.exception))
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)

    def test_an_applied_file_without_its_snapshot_cannot_be_restored(self):
        self.apply(GOLD)
        (self.dirs.snapshot / "debug.json").unlink()
        applied = support.game_bytes(self.dirs)
        for action in (lambda: store.restore(self.dirs, self.cat, support.VERSION), lambda: self.apply(BOOKS)):
            with self.assertRaises(StoreError) as caught:
                action()
            self.assertIn("스냅샷이 없", str(caught.exception))
        self.assertEqual(support.game_bytes(self.dirs), applied)

    def test_a_preset_that_cannot_be_applied_writes_nothing(self):
        with self.assertRaises(Exception):
            self.apply(make({"path": "budget_money", "set": 5000}, {"path": "no_such_key", "set": 1}))
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)
        self.assertFalse((self.dirs.state / "state.json").exists())

    def test_plan_writes_nothing(self):
        status, data, edits = store.plan(self.dirs, self.cat, GOLD, support.VERSION)
        self.assertEqual((sorted(data), len(edits)), (["debug.json"], 1))
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)
        self.assertFalse(self.dirs.snapshot.exists())
        self.assertFalse(self.dirs.state.exists())

    def test_a_crash_at_any_write_leaves_files_the_tool_still_recognises(self):
        both = make({"path": "budget_money", "set": 7000},
                    {"file": "books/*.json", "path": "upgrade_skill[*].value", "mul": 2}, name="Both")
        self.apply(both)                                                   # 스냅샷을 미리 만들어 둔다
        store.restore(self.dirs, self.cat, support.VERSION)
        real = store.write
        # Gold 가 입혀진 위에 Both 를 입히면 쓰기는 넷이다: debug.json 되돌리기, 상태, books/a.json, debug.json
        for crash_at in (1, 2, 3, 4):
            self.apply(GOLD)
            calls = []

            def flaky(path, data):
                calls.append(path)
                if len(calls) == crash_at:
                    raise OSError("죽었다")
                real(path, data)

            with unittest.mock.patch.object(store, "write", flaky):
                with self.assertRaises(OSError):
                    self.apply(both)
            self.assertNotIn("unknown", store.classify(self.dirs, self.cat).values(), f"{crash_at}번째 쓰기")
            store.restore(self.dirs, self.cat, support.VERSION)
            self.assertEqual(support.game_bytes(self.dirs), self.vanilla, f"{crash_at}번째 쓰기")

    def test_no_temporary_files_are_left_behind(self):
        self.apply(GOLD)
        self.apply(BOOKS)
        store.restore(self.dirs, self.cat, support.VERSION)
        root = pathlib.Path(self.tmp.name)
        self.assertEqual([p for p in root.rglob("*") if p.name.endswith(store.TMP_SUFFIX)], [])

    def test_a_bom_and_crlf_survive_apply_and_restore(self):
        files = dict(support.FILES)
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        dirs = support.make_game(tmp.name, files)
        raw = b"\xef\xbb\xbf" + files["debug.json"].encode("utf-8")
        (dirs.game / "debug.json").write_bytes(raw)
        data = support.files_data(files)
        data["files"]["debug.json"] = store.digest(raw)
        cat = catalog.build(support.keys_data(), data)
        store.apply(dirs, cat, GOLD, support.VERSION)
        self.assertEqual((dirs.game / "debug.json").read_bytes(), raw.replace(b'"budget_money": 2000', b'"budget_money": 5000'))
        store.restore(dirs, cat, support.VERSION)
        self.assertEqual((dirs.game / "debug.json").read_bytes(), raw)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 시험이 실패하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests`
Expected: FAIL. `ModuleNotFoundError: No module named 'store'`. `support`를 쓰는 시험 모듈(`test_catalog`, `test_preset`, `test_compose`, `test_store`)이 모두 이 오류로 읽히지 않는다. Step 3 뒤에 함께 풀린다.

- [ ] **Step 3: 구현한다**

`tools/overlay/store.py`:

```python
"""게임 폴더에 프리셋을 입히고 되돌린다. 게임의 데이터 파일을 쓰는 곳은 이 모듈뿐이다.

- 바닐라인지는 카탈로그의 SHA256 으로 판정한다. 스냅샷(backups/data/<버전>/)은 그 바이트의 사본이다.
- 적용은 항상 바닐라에서 출발한다: 앞서 입힌 것을 모두 되돌린 뒤 새 프리셋을 입힌다.
- 상태(backups/overlay/<버전>/state.json)는 새 내용을 쓰기 전에 먼저 적는다. 도중에 죽어도 게임 파일은
  '바닐라' 아니면 '상태에 적힌 해시' 가운데 하나다.
"""
import dataclasses
import datetime
import hashlib
import json
import os
import pathlib

import compose
import jsonedit
from jsonedit import OverlayError

TMP_SUFFIX = ".nltoybox-tmp"
STATE_NAME = "state.json"


class StoreError(OverlayError):
    pass


@dataclasses.dataclass(frozen=True)
class Dirs:
    game: pathlib.Path          # 게임 폴더
    snapshot: pathlib.Path      # backups/data/<게임 버전>
    state: pathlib.Path         # backups/overlay/<게임 버전>


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def at(root, rel):
    return pathlib.Path(root).joinpath(*rel.split("/"))


def write(path, data):
    """임시 파일에 쓰고 바꿔치기한다. 도중에 죽어도 반쯤 쓰인 파일이 남지 않는다."""
    tmp = path.with_name(path.name + TMP_SUFFIX)
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp.write_bytes(data)
        os.replace(tmp, path)
    finally:
        if tmp.exists():
            tmp.unlink()
    if path.read_bytes() != data:
        raise StoreError(f"쓴 뒤에 읽은 내용이 다르다: {path}")


def read_state(dirs):
    path = dirs.state / STATE_NAME
    if not path.exists():
        return {"preset": None, "files": {}}
    try:
        with open(path, encoding="utf-8") as f:
            state = json.load(f)
    except (OSError, ValueError) as error:
        raise StoreError(f"상태 파일을 읽을 수 없다: {path}: {error}") from None
    if not isinstance(state, dict) or not isinstance(state.get("files"), dict):
        raise StoreError(f"상태 파일의 꼴이 다르다: {path}")
    return state


def classify(dirs, cat):
    """카탈로그의 파일마다 지금의 상태: vanilla, applied(이 도구가 마지막에 쓴 것), unknown, missing."""
    applied = read_state(dirs)["files"]
    status = {}
    for rel, vanilla in cat.files.items():
        path = at(dirs.game, rel)
        if not path.is_file():
            status[rel] = "missing"
            continue
        sha = digest(path.read_bytes())
        status[rel] = "vanilla" if sha == vanilla else "applied" if sha == applied.get(rel) else "unknown"
    return status


def _check(dirs, cat, game_version, preset=None):
    if cat.game_version != game_version:
        raise StoreError(f"카탈로그는 {cat.game_version} 의 것이고 게임은 {game_version} 이다. 이 버전의 카탈로그가 필요하다")
    if preset is not None and preset.game_version != game_version:
        raise StoreError(f"프리셋 {preset.name!r} 은 {preset.game_version} 의 것이고 게임은 {game_version} 이다")
    status = classify(dirs, cat)
    bad = sorted(rel for rel, state in status.items() if state in ("unknown", "missing"))
    if bad:
        raise StoreError("게임 파일이 바닐라도 아니고 이 도구가 마지막에 쓴 것도 아니다(없거나 밖에서 바뀌었다): "
                         + ", ".join(bad) + ". 게임이 갱신됐으면 새 버전의 카탈로그가 필요하다. "
                         "직접 고친 파일이면 tools\\data-restore.ps1 이나 Steam 무결성 검사로 되돌린다")
    return status


def vanilla_bytes(dirs, cat, rel, status, create):
    """그 파일의 바닐라 바이트. 스냅샷이 없으면 바닐라인 게임 파일을 쓰고, create 면 스냅샷을 만든다."""
    snap = at(dirs.snapshot, rel)
    if snap.is_file():
        data = snap.read_bytes()
        if digest(data) != cat.files[rel]:
            raise StoreError(f"스냅샷이 카탈로그의 바닐라와 다르다: {snap}. 지우고 Steam 무결성 검사 뒤에 다시 만든다")
        return data
    if status[rel] != "vanilla":
        raise StoreError(f"스냅샷이 없는데 게임 파일이 바닐라가 아니다: {rel}. Steam 무결성 검사로 되돌린다")
    data = at(dirs.game, rel).read_bytes()
    if create:
        write(snap, data)
    return data


def plan(dirs, cat, preset, game_version, create_snapshots=False):
    """프리셋을 입히면 무엇이 바뀌는지 계산한다. (상태, {상대경로: 새 바이트}, [Edit])"""
    status = _check(dirs, cat, game_version, preset)
    decoded = {}
    for rel in compose.targets(preset, cat):
        try:
            decoded[rel] = jsonedit.decode(vanilla_bytes(dirs, cat, rel, status, create_snapshots))
        except jsonedit.JsonError as error:
            raise StoreError(f"{rel}: {error}") from None
    texts, edits = compose.compose(preset, cat, {rel: text for rel, (_, text) in decoded.items()})
    return status, {rel: jsonedit.encode(decoded[rel][0], text) for rel, text in texts.items()}, edits


def _originals(dirs, cat, status):
    return {rel: vanilla_bytes(dirs, cat, rel, status, False) for rel, state in sorted(status.items()) if state == "applied"}


def apply(dirs, cat, preset, game_version):
    """프리셋을 입힌다. ([Edit], 쓴 파일들, 바닐라로 되돌린 파일들)"""
    status, data, edits = plan(dirs, cat, preset, game_version, create_snapshots=True)
    originals = _originals(dirs, cat, status)       # 쓰기 전에 모두 읽어 둔다. 스냅샷이 없으면 여기서 멈춘다
    for rel, original in originals.items():
        write(at(dirs.game, rel), original)
    state = {"preset": preset.name, "game_version": game_version,
             "applied_at": datetime.datetime.now().isoformat(timespec="seconds"),
             "files": {rel: digest(new) for rel, new in sorted(data.items())}}
    write(dirs.state / STATE_NAME, json.dumps(state, ensure_ascii=False, indent=2).encode("utf-8"))
    for rel, new in sorted(data.items()):
        write(at(dirs.game, rel), new)
    return edits, sorted(data), sorted(rel for rel in originals if rel not in data)


def restore(dirs, cat, game_version):
    """이 도구가 쓴 파일을 바닐라로 되돌리고 상태를 지운다. 되돌린 파일들을 돌려준다."""
    status = _check(dirs, cat, game_version)
    originals = _originals(dirs, cat, status)
    for rel, original in originals.items():
        write(at(dirs.game, rel), original)
    state = dirs.state / STATE_NAME
    if state.exists():
        state.unlink()
    return sorted(originals)
```

- [ ] **Step 4: 시험이 통과하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests`
Expected: `Ran 65 tests`, `OK`

- [ ] **Step 5: 커밋한다**

```powershell
git -C E:\NlToyBox add tools/overlay/store.py tools/overlay/tests/support.py tools/overlay/tests/test_store.py
git -C E:\NlToyBox commit -m @'
feat(overlay): 적용과 복원 — 바닐라에서 출발하고, 모르는 파일이 있으면 쓰지 않는다

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 6: 런타임 확인과 명령줄 (`verify`, `cli`)

**Files:**
- Create: `tools/overlay/verify.py`, `tools/overlay/cli.py`
- Test: `tools/overlay/tests/test_verify.py`, `tools/overlay/tests/test_cli.py`

**Interfaces:**
- Consumes: `store.Dirs`, `store.plan`, `store.apply`, `store.restore`, `store.classify`, `store.read_state`, `store.vanilla_bytes`, `store.digest`, `store.write`, `catalog.load`, `catalog.read_json`, `catalog.parse_keys`, `catalog.file_matches`, `preset.load`, `compose.Edit`, `jsonedit.decode`, `jsonedit.encode`, `jsonedit.scan`, `jsonedit.value_of`, `jsonedit.apply_edits`, `jsonedit.format_number`, `paths.show`
- Produces:
  - `verify.anchor(path: tuple) -> str | None` — 경로의 마지막 키 이름(양끝 공백을 뗀다)
  - `verify.probe_request(edits, delay_seconds=60) -> str` — `tools/probe.ps1 -Request`에 넘길 요청 파일의 글
  - `verify.verdicts(edits, dump: dict) -> list[tuple[Edit, str, list[str]]]` — 판정은 `anchored`, `value-only`, `absent`
  - `cli.main(argv: list[str]) -> int` — 종료 코드. 0 성공, 1 거부(`FAIL: …`을 찍는다), 2 사용법 오류
  - 명령줄: `cli.py <명령> --game-dir D --game-version V --catalog-dir D --snapshot-dir D --state-dir D [--preset F] [--out F] [--dump F] [--file 패턴]`
  - `cli.py`가 찍는 줄 가운데 다른 도구와 시험이 기대는 것: `check ok (쓰지 않았다)`, `apply ok: <프리셋 이름> (쓴 파일 N개). …`, `restore ok (N)`, `pin ok: N개 -> <경로>`, `scan ok`, `keys ok: N줄 -> <경로>`, `anchored A, value-only B, absent C`, `FAIL: <이유>`

- [ ] **Step 1: 런타임 확인의 실패하는 시험을 쓴다**

`tools/overlay/tests/test_verify.py`:

```python
"""verify.py 의 시험."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import support
import compose
import preset
import verify


def edits_for(*changes):
    p = preset.parse({"name": "T", "game_version": support.VERSION, "allow_experimental": True, "changes": list(changes)})
    return compose.compose(p, support.make_catalog(), dict(support.FILES))[1]


EDITS = edits_for({"file": "debug.json", "path": "budget_money", "set": 2345},
                  {"file": "debug.json", "path": "production_cost.ale", "set": 2311.5},
                  {"file": "debug.json", "path": "building_resources.hut[0][1]", "set": 1517},
                  {"file": "debug.json", "path": "factor", "set": 0.5731})


class VerifyTests(unittest.TestCase):
    def test_probe_request_finds_every_new_value_and_names_what_sits_in_arrays(self):
        self.assertEqual(verify.probe_request(EDITS).splitlines()[1:],
                         ["delay_seconds=60", "max_hits=5000", "find=2345", "find=0.5731", "find=2311.5", "find=1517", "find_name=hut"])

    def test_probe_request_writes_each_value_once(self):
        edits = edits_for({"file": "debug.json", "path": "production_cost.*", "set": 77})
        self.assertEqual(verify.probe_request(edits).count("find=77"), 1)

    def test_verdicts_tell_anchored_from_value_only_from_absent(self):
        dump = {
            "find_ds": {"hits": [
                {"path": "ds_map[150].budget_money", "why": "name", "kind": "number", "value": 2345},
                {"path": "ds_map[150].budget_money", "why": "value", "kind": "number", "value": 2345},
                {"path": "ds_list[412][1]", "why": "value", "kind": "number", "value": 1517},
                {"path": "ds_map[150].other", "why": "value", "kind": "bool", "value": True}]},
            "find_instances": {"hits": [
                {"path": "instance:o_debug#0.debug_factor", "why": "value", "kind": "number", "value": 0.5731}]},
        }
        result = {edit.path: (verdict, where) for edit, verdict, where in verify.verdicts(EDITS, dump)}
        self.assertEqual(result[("budget_money",)], ("anchored", ["ds_map[150].budget_money"]))
        self.assertEqual(result[("factor",)], ("anchored", ["instance:o_debug#0.debug_factor"]))
        self.assertEqual(result[("building_resources", "hut", 0, 1)], ("value-only", ["ds_list[412][1]"]))
        self.assertEqual(result[("production_cost", "ale")], ("absent", []))

    def test_a_name_hit_alone_is_not_evidence(self):
        dump = {"find_ds": {"hits": [{"path": "ds_map[150].budget_money", "why": "name", "kind": "number", "value": 2345}]}}
        self.assertEqual([verdict for _, verdict, _ in verify.verdicts(EDITS[:1], dump)], ["absent"])

    def test_the_anchor_ignores_the_trailing_space_of_a_key(self):
        self.assertEqual(verify.anchor(("paper", "messenger_cost ")), "messenger_cost")
        self.assertEqual(verify.anchor(("a", "b", 0, 1)), "b")
        self.assertIsNone(verify.anchor((0, 1)))


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 시험이 실패하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests -p test_verify.py`
Expected: FAIL. `ModuleNotFoundError: No module named 'verify'`

- [ ] **Step 3: 런타임 확인을 구현한다**

`tools/overlay/verify.py`:

```python
"""프리셋이 쓴 값이 런타임에 올라왔는지 모듈의 덤프로 확인한다. 카탈로그의 runtime 등급을 올리는 근거다."""
import jsonedit

FIND_SECTIONS = ("find_global", "find_ds", "find_instances")


def anchor(path):
    """경로에서 마지막 키 이름. 런타임 경로에 이 이름이 보이면 우연한 일치가 아니라고 본다."""
    return next((part.strip() for part in reversed(path) if isinstance(part, str)), None)


def probe_request(edits, delay_seconds=60):
    """edits 의 새 값을 값으로 찾는 요청 파일의 글(tools/probe.ps1 -Request 로 넘긴다).
    배열 안의 값은 런타임 경로에 이름이 남지 않으므로 가장 가까운 키 이름도 찾는다."""
    values, names = [], []
    for edit in edits:
        value = jsonedit.format_number(edit.new, "")
        if value not in values:
            values.append(value)
        if isinstance(edit.path[-1], int) and anchor(edit.path) and anchor(edit.path) not in names:
            names.append(anchor(edit.path))
    lines = ["# tools/overlay/cli.py probe-request 가 만들었다. 프리셋이 쓴 값을 메인 메뉴의 런타임에서 찾는다.",
             f"delay_seconds={delay_seconds}", "max_hits=5000"]
    return "\n".join(lines + [f"find={value}" for value in values] + [f"find_name={name}" for name in names]) + "\n"


def verdicts(edits, dump):
    """edit 마다 (edit, 판정, 경로들)을 돌려준다.
    anchored: 그 값이 있고 경로에 키 이름도 있다.  value-only: 값은 있으나 이름이 없다(사람이 판단한다).  absent: 없다."""
    hits = []
    for section in FIND_SECTIONS:
        for hit in dump.get(section, {}).get("hits", []):
            value = hit.get("value")
            if hit.get("why") == "value" and isinstance(value, (int, float)) and not isinstance(value, bool):
                hits.append((str(hit.get("path")), value))
    out = []
    for edit in edits:
        found = [path for path, value in hits if value == edit.new]
        name = anchor(edit.path)
        anchored = [path for path in found if name and name in path]
        out.append((edit, "anchored" if anchored else "value-only" if found else "absent", anchored or found))
    return out
```

- [ ] **Step 4: 시험이 통과하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests -p test_verify.py`
Expected: `Ran 5 tests`, `OK`

- [ ] **Step 5: 명령줄의 실패하는 시험을 쓴다**

`tools/overlay/tests/test_cli.py`:

```python
"""cli.py 의 시험. 명령을 끝에서 끝까지 임시 폴더의 가짜 게임으로 돌린다."""
import contextlib
import io
import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import support
import cli


class CliTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = pathlib.Path(tmp.name)
        self.dirs = support.make_game(self.root)
        self.catalog_dir = self.root / "catalog"
        self.catalog_dir.mkdir()
        (self.catalog_dir / "keys.json").write_text(json.dumps(support.keys_data()), encoding="utf-8")
        (self.catalog_dir / "files.json").write_text(json.dumps(support.files_data()), encoding="utf-8")
        self.preset = self.root / "gold.json"
        self.preset.write_text(json.dumps({"name": "Gold", "game_version": support.VERSION, "changes": [
            {"file": "debug.json", "path": "budget_money", "set": 5000},
            {"file": "debug.json", "path": "production_cost.ale", "mul": 2}]}), encoding="utf-8")
        self.vanilla = support.game_bytes(self.dirs)

    def run_cli(self, command, *extra, version=support.VERSION):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = cli.main([command, "--game-dir", str(self.dirs.game), "--game-version", version,
                             "--catalog-dir", str(self.catalog_dir), "--snapshot-dir", str(self.dirs.snapshot),
                             "--state-dir", str(self.dirs.state), *extra])
        return code, out.getvalue()

    def test_check_shows_the_changes_and_writes_nothing(self):
        code, out = self.run_cli("check", "--preset", str(self.preset))
        self.assertEqual(code, 0, out)
        self.assertIn("debug.json  budget_money  2000 -> 5000  [VERIFIED/UNKNOWN]", out)
        self.assertIn("debug.json  production_cost.ale  2 -> 4  [SEEN/UNKNOWN]", out)
        self.assertIn("변경 2개, 파일 1개. 런타임 반영을 확인하지 않은 키 1개", out)
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)
        self.assertFalse(self.dirs.snapshot.exists())

    def test_apply_status_restore_round_trip(self):
        code, out = self.run_cli("apply", "--preset", str(self.preset))
        self.assertEqual(code, 0, out)
        self.assertIn("apply ok: Gold", out)
        self.assertIn(b'"budget_money": 5000', (self.dirs.game / "debug.json").read_bytes())
        code, out = self.run_cli("status")
        self.assertEqual(code, 0, out)
        self.assertIn("applied: 1", out)
        self.assertIn("프리셋: Gold", out)
        code, out = self.run_cli("restore")
        self.assertEqual(code, 0, out)
        self.assertIn("restore ok (1)", out)
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)
        code, out = self.run_cli("status")
        self.assertIn("프리셋: (없음. 바닐라)", out)

    def test_a_refusal_prints_fail_and_exits_1(self):
        code, out = self.run_cli("apply", "--preset", str(self.preset), version="9.9.9.9")
        self.assertEqual(code, 1)
        self.assertIn("FAIL: 카탈로그", out)
        code, out = self.run_cli("apply", "--preset", str(self.root / "none.json"))
        self.assertEqual(code, 1)
        self.assertIn("FAIL: 파일이 없다", out)

    def test_status_exits_1_when_a_file_is_unknown(self):
        (self.dirs.game / "books" / "a.json").write_bytes(b"{}")
        code, out = self.run_cli("status")
        self.assertEqual(code, 1)
        self.assertIn("unknown: 1", out)
        self.assertIn("books/a.json", out)

    def test_keys_lists_every_number_with_its_grades(self):
        target = self.root / "out" / "keys.tsv"
        code, out = self.run_cli("keys", "--out", str(target))
        self.assertEqual(code, 0, out)
        lines = target.read_text(encoding="utf-8").splitlines()
        self.assertEqual(lines[0], "file\tpath\tvalue\truntime\teffect\texperimental")
        self.assertIn("debug.json\tbudget_money\t2000\tVERIFIED\tUNKNOWN\tN", lines)
        self.assertIn("debug.json\tbuilding_resources.hut[0][1]\t10\tUNSEEN\tUNKNOWN\tY", lines)
        self.assertIn("books/a.json\ttag\t0\t-\t-\t-", lines)              # 카탈로그에 없는 키도 보인다
        self.assertEqual(len(lines), 1 + 11)

    def test_keys_reads_vanilla_even_while_a_preset_is_applied(self):
        self.run_cli("apply", "--preset", str(self.preset))
        code, out = self.run_cli("keys", "--file", "debug.json")
        self.assertEqual(code, 0, out)
        self.assertIn("debug.json\tbudget_money\t2000\t", out)

    def test_pin_writes_the_hashes_of_the_files_the_keys_name(self):
        (self.catalog_dir / "files.json").unlink()
        code, out = self.run_cli("pin")
        self.assertEqual(code, 0, out)
        self.assertEqual(json.loads((self.catalog_dir / "files.json").read_text(encoding="utf-8")), support.files_data())
        code, out = self.run_cli("pin")
        self.assertEqual(code, 1)
        self.assertIn("이미 있다", out)

    def test_scan_checks_every_json_in_the_game_folder(self):
        code, out = self.run_cli("scan")
        self.assertEqual(code, 0, out)
        self.assertIn("파일 3개, 수 11개, 표준 파서가 못 읽는 파일 1개, 실패 0개", out)
        (self.dirs.game / "bad.json").write_bytes(b'{"a": 1 // x\n}')
        code, out = self.run_cli("scan")
        self.assertEqual(code, 1)
        self.assertIn("FAIL bad.json", out)

    def test_probe_request_and_verify(self):
        request = self.root / "req.txt"
        code, out = self.run_cli("probe-request", "--preset", str(self.preset), "--out", str(request))
        self.assertEqual(code, 0, out)
        self.assertIn("find=5000\nfind=4\n", request.read_text(encoding="utf-8"))
        dump = self.root / "dump.json"
        dump.write_text(json.dumps({"find_ds": {"hits": [
            {"path": "ds_map[1].budget_money", "why": "value", "kind": "number", "value": 5000}]}}), encoding="utf-8")
        code, out = self.run_cli("verify", "--preset", str(self.preset), "--dump", str(dump))
        self.assertEqual(code, 1, out)                                     # 하나가 absent 다
        self.assertIn("anchored    debug.json  budget_money = 5000", out)
        self.assertIn("absent      debug.json  production_cost.ale = 4", out)
        self.assertIn("anchored 1, value-only 0, absent 1", out)

    def test_a_command_without_its_argument_is_a_usage_error(self):
        with self.assertRaises(SystemExit) as caught, contextlib.redirect_stderr(io.StringIO()):
            self.run_cli("apply")
        self.assertEqual(caught.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 6: 시험이 실패하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests -p test_cli.py`
Expected: FAIL. `ModuleNotFoundError: No module named 'cli'`

- [ ] **Step 7: 명령줄을 구현한다**

`tools/overlay/cli.py`:

```python
"""데이터 오버레이의 명령줄. 보통은 tools/overlay.ps1 이 경로를 채워서 부른다.

  check          프리셋을 입히면 무엇이 바뀌는지 보여 준다. 쓰지 않는다
  apply          바닐라로 되돌린 뒤 프리셋을 입힌다
  restore        이 도구가 쓴 파일을 바닐라로 되돌린다
  status         카탈로그의 파일마다 지금의 상태
  keys           카탈로그의 파일에 든 수를 모두 적는다(경로, 값, 등급). 탭으로 나눈 표
  pin            keys.json 의 파일 패턴을 게임 폴더에서 찾아 files.json 을 만든다(게임 파일이 바닐라일 때만 쓴다)
  scan           게임 폴더의 모든 *.json 을 읽어 본다. 읽기 전용
  probe-request  프리셋이 쓰는 값을 런타임에서 찾는 요청 파일을 만든다
  verify         덤프에서 프리셋의 값을 찾아 판정한다
"""
import argparse
import json
import pathlib
import sys

import catalog
import jsonedit
import paths
import preset as presets
import store
import verify
from jsonedit import OverlayError


def _dirs(args):
    return store.Dirs(pathlib.Path(args.game_dir), pathlib.Path(args.snapshot_dir), pathlib.Path(args.state_dir))


def _show(edits):
    for edit in edits:
        mark = " 실험" if edit.group.experimental else ""
        print(f"  {edit.file}  {paths.show(edit.path)}  {edit.old_text} -> {edit.new_text}"
              f"  [{edit.group.runtime}/{edit.group.effect}{mark}]")
    unverified = sum(1 for edit in edits if edit.group.runtime != "VERIFIED")
    print(f"변경 {len(edits)}개, 파일 {len({edit.file for edit in edits})}개"
          + (f". 런타임 반영을 확인하지 않은 키 {unverified}개" if unverified else ""))


def cmd_check(args):
    cat = catalog.load(args.catalog_dir)
    _, _, edits = store.plan(_dirs(args), cat, presets.load(args.preset), args.game_version)
    _show(edits)
    print("check ok (쓰지 않았다)")


def cmd_apply(args):
    cat = catalog.load(args.catalog_dir)
    preset = presets.load(args.preset)
    edits, written, restored = store.apply(_dirs(args), cat, preset, args.game_version)
    _show(edits)
    for rel in restored:
        print(f"  바닐라로 되돌림: {rel}")
    print(f"apply ok: {preset.name} (쓴 파일 {len(written)}개). 게임을 다시 켜야 반영된다")


def cmd_restore(args):
    restored = store.restore(_dirs(args), catalog.load(args.catalog_dir), args.game_version)
    for rel in restored:
        print(f"  바닐라로 되돌림: {rel}")
    print(f"restore ok ({len(restored)})")


def cmd_status(args):
    cat = catalog.load(args.catalog_dir)
    dirs = _dirs(args)
    status = store.classify(dirs, cat)
    state = store.read_state(dirs)
    print(f"게임 버전 {args.game_version}, 카탈로그 {cat.game_version}, 파일 {len(status)}개")
    for name in ("vanilla", "applied", "unknown", "missing"):
        rels = sorted(rel for rel, value in status.items() if value == name)
        print(f"  {name}: {len(rels)}")
        if name != "vanilla":
            for rel in rels:
                print(f"    {rel}")
    applied = any(value == "applied" for value in status.values())
    print(f"프리셋: {state.get('preset') if applied else '(없음. 바닐라)'}")
    return 1 if any(value in ("unknown", "missing") for value in status.values()) else 0


def cmd_keys(args):
    cat = catalog.load(args.catalog_dir)
    dirs = _dirs(args)
    status = store.classify(dirs, cat)
    lines = ["file\tpath\tvalue\truntime\teffect\texperimental"]
    for rel in sorted(cat.files):
        if args.file and not catalog.file_matches(args.file, rel):
            continue
        _, text = jsonedit.decode(store.vanilla_bytes(dirs, cat, rel, status, False))
        for slot in jsonedit.scan(text):
            if slot.kind != "number":
                continue
            group = cat.find(rel, slot.path)
            grades = [group.runtime, group.effect, "Y" if group.experimental else "N"] if group else ["-", "-", "-"]
            lines.append("\t".join([rel, paths.show(slot.path), text[slot.start:slot.end]] + grades))
    if args.out:
        out = pathlib.Path(args.out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"keys ok: {len(lines) - 1}줄 -> {out}")
    else:
        print("\n".join(lines))


def cmd_pin(args):
    directory = pathlib.Path(args.catalog_dir)
    target = directory / "files.json"
    if target.exists():
        raise OverlayError(f"이미 있다: {target}. 다시 만들려면 게임 파일이 바닐라인 것을 확인하고 지운 뒤 실행한다")
    version, groups = catalog.parse_keys(catalog.read_json(directory / "keys.json"))
    if version != args.game_version:
        raise OverlayError(f"keys.json 은 {version} 의 것이고 게임은 {args.game_version} 이다")
    game = pathlib.Path(args.game_dir)
    files = {}
    for group in groups:
        found = sorted(path for path in game.glob(group.file) if path.is_file())
        if not found:
            raise OverlayError(f"게임 폴더에 맞는 파일이 없다: {group.file}")
        for path in found:
            files[path.relative_to(game).as_posix()] = store.digest(path.read_bytes())
    data = {"game_version": version, "files": dict(sorted(files.items()))}
    store.write(target, (json.dumps(data, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    print(f"pin ok: {len(files)}개 -> {target}")


def _lookup(node, path):
    for part in path:
        node = node[part]
    return node


def cmd_scan(args):
    game = pathlib.Path(args.game_dir)
    files = numbers = lenient_only = 0
    failed = []
    for path in sorted(game.rglob("*.json")):
        rel = path.relative_to(game).as_posix()
        raw = path.read_bytes()
        files += 1
        try:
            bom, text = jsonedit.decode(raw)
            slots = jsonedit.scan(text)
        except jsonedit.JsonError as error:
            failed.append(f"{rel}: {error}")
            continue
        number_slots = [slot for slot in slots if slot.kind == "number"]
        numbers += len(number_slots)
        same = jsonedit.apply_edits(text, [(slot, text[slot.start:slot.end]) for slot in number_slots])
        if jsonedit.encode(bom, same) != raw:
            failed.append(f"{rel}: 아무것도 바꾸지 않았는데 바이트가 달라졌다")
            continue
        try:
            parsed = json.loads(text)
        except ValueError:
            lenient_only += 1           # 닫는 괄호 앞의 쉼표. 표준 파서와 견줄 수 없다
            continue
        for slot in slots:
            if slot.kind in ("object", "array"):
                continue
            if _lookup(parsed, slot.path) != jsonedit.value_of(text, slot):
                failed.append(f"{rel}: {paths.show(slot.path)} 의 값이 표준 파서와 다르다")
                break
    print(f"파일 {files}개, 수 {numbers}개, 표준 파서가 못 읽는 파일 {lenient_only}개, 실패 {len(failed)}개")
    for line in failed[:50]:
        print(f"  FAIL {line}")
    if failed:
        return 1
    print("scan ok")


def cmd_probe_request(args):
    cat = catalog.load(args.catalog_dir)
    _, _, edits = store.plan(_dirs(args), cat, presets.load(args.preset), args.game_version)
    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(verify.probe_request(edits), encoding="utf-8", newline="\n")
    print(f"probe-request ok: 값 {len(edits)}개 -> {out}")


def cmd_verify(args):
    cat = catalog.load(args.catalog_dir)
    _, _, edits = store.plan(_dirs(args), cat, presets.load(args.preset), args.game_version)
    with open(args.dump, encoding="utf-8") as f:
        dump = json.load(f)
    counts = {"anchored": 0, "value-only": 0, "absent": 0}
    for edit, verdict, where in verify.verdicts(edits, dump):
        counts[verdict] += 1
        print(f"{verdict:10}  {edit.file}  {paths.show(edit.path)} = {edit.new_text}")
        for path in where[:5]:
            print(f"              {path}")
    print(f"anchored {counts['anchored']}, value-only {counts['value-only']}, absent {counts['absent']}")
    return 1 if counts["absent"] else 0


COMMANDS = {"check": cmd_check, "apply": cmd_apply, "restore": cmd_restore, "status": cmd_status, "keys": cmd_keys,
            "pin": cmd_pin, "scan": cmd_scan, "probe-request": cmd_probe_request, "verify": cmd_verify}


def main(argv):
    parser = argparse.ArgumentParser(prog="cli.py", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=sorted(COMMANDS))
    parser.add_argument("--game-dir", required=True)
    parser.add_argument("--game-version", required=True)
    parser.add_argument("--catalog-dir", required=True)
    parser.add_argument("--snapshot-dir", required=True)
    parser.add_argument("--state-dir", required=True)
    parser.add_argument("--preset")
    parser.add_argument("--out")
    parser.add_argument("--dump")
    parser.add_argument("--file")
    args = parser.parse_args(argv)
    needs = {"check": ["preset"], "apply": ["preset"], "probe-request": ["preset", "out"], "verify": ["preset", "dump"]}
    missing = [name for name in needs.get(args.command, []) if not getattr(args, name)]
    if missing:
        parser.error(f"{args.command} 에는 --{' --'.join(missing)} 가 필요하다")
    try:
        return COMMANDS[args.command](args) or 0
    except (OverlayError, OSError) as error:
        print(f"FAIL: {error}")
        return 1


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main(sys.argv[1:]))
```

- [ ] **Step 8: 시험이 통과하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests`
Expected: `Ran 80 tests`, `OK`

- [ ] **Step 9: 커밋한다**

```powershell
git -C E:\NlToyBox add tools/overlay/verify.py tools/overlay/cli.py tools/overlay/tests/test_verify.py tools/overlay/tests/test_cli.py
git -C E:\NlToyBox commit -m @'
feat(overlay): 명령줄과 런타임 확인(요청 만들기, 덤프 판정)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 7: 래퍼 `tools/overlay.ps1`

**Files:**
- Create: `tools/overlay.ps1`
- Modify: `tools/common.ps1` (`Get-NlDataSnapshotDir` 바로 아래에 함수 하나)
- Modify: `tools/tests/safety.tests.ps1` (시험 다섯 개와 뒷정리 두 줄)

**Interfaces:**
- Consumes: `tools/overlay/cli.py`의 명령줄(Task 6), `common.ps1`의 `Get-NlGameDir`, `Get-NlExeInfo`, `Get-NlRepoRoot`, `Get-NlDataSnapshotDir`, `Assert-NlGameNotRunning`
- Produces:
  - `Get-NlOverlayStateDir` → `<레포>\backups\overlay\<게임 버전>`
  - `pwsh -File tools/overlay.ps1 <check|apply|restore|status|keys|pin|scan|probe-request|verify> [-Preset F] [-Out F] [-Dump F] [-File 패턴] [-CatalogDir D]` — 종료 코드는 `cli.py`의 것. `-CatalogDir`을 주지 않으면 `<레포>\catalog\<게임 버전>`

- [ ] **Step 1: 실패하는 시험을 쓴다**

`tools/tests/safety.tests.ps1`에서 `    Write-Host "safety tests: $($script:passed) passed"` 줄 **바로 앞에** 더한다(들여쓰기 4칸. `try` 안이다):

```powershell
    # 데이터 오버레이. 가짜 게임의 버전으로 작은 카탈로그와 프리셋을 만든다.
    $ovFile = Join-Path $fake 'ov.json'
    $ovCat = Join-Path $fake 'ov-catalog'
    $ovPreset = Join-Path $fake 'ov-preset.json'
    $ovVanilla = "{`r`n`t`"gold`": 2000,`r`n`t`"cost`": {`"wood`": 15, },`r`n}"      # CRLF 와 닫는 괄호 앞의 쉼표
    $ovApplied = "{`r`n`t`"gold`": 5000,`r`n`t`"cost`": {`"wood`": 8, },`r`n}"
    [IO.File]::WriteAllText($ovFile, $ovVanilla)
    New-Item -ItemType Directory -Force -Path $ovCat | Out-Null
    [IO.File]::WriteAllText((Join-Path $ovCat 'keys.json'), (@{ game_version = $orig.Version; groups = @(
        @{ file = 'ov.json'; paths = @('gold', 'cost.*'); runtime = 'SEEN'; effect = 'UNKNOWN' }) } | ConvertTo-Json -Depth 5))
    [IO.File]::WriteAllText((Join-Path $ovCat 'files.json'), (@{ game_version = $orig.Version; files = @{
        'ov.json' = (Get-FileHash -LiteralPath $ovFile -Algorithm SHA256).Hash } } | ConvertTo-Json))
    [IO.File]::WriteAllText($ovPreset, (@{ name = '시험'; game_version = $orig.Version; changes = @(
        @{ file = 'ov.json'; path = 'gold'; set = 5000 }, @{ file = 'ov.json'; path = 'cost.*'; mul = 0.5; round = 'ceil' }) } | ConvertTo-Json -Depth 5))
    $ovArgs = "-CatalogDir '$ovCat'"

    Test-Case 'overlay check 는 무엇이 바뀌는지 보여 주고 쓰지 않는다' {
        $r = Invoke-Tool 'overlay.ps1' "check -Preset '$ovPreset' $ovArgs"
        Assert-Equal $r.Exit 0 "check 종료 코드`n$($r.Out)"
        Assert-True ($r.Out -match 'gold  2000 -> 5000' -and $r.Out -match '쓰지 않았다') "바뀔 값을 보여 줘야 한다`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText($ovFile)) $ovVanilla '파일은 그대로여야 한다'
    }

    Test-Case 'overlay apply 는 게임이 켜져 있으면 쓰지 않는다' {
        $running = "function Get-Process { [pscustomobject]@{ Id = 4242; Name = 'Norland' } };"
        $r = Invoke-Tool 'overlay.ps1' "apply -Preset '$ovPreset' $ovArgs" $running
        Assert-True ($r.Exit -eq 1 -and $r.Out -match '실행 중') "켜져 있으면 거부해야 한다`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText($ovFile)) $ovVanilla '파일은 그대로여야 한다'
    }

    Test-Case 'overlay apply 는 값의 자리만 바꾸고, restore 는 바이트까지 되돌린다' {
        $r = Invoke-Tool 'overlay.ps1' "apply -Preset '$ovPreset' $ovArgs"
        Assert-Equal $r.Exit 0 "apply 종료 코드`n$($r.Out)"
        Assert-True ($r.Out -match 'apply ok: 시험') "프리셋 이름의 한글이 깨지지 않아야 한다`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText($ovFile)) $ovApplied '값의 자리만 바뀌어야 한다'
        $r = Invoke-Tool 'overlay.ps1' "status $ovArgs"
        Assert-True ($r.Exit -eq 0 -and $r.Out -match 'applied: 1' -and $r.Out -match '프리셋: 시험') "status 가 입힌 프리셋을 알려야 한다`n$($r.Out)"
        $r = Invoke-Tool 'overlay.ps1' "restore $ovArgs"
        Assert-Equal $r.Exit 0 "restore 종료 코드`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText($ovFile)) $ovVanilla '바닐라의 바이트로 돌아와야 한다'
    }

    Test-Case 'overlay 는 밖에서 바뀐 파일이 있으면 아무것도 쓰지 않는다' {
        $changed = $ovVanilla.Replace('2000', '2001')                   # 게임 갱신이나 손으로 고친 것
        [IO.File]::WriteAllText($ovFile, $changed)
        foreach ($cmd in "apply -Preset '$ovPreset'", 'restore') {
            $r = Invoke-Tool 'overlay.ps1' "$cmd $ovArgs"
            Assert-True ($r.Exit -eq 1 -and $r.Out -match 'FAIL: 게임 파일이 바닐라도 아니고') "거부해야 한다: $cmd`n$($r.Out)"
            Assert-Equal ([IO.File]::ReadAllText($ovFile)) $changed '파일은 그대로여야 한다'
        }
        [IO.File]::WriteAllText($ovFile, $ovVanilla)
    }

    Test-Case 'overlay 는 이 게임 버전의 카탈로그가 없으면 거부한다' {
        $r = Invoke-Tool 'overlay.ps1' "apply -Preset '$ovPreset'"         # -CatalogDir 없이: catalog\<가짜 게임의 버전>
        Assert-True ($r.Exit -eq 1 -and $r.Out -match '카탈로그가 없습니다') "거부해야 한다`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText($ovFile)) $ovVanilla '파일은 그대로여야 한다'
    }
```

같은 파일의 `finally` 안, `if (Test-Path -LiteralPath $fakeData) { Remove-Item -LiteralPath $fakeData -Recurse -Force }` 줄 바로 아래에 더한다:

```powershell
    $fakeOverlay = Join-Path $backupDir "overlay\$($orig.Version)"      # 가짜 게임의 오버레이 상태
    if (Test-Path -LiteralPath $fakeOverlay) { Remove-Item -LiteralPath $fakeOverlay -Recurse -Force }
```

- [ ] **Step 2: 시험이 실패하는 것을 본다**

Run: `pwsh -File E:\NlToyBox\tools\tests\safety.tests.ps1`
Expected: FAIL. 앞의 시험 19개는 `ok -`로 지나가고, `overlay check …`에서 `check 종료 코드`와 `overlay.ps1` … `is not recognized`가 든 오류로 멈춘다. 종료 코드 1.

- [ ] **Step 3: 상태 폴더의 자리를 `common.ps1`에 더한다**

`tools/common.ps1`의 `Get-NlDataSnapshotDir` 함수 바로 아래에 더한다:

```powershell
# 데이터 오버레이의 상태(마지막에 입힌 프리셋과 그때 쓴 파일의 해시)를 두는 곳. 게임 버전별로 나눈다.
# 스냅샷 폴더 안에 두지 않는다: data-restore.ps1 은 스냅샷 폴더의 모든 파일을 게임 폴더로 복사한다.
function Get-NlOverlayStateDir {
    Join-Path (Get-NlRepoRoot) "backups\overlay\$((Get-NlExeInfo).Version)"
}
```

- [ ] **Step 4: 래퍼를 쓴다**

`tools/overlay.ps1`:

```powershell
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
```

- [ ] **Step 5: 시험이 통과하는 것을 본다**

Run: `pwsh -File E:\NlToyBox\tools\tests\safety.tests.ps1`
Expected: 마지막 줄 `safety tests: 24 passed`, 종료 코드 0.

- [ ] **Step 6: 시험이 남긴 것이 없는지 본다**

```powershell
Test-Path -LiteralPath E:\NlToyBox\backups\overlay
(Get-ChildItem -LiteralPath E:\NlToyBox\backups\data -Directory).Name
git -C E:\NlToyBox status --short
```

Expected: `False`, `0.5588.9777.0` 하나, 그리고 고친 세 파일만(`?? tools/overlay.ps1`, ` M tools/common.ps1`, ` M tools/tests/safety.tests.ps1`).

- [ ] **Step 7: 커밋한다**

```powershell
git -C E:\NlToyBox add tools/overlay.ps1 tools/common.ps1 tools/tests/safety.tests.ps1
git -C E:\NlToyBox commit -m @'
feat(overlay): 래퍼 overlay.ps1 — 게임이 켜져 있으면 쓰지 않는다

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

---

### Task 8: 이 게임 버전의 카탈로그와 프리셋

**Files:**
- Create: `catalog/0.5588.9777.0/keys.json`
- Create: `catalog/0.5588.9777.0/files.json` (`pin`이 만든다)
- Create: `presets/example.json`, `presets/verify-stage1.json`
- Test: `tools/overlay/tests/test_repo_data.py`

**Interfaces:**
- Consumes: `tools/overlay.ps1`의 `pin`, `status`, `scan`, `check`, `keys`(Task 7), `catalog.load`, `preset.load`, `compose.targets`
- Produces: `catalog/0.5588.9777.0/`(파일 125개, 묶음 20개), `presets/verify-stage1.json`(Task 10이 쓴다. 변경 29개, 파일 6개)

`keys.json`의 등급은 `research/03-data-files.md`의 "키별 런타임 근거" 표를 옮긴 것이다. 고칠 때는 그 표와 함께 고친다.

- [ ] **Step 1: 실패하는 시험을 쓴다**

`tools/overlay/tests/test_repo_data.py`:

```python
"""레포에 든 카탈로그와 프리셋이 읽히는지 본다. 게임 폴더는 보지 않는다."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import catalog
import compose
import preset

REPO = pathlib.Path(__file__).resolve().parents[3]


class RepoDataTests(unittest.TestCase):
    def test_every_catalog_loads_and_is_named_after_its_game_version(self):
        directories = sorted(path for path in (REPO / "catalog").iterdir() if path.is_dir())
        self.assertTrue(directories)
        for directory in directories:
            with self.subTest(catalog=directory.name):
                self.assertEqual(catalog.load(directory).game_version, directory.name)

    def test_every_preset_loads_and_names_files_its_catalog_knows(self):
        files = sorted((REPO / "presets").glob("*.json"))
        self.assertTrue(files)
        for path in files:
            with self.subTest(preset=path.name):
                loaded = preset.load(path)
                cat = catalog.load(REPO / "catalog" / loaded.game_version)
                self.assertTrue(compose.targets(loaded, cat))


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 시험이 실패하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests -p test_repo_data.py`
Expected: FAIL. 두 시험 모두 오류: `FileNotFoundError` … `catalog`, 그리고 `AssertionError: [] is not true`(`presets`에 파일이 없다).

- [ ] **Step 3: 게임 파일이 바닐라인지 확인한다**

`pin`은 지금의 게임 파일을 바닐라로 적는다. 먼저 두 가지를 본다.

```powershell
pwsh -NoProfile -Command "& 'E:\NlToyBox\tools\data-snapshot.ps1' -Files 'debug_params.json','gameplay_variables.json','battle_params.json','director_params.json','knowledge\technology\cultural_knowledge\addiction_resist.json'"
```

Expected: `이미 있음:`으로 시작하는 줄 다섯 개(게임 파일이 스냅샷과 같다). 종료 코드 0.

```powershell
. E:\NlToyBox\tools\common.ps1
@(Get-ChildItem -LiteralPath (Get-NlGameDir) -Recurse -File -Filter *.json | Where-Object { $_.FullName -notmatch '\\\.omc\\' -and $_.LastWriteTime.ToString('yyyy-MM-dd HH:mm') -ne '2026-10-04 16:20' }).Count
```

Expected: `0` (Steam이 2026-10-04 16:20에 쓴 뒤 고쳐진 JSON이 없다. 근거는 `research/03-data-files.md`의 "스냅샷이 바닐라라는 근거"). 0이 아니면 멈추고 사용자에게 알린다. 게임이 갱신됐거나 누가 파일을 고친 것이다.

- [ ] **Step 4: `keys.json`을 쓴다**

`catalog/0.5588.9777.0/keys.json`:

```json
{
  "game_version": "0.5588.9777.0",
  "groups": [
    {
      "file": "debug_params.json",
      "paths": ["budget_money"],
      "runtime": "VERIFIED",
      "effect": "UNKNOWN",
      "note": "2345 로 바꾸자 ds_map 의 budget_money 와, 게임을 시작한 뒤 o_province_controller.default_budget_money 가 2345 가 됐다. 화면의 시작 금화는 3000 이었다. 이 키가 시작 금화를 그대로 정하지는 않는다 (research/02)"
    },
    {
      "file": "debug_params.json",
      "paths": ["product_count.*"],
      "runtime": "SEEN",
      "effect": "TESTED",
      "effect_by": "community",
      "note": "시작 자원. Steam 글 'Tested some Modding'(DexteR, 2024-07-25, 옛 빌드)이 바꿔서 통했다고 적었다. 이 빌드에서는 효과를 재지 않았다. 런타임: ds_map 에 세 값이 그대로 있다 (research/03)"
    },
    {
      "file": "debug_params.json",
      "paths": [
        "production_cost.*", "fair_trade.fair_trade_purchase.*", "fair_trade.fair_trade_sale.*",
        "fair_trade.migrants_limit_dummy", "fair_trade.migrants_1_migrant_hours_reduce",
        "fair_trade.migrants_1_migrant_hours_min"
      ],
      "runtime": "SEEN",
      "effect": "UNKNOWN",
      "note": "메뉴의 ds_map 에 이름과 값이 파일과 같게 있다: 35 + 39 + 39 + 3개 (research/03)"
    },
    {
      "file": "debug_params.json",
      "paths": [
        "building_duration_factor", "romantic_slowdown_factor", "romantic_decrease_factor", "matching_by_opinion",
        "dialogue_chitchat_count", "dialogue_random_threshold"
      ],
      "runtime": "SEEN",
      "effect": "UNKNOWN",
      "note": "게임을 시작한 뒤 o_debug.debug_<이름> 에 같은 값이 있다 (matching_by_opinion 은 debug_matching_by_opinion_value) (research/03)"
    },
    {
      "file": "debug_params.json",
      "paths": ["building_resources.*[*][1]"],
      "runtime": "UNSEEN",
      "effect": "UNKNOWN",
      "note": "건물 101개의 묶음은 메뉴의 ds_map 에 있다. 안의 수는 보지 못했다 (research/03)"
    },
    {
      "file": "debug_params.json",
      "paths": ["bet.*"],
      "runtime": "UNSEEN",
      "effect": "UNKNOWN",
      "note": "찾아보지 않았다"
    },
    {
      "file": "battle_params.json",
      "paths": ["battle_dodge_base"],
      "runtime": "VERIFIED",
      "effect": "UNKNOWN",
      "note": "23 으로 바꾸자 ds_map 과, 게임을 시작한 뒤 o_debug.battle_dodge_base 가 23 이 됐다 (research/02)"
    },
    {
      "file": "battle_params.json",
      "paths": [
        "battle_dodge_shift_better", "battle_dodge_shift_worse", "soldier_hiring_price_skill_factor",
        "number_of_most_painful_mind"
      ],
      "runtime": "SEEN",
      "effect": "UNKNOWN",
      "note": "게임을 시작한 뒤 o_debug 의 같은 이름에 같은 값이 있다 (research/03)"
    },
    {
      "file": "battle_params.json",
      "paths": [
        "debug_right_team_mind_modify", "debug_left_team_mind_modify", "is_hide_extra_traits", "debug_battle.*",
        "unit_skill_stage.*", "limit.*", "patrol_setup.*", "pain.*", "attack.*.*", "defence.*.*"
      ],
      "runtime": "UNSEEN",
      "effect": "UNKNOWN",
      "note": "스칼라 셋은 o_debug 에 그 이름이 없었다. 나머지는 찾아보지 않았다 (research/03)"
    },
    {
      "file": "director_params.json",
      "paths": ["group_cooldown_days.EPIDEMY"],
      "runtime": "VERIFIED",
      "effect": "UNKNOWN",
      "note": "3391 로 바꾸자 ds_map 의 EPIDEMY.__cooldown_days 와 o_data.__game_director_events_data.__params.group_cooldown_days.EPIDEMY 가 3391 이 됐다 (research/02)"
    },
    {
      "file": "director_params.json",
      "paths": ["group_cooldown_days.*", "group_min_spawn_days.EPIDEMY", "group_super_priority_days.EPIDEMY"],
      "runtime": "SEEN",
      "effect": "UNKNOWN",
      "note": "group_cooldown_days 의 나머지 10개는 o_data 의 같은 자리에 같은 값이 있다. EPIDEMY 의 두 값은 ds_map 의 EPIDEMY.__min_spawn_day(15), __super_priority_days(30) 로 있다 (research/03)"
    },
    {
      "file": "director_params.json",
      "paths": [
        "group_min_spawn_days.*", "group_super_priority_days.*", "events.*.tickets", "events.*.min_population",
        "events.*.max_population", "events.*.cooldown_days.min", "events.*.cooldown_days.max"
      ],
      "runtime": "UNSEEN",
      "effect": "UNKNOWN",
      "note": "찾아보지 않았다"
    },
    {
      "file": "knowledge/technology/cultural_knowledge/addiction_resist.json",
      "paths": ["available_parameters[0].population"],
      "runtime": "VERIFIED",
      "effect": "UNKNOWN",
      "note": "3767 로 바꾸자 ds_map 의 addiction_resist.__available_parameters[0].__population 과 o_data.__knowledge_data.__array_of_population_tiers[6] 이 3767 이 됐다 (research/02)"
    },
    {
      "file": "knowledge/technology/textbooks/*.json",
      "paths": ["upgrade_skill[*].value"],
      "runtime": "UNSEEN",
      "effect": "TESTED",
      "effect_by": "community",
      "note": "Steam 글 'Any way to edit stats (cheat/hack)?'(2024-08-10, 옛 빌드)이 value 를 99 로 올려 책으로 얻는 경험치가 늘었다고 적었다. 이 빌드에서는 재지 않았다"
    },
    {
      "file": "knowledge/technology/*/*.json",
      "paths": ["available_parameters[*].population", "technology_effects[*].value", "allow_to_upgrade[*].value"],
      "runtime": "UNSEEN",
      "effect": "UNKNOWN",
      "note": "population 은 addiction_resist 한 파일에서만 확인했다. 나머지 파일과 나머지 키는 보지 않았다"
    },
    {
      "file": "gameplay_variables.json",
      "paths": ["global_map.ai_economy.initial_budget"],
      "runtime": "VERIFIED",
      "effect": "TESTED",
      "effect_by": "nltoybox",
      "note": "745 로 바꾸자 global.__gameplay_vars.global_map_ai_economy_initial_budget 가 745 가 됐고, 새 게임에서 AI 도시들의 __initial_wealth 가 745 였다 (research/01, 02)"
    },
    {
      "file": "gameplay_variables.json",
      "paths": [
        "bribe.give_rings", "free_lord.ring_by_n_levels", "church.max_capacity", "tavern.max_capacity",
        "pregnancy.from_dummy_chance"
      ],
      "runtime": "SEEN",
      "effect": "TESTED",
      "effect_by": "community",
      "note": "Steam 글 'Tested some Modding'(DexteR, 2024-07-25, 옛 빌드)이 바꿔서 통했다고 적었다. 이 빌드에서는 효과를 재지 않았다. 런타임: global.__gameplay_vars 에 이름과 값이 같게 있다 (research/01)"
    },
    {
      "file": "gameplay_variables.json",
      "paths": [
        "slave.cost", "slave.sex_dayly_chance", "slave.combat_level_cost", "slave.sex_auto_luck",
        "slave.pregnant_chance", "slave.slowdown_cost", "slave.without_free_mind_cost", "slave.fertility_cost",
        "slave.age_cost", "slave.trauma_cost"
      ],
      "runtime": "SEEN",
      "effect": "UNKNOWN",
      "experimental": true,
      "note": "사용자 참조 문서(Norland_Modding_Reference.md §7)의 방침으로 실험 키로 둔다: 값을 크게 바꾸면 충돌했다는 보고가 있다고 한다. 그 보고의 원문은 찾지 못했다"
    },
    {
      "file": "gameplay_variables.json",
      "paths": [
        "global_map.trade_paper_cost.easy", "global_map.trade_paper_cost.normal",
        "global_map.trade_paper_cost.hard", "global_map.ai_economy.gdp_k1",
        "global_map.ai_economy.soldier_discount", "global_map.ai_economy.soldier_support_price",
        "global_map.ai_economy.army_budget_factor", "global_map.attack_one_target_cooldown_turns",
        "global_map.sir_gdp_factor", "global_map.defence_alliance_create_cooldown", "global_map.ai_force.cooldown",
        "global_map.ai_force.cooldown_before_onboard", "global_map.ai_force.cooldown_faceless",
        "global_map.vassal.tribute_factor", "global_map.trade_quests_chance", "global_map.ai_king_abuse_chance",
        "actor.bravery_threshold_loyalty_factor", "actor.battle_skill_increase_in_fight",
        "actor.nectar_rage_chance_max", "actor.matching_for_love_threshold", "actor.patrol_view_cells",
        "actor.child.education_exp_factor", "actor.talent_expirience_gain", "squad.banner_size.min",
        "squad.banner_size.max", "building.open_zoom", "pregnancy.chance", "game.death_chance_mult",
        "inspection.performance.progressive", "inspection.performance.base_factor", "inspection.performance.base",
        "executor_work.intimidation_duration", "wolves.max_distance_in_cells", "skill.increase_in_use.command",
        "skill.increase_in_use.education", "skill.increase_in_use.negotiation", "skill.increase_in_use.oratory",
        "skill.increase_in_use.management", "skill.increase_in_use.knowledge", "bush.berry_renew_percent",
        "bush.renew_percent", "render.border_offset", "render.quest_token_size", "render.border_width",
        "render.border_radius_factor", "render.quest_avatar_scale", "free_lord.stay_duration",
        "combat_training.exp_per_trainer_level", "combat_training.skill_points", "farm.hop.duration",
        "farm.hop.grow_cost", "farm.carrot.duration", "farm.carrot.grow_cost", "farm.swede.duration",
        "farm.swede.grow_cost", "farm.rye.duration", "farm.rye.grow_cost", "genius_king.contempt_strength",
        "genius_king.gdp_factor", "genius_king.appear_chance", "genius_king.number_on_start",
        "genius_king.army_multiply_factor", "genius_king.max_number_in_world", "genius_king.army_budget_factor",
        "dummy.leave_to_forest_chance", "dummy.thug_combat_level.min", "dummy.thug_combat_level.max",
        "dummy.join_to_thug_when_punish", "dummy.turn_to_bandit_chance", "dummy.criminal_days_to_thug",
        "battle.skill_tickets.change_factor", "battle.skill_tickets.enabled", "battle.skill_tickets.zero_factor",
        "paper[\"messenger_cost \"]", "mind.crime_not_punished_modify", "mind.limit_most_powerful_minds",
        "soldier.bow.prepare_time", "soldier.cost_for_combat_level"
      ],
      "runtime": "SEEN",
      "effect": "UNKNOWN",
      "note": "global.__gameplay_vars 에 키 경로를 _ 로 이은 이름으로, 파일과 같은 값이 있다 (research/01)"
    },
    {
      "file": "gameplay_variables.json",
      "paths": [
        "global_map.bandit_camp.gold_for_one_bandit",
        "global_map.global_map_defence_alliance_create_cooldown_before_onboard",
        "global_map.decoration_per_day.max", "actor.panic_run", "actor.child.age_exp_limit",
        "bandits.maximum_command_skill_level", "squad.collision_push.min", "squad.collision_push.max",
        "prestige.for_knowledge_mid", "prestige.for_population", "prestige.for_knowledge_low",
        "prestige.for_allias_population", "prestige.for_decoration", "prestige.for_knowledge_high",
        "trait.insightful_call_chance", "building.breakdown_chance", "migration.happiness_base",
        "migration.happiness_factor", "migration.workplaces_factor", "gallows.punishment_watch_radius",
        "trees.number_of_woods", "trees.number_of_woods_hard", "loyalty.increase_default",
        "loyalty.king_relations_factor", "loyalty.increase_happy", "loyalty.decrease_unhappy",
        "loyalty.bribe_factor", "executor_work.intimidate_talk_minutes", "executor_work.witness_talk_minutes",
        "executor_work.witness_count", "wolves.wolves_max_distance_to_atack_in_cells", "bribe.cooldown",
        "bribe.leave_chance", "bribe.add_opinion", "bribe.loss_loyalty", "skill.increase_in_use.manners",
        "dummy.steal_minimal_val", "dummy.steal_maximal_val", "dialogue.zoom_border",
        "battle.lottery_tickets.shield_defence_from_arrows_tanaya",
        "battle.lottery_tickets.shield_defence_from_arrows", "book.rewrite_paper_cost", "moral.leaving_to_forest",
        "knowledge.price_trader", "knowledge.price_our", "mind.crime_victim",
        "mind.battle_enemy_commander_retreat.modify", "mind.quality_of_life_high.modify",
        "mind.battle_enemy_commander_dead.modify", "mind.fatigue_from_inspection.modify",
        "mind.fatigue_from_work.modify", "mind.fatigue_from_work.duration",
        "mind.battle_enemy_soldier_retreat.modify", "mind.battle_commander_bonus_factor",
        "camera.smooth_move_speed", "soldier.distance_to_patrol"
      ],
      "runtime": "UNSEEN",
      "effect": "UNKNOWN",
      "note": "global.__gameplay_vars 에 그 이름이 없다. 다른 자리로 읽히는지, 쓰이지 않는 키인지 모른다 (research/01)"
    }
  ]
}
```

- [ ] **Step 5: 프리셋 둘을 쓴다**

`presets/example.json` (형식을 보여 주는 예. 실험 키를 쓰지 않는다):

```json
{
  "name": "example",
  "game_version": "0.5588.9777.0",
  "changes": [
    { "file": "gameplay_variables.json", "path": "global_map.ai_economy.initial_budget", "mul": 2 },
    { "file": "debug_params.json", "path": "production_cost.*", "mul": 0.5 }
  ]
}
```

`presets/verify-stage1.json` (Task 10의 실측용. `VERIFIED`가 아닌 묶음마다 값 하나씩, 흔치 않은 수로 바꾼다):

```json
{
  "name": "verify-stage1",
  "game_version": "0.5588.9777.0",
  "allow_experimental": true,
  "changes": [
    { "file": "debug_params.json", "path": "product_count.wood", "set": 30731 },
    { "file": "debug_params.json", "path": "production_cost.ale", "set": 2311 },
    { "file": "debug_params.json", "path": "fair_trade.fair_trade_purchase.ale", "set": 4127 },
    { "file": "debug_params.json", "path": "fair_trade.fair_trade_sale.ale", "set": 3119 },
    { "file": "debug_params.json", "path": "fair_trade.migrants_limit_dummy", "set": 1013 },
    { "file": "debug_params.json", "path": "building_duration_factor", "set": 0.5731 },
    { "file": "debug_params.json", "path": "building_resources.woodcutter_lvl_1[0][1]", "set": 15173 },
    { "file": "debug_params.json", "path": "bet.dummy", "set": 5099 },

    { "file": "battle_params.json", "path": "battle_dodge_shift_better", "set": 1117 },
    { "file": "battle_params.json", "path": "limit.cut", "set": 5021 },
    { "file": "battle_params.json", "path": "attack.sword.cut", "set": 28051 },
    { "file": "battle_params.json", "path": "defence.shield.bruise", "set": 2039 },
    { "file": "battle_params.json", "path": "pain.cut", "set": -6131 },

    { "file": "director_params.json", "path": "group_cooldown_days.RAID", "set": 4057 },
    { "file": "director_params.json", "path": "group_min_spawn_days.RAID", "set": 2063 },
    { "file": "director_params.json", "path": "group_super_priority_days.RAID", "set": 1069 },
    { "file": "director_params.json", "path": "events.vassal_player_demand.tickets", "set": 100937 },
    { "file": "director_params.json", "path": "events.vassal_player_demand.cooldown_days.max", "set": 7307 },

    { "file": "knowledge/technology/textbooks/skill_combat_1.json", "path": "upgrade_skill[0].value", "set": 1613 },
    { "file": "knowledge/technology/economic/07!building_internal_trade.json", "path": "available_parameters[0].population", "set": 4273 },

    { "file": "gameplay_variables.json", "path": "bribe.give_rings", "set": 1019 },
    { "file": "gameplay_variables.json", "path": "bribe.cooldown", "set": 1213 },
    { "file": "gameplay_variables.json", "path": "free_lord.stay_duration", "set": 1031 },
    { "file": "gameplay_variables.json", "path": "church.max_capacity", "set": 5023 },
    { "file": "gameplay_variables.json", "path": "tavern.max_capacity", "set": 3037 },
    { "file": "gameplay_variables.json", "path": "paper[\"messenger_cost \"]", "set": 1049 },
    { "file": "gameplay_variables.json", "path": "soldier.cost_for_combat_level", "set": 2029 },
    { "file": "gameplay_variables.json", "path": "skill.increase_in_use.command", "set": 2053 },
    { "file": "gameplay_variables.json", "path": "prestige.for_population", "set": 1061 }
  ]
}
```

- [ ] **Step 6: `files.json`을 만든다**

```powershell
pwsh -File E:\NlToyBox\tools\overlay.ps1 pin
Select-String -LiteralPath E:\NlToyBox\catalog\0.5588.9777.0\files.json -Pattern '"(debug_params|gameplay_variables|battle_params|director_params)\.json"' | ForEach-Object { $_.Line.Trim() }
```

Expected: `pin ok: 125개 -> E:\NlToyBox\catalog\0.5588.9777.0\files.json`, 그리고 네 줄(`research/03-data-files.md`의 해시와 같다):

```
"battle_params.json": "4F75F4123EE5B1D600C8042DEBA2FF4666C2B016D5489E7FC78EF8B9C77EE74B",
"debug_params.json": "D12254C3311A003134FE217B082F768A53B2F3F377E5DBE975FE414A8955580A",
"director_params.json": "100626D17B936B3094263183D561FE71958EF85898244F8B2F13A7C5B4E12FA4",
"gameplay_variables.json": "F8E3ADECAD955F2808FBF6F9699E242A418C8BB640B3A2AC4B8B3DBC8AD227F3",
```

- [ ] **Step 7: 시험이 통과하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests`
Expected: `Ran 82 tests`, `OK`

- [ ] **Step 8: 실제 게임 파일에 읽기만 하는 명령을 돌린다**

```powershell
pwsh -File E:\NlToyBox\tools\overlay.ps1 status
pwsh -File E:\NlToyBox\tools\overlay.ps1 scan
pwsh -File E:\NlToyBox\tools\overlay.ps1 check -Preset E:\NlToyBox\presets\example.json
pwsh -File E:\NlToyBox\tools\overlay.ps1 check -Preset E:\NlToyBox\presets\verify-stage1.json
pwsh -File E:\NlToyBox\tools\overlay.ps1 keys -Out E:\NlToyBox\refs\registry\0.5588.9777.0.tsv
```

Expected, 차례로:

- `status`: `게임 버전 0.5588.9777.0, 카탈로그 0.5588.9777.0, 파일 125개`, `vanilla: 125`, `applied: 0`, `unknown: 0`, `missing: 0`, `프리셋: (없음. 바닐라)`.
- `scan`: `표준 파서가 못 읽는 파일 3개, 실패 0개`가 든 줄과 `scan ok`. 파일 수는 4,701 안팎이다(게임 폴더의 `.omc\`에 플러그인이 쓰는 JSON이 있어 하나쯤 다를 수 있다).
- 첫 `check`: `변경 36개, 파일 2개. 런타임 반영을 확인하지 않은 키 35개`, `check ok (쓰지 않았다)`.
- 둘째 `check`: `변경 29개, 파일 6개. 런타임 반영을 확인하지 않은 키 29개`, `check ok (쓰지 않았다)`. 줄마다 `<파일>  <경로>  <옛 글> -> <새 글>  [<runtime>/<effect>]` 꼴이다. 예: `debug_params.json  building_resources.woodcutter_lvl_1[0][1]  15 -> 15173  [UNSEEN/UNKNOWN 실험]`.
- `keys`: `keys ok: 1437줄 -> E:\NlToyBox\refs\registry\0.5588.9777.0.tsv`.

다섯 명령 모두 종료 코드 0이다. 게임 폴더와 `backups\`에는 아무것도 쓰지 않는다:

```powershell
Test-Path -LiteralPath E:\NlToyBox\backups\overlay
(Get-ChildItem -LiteralPath E:\NlToyBox\backups\data\0.5588.9777.0 -Recurse -File).Count
```

Expected: `False`, `5`.

- [ ] **Step 9: 커밋한다**

```powershell
git -C E:\NlToyBox add catalog presets tools/overlay/tests/test_repo_data.py
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"
git -C E:\NlToyBox commit -m @'
feat(overlay): 0.5588.9777.0 의 카탈로그와 프리셋 둘

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

Expected: 둘째 명령의 출력이 비어 있다(`refs\registry\`의 표는 추적되지 않는다).

---

### Task 9: 문서와 전체 확인

**Files:**
- Modify: `CLAUDE.md`
- Modify: `README.md`

**Interfaces:**
- Consumes: Task 1~8의 산출물 전부
- Produces: 없음(문서)

- [ ] **Step 1: `CLAUDE.md`를 고친다**

(1) "브랜치 전략"의 CI 항목에서 `` `py -3.14 -m unittest discover -s tools/re/tests` 통과, `` 를 다음으로 바꾼다:

```
`py -3.14 -m unittest discover -s tools/re/tests`와 `py -3.14 -m unittest discover -s tools/overlay/tests` 통과,
```

같은 항목의 `앞의 셋은 게임을 켜지 않는다`를 `앞의 넷은 게임을 켜지 않는다`로 바꾼다.

(2) "게임에 가하는 변경" 절의 `- 데이터 파일은 `tools/data-snapshot.ps1`으로 바닐라 사본을 뜬 뒤에만 고친다.`로 시작하는 항목 **앞에** 더한다:

```
- 프리셋은 `tools/overlay.ps1`로 입히고 되돌린다(아래 "데이터 오버레이"). `data-edit.ps1`은 실측용으로 남겨 둔다.
  둘을 섞어 쓰지 않는다: `data-edit.ps1`로 고친 파일은 `overlay.ps1`이 "모르는 것"으로 보고 거부한다(`data-restore.ps1`로 먼저 되돌린다).
```

(3) "게임에 가하는 변경" 절과 "의존" 절 사이에 새 절을 넣는다:

```
## 데이터 오버레이

    pwsh -File tools/overlay.ps1 check -Preset presets\example.json   # 무엇이 바뀌는지 본다. 쓰지 않는다
    pwsh -File tools/overlay.ps1 apply -Preset presets\example.json   # 바닐라로 되돌린 뒤 입힌다
    pwsh -File tools/overlay.ps1 restore                              # 바닐라로 되돌린다
    pwsh -File tools/overlay.ps1 status

- 프리셋(`presets/*.json`)은 카탈로그(`catalog/<게임 버전>/keys.json`)에 있는 키의 **수**만 바꾼다. 형식은 스펙
  (`docs/superpowers/specs/2026-10-04-data-overlay-design.md`) §4.2, 등급의 뜻은 §4.3.
- 입힌 뒤에는 게임을 다시 켜야 반영된다(값은 게임을 켤 때 읽힌다). 게임이 켜져 있으면 `apply`와 `restore`는 거부한다.
- 게임의 데이터 파일을 쓰는 코드는 `tools/overlay/store.py` 하나다. 바닐라인지는 `catalog/<게임 버전>/files.json`의 SHA256으로
  판정한다. 바닐라도 아니고 이 도구가 마지막에 쓴 것도 아닌 파일이 하나라도 있으면 아무것도 쓰지 않는다.
- 상태는 `backups\overlay\<게임 버전>\state.json`, 스냅샷은 `backups\data\<게임 버전>\`에 있다(추적 안 함).
- 카탈로그의 `runtime` 등급은 이 레포가 이 빌드에서 잰 것만 올린다: 흔치 않은 값을 쓰는 프리셋을 입히고
  `overlay.ps1 probe-request` → `tools/probe.ps1` → `overlay.ps1 verify`. **`SEEN`은 "반영된다"가 아니다**(같은 이름·값이 런타임에
  있었을 뿐이고 코드의 기본값과 가려지지 않는다). 커뮤니티의 보고는 `effect_by: community`와 출처로 따로 적는다.
- 카탈로그와 프리셋에는 게임 파일의 값을 옮겨 적지 않는다(키 이름, 등급, 해시만). 값이 든 표는
  `overlay.ps1 keys -Out refs\registry\<버전>.tsv`로 뽑는다(추적 안 함).
- 수정기(`tools/overlay/jsonedit.py`)는 값의 글자만 바꾼다. 게임 파일에 한 번도 없던 문법(주석, 작은따옴표, `NaN`, 같은 키의 중복)은
  추측해서 읽지 않고 오류를 낸다. 문법의 근거는 `research/03-data-files.md`.
- 게임이 갱신되면 그 버전의 카탈로그가 없어 `overlay.ps1`이 거부한다. Steam이 파일을 쓴 직후에 `catalog/<새 버전>/keys.json`을 놓고
  `overlay.ps1 pin`으로 `files.json`을 만든 뒤 `overlay.ps1 scan`과 `check`로 키가 그대로인지 본다. 등급은 새 빌드에서 다시 잰다.
```

(4) "도구" 절에 한 줄을 더한다:

```
- 오버레이 도구는 표준 라이브러리만 쓴다. 시험은 `py -3.14 -m unittest discover -s tools/overlay/tests`(게임을 켜지 않는다. 임시 폴더로 돈다).
```

- [ ] **Step 2: `README.md`를 고친다**

`- (조사 도구만) Python 3.14`를 `- Python 3.14 (데이터 오버레이와 조사 도구)`로 바꾼다.

"바닐라로 되돌리기" 절 **앞에** 새 절을 넣는다:

```
## 데이터 파일에 프리셋 입히기

모듈 없이도 된다(exe를 패치하지 않는다). 게임을 끈 상태에서:

    pwsh -File tools/overlay.ps1 check -Preset presets\example.json
    pwsh -File tools/overlay.ps1 apply -Preset presets\example.json
    pwsh -File tools/overlay.ps1 restore

프리셋은 카탈로그(`catalog/<게임 버전>/keys.json`)에 있는 키의 수만 바꾼다. 입힌 뒤에는 게임을 다시 켠다.
```

"문서" 절의 `research/00-game-structure.md` 줄 아래에 더한다:

```
- `research/03-data-files.md` — 데이터 파일의 모양과 키별 근거
```

- [ ] **Step 3: 전체 확인을 돌린다**

```powershell
pwsh -File E:\NlToyBox\tools\build.ps1
pwsh -File E:\NlToyBox\tools\test-native.ps1
pwsh -File E:\NlToyBox\tools\tests\safety.tests.ps1
py -3.14 -m unittest discover -s E:\NlToyBox\tools\re\tests
py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"
```

Expected, 차례로: `build ok`가 든 줄, `core tests: 18 passed`, `safety tests: 24 passed`, `Ran 9 tests` + `OK`, `Ran 82 tests` + `OK`, 마지막 명령은 출력이 비어 있다.

- [ ] **Step 4: 커밋한다**

```powershell
git -C E:\NlToyBox add CLAUDE.md README.md
git -C E:\NlToyBox commit -m @'
docs: 데이터 오버레이를 쓰는 법과 규칙

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
```

- [ ] **Step 5: 게임을 켜기 전에 전체 브랜치 검토를 받는다**

실행 방식이 정한 최종 검토를 **여기서** 한다(Global Constraints). 검토 범위는 `git -C E:\NlToyBox merge-base develop HEAD`부터 `HEAD`까지다. 검토자에게 이 계획의 Review Focus 다섯 줄과 스펙 §4를 준다. Critical·Important는 실패하는 시험을 먼저 쓰고 고친 뒤 Step 3의 확인을 다시 돌린다.

---

### Task 10: 실측 실행 — 프리셋의 값이 런타임에 올라오는가

**게임을 한 번 켠다.** 메인 메뉴까지만 가고, 덤프가 끝나면 도구가 게임을 끈다. 새 게임을 시작하지 않으므로 세이브가 생기지 않는다.

**Files:**
- Create: `tools/probes/stage1-verify.txt` (`probe-request`가 만든다)
- Create: `research/04-overlay-verify.md`
- Modify: `catalog/0.5588.9777.0/keys.json` (판정에 따라 등급을 올린다)
- Modify: `docs/superpowers/specs/2026-10-04-data-overlay-design.md` (상태 줄, §4.3의 수), `research/03-data-files.md` (키별 표), `CLAUDE.md` (필요하면)

**Interfaces:**
- Consumes: `tools/overlay.ps1`(Task 7), `presets/verify-stage1.json`(Task 8), 기존 도구 `tools/build.ps1`, `setup-aurie.ps1`, `deploy.ps1`, `probe.ps1`, `restore-game.ps1`, `game-status.ps1`, `saves-backup.ps1`, `tools/re/dump_tool.py`
- Produces: `refs\runtime\stage1-verify.menu.json`(추적 안 함), 판정표

- [ ] **Step 1: 사용자에게 승인을 받는다**

사용자에게 알리고 답을 기다린다:

> 게임을 한 번 켭니다(약 2분). 데이터 파일의 값 30개를 게임 파일 어디에도 없는 수로 바꾼 채 메인 메뉴까지 가서 런타임 덤프를 받고, 도구가 게임을 끕니다. **게임 창을 누르지 말아 주세요.** 새 게임을 시작하지 않으므로 세이브는 생기지 않습니다. 끝나면 데이터 파일과 exe를 바닐라로 되돌립니다. 값 가운데 하나가 게임을 죽이면 메뉴에 닿지 못할 수 있습니다. 그것도 결과로 기록하고 되돌립니다.

승인이 없으면 여기서 멈춘다. Task 0~9의 결과는 그대로 `develop`에 합칠 수 있다(Step 9의 머지 절차만 한다. 스펙의 상태 줄에는 "실측 실행은 하지 않았다"고 적는다).

- [ ] **Step 2: 준비한다**

```powershell
pwsh -File E:\NlToyBox\tools\saves-backup.ps1
pwsh -File E:\NlToyBox\tools\build.ps1
pwsh -File E:\NlToyBox\tools\setup-aurie.ps1
pwsh -File E:\NlToyBox\tools\deploy.ps1
pwsh -File E:\NlToyBox\tools\game-status.ps1
```

Expected, 차례로: `saves backup ok (N) -> E:\NlToyBox\backups\saves\<시각>`(이 경로를 Step 6에서 쓴다), `build ok -> …`, `setup ok -> …`, `deployed -> …`, 그리고 `game-status`의 `패치 여부  : 패치됨 (.aurie 섹션 있음)`.

- [ ] **Step 3: 프리셋을 입히고 요청 파일을 만든다**

```powershell
pwsh -File E:\NlToyBox\tools\overlay.ps1 apply -Preset E:\NlToyBox\presets\verify-stage1.json
pwsh -File E:\NlToyBox\tools\overlay.ps1 status
pwsh -File E:\NlToyBox\tools\overlay.ps1 probe-request -Preset E:\NlToyBox\presets\verify-stage1.json -Out E:\NlToyBox\tools\probes\stage1-verify.txt
pwsh -File E:\NlToyBox\tools\test-native.ps1
```

Expected: `apply ok: verify-stage1 (쓴 파일 6개). 게임을 다시 켜야 반영된다`. `status`에 `vanilla: 119`, `applied: 6`, `프리셋: verify-stage1`, 종료 코드 0(`부분 적용`이라고 나오면 멈추고 원인을 본다). `probe-request ok: 값 30개 -> …\stage1-verify.txt`. `core tests: 18 passed`(요청 파일이 모듈의 읽기 규칙에 맞는다).

요청 파일은 `delay_seconds=60`, `max_hits=5000`, `find=` 30줄, `find_name=woodcutter_lvl_1` 한 줄이다.

프리셋의 첫 변경(`global_map.ai_economy.initial_budget` → 7450319)은 **양성 대조**다. 이 키는 값을 바꿔 런타임에서 본 적이 두 번 있다(`research/01`, `research/02`). 이것이 `anchored`로 나오지 않으면 이 실행의 `absent`를 믿지 않는다.

- [ ] **Step 4: 게임을 켜서 덤프를 받는다**

```powershell
pwsh -File E:\NlToyBox\tools\probe.ps1 -Request E:\NlToyBox\tools\probes\stage1-verify.txt -Out E:\NlToyBox\refs\runtime\stage1-verify.json
```

Expected: 마지막 줄 `PASS`, 종료 코드 0, `덤프: E:\NlToyBox\refs\runtime\stage1-verify.menu.json`. 출력의 `--- NlToyBox.log ---` 아래에 `NlToyBox 0.2.0 loaded`, `builtin code_is_compiled = true`, `script … = found`, `probe done`, `dump done`이 있다(`check-load.ps1`을 갈음한다).

게임이 꺼진 뒤 데이터 파일을 본다(게임이 종료하면서 데이터 파일을 다시 쓰는지는 잰 적이 없다):

```powershell
pwsh -File E:\NlToyBox\tools\overlay.ps1 status
```

Expected: `applied: 6`, `unknown: 0`. `unknown`이 있으면 게임이 그 파일을 다시 쓴 것이다. 그 사실을 결과에 적고, Step 6에서 `overlay.ps1 restore` 대신 `tools\data-restore.ps1`로 되돌린 뒤(스냅샷 여섯 개가 모두 있다) `backups\overlay\0.5588.9777.0\state.json`을 지운다.

`FAIL`이면: 출력의 로그에서 어디까지 갔는지 본다. `dump menu start`가 없으면 게임이 메뉴에 닿기 전에 끝난 것이다. **다시 켜지 않는다.** Step 6으로 가서 되돌리고, `research/04-overlay-verify.md`에 "30개를 함께 바꾸면 게임이 메뉴에 닿지 못했다"와 로그를 적고, 값을 나눠 다시 잴지 사용자에게 묻는다.

- [ ] **Step 5: 덤프를 믿어도 되는지 보고, 판정한다**

```powershell
py -3.14 E:\NlToyBox\tools\re\dump_tool.py controls E:\NlToyBox\refs\runtime\stage1-verify.menu.json
pwsh -File E:\NlToyBox\tools\overlay.ps1 verify -Preset E:\NlToyBox\presets\verify-stage1.json -Dump E:\NlToyBox\refs\runtime\stage1-verify.menu.json | Tee-Object -FilePath E:\NlToyBox\refs\runtime\stage1-verify.verdicts.txt
py -3.14 E:\NlToyBox\tools\re\dump_tool.py hits E:\NlToyBox\refs\runtime\stage1-verify.menu.json woodcutter_lvl_1
```

Expected:

- `controls`: 모든 줄이 `ok`로 시작한다(`selfcheck`, `ds_range`, `truncated`, `hits_cut`, `enumeration`, `instances`). `FAIL`이 하나라도 있으면 `absent`를 "없다"로 쓰지 않는다. 그 키는 "확인하지 못함"으로 적는다.
- `verify`: 값마다 한 줄(`anchored` / `value-only` / `absent`와 런타임 경로), 끝에 `anchored A, value-only B, absent C, unknown D` (A + B + C + D = 30). **얼마일지는 모른다.** 그것을 재는 실행이다. `absent`가 하나라도 있으면 종료 코드가 1이다. 실패가 아니라 결과다.
  - 먼저 양성 대조를 본다: `gameplay_variables.json  global_map.ai_economy.initial_budget = 7450319` 줄이 `anchored`여야 한다. 아니면 이 실행의 `absent`는 모두 "확인하지 못함"으로 적는다.
  - `주의:` 줄이 있으면(구역이 없다, `hits_cut` 등) 못 찾은 값은 `unknown`으로 나온다. "없다"로 쓰지 않는다.
- `hits`: `ds_map[…].woodcutter_lvl_1`의 이름 히트와 그 안의 한 단계. `building_resources.woodcutter_lvl_1[0][1]`이 `value-only`로 나왔으면, 값이 맞은 `ds_list[N][1]`의 `N`이 이 이름 히트가 가리키는 리스트에서 이어지는지 여기서 본다. 이어지면 근거와 함께 `VERIFIED`로 본다. 덤프의 한 단계로 이어짐이 보이지 않으면 `value-only`로 남긴다(추측으로 잇지 않는다).

- [ ] **Step 6: 되돌린다**

```powershell
pwsh -File E:\NlToyBox\tools\overlay.ps1 restore
pwsh -File E:\NlToyBox\tools\restore-game.ps1
pwsh -File E:\NlToyBox\tools\overlay.ps1 status
pwsh -File E:\NlToyBox\tools\game-status.ps1
pwsh -File E:\NlToyBox\tools\saves-backup.ps1 -Diff <Step 2가 찍은 사본의 경로>
```

Expected: `restore ok (6)`, `restore ok`, `status`에 `vanilla: 125`와 `프리셋: (없음. 바닐라)`(여섯 파일의 수정 시각도 2026-10-04 16:20으로 돌아온다), `game-status`에 `패치 여부  : 바닐라`·`mods\      : (없음)`·백업이 `exe 와 일치`. `saves-backup -Diff`가 달라진 파일을 적는다(게임이 메뉴에서 쓰는 설정 파일. 새 세이브가 없어야 한다). 그 목록을 Step 8의 문서와 사용자 보고에 적는다.

- [ ] **Step 7: 카탈로그의 등급을 판정에 맞춘다**

`catalog/0.5588.9777.0/keys.json`을 Step 5의 판정대로 고친다. 규칙:

- `anchored`인 키: **그 파일의 첫 묶음 앞에** 새 묶음을 넣는다. `paths`는 그 키의 구체 경로 하나(프리셋에 적은 그대로), `runtime`은 `VERIFIED`, `effect`와 `effect_by`는 그 키가 지금 속한 묶음의 것을 그대로, `note`는 `"<새 값> 으로 바꾸자 <런타임 경로> 에서 그 값을 봤다 (research/04)"`. 같은 파일의 `anchored` 키들은 한 묶음에 모아도 된다(`effect`가 같을 때).
- `value-only`인데 Step 5에서 이어짐을 확인한 키: 위와 같이 하고 `note`에 이어짐의 근거(이름 히트의 경로와 리스트 번호)를 적는다.
- `absent`인 키(`controls`가 모두 `ok`일 때만): 그 파일의 첫 묶음 앞에 새 묶음. `runtime`은 `UNSEEN`, `note`는 `"<새 값> 으로 바꿔 메인 메뉴에서 값으로 찾았으나 없었다 (research/04)"`.
- 같은 객체의 다른 키는 올리지 않는다. `production_cost.ale`이 `VERIFIED`가 돼도 `production_cost.*`는 `SEEN`으로 둔다. 묶음째 올릴지는 사용자가 정한다(보고에 적는다).
- 예전 묶음의 `paths`는 고치지 않는다(앞의 묶음이 먼저 맞는다).

고친 뒤:

```powershell
py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests
pwsh -File E:\NlToyBox\tools\overlay.ps1 check -Preset E:\NlToyBox\presets\verify-stage1.json
```

Expected: `Ran 82 tests`, `OK`. `check`의 줄에서 `anchored`였던 키가 `[VERIFIED/…]`로 보이고, 끝줄의 `런타임 반영을 확인하지 않은 키`의 수가 29 − (올린 키의 수)다(양성 대조는 처음부터 `VERIFIED`다).

- [ ] **Step 8: 결과를 적는다**

`research/04-overlay-verify.md`를 쓴다. 들어갈 것:

1. 조사일, 게임 버전, 게임을 켠 횟수, 다시 재는 명령(Step 3~5).
2. `controls`의 출력 그대로.
3. 판정표: 30줄(첫 줄은 양성 대조). 열은 파일, 키, 바닐라 값 → 바꾼 값, 판정, 런타임 경로(`verify`의 출력에서), 카탈로그의 등급(앞 → 뒤).
4. `value-only`를 어떻게 판단했는지와 그 근거.
5. 처음 알게 된 것(예: `__gameplay_vars`에 이름이 없던 키가 ds_map에는 있는가).
6. 세이브 폴더에서 달라진 파일(Step 6).
7. 확인하지 못한 것: 효과(`effect`), 게임을 시작한 뒤의 자리, 허용 범위.

`research/03-data-files.md`의 "키별 런타임 근거" 표에서 등급이 바뀐 줄을 고치고 `research/04`를 가리킨다. 스펙의 상태 줄을 `단계 1 완료(…research/04-overlay-verify.md)`로, §4.3의 `VERIFIED 5, SEEN 234, UNSEEN 916`을 새 수로 고친다(새 수는 `overlay.ps1 keys`의 표에서 센다).

```powershell
pwsh -File E:\NlToyBox\tools\overlay.ps1 keys -Out E:\NlToyBox\refs\registry\0.5588.9777.0.tsv
Import-Csv -LiteralPath E:\NlToyBox\refs\registry\0.5588.9777.0.tsv -Delimiter "`t" | Group-Object runtime | ForEach-Object { "$($_.Name) $($_.Count)" }
```

Expected: `VERIFIED`, `SEEN`, `UNSEEN`, `-` 네 줄. 합이 1437이고 `-`는 282다.

- [ ] **Step 9: 커밋하고 합친다**

```powershell
git -C E:\NlToyBox add tools/probes/stage1-verify.txt catalog research docs
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"
git -C E:\NlToyBox commit -m @'
docs(research): 프리셋의 값이 런타임에 올라오는지 잰 결과, 카탈로그 등급 갱신

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
'@
pwsh -File E:\NlToyBox\tools\test-native.ps1
pwsh -File E:\NlToyBox\tools\tests\safety.tests.ps1
py -3.14 -m unittest discover -s E:\NlToyBox\tools\re\tests
py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests
git -C E:\NlToyBox switch develop
git -C E:\NlToyBox merge --no-ff feat/overlay-stage1 -m "Merge branch 'feat/overlay-stage1' into develop"
git -C E:\NlToyBox branch -d feat/overlay-stage1
git -C E:\NlToyBox status --short
```

Expected: 둘째 명령의 출력이 비어 있다. `core tests: 18 passed`, `safety tests: 24 passed`, `Ran 9 tests` + `OK`, `Ran 82 tests` + `OK`. 머지 뒤 `status`가 비어 있다.

- [ ] **Step 10: 사용자에게 보고한다**

판정표(몇 개가 `VERIFIED`가 됐고 무엇이 `absent`였는가), 게임이 바닐라라는 것, 세이브 폴더에서 달라진 파일, 쓴 게임 실행 횟수를 알린다. 사용자가 정할 것을 묻는다:

- 같은 객체의 나머지 키를 묶음째 올릴지(예: `production_cost.ale`이 확인됐을 때 `production_cost.*`).
- 다음에 무엇을 잴지: 효과(`effect`. 새 게임을 시작해 화면에서 본다), 시작 금화 3000의 출처, 세이브를 불러올 때의 신호.
