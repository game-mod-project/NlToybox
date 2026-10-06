# 월드: 계절·세계 지도·자동 저장 — 실행 계획

**목표:** 가혹한 계절을 보이고 미루고 넘기고, 세계 지도의 안개를 걷고, 게임의 자동 저장을 끈다. 6단계에서 재지 못해 남긴 "계절·날씨, 지도 공개"를 채운다.

**스펙:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(월드: "지도·지역 공개, 자원 위치, 이동 제한"), §10 의 6단계 줄("계절·날씨, 지도 공개는 하지 못했다"). 사용자 요청 2026-10-06("인구(임신·출생), 월드 순으로").

**게임 실행:** 5번 승인(임신에서 남은 1 + 4). 4번 썼다.

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

## 재야 하는 것 (실행 2) — 쟀다. 결과는 아래 "결과"와 `research/25`

1. `is_save_disabled`를 켠 채 저녁 자동 저장의 시각(16:30~17:37)을 넘겨 새 파일이 생기지 않는가(시험 값이 없는 깨끗한 상태에서 깃발만 켠다).
2. 단계가 넘어가는 길: 시작 시각을 당겨 남은 시간을 0 가까이로 만들면 게임이 다음 단계로 넘기는가(매시 호출에서인가), 그때 `__set_phase`를 어떤 꼴로 부르는가, 가혹한 계절에 들어가면 무엇이 보이는가, 같은 방법으로 끝낼 수 있는가.
3. 세계 지도를 `open_map()`으로 열고(처음 부르는 함수) 화면을 받는다. `is_in_fog_of_war`·`is_visible`·`is_initial_area_visible`의 값을 바꿨을 때 화면이 어떻게 달라지는가.
4. 늑대의 최대 수 함수, 광산의 매장량 함수가 무엇을 받고 돌려주는가(게임이 부를 때의 기록).

## 파일

- `src/core/SeasonPlan.hpp/.cpp` (새로): 남은 시간의 글, 미루려 쓸 시작 시각과 그 결과의 글, 단계를 끝내려 쓸 시작 시각(0 보다 앞은 쓰지 않는다), 붙들기의 한 걸음, 쓴 뒤의 확인.
- `src/core/PlaceKey.hpp` (새로): 기억한 값이 어느 게임·어느 지도의 것인지(관리자 구조체의 주소 + 지도 관리 인스턴스).
- `src/core/WorldPlan.*`: `WorldAct`에 계절의 일 셋(보기, 미루기, 끝내기), 광산 매장량의 `KeepStock`·`StockBook`·`EnterStockPlace`.
- `src/World.cpp`: 월드 패널(계절의 상태와 단추), 계절 붙들기와 광산 매장량 붙들기의 틱.
- `src/core/CheatTable.*`: `reveal_map`(훅), `no_wolves`(수를 돌려주는 훅), `season_hold`·`mine_stock_hold`(Custom), `no_autosave`(Toggle, 그 실행에서만: `ThisRunOnly`), `fast_map_moving`(Toggle, 확인 전).
- `src/core/RemoteCommand.*`, `src/Remote.cpp`: `world season|season_delay|season_end`.
- `tools/load-save.ps1`: 실행 묶음이면 세이브를 불러온 뒤 게임의 저장을 끈다.

## 차례

1. [x] 실행 1: 계절·날씨·세계 지도의 구조와 함수의 꼴(읽기와 기록), 세계 지도를 열어 안개의 판정 셋을 견줬다. 도중에 게임이 다시 흘러 값을 쓰는 실험은 다음 실행으로 넘겼다.
2. [x] 시험 먼저 → 코어·명령·표·화면 구현(실행 1 이 켜져 있는 동안). 빌드와 시험 묶음 넷.
3. [x] 실행 2: 저장 끄기, 지도 공개, 계절의 넷, 늑대, 광산, 패널.
4. [x] 독립 검토 → 고쳤다(0.26.2). 그동안 실행 3(대조)으로 게임의 오류가 모듈과 무관함을 가렸다.
5. [x] 실행 4: 고친 판의 확인과 적재 판정.
6. [x] 문서(`research/25`, CLAUDE, README, 스펙, 이 계획의 결과) → develop 으로 머지.

## 검토에서 볼 것

- 게임 화면이 아닐 때, 계절 관리자를 읽지 못했을 때 쓰거나 부르지 않는가.
- 시작 시각에 쓰는 수가 유한한가. 지금보다 뒤의 시각을 쓰지 않는가.
- 계절을 붙드는 틱이 시각부터 보는가(오브젝트 이벤트마다 불린다). 끈 뒤에 쓰지 않는가.
- 계절의 상태가 세이브에 남는다는 것을 창이 말하는가. 재지 않은 것을 사실처럼 말하지 않는가.
- 안개의 훅이 불리언을 돌려주던 함수에 불리언을 돌려주는가.

## 결과

모듈 0.26.2. 잰 것은 `research/25-world.md`.

- **됐다(플레이에서 확인)**: 계절 패널(가혹한 계절까지 남은 시간, 하루 미루기, 지금 단계 끝내기)과 원격 `world season…`, `season_hold`, `reveal_map`, `no_wolves`, `mine_stock_hold`, `no_autosave`.
  `지금 단계 끝내기`로 가혹한 계절에 들어가고 나오는 것까지 봤다.
- **도구**: `tools/load-save.ps1`이 실행 묶음이면 세이브를 불러온 뒤 게임의 저장을 끈다. 그 뒤로는 자동 저장의 시각을 넘겨도 파일이 생기지 않는다.
- **확인 전으로 남은 것**: `fast_map_moving`, 앞서 있던 `fast_global_tasks`·`no_tree_growth`.
- **만들지 않은 것**: 비·눈·폭풍을 일으키기(게임의 함수에 넘길 구조체를 모른다), 가혹한 계절의 종류 바꾸기.
- **틀렸던 것**: 실행 1 에서 멈춰 둔 게임이 다시 흐른 것을 사용자가 푼 것으로 적었다. 가리지 못한 일이다(실행 2 의 끝에도 같은 일이 있었다).
  그래서 그 실행을 "사용자가 플레이하는 게임"으로 보고 지켜보기만 했고, 끌 때 사용자에게 알리고 껐다.
- **게임의 오류**: 실행 2·3 에서 게임이 제 오류 파일에 매복 이벤트의 오류를 쏟아 냈다(게임 시각 1분에 15건쯤). 대조 실행으로 모듈의 기능·배속·저장 끄기와 무관함을 가렸다.
  그 파일이 15 MB 쯤으로 커졌다(게임의 파일이다. 건드리지 않았다).
- **독립 검토**: Critical 0, Important 5, Minor 12, 판단 보류 7. 고친 것 — 붙들기 둘이 다른 세이브·다른 지도의 값으로 쓰지 않게(자리 표식), 미루기가 잘렸을 때의 답,
  저장 끄기는 그 실행에서만, 도움말이 잰 것만 말하게(광산이 0 이 될 때, 가혹한 계절 중의 붙들기, 이미 나온 늑대), 시험의 계절 이름을 지어낸 글로, 조사 기록.
- **판단**: 새 항목을 프리셋에 넣지 않았다(세이브에 남는 값을 쓰거나 난이도의 것이 아니다). 0 보다 앞의 시작 시각을 써야 하는 끝내기(게임 초반)는 거절한다(게임에서 돌려 보지 못했다).
  `reveal_map`이 켜진 동안 게임이 "본 것"을 세이브에 굳히는지는 모른다. 광산의 수는 값으로 바로 쓴다(게임의 광산 창은 보지 못했다).
- **미룬 것**(검토의 Minor): 쓰기에 실패한 뒤의 알림이 남는다, 쓰기 실패가 로그에 없다, "다음 정각"이 한 시간 늦을 수 있다는 말이 없다, 읽지 못한 수에 대한 거절의 글이 까닭을 섞는다,
  월드 영역의 `Panel` 표시, 단추로 한 계절 변경의 결과가 로그에 없다, 현지화 파일을 읽지 못한 까닭을 버린다, 빠진 경계 시험 몇.
