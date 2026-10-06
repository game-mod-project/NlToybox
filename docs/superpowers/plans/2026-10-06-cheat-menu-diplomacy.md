# 치트 메뉴: 외교(왕국과의 관계) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 사용자가 준 목록의 외교 가운데 "국가 관계 설정, 호감도 변경, 적대도 제거, 전쟁·평화 강제"를 모드창에서 되게 한다.

**Architecture:** 왕국 사이의 관계(종류)는 게임이 왕끼리의 평판에서 다시 셈해 행렬에 쓴다(`Faction.__update_relations`. `research/19`). 행렬에 바로 쓴 값은 되돌아간다.
그래서 게임의 함수 `Faction.attach_opinion_about_faction(대상 세력, 평판의 자료)`로 게임의 디버그 평판(±5)을 붙이고 다시 셈하게 한다.
패널은 왕국의 이름과 양쪽의 관계를 보이고, 바라는 관계(우호·중립·적대)가 될 때까지 한 걸음씩 붙인다. 판단은 `core/DiplomacyPlan`, 러너는 `src/Diplomacy.cpp`.

**Tech Stack:** C++20 모듈(Aurie v2.0.2, YYToolkit v5.0.0c, Dear ImGui), `tools/session.ps1`·`ask.ps1`·`load-save.ps1`, `tests/native`.

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(외교의 줄).

## Global Constraints

- 게임은 경제와 합해 5번까지(사용자 승인 2026-10-06). 백그라운드로, 게임 창과 마우스를 건드리지 않는다.
- 게임이 스스로 부르지 않는 함수는 실행의 맨 마지막에 하나만 부른다(사용자 승인. 오류 창이 뜰 수 있다고 알린다).
- 시험 값이 든 채 게임 시각 16:30(405000)을 넘기지 않는다. 저장하지 않는다.
- 게임의 함수는 기록에서 본 꼴로만 부른다. 부르기 전에 로그. 그리는 쪽에서 러너를 부르지 않는다.

## Review Focus

1. 왕국이 아닌 세력(도적, 상인, 교단, 플레이어의 꾸러미)이나 플레이어 자신에게 평판이 붙는 일.
2. 세력의 자리(`__array_of_factions[n]`)가 밀렸을 때 다른 세력에 붙는 일.
3. 동맹·봉신·주군 관계나 읽지 못한 관계에서 끝없이 붙이는 일, 한도를 넘겨 붙이는 일.
4. 게임의 디버그 평판 자료가 잰 것과 다를 때(다른 빌드) 그대로 붙이는 일.
5. 실패(붙지 않음, 한도까지 붙였지만 안 됨)를 성공처럼 적는 일, 창의 글이 재지 않은 것(세이브, 기한)을 사실로 적는 일.

---

### Task 1: 조사 (경제의 실행 둘에 얹었다. 끝났다)

- [x] `eco-session1`: 관계의 종류, 게임의 다시 셈하기, 평판을 붙이는 함수와 문턱, 왕국의 이름. `eco-session2`: 평판을 떼는 함수. `research/19-diplomacy.md`.

### Task 2: 외교 패널

- [x] 시험 먼저: `RelationLabel`, `IsKingdom`, `StepToward`, `OpinionSteps`, `CheckDiplomacy`, `DiplomacyReport`, 원격 `diplomacy …`.
- [x] `src/core/DiplomacyPlan`, `src/Diplomacy.cpp`(왕국 모으기, 일의 한 걸음, 패널), `src/Menu.cpp`·`src/Remote.cpp`·`CMakeLists.txt`.

### Task 3: 확인 실행, 검토, 문서, 머지

- [x] 원격 `diplomacy …`와 화면으로 본다(게임이 스스로 다시 셈한 뒤에도 남는가). 검토의 지적을 고친다. `research/19`, `CLAUDE.md`, `README.md`, 스펙 §8·§10.

## 결과 (2026-10-06)

- 모듈 0.20.1. 게임은 경제와 합해 4번 켰다(승인 5번). 조사는 경제의 실행 둘에 얹었고, 확인은 `dip-session1`(0.19.0)과 `dip-session2`(0.20.1. 머지한 빌드).
- 된 것: 왕국 24개의 이름과 양쪽의 관계, 한 왕국·모든 왕국을 우호·중립으로, 한 왕국을 적대로, 평판 하나씩, 평화 협정·교역 협정·방어 동맹. 게임이 스스로 다시 셈한 뒤에도 남는다.
- 계획에 없던 것: 협정 맺기(실행 3의 마지막 호출에서 `set_agreement`가 됐다).
- 하지 않은 것: 관계의 종류 동맹(0)·봉신·주군, 협정을 푸는 것, 붙인 평판을 떼는 것, 외교 비용·성공률.
- 확인하지 못한 것: 붙인 평판과 협정이 세이브에 남는가, 얼마나 오래 남는가, 게임이 협정을 어떻게 따르는가, 세계 지도의 표시, 망한 왕국에서의 동작(코드는 건드리지 않게 막았다).
