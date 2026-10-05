# 치트 메뉴: 전투 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 사용자가 준 목록의 "전투"(공격력·방어력 배율, 받는 피해 0, 적 약화)를 모드창에서 되게 하고, 확인 전으로 남은 "아군 무적"을 실제 싸움에서 확인한다.

**Architecture:** 영혼의 함수 셋에 훅을 걸고 self 가 플레이어의 영혼인지로 아군과 적을 가린다(`NlRecorder::Forced::Who`. 군대 단계에서 만들었다).
상처를 입히는 `take_damage`는 아군에게 건너뛰고(아군 무적), 싸울 때의 전투 기술 `get_combat_level_in_battle`과 치명적인 통증의 한도 `get_mortal_pain_threshold`가 돌려주는 수에는
아군과 적에게 다른 배율을 곱한다(`Forced::Other`. 한 함수에 훅 하나). 훅은 게임의 자료를 쓰지 않으므로 세이브에 남는 것이 없다.

**Tech Stack:** C++20 모듈(Aurie v2.0.2, YYToolkit v5.0.0c, Dear ImGui), `tools/session.ps1`·`ask.ps1`·`load-save.ps1`, `tests/native`.

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(군대·전투의 줄).

## Global Constraints

- 게임은 3번까지(사용자 승인 2026-10-06). 실행은 백그라운드로, 게임 창과 마우스를 건드리지 않는다.
- 게임 시각 18시(저녁 자동 저장)를 넘겨 밤까지 돌려도 된다. **넘기기 전에는 소환·값 쓰기를 하지 않고 훅만 켠다.** 다음 날 06시(아침 자동 저장) 전에 끈다. 켜기 전에 세이브의 사본을 뜬다.
- 아직 안 불러 본 `AmbushManager.start_ambush_wolves()`는 조사 실행의 맨 끝에 한 번만 부른다(오류 창이 뜰 수 있다는 것을 사용자가 알고 승인했다).
- `Verified`는 실제 싸움에서 효과를 본 항목만 참으로 바꾼다.

## Review Focus

1. `Recorder::Handle`의 배율 갈래가 기존 항목(`Who == 'a'`, `Other == 1`, `Cap == 0`)에서 글자 그대로 같은 동작인가.
2. 전투 기술에 곱한 값이 번호로 쓰여 범위를 벗어나는 일(한도 20), 정수가 아닌 값.
3. 아군·적의 두 항목이 한 훅을 나눠 쓸 때의 켜고 끄기(하나만 끔, 배율만 바꿈, 메뉴로 나감).
4. "적"이 플레이어의 사람이 아닌 모두라는 것(손님, 상인, 다른 세력끼리)을 글이 적는가.

---

### Task 1: 아군·적의 배율

- [ ] 시험 먼저(`HookFactor`, `ScaleCapped`, `PlanSides`), `Forced::Other`·`Cap`, 치트 표의 넷(`ally_power`, `enemy_power`, `ally_toughness`, `enemy_toughness`), `src/People.cpp`의 `BattleTick`.

### Task 2: 실전 확인, 검토, 문서, 머지

- [ ] 확인 실행: 훅만 켠 채 06시부터 밤까지 돌려 게임이 스스로 건 싸움에서 표본과 상처를 본다. 맨 끝에 늑대 매복 한 번.
- [ ] 독립 코드 검토 → 고친다. `research/16-battle.md`, `CLAUDE.md`, `README.md`, 스펙. `git merge --no-ff` → `develop`.

## Self-Review

- **스펙 대조**: §8 의 "공격·방어·피해 배율, 받는 피해 0"이 Task 1·2 에 있다. 사기와 즉시 승리(분대의 전투)는 이 계획에 없다: 분대를 만드는 길을 재지 못했다.
