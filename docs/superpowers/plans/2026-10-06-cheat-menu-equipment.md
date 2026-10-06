# 치트 메뉴: 장비 지급 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 사용자가 준 목록의 "장비 지급"을 모드창에서 되게 한다: 병사에게 넣어 준 갑옷·무기·방패가 벗겨지지 않고 남는다.

**Architecture:** 게임은 영혼마다 선호 장비(`__soul.__preferred_equipment`)를 두고 거기에 없는 장비를 무기고로 돌려보낸다(`research/17`).
그래서 게임의 세터 `SoulBasic.set_preferred_equipment(선호 장비의 묶음)`로 병사의 선호 장비를 정하고(묶음은 `o_data.__preferred_equipment_data`의 멤버),
그 묶음의 갑옷·무기·방패 가운데 없는 것을 소지품에 넣는다(`ComponentInventory.change`. 아이템 패널과 같은 길). 인물에게 하는 일 하나(`PersonAct::Equip`)로 더한다.

**Tech Stack:** C++20 모듈(Aurie v2.0.2, YYToolkit v5.0.0c, Dear ImGui), `tools/session.ps1`·`ask.ps1`·`load-save.ps1`, `tests/native`.

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(군대·전투의 "장비 지급" 줄, 아이템의 줄).

## Global Constraints

- 게임은 3번까지(사용자 승인 2026-10-06). 백그라운드로, 게임 창과 마우스를 건드리지 않는다. 디버그 함수는 부르지 않는다.
- **게임 시각 16:30(405000) 전에 끈다**: 저녁 자동 저장은 18시보다 이르다(`research/17`). 둘째 조사 실행이 그것을 넘겨 자동 저장 파일 하나를 만들었다.
- 병과의 구조체에는 쓰지 않는다(영혼들이 함께 쓴다). 사람을 여러 시간 따라갈 때는 uuid 로 다시 찾는다.

## Review Focus

1. 병사가 아닌 사람(주민, 영주, 손님)에게 선호 장비가 걸리는 일.
2. 게임의 자료에 그 묶음이 없거나 구조체가 아닐 때 세터를 부르는 일.
3. 넣어 줄 자원 번호가 범위를 벗어나거나 0 번·음수인 일, 이미 가진 것을 또 넣는 일.
4. 여럿에게 할 때 일부가 실패한 것을 성공으로 적는 일.
5. 창의 글이 재지 않은 것(며칠 뒤, 세이브)을 사실로 적는 일.

---

### Task 1: 조사 (게임 2회. 끝났다)

- [x] `equip-session1`: 넣어 준 장비가 벗겨져 창고로 가는 것, 그동안 게임이 부르는 함수들.
- [x] `equip-session2`: 선호 장비와 병과의 자료, 게임의 세터를 불러 선호 장비가 하루 동안 남는 것. `research/17-equipment.md`.

### Task 2: 장비 지급

- [ ] 시험 먼저(`PersonAct::Equip`, `Loadouts`, `EquipGifts`, `IsSoldier`, 원격 `person … equip name=<묶음>`), `src/People.cpp`의 `One()`에 case, 군대 패널의 단추.

### Task 3: 확인 실행, 검토, 문서, 머지

- [ ] 확인 실행(게임 3/3): 아침 06시에 병사 넷(대조: 장비만 / 지급 `h_swordman` / 지급 `any` + 장비 / 지급 `h_axeman`)을 두고 16:15 까지 돌려 누가 장비를 지키는지 본다.
- [ ] 독립 코드 검토 → 고친다. `CLAUDE.md`, `README.md`, 스펙. `git merge --no-ff` → `develop`.

## Self-Review

- **스펙 대조**: §8 의 "장비 지급"이 Task 2·3 에 있다. "최고 등급 장비"는 중갑·검·방패의 묶음이 그것이다. 장비의 삭제는 아이템 패널의 빼기가 한다.
- **빈칸**: 확인 실행이 "벗겨진다"로 나오면 이 길은 틀린 것이다. 그때는 적고 사용자에게 다음 조사를 묻는다.
