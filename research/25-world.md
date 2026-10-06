# 25. 월드: 계절, 세계 지도의 안개, 늑대, 광산, 게임의 저장

2026-10-06 · 게임 0.5588.9777.0 · 모듈 0.25.2(조사), 0.26.0 ~ 0.26.1(만든 것) · 세이브: 사용자의 `1_day_5_…`와 그 실행에서 생긴 아침 자동 저장(불러오기만 했다. 저장하지 않았다)

**요약**: 계절은 단계 넷으로 돌고 단계의 남은 시간은 시작 시각에서 셈한다. 시작 시각을 쓰면 가혹한 계절을 미루거나 붙들거나 끝낼 수 있다(게임이 매시 정각에 넘긴다).
세계 지도의 안개는 "이 지역이 보이는가"를 묻는 함수 하나의 답으로 걷힌다. 늑대는 "최대 수"에 0 을 답하면 나오지 않고, 광산의 매장량은 줄어든 수를 되돌려 쓰면 줄지 않는다.
게임의 디버그 깃발 `is_save_disabled`를 켜면 자동 저장이 생기지 않는다.

자료는 `refs\runtime\world-session*.answer.txt`, `world-session*.modlog.txt`(추적 안 함). 화면은 `refs\ui\x1-*.png`, `y2-*.png`.

## 계절

- 관리자: `inst:o_game_map_controller.__current_local_map.__season_manager`(ExtremeSeasonManager. 지도마다 하나).
  칸: `__current_phase`(0..3), `__start_phase_time`(게임 시각의 초), `__extreme_season`(`__name` `"drought"`, `__caption`은 현지화 열쇠 `extreme_season.<이름>`,
  `__modifiers`, `__season_mind`), `__biome`(`__season_phases[4]`의 `type` 0..3, `__cached_season_phase_duration_days` [4, 4, 4, 4]),
  `__is_begin_transition_event_sended`, `__is_display_event_sended`.
- 게임의 현지화 파일에는 가혹한 계절의 이름이 셋 있다(열쇠 `extreme_season.winter`·`drought`·`rains`). 모듈은 그 파일에서 읽어 보인다(레포에 글을 싣지 않는다).
- **인자의 수(본문의 기계어)**. 인자 없음: `get_remain_time_to_extreme_season`, `__get_remain_time_of_current_phase`, `get_remain_time_to_end_of_extreme_season`,
  `get_elapsed_time_from_season_start`, `is_extreme`, `get_extreme_season`, `get_current_phase`, `get_snow_progress`, `get_transition_info`, `__extreme_season_hour_call`,
  `__extreme_season_daily_call`, `step`. 하나: `__get_remain_time_to_phase`, `get_phase_duration`, `__get_next_phase`, `__get_previous_phase`, `__normalize_phase_start_time`, `override_biome`.
  셋까지 읽는다: `__set_phase`(게임은 수 하나로 부른다. 아래).
- **게임이 부르는 꼴(기록)**: `get_remain_time_to_extreme_season() -> 수`, `__get_remain_time_of_current_phase() -> 수`, `__get_remain_time_to_phase(2) -> 수`,
  `get_phase_duration(0) -> 345600`, `is_extreme() -> 불리언`, `get_extreme_season() -> 구조체`, `get_current_phase() -> 구조체`(프레임마다), `get_transition_info() -> undefined`.
  매시 정각에 `__extreme_season_hour_call() -> undefined`. `get_remain_time_to_end_of_extreme_season()`은 게임이 부르는 것을 보지 못했다(가혹한 계절이 아닐 때 직접 불러 수를 받았다).
- **셈**: 게임 시각 537321.83, 시작 374400 일 때 지나간 시간 162921.83, 이 단계의 남은 시간 182678.17(= 345600 − 지나간 시간), 가혹한 계절까지 528278.17(= 남은 시간 + 단계 하나),
  가혹한 계절이 끝날 때까지 873878.17(= 그것 + 단계 하나). **가혹한 계절은 단계 2 다**(게임이 `__get_remain_time_to_phase(2)`를 부른다).
- **남은 시간은 시작 시각에서 셈한다**: `__start_phase_time`을 86400 뒤로 쓰자 두 함수의 값이 86400 늘었고 되돌리자 돌아왔다.
- 화면의 "N일 차"는 06:00 에 넘어간다(05:48 에 5일 차, 12:03 에 6일 차). "N일 후 가뭄"은 가혹한 계절이 시작하는 날에서 오늘을 뺀 수였다.
- **단계는 매시 정각에 넘어간다**: 남은 시간이 다 지난 뒤에도(00:21) 단계는 그대로였고 다음 정각(01:00)에 게임이 `__set_phase(1) -> undefined`를 불렀다.
  새 단계의 시작 시각은 가장 가까운 지난 08:00 이 됐다(547200). 남은 시간의 함수는 다 지난 뒤 0 을 돌려준다(음수가 아니다).
- 세이브에 남는다: `"seasons":{"current_phase", "start_phase_time", "is_begin_transition_event_sended", "is_display_event_sended"}`.

### 만든 것과 본 것 (실행 2, 0.26.0)

| 한 것 | 본 것 |
|---|---|
| `world season` | "가혹한 계절(이름)까지 6일 1시간 (단계 1, 이 단계는 2일 1시간 남음)". 게임의 화면("6일 후")과 맞는다 |
| `world season_delay`(시작 시각 +86400. 지금보다 뒤로는 밀지 않는다) | 374400 → 460800, 남은 시간이 하루 늘었다. 모드창의 단추로도 됐다(547200 → 633600) |
| 붙들기(`cheat season_hold on`): 1초마다 시작 시각을 흐른 만큼 민다 | 남은 시간을 2시간 반으로 만든 뒤 켜고 네 시간을 돌렸다(정각을 네 번 지났다): 남은 시간·단계 그대로, `__set_phase` 0번. 끄자 01:00 에 넘어갔다 |
| `world season_end`(남은 시간을 60초로) 단계 1 에서 | 다음 정각에 `__set_phase(2)`. `is_extreme()` 참, "가혹한 계절(이름) 중입니다. 끝나기까지 3일 5시간". 게임의 알림 둘(곧 시작한다는 것, 밭을 일찍 거뒀다는 것) |
| 가혹한 계절 중의 미루기 | 거절한다(미루면 길어진다) |
| `world season_end` 가혹한 계절 중에 | 다음 정각에 `__set_phase(3)`. `is_extreme()` 거짓, "가혹한 계절까지 11일 4시간" |
| 붙들기를 켠 동안의 끝내기 | 거절한다 |

- 붙들기는 실제 시간 1초마다 쓴다. 게임의 배속이 높으면 그 사이에 게임 시각이 많이 흐른다(24배속이면 24분). 남은 시간이 그보다 적을 때 켜면 정각에 넘어갈 수 있다.
- 가혹한 계절의 효과(생산 속도, 기술 보정, 생각)는 재지 않았다. 본 것은 게임이 그 단계에 들어가고 나오는 것까지다.

## 날씨

- 관리자: `…__current_local_map.__weather_manager`(WeatherManager). 칸 `__is_rain_started`, `__is_snow_started`, `__last_rain_day`, `__rain_randomizer`, `__wind_direction`.
- 게임이 부르는 꼴: `is_rain_started() -> 0`(프레임마다), `get_days_without_rain() -> 수`, 매시 정각에 `hour_weather_handle(시) -> undefined`, 비가 올 때 `__start_rain(구조체, 수) -> undefined`(한 번 봤다. 구조체가 무엇인지 모른다).
- 인자의 수: `__start_rain` 셋까지, `__start_snow`·`hour_weather_handle`·`weather_factory` 하나, `__start_dust_storm`·`__start_snow_storm`은 들머리의 인자 맞춤 2.
- **비·눈을 일으키는 것은 만들지 않았다**(넘길 구조체를 모른다). 세이브의 열쇠: `last_rain_day`, `is_rain_started`.

## 세계 지도

- 관리자: `inst:o_global_map.__m_global_map`(GlobalMapManager). `o_game_map_controller.__province`가 아니다(그것은 영지다).
  칸: `__map_objects[96]`, `__map_objects_alived`, `__cached_array_of_initial_areas[25]`(GlobalMapArea), `__map_of_cached_initial_area_visibility`(ds_map: 지역 구조체 → 불리언),
  `__is_open`, `__debug_fast_moving`, `__is_debug_new_day_events_disabled`, `__ais[24]`.
- 지도 물체의 갈래(`__object_type`): 0 왕국의 도시(25), 2 움직이는 인물, 3 도적 기지, 4 마을(44), 5 불탄 마을 … 물체마다 `__initial_area`, `__is_new`, `__number_of_watch_towers`.
- 인자 없음(기계어): `open_map`, `close_map`, `is_can_open_or_close_map`, `initial_area_visible_clean_cache`, `__next_turn`, 물체의 `is_in_fog_of_war`·`is_visible`·`is_invisible`.
  `is_initial_area_visible`은 셋까지 읽고 게임은 **지역 구조체 하나**로 부른다(→ 불리언).
- `is_open()`은 초당 수천 번 불린다(기록을 걸어 두지 않는다). `__next_turn()`은 하루에 두 번쯤 불리고 그때 `initial_area_visible_clean_cache()`가 25번, `is_in_fog_of_war()`가 260번쯤 불렸다.
- **지도를 여닫는 함수를 직접 불러도 된다**: `open_map()`으로 `__is_open`이 참이 되고 화면에 세계 지도가 나왔다. `close_map()`으로 닫혔다. 게임은 멈춘 채였다.
  지도가 열린 동안 `is_visible()`과 `is_initial_area_visible(지역)`이 프레임마다 불린다.
- 지도의 확대: 지도가 열린 동안 `global.__current_camera.__zoom_ratio`(1.8 가장 가깝게 ~ 0.7 가장 멀리). 써 넣으면 화면이 따라온다(보기의 상태다. 시험에서만 썼다).
- **안개**: 이 세이브에서 지역 25개 가운데 3개가 보이고 물체 96개 가운데 83개가 안개 속이었다(`is_in_fog_of_war()`).

  | 돌려주는 값을 바꾼 것 | 넓게 본 화면 |
  |---|---|
  | `is_initial_area_visible` → 참 | 안개 속 지역이 밝아지고 마을의 이름과 지역의 자원 아이콘이 나온다. 그만두면 돌아간다 |
  | 물체의 `is_visible` → 참 | 이름과 아이콘은 나오지만 지역은 어두운 채다 |
  | 물체의 `is_in_fog_of_war` → 거짓 | 화면이 그대로다 |

  `is_initial_area_visible`을 바꿔도 물체의 `is_in_fog_of_war()`는 그대로 83개가 참이다(캐시를 지우는 게임의 함수를 부른 뒤에도). 게임의 다른 판정이 그것을 쓰는지는 모른다.
  치트 표의 `reveal_map`은 앞의 것 하나다. 켜고 꺼서 같은 화면을 봤다. 보이게 된 도시를 눌러 무엇을 할 수 있는지는 재지 않았다(게임 창을 누르지 않는 실행이었다).
- `__debug_fast_moving`(표의 `fast_map_moving`), `inst:o_debug.is_fast_action_task_on_global_map`: 재지 않았다.

## 늑대

- 관리자: `…__current_local_map.__wolf_manager`(WolvesManager). 게임이 밤에 한 번 `night_call() -> undefined`, `get_max_number_of_wolves() -> 11.1`을 부르고 `spawn_wolf(구조체) -> ref`로 만든다.
- `get_max_number_of_wolves`가 0 을 돌려주게 한 밤(둘)에는 `spawn_wolf`가 0번, 그러지 않은 밤(둘)에는 1번과 3번 불렸다. 표의 `no_wolves`(수를 돌려주던 함수에 수: `HookNumber`).

## 광산

- 관리자: `…__current_local_map.__mines_manager`(MinesManager). 매장량: `__mines_stock.<자리>`(구조체. 열쇠는 `"69_156"` 꼴, 값은 남은 수).
- 게임이 캘 때마다 `__change_mine_stock(ref, 1) -> undefined`를 부르고 그 수가 1 준다(22 → 16: 여섯 번). 세이브에 남는다(`mines_stock`).
- 표의 `mine_stock_hold`: 1초마다 줄어든 수를 줄기 전의 수로 되돌려 쓴다. 켠 뒤 게임이 네 번 더 캤는데 16 그대로였다. 바닥난 광산을 되살리는지는 재지 않았다.

## 게임의 저장

- `inst:o_debug.is_save_disabled`(불리언), `auto_save_timer`(0). 세이브 파일에 그 열쇠는 없다.
- **켠 채 자동 저장의 시각을 세 번(저녁, 아침, 저녁) 넘겼는데 세이브 폴더에 새 파일이 생기지 않았다.** 끈 채였던 실행 1 에서는 아침(06:00)과 저녁에 하나씩 생겼다.
  직접 하는 저장이 막히는지는 재지 않았다.
- 표의 `no_autosave`. **실행 묶음에서는 세이브를 불러온 뒤 이것부터 켠다**: 그 뒤로는 시험 값이 든 채 자동 저장의 시각을 넘겨도 파일이 생기지 않는다.

## 시간을 멈추는 것

- `__set_warp(0)`으로 멈춘 게임이 얼마 뒤 배속 1 로 다시 흐르는 일이 세 번 있었다(실행 1 에서 두 번, 실행 2 의 끝에 한 번). 누가 풀었는지 가리지 못했다
  (실행 1 에서는 사용자가 모드창을 여닫았다. 게임이 스스로 속도를 다시 건 것일 수도 있다). **멈춰 둔 것에 기대어 시험 값을 남겨 두지 않는다.** 저장 끄기를 먼저 켠다.
- 실행 1 에서 그렇게 흘러 06:00 을 넘겼고 게임이 아침 자동 저장을 만들었다. 그때 게임에 시험 값은 없었다.

## 게임이 적는 오류: 매복

- 실행 2 와 3 에서 게임의 오류 파일(`catched_errors_<버전>.txt`)에 잡힌 오류가 쏟아졌다: `o_game_map_controller`의 스텝, "trying to index variable that is not an array",
  이벤트 `ambush_squad`, 스택은 매복 관리자 → 매복 부대 → 이벤트 → 사람마다의 처리 → `actor_is_outdoors` → `map_get_building_id`.
  7일차 오후부터 났다. 그때 지도 입구에 적 부대 둘이 서 있었고 매복 관리자의 `__array_of_ambush`에 하나가 있었다.
- **모듈의 월드 기능 때문이 아니다**(대조 실행 3: 같은 세이브, 저장 끄기 깃발만 켜고 아무 명령도 보내지 않았다. 7일차 17:50 까지 1,634건, 8일차 01:30 까지 6,323건).
  **배속 때문도, 저장 끄기 깃발 때문도 아니다**: 배속 1 로 게임 시각 40분에 598건, 깃발을 끄고 40분에 592건.
- 오류는 게임이 잡아서 적는 것이고 게임은 계속 돌았다. 그 파일은 하루 사이에 15 MB 쯤으로 커졌다(게임의 파일이다. 도구는 읽기만 한다).
- 실행 2 에서 한 번: `o_time_controller`의 스텝, "I32 argument is undefined", 이벤트 `action_task_created`(세계 지도의 도시가 행동을 만들 때). 원인을 가리지 못했다.

## 하지 않은 것

- 비·눈·폭풍을 일으키기(함수에 넘길 구조체를 모른다), 가혹한 계절의 종류 바꾸기(`override_biome`의 인자를 보지 못했다).
- 세계 지도의 이동·행동 속도(깃발 둘은 표에 있지만 재지 않았다), 감시탑의 수(`__number_of_watch_towers`)와 시야의 관계.
- 지도 물체의 안개 판정(`is_in_fog_of_war`)을 바꿨을 때 게임의 무엇이 달라지는지.
