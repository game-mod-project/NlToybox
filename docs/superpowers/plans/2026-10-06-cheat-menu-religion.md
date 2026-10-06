# 종교 — 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 종교 영역을 채운다: 주교와의 평판, 신앙심 유지와 신앙 감소, 성스러운 보호와 종교 반란의 판정, 종교 행동·설교의 비용, 기도·예배의 신앙 회복.

**Architecture:** 주교와의 평판은 영주의 호감(`src/Court.cpp`, `core/CourtPlan`)에 주교를 평판의 주체로 더해 같은 길을 태운다. 신앙심 유지는 인구 항목의 바퀴(`src/People.cpp`)에,
자료를 고쳐 쓰는 둘(비용 0, 회복 배율)은 `src/Production.cpp`의 일(`Job`)에, 판정 둘은 치트 표의 훅 항목에 넣는다.

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(종교), §10. 잰 것: `research/21-religion.md`.

## Global Constraints

- 게임의 함수는 `record`에서 본 꼴로만 부른다. 부르기 전에 로그를 남기고, 쓴 뒤 다시 읽어 확인한다. 그리는 쪽에서 러너를 부르지 않는다.
- `Verified`는 플레이에서 효과를 본 뒤에만 참으로 둔다. 창의 글에 재지 않은 것을 사실처럼 적지 않는다.
- 게임 실행은 승인받은 3번(1번 씀). 백그라운드, 저장하지 않음, 16:30 전에 끔. 처음 부르는 함수는 실행의 맨 끝에 하나만.

## 잰 것 (실행 1, `rel-session1`)

- `ReligiosityManager.get_bishop_opinion()`(인자 없음. 게임이 6시간에 15번)은 **주교가 우리 왕을 보는 평판**이다: 주교의 평판 구조체에
  `opinion_attach(왕의 인물 영혼, debug_positive, 1, undefined)`를 부르자 0 → 5 → 15. 주교는 `o_character`(진영 `holy_synod`), uuid 는 `…__religiosity_manager.__bishop_uuid`.
- 신앙심(욕구 3번)은 `inst:o_debug.debug_piety_decrease_per_hour`(0.83)를 따른다: 대조 1.8시간에 14명이 시간당 −0.81 또는 −0.40, 0 으로 쓴 1.37시간에 14명 모두 0.00.
- 성스러운 보호: `inst:o_game_map_controller.__onboard_manager.__is_under_holy_defence`(1), `__population_to_remove_holy_defence`(65). `is_under_holy_defence()`(인자 없음)가 그 수를 돌려준다
  (게임이 6시간에 1번). 훅으로 `n:1`을 걸면 자료를 0 으로 써도 1 을 돌려준다. `is_allow_to_religious_riot() -> false`(1번).
- 종교의 게임 변수 32개가 `global.__gameplay_vars`에 있다(비용 여섯, 신앙 회복 넷 …). 설교 열 가지는 `inst:o_data.__preach_data.__preach_list[n]`(`__name`, `__cost` 0 ~ 150 …).
  써지고 되돌려진다. 세이브 파일에는 그 열쇠가 없다.
- 설교·헌금의 스크립트 아홉은 6시간에 한 번도 불리지 않았다(교회는 있지만 설교가 정해져 있지 않다). 설교의 효과는 이 단계에서 재지 못한다.

## Review Focus

1. 주교가 떠나거나 죽은 뒤(자리가 밀린 뒤)의 걸음, 주교가 "영주 모두"의 일에 섞여 들어가지 않는가.
2. 새 훅 종류(수를 돌려주게 한다)가 저장된 상태에서 켜진 채로 시작하지 않는가, 불리언 훅과 섞이지 않는가.
3. 자료를 고쳐 쓰는 일(비용 0, 회복 배율)이 끌 때 원래 값으로 되돌리는가, 둘이 같은 자리를 건드리지 않는가, 게임 화면이 아닐 때.
4. 신앙심 유지가 다른 욕구 항목(배고픔, 피로, 모든 욕구)과 함께 켜졌을 때의 고르기.
5. 창의 글이 확인하지 않은 효과를 사실처럼 적지 않는가.

## Tasks

### Task 1: 코어와 표 (시험 먼저)
- `tests/native/core_tests.cpp`: 표의 새 항목 여섯과 `piety_decrease`의 확인, `CheatKind::HookNumber`, `NeedsToHold(…, Piety)`, `ReligionCostVars`·`PietyRestoreVars`, 주교(`CourtLord::Bishop`, `"bishop"`).
- `src/core/CheatTable.*`, `PeoplePlan.*`, `WorldPlan.*`, `CourtPlan.*`.

### Task 2: 러너 쪽
- `src/Cheats.cpp`(훅의 형), `src/People.cpp`(신앙심 유지), `src/Production.cpp`(일 둘), `src/Court.cpp`(주교, `DrawBishop`), `src/Menu.cpp`.

### Task 3: 검토, 확인 실행, 문서
- 독립 검토(확인 실행 전) → 고치기 → 실행 2(주교와의 평판을 게임의 함수로, 신앙심 유지, 훅 둘, 자료 쓰기와 되돌리기, 화면) → `research/21`, `CLAUDE.md`, `README.md`, 스펙, 이 문서의 결과.
