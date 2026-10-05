# 치트 메뉴 7단계: 프리셋·유틸 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 사용자가 준 목록의 "프리셋"(God, Sandbox, Easy, Normal)과 "유틸"의 시간 정지를 모드창에서 되게 한다.

**Architecture:** 프리셋은 치트 표의 **확인된 항목의 묶음**이다(`src/core/Presets`). 누르면 묶음에 없는 표의 항목은 끄고 묶음의 항목은 켠다(배율은 묶음의 것으로).
게임에 새로 닿는 것은 없다: 항목들은 앞 단계에서 플레이로 확인한 것이고, 시간 정지는 실행 묶음에서 수십 번 부른 두 함수(`__set_warp(0)`, `set_time_speed(0)`)다.

**Tech Stack:** C++20 모듈(Aurie v2.0.2, YYToolkit v5.0.0c, Dear ImGui), `tests/native`.

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(프리셋, 유틸의 줄), §10(7).

## Global Constraints

- 프리셋에는 `Verified` 인 항목만 넣는다(시험이 지킨다). 값을 써 넣는 항목(`CheatKind::Number`)은 넣지 않는다(세이브에 남는 값이 있다).
- 탐색기의 잠금, 배율 7개, 한 번 하는 단추는 프리셋이 건드리지 않는다.
- 게임은 3번까지. 재지 않은 호출을 넣지 않는다. 실행은 백그라운드로, 게임 창과 마우스를 건드리지 않는다.

## Review Focus

1. 프리셋을 걸 때 켜져 있던 항목이 껐다 켜지며 값이 두 번 곱해지거나 되돌릴 값이 사라지는 일.
2. 확인 전의 항목이나 세이브에 남는 값을 쓰는 항목이 묶음에 섞이는 일.
3. 멈춤·다시 흐르게를 게임 화면이 아닐 때 누르는 일.
4. 창의 글이 프리셋이 하지 않는 일을 한다고 적는 일.

---

### Task 1: 프리셋과 시간 정지

- [ ] `core/Presets`(시험 먼저: 묶음 넷, `CheckPreset`), 원격 `preset <이름>`·`time pause|resume`, 프리셋 패널, 시간 패널의 멈춤·다시 흐르게.

### Task 2: 검토, 확인 실행, 문서, 머지

- [ ] 독립 코드 검토 → Critical·Important 를 시험과 함께 고친다. 확인 실행(프리셋 넷을 차례로 걸고 표의 상태와 게임의 값으로 본다). 문서. `git merge --no-ff` → `develop`.

## Self-Review

- **스펙 대조**: §8 의 프리셋 줄(God, Sandbox, Easy, Normal)과 유틸의 "시간 정지"가 Task 1 에 있다. "저장"(게임의 저장 함수)과 "인물·아이템 검색"은 재지 않아 넣지 않았다(탐색기의 찾기와 인물 목록이 있다).
