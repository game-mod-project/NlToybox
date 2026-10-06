# 월드: 계절·세계 지도·자동 저장 — 실행 계획

**목표:** 가혹한 계절을 보이고 미루고 넘기고, 세계 지도의 안개를 걷고, 게임의 자동 저장을 끈다. 6단계에서 재지 못해 남긴 "계절·날씨, 지도 공개"를 채운다.

**스펙:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(월드: "지도·지역 공개, 자원 위치, 이동 제한"), §10 의 6단계 줄("계절·날씨, 지도 공개는 하지 못했다"). 사용자 요청 2026-10-06("인구(임신·출생), 월드 순으로").

**게임 실행:** 5번 승인(임신에서 남은 1 + 4).

## 근거 (실행 1, `world-session1`. `research/25`)

| 쓰는 것 | 어디서 봤나 |
|---|---|
| 계절 관리자 `inst:o_game_map_controller.__current_local_map.__season_manager`(ExtremeSeasonManager): `__current_phase`, `__start_phase_time`, `__extreme_season.__name`·`__caption`, `__biome.__season_phases[4]`(`type` 0..3) | `list`, `statics`, `tree` |
| `get_remain_time_to_extreme_season()`, `__get_remain_time_of_current_phase()`, `get_remain_time_to_end_of_extreme_season()`, `get_elapsed_time_from_season_start()`, `is_extreme()` 인자 없음. 수·불리언 | 기계어(argc 를 옮기지 않는다), 게임이 부른 기록, 직접 부른 값 |
| 남은 시간은 `__start_phase_time`에서 셈한다: 시작을 하루 뒤로 쓰자 남은 시간 둘이 86400 늘었고 되돌리자 돌아왔다 | `write` 와 함수의 값 |
| 가혹한 계절은 단계 2 다: 게임이 `__get_remain_time_to_phase(2)`를 부른다. 단계마다 4일(`get_phase_duration(0) -> 345600`) | 기록 |
| 매시 정각에 게임이 `__extreme_season_hour_call()`과 `hour_weather_handle(시)`를 부른다 | 기록(06, 07, 08, 09시) |
| 계절의 상태는 세이브에 남는다: `"seasons":{"current_phase", "start_phase_time", …}` | 세이브 파일 |
| 세계 지도 `inst:o_global_map.__m_global_map`(GlobalMapManager): `__map_objects[96]`, `__is_open`, `__debug_fast_moving`. 지도 물체의 `is_in_fog_of_war()`, `is_visible()`, `is_invisible()` 인자 없음, 불리언 | `statics`, 기계어, 직접 부른 값(96개 가운데 83개가 안개 속) |
| `is_initial_area_visible(지역 구조체) -> 불리언` | `is_visible()`을 부르자 게임이 그 꼴로 불렀다 |
| `open_map()`, `close_map()`, `is_can_open_or_close_map()` 인자 없음 | 기계어. **앞의 둘은 아직 부르지 않았다** |
| `inst:o_debug.is_save_disabled`(불리언), 세이브에 그 열쇠는 없다 | `list`, 세이브 파일. **효과는 재지 않았다** |

## 재야 하는 것 (실행 2)

1. `is_save_disabled`를 켠 채 저녁 자동 저장의 시각(16:30~17:37)을 넘겨 새 파일이 생기지 않는가(시험 값이 없는 깨끗한 상태에서 깃발만 켠다).
2. 단계가 넘어가는 길: 시작 시각을 당겨 남은 시간을 0 가까이로 만들면 게임이 다음 단계로 넘기는가(매시 호출에서인가), 그때 `__set_phase`를 어떤 꼴로 부르는가, 가혹한 계절에 들어가면 무엇이 보이는가, 같은 방법으로 끝낼 수 있는가.
3. 세계 지도를 `open_map()`으로 열고(처음 부르는 함수) 화면을 받는다. `is_in_fog_of_war`·`is_visible`·`is_initial_area_visible`의 값을 바꿨을 때 화면이 어떻게 달라지는가.
4. 늑대의 최대 수 함수, 광산의 매장량 함수가 무엇을 받고 돌려주는가(게임이 부를 때의 기록).

## 파일 (실행 2 의 결과로 고친다)

- `src/core/SeasonPlan.hpp/.cpp` (새로): 남은 시간의 글, 단계를 끝내려 쓸 시작 시각, 미루려 쓸 시작 시각, 붙들어 둘 때 쓸 시작 시각.
- `src/core/WorldPlan.*`: `WorldAct`에 계절의 일(다음 단계로, 미루기)을 더한다.
- `src/World.cpp`: 월드 패널(계절의 상태와 단추), 계절 붙들기의 틱.
- `src/core/CheatTable.cpp`: 안개 걷기(훅), 계절 붙들기(Custom), 자동 저장 끄기(Toggle), 세계 지도의 빠른 이동(Toggle). 확인은 실행에서 본 것만.
- `src/core/RemoteCommand.*`, `src/Remote.cpp`: `world season …`.

## 차례

1. [x] 실행 1: 계절·날씨·세계 지도의 구조와 함수의 꼴(읽기와 기록). 사용자가 그 실행에서 플레이를 이어 가서 값을 쓰는 실험은 다음 실행으로 넘겼다.
2. [ ] 실행 2: 위의 "재야 하는 것".
3. [ ] 시험 먼저 → 코어·명령·표·화면 구현. 빌드와 시험 묶음 넷.
4. [ ] 독립 검토 → 고친다.
5. [ ] 실행 3·4: 명령과 화면으로 확인. 마지막 DLL 의 적재 판정.
6. [ ] 문서(`research/25`, CLAUDE, README, 스펙, 이 계획의 결과) → develop 으로 머지.

## 검토에서 볼 것

- 게임 화면이 아닐 때, 계절 관리자를 읽지 못했을 때 쓰거나 부르지 않는가.
- 시작 시각에 쓰는 수가 유한한가. 지금보다 뒤의 시각을 쓰지 않는가.
- 계절을 붙드는 틱이 시각부터 보는가(오브젝트 이벤트마다 불린다). 끈 뒤에 쓰지 않는가.
- 계절의 상태가 세이브에 남는다는 것을 창이 말하는가. 재지 않은 것을 사실처럼 말하지 않는가.
- 안개의 훅이 불리언을 돌려주던 함수에 불리언을 돌려주는가.
