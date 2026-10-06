# 범죄: 부랑자·죄·혐의 — 실행 계획

**목표:** 왼쪽 목록에 "범죄" 영역을 새로 만든다. 부랑자(범죄자가 된 주민)를 보이고 주민으로 되돌리고, 새로 생기지 않게 하고, 영주의 죄와 범죄 혐의를 지운다.

**스펙:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` 에 없는 영역이다. 사용자 요청 2026-10-06("범죄탭을 신설하고 범죄에 관련된 데이터를 수집뒤 관련 기능을 설계 및 추가하기"). 스펙 §8·§10 에 줄을 더한다.

**게임 실행:** 5번 승인.

## 근거 (실행 1, `crime-session1`. `research/26`)

| 쓰는 것 | 어디서 봤나 |
|---|---|
| 범죄자 관리자 `inst:o_game_map_controller.__province.__criminal_manager`(CriminalManager): `__today_criminal_events`, `__last_crimes`, `__today_can_investigate` | `find`, `list`, `statics` |
| 사람마다의 범죄 구성요소 `c_criminal`(ComponentCriminal. 주민과 영주 모두): `__is_dummy_criminal`, `__is_dummy_thug`, `__criminal_begin_time`, `__stolen_gold` | `list`. 범죄자가 되면 깃발이 불리언 참이 되고 시작 시각이 적힌다 |
| 게임이 저녁 18:00 에 주민을 범죄자로 만든다: `is_criminal_immunity() -> 불리언`을 묻고 `set_criminal_scum(true, true)` 또는 `set_criminal_scum(true)` | 기록(7일차 5명, 9일차 1명) |
| `set_criminal_scum(false, true)`로 지정이 풀린다 | 한 사람에게 불렀다: 깃발 거짓, 시작 시각 -4, `is_criminal_scum()` 거짓 |
| `is_criminal_immunity`가 참을 돌려주게 하면 새 범죄자가 생기지 않는다 | 켠 저녁: 시도 2번, 생긴 사람 0. 끈 저녁 둘: 시도 5번·1번, 생긴 사람 5·1 |
| 죄는 특성이다(`sin_*` 열두 가지). 영주의 범죄 혐의는 특성 `character_crime`, `character_crime_blamed_by_bishop`, `character_crime_blamed_by_fanatics` | 게임의 특성 목록 |
| 죄의 특성을 떼면 없어지고 생각의 합이 오른다 | 영주 한 사람의 `sin_forbidden_child`: 11.18 → 20.65 |
| `return_back_stolen_to_player_warehouse()` 인자 없음, undefined | 게임이 부른 기록(사람이 지도에서 없어질 때). 훔친 것이 있는 사람에게서는 보지 못했다 |
| 게임 변수 `dummy_criminal_days_to_thug`(2), `dummy_turn_to_bandit_chance`(35)·`_peaceful`(20), `dummy_storage_steal_minimal_val`(10)·`maximal_val`(20), `mind_crime_not_punished_modify`(-7), `mind_crime_victim_modify`(-18) | `global.__gameplay_vars`의 목록. 게임이 따르는지는 모른다 |

이틀 동안 범죄자들이 범죄를 저지르지 않았다(`push_dummy_crime` 0번). 그래서 도둑질·깡패·도적의 항목은 값을 쓰는 것까지만 볼 수 있다.

## 파일

- `src/core/CrimePlan.hpp/.cpp` (새로): 죄·혐의의 특성 가리기, 범죄자의 줄과 요약의 글, 명령의 낱말과 검사, 결과의 글, 게임 변수의 열쇠들.
- `src/Crime.cpp/.hpp` (새로): 범죄 패널. 틱이 플레이어의 주민에게서 범죄자를, 영주에게서 죄와 혐의를 읽어 글로 두고, 단추의 일을 한다.
- `src/core/CheatTable.*`: 영역 `Area::Crime`(제 패널이 있다), 항목 `no_new_criminals`(훅), 게임 변수의 넷(확인 전).
- `src/Production.cpp`: 게임 변수를 쓰는 일 넷.
- `src/core/RemoteCommand.*`, `src/Remote.cpp`: `crime list|clear|absolve|acquit|return_stolen`.
- `src/People.*`: 특성의 화면 이름을 내주는 `NlPeople::TraitName`.
- `src/Menu.cpp`, `src/ModuleMain.cpp`, `CMakeLists.txt`.

## 차례

1. [x] 실행 1: 구조와 함수의 꼴, 범죄자가 생기는 때, 지정 풀기와 면책 훅의 대조, 죄 떼기.
2. [x] 시험 먼저 → 코어·명령·표·화면 구현. 빌드와 시험 묶음 넷.
3. [x] 실행 2: 명령과 화면으로 확인. 실행 3: 범죄자가 있는 화면과 단추(검토가 도는 동안).
4. [x] 독립 검토 → 고친다(시험부터. 0.27.1).
5. [x] 실행 4: 고친 판의 확인과 적재 판정.
6. [x] 문서(`research/26`, CLAUDE, README, 스펙, 이 계획의 결과) → PR 로 develop 에 머지.

## 검토에서 볼 것

- 플레이어의 주민이 아닌 사람(손님, 다른 진영, 적 부대)에게 지정 풀기를 부르지 않는가. 범죄자가 아닌 사람에게 부르지 않는가.
- 사람의 번호가 바뀌었을 때(누가 떠났다) 다른 사람에게 부르지 않는가(부르기 전에 uuid 를 다시 본다).
- 죄·혐의가 아닌 특성을 떼는 길이 있는가. 플레이어의 영주가 아닌 사람의 특성을 떼는가.
- 그리는 쪽이 러너를 부르지 않는가. 틱이 시각부터 보는가. 인물 쪽이 바쁠 때 쌓인 일을 버리지 않는가.
- 창의 글이 재지 않은 것(훔친 것 되돌리기, 게임 변수의 효과)을 사실처럼 말하지 않는가.

## 결과 (모듈 0.27.1. 게임 실행 5번 가운데 4번을 썼다)

- 만든 것: 왼쪽 목록의 "범죄" 영역, 범죄자의 목록과 주민으로 되돌리기(한 사람, 전원), 표의 `no_new_criminals`(확인됨)와 게임 변수의 넷(확인 전), 영주의 죄 지우기(확인됨)와
  범죄 혐의 지우기(확인 전), 훔친 것 되돌리기(확인 전), 원격 `crime list | clear | return_stolen | absolve | acquit`.
- 계획과 달라진 것: 같은 저녁이라도 게임의 시도가 실행마다 달라(5, 1, 0번) 면책 훅의 근거를 시도마다 셌다. 시험용 범죄자·죄·혐의는 게임이 부르는 꼴로 직접 만들었다.
- 검토(Critical 0, Important 5, Minor 12)에서 고친 것: 읽지 못한 주민·영주를 "없다"·"됐다"로 적던 것(따로 센다), 훔친 것 되돌리기를 전원에게 근거 없이 부르던 것(사람을 짚고 금화의 앞뒤를 적는다),
  혐의 지우기를 잰 것처럼 보이던 것("확인 전"), 깡패까지 되돌리던 것(뺀다), 음수의 깃발을 범죄자로 읽던 것, 재지 않은 관리자의 수를 요약에 적던 것, 결과의 줄이 두 구역에 함께 쓰이던 것.
- 미룬 것: 결과 글에서 까닭이 마지막 하나로 뭉치는 것, 같은 특성이 두 번 나오면 두 번 세는 것, "N일째"의 셈(24시간 단위), 사람이 많을 때의 훑기 비용과 인물 쪽 "마지막 결과" 줄을 덮어쓰는 것,
  "모두" 단추의 확인 칸, `Crime.cpp` 안의 대상 고르기의 시험, 쌓인 명령이 여덟을 넘으면 조용히 버려지는 것, 앞선 결과의 줄이 지금의 목록과 어긋나 보이는 것(실행 4 의 화면).
- 남은 것(게임에서 일어나야 볼 수 있다): 범죄자가 저지르는 범죄와 훔친 것, 게임이 만든 깡패, 도적으로 넘어가는 것, 게임이 영주에게 건 혐의. 순찰·교수대·조사·영주의 처벌은 만들지 않았다.
