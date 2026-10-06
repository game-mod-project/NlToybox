# 치트 메뉴 4단계: 인물·영주·인구·욕구 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 사용자가 준 목록의 "캐릭터", "캐릭터 생성/편집", "Lord", "인구", "음식/욕구"를 모드창에서 되게 한다: 고른 인물의 능력치·기분·충성·욕구·건강·나이·특성을 고치고,
영주 전원에 한꺼번에 걸고, 인구를 늘리고, 배고픔·피로·노화·사망을 끈다.

**Architecture:** 3나-2·3나-3 과 같은 길이다. 켜 둔 게임에서 `tree`·`statics`·`record`로 값의 자리와 그것을 바꾸는 게임의 함수를 찾고, `write`·`method`·`call`·`override`로 그 자리에서 풀어 본 뒤 굳힌다.
인물은 여럿이라 치트 표의 한 줄로는 안 된다. 경제 패널(`src/Economy.cpp`)의 꼴로 인물 패널을 둔다: `GameTick`이 인물의 목록과 값을 글로 떠 두고(스냅샷), 창은 명령만 남기고, 다음 틱이 실행하고 다시 읽어 확인한다.
수단은 A(게임의 스위치) → B(값 쓰기) → C(게임의 함수 부르기) → D(반환값 바꾸기)의 차례로 내려간다.

**Tech Stack:** C++20 모듈(Aurie v2.0.2, YYToolkit v5.0.0c, Dear ImGui), `tools/session.ps1`·`ask.ps1`, `tests/native`.

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(인물, 영주, 인구·욕구의 줄), §10(4), §14(원격 명령).

## 아는 것과 모르는 것

아는 것(출처):

- 인물은 `o_character`, 주민은 `o_dummy`다. 둘 다 `__soul`을 가진다(`refs/runtime/stage3-session1.answer.txt`의 `list`).
  `o_character`의 메서드: `get_skills`, `get_traits`, `get_minds`, `get_motive`, `get_inventory`, `get_equipment`, `get_family`, `is_player_faction`, `is_king`, `get_debug_name`, `get_uuid`. 변수 `starving_hours`.
- `__soul`의 자식(같은 답 파일): `__skills.__level.<combat|command|education|knowledge|management|manners|negotiation|oratory>`(수 2~9), `__moral`(90.77), `__moral_changed`, `__happiness_state`,
  `__motive`, `__minds`, `__traits`, `__fealty`, `__aging`(`__kid`, `__old`), `__born`(-76), `__pregnancy`, `__faction`, `__social_strata`(3), `__inventory.__money`, `__equipment`, `__name`(`"m_name.gwelts.450"`), `__uuid`.
  구성요소 `c_status`(`__is_dead`, `__is_sleeping`, …), `c_personality`, `c_battle`, `c_activity`.
- 이름 있는 스크립트와 직접 호출 수(`tools/re/script_calls.py`): `actor_trait_attach` 24, `actor_trait_detach` 16, `actor_mind_attach` 14, `actor_mind_detach` 7, `mind_get_by_system_name` 4, `character_gold_add` 18,
  `born_kid` 3, `debug_sleep` 1, `rebellion_debug_spawn_player_dummy` 5, `rebellion_debug_spawn_player_character` 2, `rebellion_debug_spawn_player_soldier` 1. `motive_change`·`motive_get`은 0곳(훅에 오지 않는다).
- `rebellion_debug_spawn_player_dummy`(`0x141942CE0`)는 인자를 둘로 맞춰 받는다(본문의 기계어: `argc`를 2 로 채우는 호출, 둘째는 `undefined`이면 `false`). 첫째의 형은 모른다.
- exe 의 이름(뜻은 추정): `add_skill_level`, `change_skill_level`, `skill_up`, `add_free_skill_points`, `trait_attach`, `trait_detach`, `cure_all_disease`, `cure_bleeding`, `kill_pain`, `set_moral`, `moral_add`, `moral_change`,
  `mind_debug_totally_happy`, `loyalty_change`, `debug_change_people_loyalty`, `soul_change_fealty`, `set_age`, `add_age`, `change_age`, `get_die_age`, `debug_pregnancy_next_stage`, `set_motive_restore`,
  `spawn_new_player_dummy`, `add_peasant`, `change_population`, `set_population`, `spawn_free_lord`, `debug_spawn_new_player_character`, `debug_spawner`, `debug_is_can_die_of_old_age`, `__debug_is_must_die_from_pregnancy`.
- 치트 표에 이미 있는 것: `rest_decrease`, `no_occupational_disease`(확인 전), 배율 `free_lord_stay`·`tavern_capacity`.

모르는 것(Task 1 이 잰다): 위 이름들이 어느 구조체의 메서드이고 무엇을 받는가. 능력치·기분을 바로 쓰면 게임과 인물 창이 따르는가(캐시, 시간마다 다시 셈). 욕구·건강·충성의 값이 어디에 있는가.
플레이어의 영주를 어떻게 가려내는가. 인구를 늘리는 게임의 길. 고친 값이 세이브에 남는가(인물의 값은 세이브의 자료다. 남는 것이 맞다).

## Global Constraints

- 게임 스크립트는 `gml_Script_` 이름으로만 부르고 훅을 건다. 인자의 수와 형을 본 함수만 부른다(`record`의 기록이나 기계어). 자주 불리는 함수에는 기록을 오래 걸어 두지 않는다. 훅 자리는 한 실행에 64개다.
- 불리언·수를 돌려주는 함수만 `override`한다. 위험한 호출(소환, 특성 붙이기, 죽음)은 요청마다 따로, 그 요청의 맨 뒤에 둔다.
- 게임을 켜는 횟수는 사용자에게 승인받은 만큼만. 켜기 전에 세이브의 사본을 뜬다. 실행 중에는 사용자가 하는 것을 로그와 값으로 따라간다(차례를 주고 기다리지 않는다).
- 인물의 값은 세이브에 남는다. 시험으로 바꾼 값은 끄기 전에 되돌리고, 되돌리지 못한 것은 적는다. 사용자의 세이브에 덮어 저장하지 않는다.
- `Verified`는 플레이에서 효과를 본 뒤에만 참. 확인 전의 `Hook`·`Custom`은 꺼진 채로 시작한다.
- 그리는 쪽에서 러너를 부르지 않는다. `RValue`를 틱 너머로 들지 않는다(인물은 주소와 `__uuid`의 글로 가리킨다. `inst:o_character:<n>`의 n 은 바뀔 수 있다).
- `refs/`, `backups/`, `downloads/`, `build/`는 커밋하지 않는다. 모드창의 글에는 한글과 라틴-1 만 쓴다.

## Review Focus

1. 고른 인물이 사라지거나(죽음, 떠남) `inst:o_character:<n>`이 다른 인물을 가리키게 됐을 때 엉뚱한 인물을 고치는 일(`__uuid`로 다시 확인한다).
2. 능력치·나이에 범위 밖의 수(음수, 아주 큰 수, 소수)를 썼을 때 게임이 배열 밖을 읽거나 인물 창이 깨지는 일.
3. "영주 전원"이 손님·포로·적 진영의 인물에게까지 걸리는 일.
4. 값을 계속 써 넣는 항목(배고픔 없음, 기분 고정)이 인물 수백 명에 틱마다 돌아 게임이 느려지는 일.
5. 소환·특성 붙이기를 인자가 틀린 채 불러 게임이 GML 오류로 끝나는 일.

---

### Task 1: 조사 실행 (게임 1회)

**Files:** Create `research/11-people.md`.

- [ ] **Step 1**: 배포(0.9.1), 세이브의 사본, `session.ps1 -Action start`, 적재 판정. 사용자가 영주와 주민이 있는 세이브를 불러온다.
- [ ] **Step 2**: 누가 있는가: `state`, `o_character`·`o_dummy`의 수, 인물마다 `__soul.__name`·`__uuid`·`__social_strata`·`__faction`, `method …is_player_faction`·`is_king`·`get_debug_name`.
- [ ] **Step 3**: 값의 나무와 메서드: `tree`와 `statics`를 `__soul`, `__soul.__skills`, `__motive`, `__minds`, `__traits`, `__fealty`, `__aging`, `__pregnancy`, `c_status`, `c_basic_personality`에.
  인구 쪽: `list inst:o_game_map_controller max=400`에서 이주·인구 관리자를 찾아 `statics`. `o_debug`의 `debug_*` 가운데 사람에 관한 것.
- [ ] **Step 4**: 이름으로 고른 후보에 `record`(특성·생각 붙이기, 능력치 바꾸기, 욕구, 이주)를 걸고 시간을 흘린다. 기록에서 꼴과 반환값을 본다.
- [ ] **Step 5**: 그 자리에서 풀어 본다: 능력치·기분·욕구·나이 쓰기(`write` 뒤 인물 창의 `shot`과 한 시간 뒤의 값), 게임의 함수 부르기(`method`), 스위치(`o_debug`). 위험한 호출은 맨 뒤에 하나씩.
- [ ] **Step 6**: 바꾼 값을 되돌리고 끈다(`session.ps1 -Action stop -Name stage4-session1`). 잰 것을 `research/11-people.md`에 적는다. 커밋.

### Task 2: 인물 패널과 영주 일괄 (Task 1 의 답으로 채운다)

- [ ] `src/core`에 러너와 무관한 부분을 시험과 함께 둔다(RED → GREEN): 인물 한 줄의 글(이름, 갈래, 값들)을 읽는 것, 명령의 꼴(누구의 무엇을 얼마로), 수의 범위를 당기는 것, "영주 전원"의 대상 고르기.
- [ ] `src/People.cpp`: 인물의 목록과 고른 인물의 값을 틱에서 떠 두고, 창의 명령(설정, 더하기, 최대, 회복)을 다음 틱에 실행한 뒤 다시 읽어 확인한다. 실행 전에 호출마다 로그를 남긴다.
- [ ] 영역 "인물"과 "영주"의 패널(`AreaInfo::Panel`), 원격 명령 `person …`(창 없이 같은 길을 태운다).

### Task 3: 인구·욕구 (Task 1 의 답으로 채운다)

- [ ] 치트 표의 항목: 게임의 스위치(`Toggle`), 수(`Number`), 판정·소비의 함수(`Hook`·`HookScale`), 인물마다 돌며 쓰는 것(`Custom`. 한 틱에 쓰는 수를 묶는다).
- [ ] 인구 추가는 게임의 함수로(기록으로 본 꼴 그대로). 패널의 단추와 원격 명령.

### Task 4: 검토, 확인 실행, 문서, 머지

- [ ] 독립 코드 검토(브랜치 전체) → Critical·Important 를 시험과 함께 고친다.
- [ ] 확인 실행(사용자 승인): 적재 판정, 인물 하나를 고쳐 인물 창과 행동으로 효과를 본다. 영주 일괄, 인구 추가, 욕구 항목. 사용자가 하는 것을 따라가며 값과 화면으로.
- [ ] `research/11`, `CLAUDE.md`, `README.md`, 스펙 §8·§10. 빌드·시험 넷·적재 판정 뒤 `git merge --no-ff feat/cheat-people` → `develop`.

### 결과 (2026-10-06)

- Task 1: 조사 실행 한 번(`stage4-session1`). 답은 `research/11-people.md`.
- Task 2·3: 인물 패널, 영주·사람 전원의 일괄, 표의 인구 항목 6개(모듈 0.10.0, 커밋 `18cae17`). 한 커밋으로 냈다.
- Task 4: 독립 검토(Critical 0, Important 2, Minor 16) → 고침(0.10.1, `f98ce66`). 확인 실행 둘(`stage4-session2`, `stage4-session3`): 세이브를 게임의 `load_save`로 직접 불러왔다
  (`tools/load-save.ps1`). 확인에서 죽음 캐시(`__is_dead` −4)의 버그를 찾아 고쳤다. 마지막 빌드에서 고친 것을 다시 봤다.

## Self-Review

- **스펙 대조**: §8 의 인물 세 줄(능력치·경험치, 건강·피로·스트레스·행복·충성·관계·욕구, 나이·성별·특성·소속·직업·장비)과 영주 두 줄이 Task 1·2 에, 인구·욕구 네 줄이 Task 1·3 에 있다.
  3나-3 에서 넘어온 임금·세금·유지비는 이 세이브에 값이 있으면 Task 1 Step 3 에서 함께 읽는다(없으면 다음 단계로 넘긴다).
- **빈칸**: Task 2·3 은 Task 1 의 답이 있어야 쓸 수 있다. 어느 함수를 부를지 지금 적으면 추측이다. 꼴만 정했다.
- **Review Focus**: 1·3 은 Task 2 의 시험(대상 고르기, uuid 확인). 2 는 표와 명령의 범위로 막고 시험한다. 4 는 Task 3 에서 틱마다 쓰는 수를 묶고 확인 실행에서 프레임을 본다. 5 는 Global Constraints 의 차례대로.
