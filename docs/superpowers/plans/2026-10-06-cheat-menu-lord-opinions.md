# 영주의 호감·충성, 특성의 한글 이름과 전체 목록 — 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 영주끼리의 평판(호감)과 왕에 대한 충성을 게임의 함수로 올리고 내리고, 인물 탭의 특성을 게임의 한국어 이름으로 보이며 전체 특성 목록(명칭·설명)을 넣는다.

**Architecture:** 판단은 `src/core/CourtPlan`(걸음, 일, 결과의 글)과 `src/core/Localization`(게임의 현지화 CSV 읽기, 힌트 글 다듬기)에 두고 시험한다. 러너에 닿는 것은 `src/Court.cpp`(영주 영역의 패널, 원격 `court`)와 `src/People.cpp`(특성의 글)다.

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(영주·인물), §10. 잰 것: `research/20-lord-opinions.md`.

## Global Constraints

- 게임의 함수는 `record`에서 본 꼴로만 부른다. 부르기 전에 로그를 남기고, 쓴 뒤 다시 읽어 확인한다. 그리는 쪽에서 러너를 부르지 않는다.
- 창의 글에 재지 않은 것을 사실처럼 적지 않는다. 글꼴에는 한글과 라틴-1 만 있다.
- 게임 파일의 글(이름, 설명)을 레포에 옮겨 적지 않는다. 모듈이 실행 중에 사용자의 게임 폴더에서 읽는다. 시험은 지어낸 글로 한다.
- 게임 실행은 승인받은 3번(1번 씀). 백그라운드, 저장하지 않음, 16:30 전에 끔.

## 잰 것 (실행 1, `lord-session1`)

- 평판을 갖는 쪽: `inst:o_character:<n>.__soul.__character_soul.__opinions`(OpinionMinds). 대상은 상대의 `__soul.__character_soul`(구조체의 주소로 맞춰 봤다).
- `opinion_attach(대상, 평판 자료, 1, undefined) -> 평판`, `detach_generic_opinion_mind(대상, 자료) -> undefined`(하나를 뗀다),
  `get_number_of_attached_opinion_minds_for_character(대상, 자료) -> 수`, `get_opinion(대상) -> 수`, `is_enemy(대상)`, `is_deadly_enemy(대상) -> 불리언`.
- `SoulBasic.get_loyalty_to_king() -> 수`(충성을 따지는 영주는 왕에 대한 평판과 같다. 아이와 왕은 100), `get_loyalty_state() -> 0|1|2`, `is_has_loyalty() -> 불리언`.
- 좋은 평판 하나에 +5(Amold 는 +6), 나쁜 것 하나에 −5. 충성 상태: 49 까지 1, 55 에서 2. −14 까지 1, −19 에서 0. `is_enemy`는 −29 에서 참.
- 주민의 충성 대상: `__soul.__fealty.__loyaled_to_uuid`(영주의 uuid, 없으면 빈 글). `reset_loyaled_to()`(인자 없음)가 빈 글로 만든다(한 명에게 불러 봤다).
- 현지화: `<게임>\localization\main.csv`(`Key,Russian,English,…,Korean,…`. 특성의 이름은 `trait.<이름>`), `hints.csv`(`Code,…`. 따옴표 안에 줄바꿈).
  특성에서 힌트의 열쇠로 가는 짝은 파일에 없다(`hint_trait_`·`hint_talent_`·`hint_` 가 섞여 있고 40개는 어느 것도 아니다). 게임에서 얻는다(실행 2 에서 잰다).

## Review Focus

1. 영주가 드나들어 `inst:o_character:<n>`의 n 이 밀린 뒤의 걸음(다른 사람에게 붙이지 않는가).
2. 겹침 한도(50)와 한 번의 한도(40)에서 "붙였다"고 잘못 적지 않는가(센 수로만 적는가).
3. 좋은 것과 나쁜 것이 함께 붙어 있을 때 올리기·내리기가 떼기부터 하는가, 그때 센 수의 확인.
4. 현지화 CSV: 따옴표 안의 쉼표·줄바꿈·`""`, 머리에 Korean 이 없는 파일, 끝이 잘린 파일, 같은 열쇠의 중복.
5. 게임 화면이 아닐 때(메뉴)와 왕이 없을 때의 명령.

## Tasks

### Task 1: core/CourtPlan (시험 먼저)
- `tests/native/core_tests.cpp`: `LoyaltyLabel`, `ParseCourtGoal`, `CheckCourt`, `PlanCourtJobs`, `PlanCourtStep`(가짜 평판으로 끝까지 돌려 본다), `CourtStepDone`, `CourtReport`, `ReleaseReport`.
- `src/core/CourtPlan.hpp/.cpp`, `CMakeLists.txt`.
- 확인: `pwsh -File tools/build.ps1` 뒤 `pwsh -File tools/test-native.ps1` 통과.

### Task 2: 원격 `court` 줄과 `src/Court.cpp`
- `core/RemoteCommand`: `court list`, `court <uuid|lords> <loyal|like|opinion|clear|release> [about=] [amount=] [goal=] [queue=1]`(모르는 열쇠 거부). 시험.
- `src/Court.cpp/.hpp`: 훑기(영주, 왕, 충성, 서로의 평판, 따르는 사람), 일의 걸음, 패널. `NlPeople::Rows`. `Menu.cpp`·`Remote.cpp`·`ModuleMain.cpp`.

### Task 3: core/Localization 과 특성의 글
- 시험: `ReadLocalization`(지어낸 CSV), `PlainHint`, `TraitCaptionKey`.
- `src/People.cpp`: 시작할 때 `main.csv`에서 `trait.*`의 한국어 이름을 읽어, 가진 특성과 전체 목록(찾기, 붙이기)에 보인다. 원격 `traits [find=]`.
- 설명(힌트)은 실행 2 에서 힌트의 열쇠를 얻는 길을 잰 뒤에 붙인다.

### Task 4: 검토, 확인 실행, 문서
- 독립 검토(확인 실행 전) → 고치기 → 실행 2(영주 확인 + 특성 이름 확인 + 힌트의 열쇠 조사) → 설명 구현 → 실행 3 → `research/20`, `CLAUDE.md`, `README.md`, 스펙, 이 문서의 결과.
