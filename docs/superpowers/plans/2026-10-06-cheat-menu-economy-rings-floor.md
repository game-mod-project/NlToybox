# 치트 메뉴: 신성 반지와 최소값 유지 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 경제 패널에서 신성 반지의 수를 다루고, 금화·신성 반지·자원마다 최소값을 두어 그 아래로 내려가지 않게 한다.

**Architecture:** 신성 반지는 영지 창고의 0번 칸(`rune`)이고 다른 자원과 같은 함수(`…__province.__warehouse.change`)로 바뀐다(`research/18`).
경제 패널이 금화 아래에 반지 줄을 그리고, "모든 자원 +N"에는 넣지 않는다. 최소값 유지는 패널의 틱이 1초마다 화면의 수를 보고
바닥에 못 미치는 것을 같은 함수로 채우는 것이다(값을 바로 쓰지 않는다). 켜고 끄는 것은 치트 표의 `Custom` 항목 하나, 바닥은 상태 파일의 `floor` 줄.

**Tech Stack:** C++20 모듈(Aurie v2.0.2, YYToolkit v5.0.0c, Dear ImGui), `tools/session.ps1`·`ask.ps1`·`load-save.ps1`, `tests/native`.

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(경제의 줄).

## Global Constraints

- 게임은 이 단계와 외교를 합해 5번까지(사용자 승인 2026-10-06. 경제 2 + 외교 3). 백그라운드로, 게임 창과 마우스를 건드리지 않는다.
- 시험 값이 든 채 게임 시각 16:30(405000)을 넘기지 않는다. 저장하지 않는다.
- 넘기는 변화량은 유한한 정수. 부르기 전에 로그. 그리는 쪽에서 러너를 부르지 않는다.

## Review Focus

1. 바닥의 수가 NaN·음수·터무니없이 큰 수일 때 게임의 함수가 불리는 일.
2. "모든 자원 +N"이나 "0 으로"가 반지를 건드리는 일.
3. 최소값 유지가 꺼져 있거나 게임 화면이 아닐 때 채우는 일, 호출이 실패할 때 1초마다 되풀이하는 일.
4. 틱이 오브젝트 이벤트마다 비싼 일을 하는 것(시각부터 봐야 한다).
5. 상태 파일의 `floor` 줄이 읽고 쓰는 사이에 사라지거나, 이 게임에 없는 열쇠 때문에 다른 줄이 깨지는 일.

---

### Task 1: 조사 (게임 1회. 끝났다)

- [x] `eco-session1`: 반지의 자리(영지 창고 0번 칸), `change(0, +5)`로 화면까지 따라오는 것, 영주의 반지(소지품 0번 칸과 `character_runes_get_count`). `research/18-economy-rings.md`.

### Task 2: 신성 반지와 최소값 유지

- [x] 시험 먼저: `RingResource`, `EconomyTargets`, `PlanFloors`, `FloorValue`, `GoodFloorKey`, `EconomyAct::FloorSet`·`GoldFloor`, 상태 파일의 `floor` 줄, 원격 `economy floor`·`gold_floor`, `person item_add index=0`.
- [x] `src/core/EconomyPlan`, `src/core/CheatState`, `src/Economy.cpp`(반지 줄, '최소' 칸, `HoldFloors`), 치트 표의 `resource_floor`, `src/Menu.cpp`의 저장.

### Task 3: 확인 실행, 검토, 문서, 머지

- [ ] `eco-session2`: 원격 `economy …`와 화면으로 반지와 최소값을 본다. 검토의 지적을 고친다. `research/18`, `CLAUDE.md`, `README.md`, 스펙 §8·§10.
