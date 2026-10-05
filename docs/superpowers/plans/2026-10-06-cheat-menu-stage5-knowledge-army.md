# 치트 메뉴 5단계: 지식·아이템·군대·전투 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 사용자가 준 목록의 "연구/Knowledge", "아이템/장비", "군대/전투"를 모드창에서 되게 한다: 지식을 하나·전부 해금하고 연구를 빠르게 하고, 인물에게 자원과 장비를 주고,
병사를 늘리고 모집 비용을 없애고, 전투의 배율(피해, 사기)을 건다.

**Architecture:** 3나-2 ~ 4단계와 같은 길이다. 켜 둔 게임에서 `tree`·`statics`·`record`와 기계어로 값의 자리와 게임의 함수를 찾고(`research/11`의 "인자가 없는 함수는 기계어로 가린다"),
`write`·`method`·`call`·`override`로 그 자리에서 풀어 본 뒤 굳힌다. 판정·비용은 훅(`Hook`·`HookScale`), 여럿을 돌며 쓰는 것은 `Custom`, 한 번 하는 일(해금, 지급, 소환)은 패널의 단추와 원격 명령으로 낸다.
세이브는 `tools/load-save.ps1`로 직접 불러온다.

**Tech Stack:** C++20 모듈(Aurie v2.0.2, YYToolkit v5.0.0c, Dear ImGui), `tools/session.ps1`·`ask.ps1`·`load-save.ps1`, `tests/native`.

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(지식, 아이템, 군대·전투의 줄), §10(5), §14(원격 명령).

## 아는 것과 모르는 것

아는 것(출처):

- 지식: `inst:o_game_map_controller.__knowledge_manager`(`KnowledgeManager`). `is_have_knowledge_to_upgrade_building(이름, 등급)`·`is_knowledge_unlocked`는 이미 훅으로 바꾼다(`research/09`).
  `__knowledge_unlockeds[19]`, `__permanent_knowledge_names`. 인물의 지식은 `__soul.__character_soul.__knowledge`(세이브의 `knowledge.technology`).
- exe 의 이름(뜻은 추정): `add_all_knowledge`, `add_knowledge`, `remove_knowledge`, `set_knowledge_as_permanent`, `add_knowledge_to_town`, `get_learn_time`, `learn_time`, `add_book`, `create_book`,
  `add_item`, `add_resource`, `add_resource_pile`, `give_possible_equipment`, `set_weapon`, `set_armor`, `__change_actor_equipment`,
  `add_soldier`, `add_soldiers_by_class`, `give_additional_soldiers`, `hire_soldiers`, `hire_cost_per_soldier`, `set_max_hiring_budget`, `instant_spawn_hired_mercenaries`, `win_battle`, `all_squad_surrender`.
  이름 있는 스크립트: `debug_spawn_army`, `rebellion_debug_spawn_player_soldier`, `get_army_strength`, `battle_squad_max_size`, `get_textbook_exp_factor`.
- 인물의 소지품: `__soul.__inventory`(`__money`, `__resources[39]`, `__books`), 장비 `__soul.__equipment`, 선호 장비 `__soul.__preferred_equipment`.
- 관리자: `…__soldiers_barracks_manager`, `__solders_barracks_hiring_manager`, `__solders_barracks_new_army_manager`, `battle_squads_manager`, `__battle_equipment_editor`, `__library_manager`, `__province.__hire_manager`.
- 치트 표에 이미 있는 전투 스위치 6개와 수 2개(`o_debug`. 확인 전), 배율 `book_exp`.

모르는 것(Task 1 이 잰다): 위 이름들이 어느 구조체의 메서드이고 무엇을 받는가. 해금이 값 쓰기로 되는가 함수로 되는가. 연구 시간·모집 비용을 돌려주는 함수. 전투의 피해·사기가 어디서 셈해지는가.
조사하는 세이브(아덴 4일차)에는 병영과 병사가 없을 수 있다: 군대 쪽은 잴 수 있는 것까지만 잰다.

## Global Constraints

- 게임 스크립트는 `gml_Script_` 이름으로만 부르고 훅을 건다. 인자의 수와 형을 본 함수만 부른다(기록이나 기계어). 훅 자리는 한 실행에 64개다: 기록은 고른 것에만 건다.
- 불리언·수를 돌려주는 함수만 `override`한다. 위험한 호출(해금, 소환, 전투 끝내기)은 요청마다 따로, 그 요청의 맨 뒤에 둔다.
- 게임을 켜는 횟수는 승인받은 3번 안에서. 켜기 전에 세이브의 사본을 뜬다. 시험 값이 든 채 게임 시각 06시·18시(자동 저장)를 넘기지 않는다.
- `Verified`는 플레이에서 효과를 본 뒤에만 참. 확인 전의 `Hook`·`Custom`은 꺼진 채로 시작한다.
- 그리는 쪽에서 러너를 부르지 않는다. `RValue`를 틱 너머로 들지 않는다. 사람은 uuid 로 가리킨다(`src/People.cpp`의 길을 쓴다).
- `refs/`, `backups/`, `downloads/`, `build/`는 커밋하지 않는다. 모드창의 글에는 한글과 라틴-1 만 쓴다.

## Review Focus

1. 해금·지급처럼 되돌릴 수 없는 일을 한 번의 클릭으로 하는 단추가 무엇을 하는지 창에 적혀 있는가(세이브에 남는다).
2. 지식 이름·자원 번호·수량에 범위 밖의 값이 게임의 함수로 들어가는 길.
3. 초당 수천 번 불리는 전투 함수에 건 훅이 전투를 느리게 하는 일.
4. 적에게도 걸리는 배율(피해, 사기)이 "아군만"이라고 적혀 있는 일.
5. 병사·장비를 준 뒤 게임의 수(병영의 정원, 창고의 수)와 어긋나는 일.

---

### Task 1: 조사 실행 (게임 1회)

**Files:** Create `research/12-knowledge-army.md`.

- [ ] **Step 1**: 세이브의 사본, `session.ps1 -Action start`, `load-save.ps1`, 적재 판정.
- [ ] **Step 2**: 값의 나무와 메서드: `list`·`statics`를 지식 관리자, 지식 자료(`inst:o_data`), 도서관 관리자, 인물의 지식·소지품·장비, 병영·모집·전투 관리자에.
- [ ] **Step 3**: 이름으로 고른 후보의 스크립트 이름(`about`)을 받아 기계어로 인자 수를 읽고, 게임이 스스로 부르는 것에는 `record`를 건다(연구는 스스로 돈다).
- [ ] **Step 4**: 그 자리에서 풀어 본다: 지식 하나 해금, 연구 시간의 배율, 인물에게 자원 주기, 모집 비용의 배율. 화면(`shot`)과 값으로 본다. 위험한 호출은 맨 뒤에 하나씩.
- [ ] **Step 5**: 끄고 잰 것을 `research/12-knowledge-army.md`에 적는다. 커밋.

### Task 2: 치트로 굳힌다 (Task 1 의 답으로 채운다)

- [ ] 러너와 무관한 부분은 `src/core`에 시험과 함께 둔다(RED → GREEN): 명령의 꼴, 이름·번호·수량의 검사.
- [ ] 치트 표의 항목(영역: 지식, 아이템, 군대·전투)과, 한 번 하는 일의 패널·원격 명령.

### Task 3: 검토, 확인 실행, 문서, 머지

- [ ] 독립 코드 검토(브랜치 전체) → Critical·Important 를 시험과 함께 고친다.
- [ ] 확인 실행: 적재 판정, 새 항목마다 효과를 본다(값, 게임의 화면).
- [ ] `research/12`, `CLAUDE.md`, `README.md`, 스펙 §8·§10. 빌드·시험 넷·적재 판정 뒤 `git merge --no-ff feat/cheat-knowledge-army` → `develop`.

## Self-Review

- **스펙 대조**: §8 의 지식 두 줄, 아이템 한 줄, 군대·전투 네 줄이 Task 1·2 에 있다. 이미 표에 있는 전투 스위치의 확인은 전투가 있는 세이브가 있어야 한다(없으면 다음으로 넘긴다).
- **빈칸**: Task 2 는 Task 1 의 답이 있어야 쓸 수 있다. 꼴만 정했다.
- **Review Focus**: 1·4 는 표의 설명과 창의 글. 2 는 `src/core`의 검사와 시험. 3 은 `record`의 호출 수로 본다. 5 는 확인 실행에서 게임의 화면과 견준다.
