# 치트 메뉴 5단계의 나머지: 군대·전투 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 사용자가 준 목록의 "군대/전투"를 모드창에서 되게 한다: 병사를 늘리고, 모집 비용과 시간을 줄이고, 전투에서 아군이 다치지 않게 한다.

**Architecture:** 5단계와 같은 길이다(`plans/2026-10-06-cheat-menu-stage5-knowledge-army.md`). 조사한 세이브(아덴 4일차)에는 병영과 병사가 없다. 그래서 먼저 게임의 디버그 함수로 병사를 만들고
(`gml_Script_rebellion_debug_spawn_player_soldier`: 인자를 하나까지 받고 생략할 수 있다. 안에서 `rebellion_debug_get_spawn_node`, `actor_spawn`, `rebellion_debug_make_soldier_loyaled_to`를 부른다. 기계어),
그 병사로 병영·모집·전투 쪽의 값과 함수를 잰다. 판정·비용은 훅(`Hook`·`HookScale`), 한 번 하는 일(병사 추가)은 패널의 단추와 원격 명령으로 낸다.

**Tech Stack:** C++20 모듈(Aurie v2.0.2, YYToolkit v5.0.0c, Dear ImGui), `tools/session.ps1`·`ask.ps1`·`load-save.ps1`, `tests/native`.

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(군대·전투의 줄), §10(5).

## 아는 것과 모르는 것

아는 것(`research/12`): 관리자 `…__soldiers_barracks_manager`(`add_soldier`, `fire`, `get_array_of_soldiers`), `…__solders_barracks_hiring_manager`(`try_to_hire(인자 4)`,
`instant_spawn_hired_mercenaries(인자 3)`, `get_hiring_time(인자 1)`, `__refresh_mercenary_market()`), `battle_squads_manager.__array_of_squads`.
`SoulBasic`의 메서드 `get_soldier_cost`, `get_soldier_cost_factors`, `get_equipment_cost`, `get_combat_level_in_battle`, `take_damage(글, 구조체, 불리언) -> true`, `battle_hit`.
이름 있는 스크립트 `debug_spawn_army`(인자 여섯까지), `get_army_strength(인자 1)`. 치트 표의 전투 스위치 6개와 수 2개(`o_debug`. 확인 전).

모르는 것(Task 1 이 잰다): 디버그 소환이 인자 없이 되는가. 병사가 무엇으로 있는가(오브젝트, 갈래, 병영의 목록). 모집 비용·시간을 돌려주는 함수. 전투를 일으키는 길과 피해가 셈해지는 자리.

## Global Constraints

- 앞 단계와 같다: `gml_Script_` 이름으로만, 꼴을 본 함수만, 위험한 호출은 요청의 맨 뒤에 따로, 훅 자리 64개, 게임은 3번까지, 세이브의 사본, 자동 저장 시각을 넘기지 않는다.
- 전투의 배율은 양쪽 모두에게 걸리는지 아군에게만 걸리는지를 잰 대로 창에 적는다.

## Review Focus

1. 병사를 만드는 단추가 병영의 정원·임금과 어긋난 병사를 남기는 일.
2. 초당 수천 번 불리는 전투 함수에 건 훅.
3. 적에게도 걸리는 배율이 아군만이라고 적혀 있는 일.
4. 소환 함수를 인자 없이 불러 게임이 끝나는 일(조사에서 먼저 잰다).

---

### Task 1: 조사 실행 (게임 1회)

- [ ] 세이브의 사본, `session.ps1 -Action start`, `load-save.ps1`.
- [ ] 병사를 만든다: `call gml_Script_rebellion_debug_spawn_player_soldier`(요청의 맨 뒤에 따로). 앞뒤로 오브젝트의 수, 병영의 목록, 새 사람의 `__soul`을 읽는다.
- [ ] 병영·모집·전투 관리자와 병사의 `statics`·`tree`, 후보에 `record`. 모집 비용·시간의 함수를 찾아 `override x:`로 풀어 본다.
- [ ] 끄고 `research/13-army.md`에 적는다. 커밋.

### Task 2: 치트로 굳힌다 (Task 1 의 답으로 채운다)

- [ ] `src/core`에 시험과 함께(RED → GREEN), 치트 표의 항목(영역: 군대·전투)과 패널·원격 명령.

### Task 3: 검토, 확인 실행, 문서, 머지

- [ ] 독립 코드 검토 → Critical·Important 를 시험과 함께 고친다. 확인 실행. `research/13`, `CLAUDE.md`, `README.md`, 스펙. `git merge --no-ff feat/cheat-army` → `develop`.

## Self-Review

- **스펙 대조**: §8 의 군대·전투 네 줄이 Task 1·2 에 있다. 전투의 배율은 전투를 일으킬 수 있을 때만 잰다(못 하면 적고 넘긴다).
- **빈칸**: Task 2 는 Task 1 의 답이 있어야 쓸 수 있다.
