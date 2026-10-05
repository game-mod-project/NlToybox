# 치트 메뉴 3나-2: 건설비 무료, 건설 조건 제거, 업그레이드 조건 제거 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 모드창의 "건설·생산"에서 (1) 건물을 자원 없이 짓고, (2) 조건에 걸려 못 짓던 건물을 짓고, (3) 조건에 걸려 못 올리던 건물을 업그레이드한다.

**Architecture:** 게임은 YYC 라 코드를 읽을 수 없다. 조건을 판정하는 곳을 **켜져 있는 게임에서 찾는다**: 생성자의 정적 메서드 이름을 늘어놓고(`statics`),
사용자가 건설 창을 다루는 동안 후보의 호출을 기록하고(`record`), 그 자리에서 반환값을 바꿔(`override`) 화면이 풀리는지 본다. 찾은 것은 치트 표의 새 종류
(훅: 스크립트 이름 + 바꿔 돌려줄 값)와 값 쓰기(건물 종류의 비용 배열을 0 으로)로 굳힌다.

**Tech Stack:** C++20(MSVC), YYToolkit v5(`MmCreateHook`, `CallGameScriptEx`), Dear ImGui, 네이티브 시험, `dumpbin`(인자 수).

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §3(수단 B·D), §8(건설·생산), §10(3나-2), §14. 잰 것: `research/07-remote.md`, `research/08-economy.md`.

## 아는 것과 모르는 것

아는 것(출처):

- 즉시 건설(`o_debug.is_instant_build_buildings`)은 놓자마자 완성시키지만 **건설 자원은 그대로 든다**(나무 350 → 315. `research/08`).
- "건설 목록 모두 열기"(`o_debug.is_can_build_all_buildings`)는 목록만 푼다. 조건에 걸리는 건물은 여전히 못 짓는다(사용자가 봤다. `research/07`).
- 건물 종류(`generic`)의 자리: `__construction_cost.levels[5]`(`money`, `resources.__array_of_resource_quantity[39]`. 성은 `levels[2]`에만 값이 있었다: 돌 40, 점토 기와 40),
  `__allow_to_build_function_list`(성은 `[0]`), `__is_forbidden_construction`, `__limit`, `__max_level`, `__grades_buildings`, `__upgrade_hint_context_func`(`research/07`의 덤프).
- 건물의 자리: `c_construction`(`__construction_status`, `__construction_progress`, `__construction_resources`), 메서드 `add_level`, `set_level`, `set_raw_level`, `is_building_locked`, `get_level`.
- 건물 종류를 돌려주는 스크립트(exe 의 기계어로 인자 수를 읽었다. `script_calls.py`, `dumpbin`):
  `building_generic_get_array_of_all_buildings()`(인자 0, 호출 2곳), `get_generic_buildings_list()`(인자 0), `building_generic_get_array_of_constructions()`(인자 0), `get_generic_building(이름)`(인자 1, 호출 56곳).
- 생성자: `GenericBuilding`(메서드 130), `BuildingComponentConstruction`(54), `ConstructionManager`(31), `GenericBuildingConstructionCost`(3), `GenericBuildingGrade`,
  `FreeBuildingToken`·`FreeBuildingTokenCollection`(`…__construction_manager.__free_buildings`), `LevelOfSettlement`(10. `__unlocks`, `__people_threshold`).

모르는 것(Task 2 가 잰다):

- "지을 수 있는가", "올릴 수 있는가"를 어느 함수가 판정하는가. 조건이 무엇인가(지식, 정착지 등급, 수 제한, 자원, 자리).
- 건설 자원을 어디서 보고 어디서 치르는가. 업그레이드의 비용은 `levels[다음 등급]`인가.
- 비용 배열을 0 으로 쓰면 게임이 따르는가(건설 창의 표시, 실제로 드는 자원). 건물 종류의 구조체가 건물마다 같은 것인가(하나를 고치면 모두 바뀌는가).
- `FreeBuildingToken`이 무엇인가.

## Global Constraints

- 추측으로 만들지 않는다. 이름에서 읽은 뜻은 추정이다. **반환값을 바꾸는 훅은 그 함수가 무엇을 돌려주는지 기록으로 본 뒤에만 건다.**
- 바꿔 돌려줄 값은 수, 불리언, `undefined` 뿐이다. 원래 함수를 부르지 않는 것(`skip`)은 그 함수가 값을 돌려주는 일만 한다는 것을 본 뒤에만 쓴다.
- 러너를 건드리는 호출은 게임 스레드의 틱에서만. 훅 안에서는 빌트인을 부르지 않는다. 한 번 건 훅은 떼지 않는다(바꾸기만 끈다).
- 게임 스크립트는 `gml_Script_` 이름으로만, 인자의 수와 형을 본 꼴로만 부른다. `treecall`은 인자가 없다는 것을 기계어로 본 스크립트에만 쓴다.
- 값 쓰기는 있는 자리에만 하고 처음 본 값을 기억해 되돌릴 수 있게 한다. 건물 종류의 비용은 세이브가 아니라 데이터에서 오므로(추정) 게임을 다시 켜면 돌아온다는 것을 Task 2 에서 확인한다.
- 치트 표의 `Verified`는 플레이에서 효과를 본 뒤에만 참. 모드창의 글에는 한글과 라틴-1 만.
- 게임을 켜는 횟수는 사용자에게 승인받은 만큼만. 켜기 전에 사용자가 할 일을 차례대로 알린다.
- 스크립트는 `pwsh`, Python 은 `py -3.14`, git 은 `git -C E:\NlToyBox`. 커밋 끝에 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. 훅이 값을 바꾸는 동안 그 함수가 되부름(재귀)되거나 다른 훅 안에서 불릴 때 → 기록과 바꾸기가 자리마다 따로 돌아야 한다(자리의 상태만 읽는다).
2. `override … skip`으로 원래 함수를 건너뛸 때 결과 자리에 남아 있던 값(문자열·배열) → `RValue`의 대입이 앞의 값을 해제한다(누수나 이중 해제가 없어야 한다).
3. `unoverride` 뒤 → 원래 값이 그대로 지나가야 한다. 기록은 계속된다.
4. `statics`를 구조체가 아닌 값(수, 문자열, 배열)에 → 아무것도 늘어놓지 않고 죽지 않는다.
5. `treecall`에 없는 스크립트 이름 → 부르지 않고 "no such script".

---

### Task 1: 조사 도구 — `statics`, `treecall`, `override` (모듈 0.6.2)

**Files:**
- Modify: `src/core/RemoteCommand.hpp`·`.cpp`(줄 읽기), `src/Recorder.hpp`·`.cpp`(반환값 바꾸기), `src/Remote.cpp`(명령), `src/ModuleMain.cpp`(버전), `tests/native/core_tests.cpp`

**Interfaces:**
- Produces:
  - 원격 명령 `statics <주소> [max=N]`, `treecall <스크립트 이름> [depth=N] [max=N]`, `override <스크립트 이름|메서드의 주소> <n:수|b:0|1|u> [skip]`, `unoverride <이름|all>`. `about`은 구조체의 `instanceof`도 적는다.
  - `NlRecorder::Forced{Kind, Number, Skip}`, `NlRecorder::Override(Target, Value, Name, Why)`, `NlRecorder::Unoverride(Name)`. 3나-2 의 치트가 같은 함수를 쓴다.

- [x] **Step 1: 실패하는 시험을 쓴다** — `tests/native/core_tests.cpp`의 "원격 명령: 줄을 읽는다"(`statics`)와 새 시험 "원격 명령: 반환값을 바꾸는 훅의 줄을 읽는다"
  (`override`의 값 셋과 `skip`, `unoverride`, `treecall`, 받아서는 안 되는 줄 11개).
- [x] **Step 2: 실패를 본다** — `core tests: 6 FAILED`(`unknown command`).
- [x] **Step 3: 구현한다** — 줄 읽기(`RemoteCommand.cpp`), 훅의 자리를 얻는 일을 `Acquire`로 묶고 `Override`·`Unoverride`를 더함(`Recorder.cpp`. 훅 안에서 원래 함수를 부른 뒤
  결과를 `RValue`의 대입으로 바꾼다. `skip`이면 부르지 않고 결과 자리를 채워 돌려준다), `DoStatics`(매뉴얼의 `static_get` 사슬, `instanceof`)·`DoTreeCall`·`DoOverride`(`Remote.cpp`).
- [x] **Step 4: 통과를 본다** — `pwsh -NoProfile -File tools/build.ps1; pwsh -NoProfile -File tools/test-native.ps1` → `core tests: 71 passed`.
- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add src tests/native/core_tests.cpp docs/superpowers
git -C E:\NlToyBox commit -m "feat(remote): statics·treecall·override — 정적 메서드의 이름을 늘어놓고, 함수가 돌려주는 값을 바꾼다 (0.6.2)"
```

코드는 커밋이 기준이다(이 계획은 코드를 다시 적지 않는다). 새 빌트인: `static_get`, `instanceof`(둘 다 `refs/exe_strings.txt`에 있고 매뉴얼의 꼴대로 쓴다).

---

### Task 2: 조사 실행 — 조건을 판정하는 곳을 찾고 그 자리에서 풀어 본다

**Files:**
- Create: `research/09-build.md`

게임을 켠다. **사용자의 승인이 있어야 한다**(실행 묶음 1회, 모듈 적재 전에 멈추면 한 번 더). 사용자가 할 일: 세이브를 불러와 일시정지 → 알리는 차례대로 건설 창을 다룬다.

안전장치(코드 검토의 지적):

- 버려도 되는 세이브로 한다(앞 실행들의 자동 저장에서 이어진 시험용 게임). 자동 저장은 바꾼 상태를 담는다(`research/08`). 켜기 전의 사본은 `tools/saves-backup.ps1`이 뜬다.
- 반환값을 바꾸는 동안은 **일시정지**해 둔다(사용자가 건설 창을 다루는 것은 멈춘 채로 된다). 한 번에 하나씩 걸고, 본 뒤에는 `unoverride all`로 끈다.
- "바꿨는데 풀리지 않았다 → 그 함수가 원인이 아니다"는 그 동작 동안 `records`에 `=>` 표본이 남았을 때만 말한다. 본문이 호출부에 들어간 함수는 훅에 오지 않는다(`research/07`).
- 훅 자리는 64개이고 다시 쓰지 못한다. 기록에 40개까지만 쓰고 나머지는 바꾸기의 몫으로 남긴다.
- `skip`은 그 함수의 표본에서 들어올 때의 `Result`가 `undefined`(또는 `unset`)인 것을 본 뒤에만 쓴다.
- 끄기 전에 `unoverride all`을 보내고 써 넣은 값을 되돌린다. 끈 뒤 사용자에게 자동 저장에 시험의 흔적이 들어 있을 수 있다고 알린다.
- 조건은 훅보다 값을 먼저 본다(`__is_forbidden_construction`, `__limit`, `__max_level`을 `poke`·`write`).

- [ ] **Step 1: 배포하고 세이브의 사본을 뜬 뒤 켠다. 적재 판정을 받는다**

```powershell
pwsh -NoProfile -File tools/deploy.ps1
pwsh -NoProfile -File tools/saves-backup.ps1
pwsh -NoProfile -File tools/session.ps1 -Action start
pwsh -NoProfile -File tools/session.ps1 -Action status
```

- [ ] **Step 2: (게임 화면) 새 도구가 되는지 본다**

먼저 이미 아는 함수로 반환값 바꾸기 자체를 잰다(`gml_Script_budget_money_get`: 인자 없이 금화를 돌려준다. `research/07`). 건설의 판정에 닿기 전이다:

```powershell
& tools\ask.ps1 -Lines 'record gml_Script_budget_money_get', 'call gml_Script_budget_money_get'
& tools\ask.ps1 -Lines 'override gml_Script_budget_money_get n:1'
& tools\ask.ps1 -Lines 'call gml_Script_budget_money_get', 'records gml_Script_budget_money_get'
& tools\ask.ps1 -Lines 'unoverride gml_Script_budget_money_get', 'call gml_Script_budget_money_get', 'unoverride gml_Script_budget_money_get', 'statics inst:o_time_controller.time_warp', 'treecall no_such_script_here'
```

Expected: 바꾼 동안 `-> number 1`, 표본에 `() -> <금화> => 1`(`[returned another value, not Result]`가 붙는지 본다). 끈 뒤에는 금화 그대로. 두 번째 `unoverride`는 `nothing is overridden`.
`statics`를 수에 → `not a struct`. 없는 스크립트의 `treecall` → `no such script`(부르지 않는다).

```powershell
& tools\ask.ps1 -Lines 'about inst:o_building.generic', 'statics inst:o_building.generic max=200'
& tools\ask.ps1 -Lines 'statics inst:o_building.c_construction max=120', 'statics inst:o_game_map_controller.__construction_manager max=80', 'statics inst:o_game_map_controller.__construction_manager.__free_buildings max=40'
& tools\ask.ps1 -Lines 'treecall building_generic_get_array_of_all_buildings depth=1 max=120'
```

Expected: `instanceof GenericBuilding`, 정적 메서드의 이름들(`# static 0` 아래). `treecall`은 건물 종류의 배열. 이름들 가운데 조건·비용·등급으로 보이는 것을 고른다
(`can`, `allow`, `lock`, `forbid`, `limit`, `cost`, `enough`, `upgrade`, `grade`, `level`, `unlock`).

- [ ] **Step 3: (게임 화면) 건물 종류의 구조와 비용의 자리를 본다**

조건에 걸리는 건물과 업그레이드할 수 있는 건물을 하나씩 정한다(사용자에게 이름을 묻거나 화면을 떠서 본다).
`tree`로 그 종류의 `__construction_cost`, `__allow_to_build_function_list`, `__grades_buildings`와 `GenericBuildingGrade`의 자식을 본다.
같은 종류의 건물 둘의 `generic`이 같은 구조체인지: 하나의 비용 한 칸에 `poke`가 아니라 `write`로 눈에 띄는 수를 쓰고 다른 건물의 같은 칸을 읽는다(되돌린다).

- [ ] **Step 4: (게임 화면) 사용자가 다루는 동안 후보를 기록한다**

Step 2 에서 고른 메서드에 `record`를 건다(자리는 64개다). 사용자에게 차례대로 부탁한다:
(가) 건설 창을 열고 조건에 걸린 건물 위에 커서를 올린다 → 놓으려 한다, (나) 지을 수 있는 건물을 하나 놓는다, (다) 업그레이드 단추가 꺼진 건물을 고른다 → 단추에 커서를 올린다,
(라) 업그레이드할 수 있는 건물을 올린다. 단계마다 `records`와 `shot`으로 어느 함수가 무엇을 받고 무엇을 돌려줬는지 받는다.

- [ ] **Step 5: (게임 화면) 그 자리에서 풀어 본다**

기록에서 "거짓(또는 0)을 돌려줘서 막는" 함수를 찾으면 `override <주소> b:1`(또는 본 값의 반대)로 바꾸고 사용자가 같은 동작을 다시 한다. 한 번에 하나씩 건다.
풀리면 `unoverride`로 되돌려 다시 막히는지 본다(그 함수가 원인이라는 것). 비용은 값 쓰기(비용 배열을 0 으로)를 먼저 해 보고, 안 되면 비용을 돌려주는 함수를 바꾼다.
각 시도의 앞뒤를 `shot`과 창고의 수로 남긴다.

- [ ] **Step 6: 끈다**

```powershell
pwsh -NoProfile -File tools/session.ps1 -Action stop -Name stage3c-session1
```

- [ ] **Step 7: 잰 것을 적는다** — `research/09-build.md`(`research/08`의 꼴): 정적 메서드의 이름(조건·비용·등급), 호출 기록, 풀린 것과 풀리지 않은 것, 확인하지 못한 것.

---

### Task 3: 치트로 굳힌다 (Task 2 의 답으로 채운다)

Task 2 가 끝나면 이 절을 채운다. 미리 정하는 것은 꼴뿐이다:

- 치트 표에 종류를 더한다: `Hook`(스크립트의 정식 이름 + 바꿔 돌려줄 값 + `skip`). 켜면 `NlRecorder::Override`, 끄면 `Unoverride`. 스크립트 이름은 이 게임 버전의 것이다
  (메서드의 `gml_Script_anon_…` 이름에 든 번호는 빌드마다 다르다. 없으면 "이 버전에는 없다"고 보이고 켜지 않는다).
- 건설비 0: 값 쓰기로 되면 건물 종류를 돌며 비용 배열을 0 으로 쓰고 처음 본 값을 기억한다(끄면 되돌린다). 러너와 무관한 부분(되돌릴 값의 기억)은 `src/core`에 두고 시험한다.
- 항목마다 `Verified`는 Task 4 에서 본 것만 참.

---

### Task 4: 확인 실행, 문서, 머지

- [ ] 독립 코드 검토(전체 브랜치) → Critical·Important 를 시험과 함께 고친다(켜기 전에).
- [ ] 실행 묶음 1회(사용자 승인): 적재 판정, 건설비 무료·건설 조건·업그레이드 조건을 사용자가 플레이로 본다. 끄고 켠 뒤 바꾼 값이 돌아오는지.
- [ ] `research/09-build.md`, `CLAUDE.md`, `README.md`, 스펙 §10·§13. `git merge --no-ff feat/cheat-build` → `develop`.

## Self-Review

- **스펙 대조**: §3 의 수단 D(스크립트 훅)가 여기서 처음 생긴다(`NlRecorder::Override`). §8 건설·생산의 "건설 비용 무료", "업그레이드 무료"와 사용자가 더한 "건설 조건 제거", "업그레이드 조건 제거". §14 의 명령에 `statics`·`treecall`·`override`를 더한다.
- **빈칸**: Task 3 은 Task 2 의 답이 있어야 쓸 수 있다. 어느 함수를 바꿀지 지금 적으면 추측이다. 꼴만 정했다.
- **이름의 일치**: `NlRecorder::Forced`·`Override`·`Unoverride`가 Task 1(만듦)과 Task 3(씀)에서 같다.
- **Review Focus**: 1~3 은 Task 2 Step 5 의 실행에서 본다(훅은 Aurie 없이 시험할 수 없다). 4·5 는 Task 2 Step 2 에서 일부러 보낸다.
