# 02 — 새 게임 상태에서 잰 것 (데이터 오버레이 단계 0b)

조사일 2026-10-05. 대상: Norland `0.5588.9777.0`. 게임을 켠 횟수: 2.

- 실행 1 (03:32~03:37): 새 게임을 시작하지 않았다. 덤프 넷이 모두 메인 메뉴의 것이다.
- 실행 2 (08:58~09:03): 새 게임을 시작해 게임 화면에서 두었다가 껐다. **아래의 답은 실행 2에서 나왔다.**
  쓴 덤프는 메뉴 덤프(`menu`, 순번 1, 08:59:20)와 게임 안의 마지막 덤프(`late1`, 순번 6, 09:03:16)다.

덤프와 로그는 `refs/runtime/stage0b-run1.*`, `stage0b-run2.*`에 있다(추적 안 함).
다시 재려면 `tools/probe.ps1 -Request tools/probes/stage0b-run2.txt -Out refs/runtime/<이름>.json`.

## 답

바꾼 값 다섯 개를 런타임에서 찾았다. 값은 게임을 켤 때 읽힌다.

| 질문 | 답 | 근거 (경로 = 값) |
|---|---|---|
| 4. `debug_params.json`의 값이 런타임에 있는가 | **그렇다** | `budget_money` 2000 → 2345. 메뉴: `ds_map[150].budget_money = 2345`. 게임 안: `o_province_controller.default_budget_money = 2345` |
| 4. `battle_params.json` | **그렇다** | `battle_dodge_base` 20 → 23. 메뉴: `ds_map[285].battle_dodge_base = 23`. 게임 안: `o_debug.battle_dodge_base = 23` |
| 4. `gameplay_variables.json` | **그렇다** | `initial_budget` 700 → 745. 메뉴: `ds_map[80].initial_budget = 745`, `global.__gameplay_vars.global_map_ai_economy_initial_budget = 745`. 게임 안: AI 도시 약 30곳의 `…__cached_town.__economy.__initial_wealth = 745` |
| 4. `director_params.json` | **그렇다** | `group_cooldown_days.EPIDEMY` 7 → 3391. `ds_map[257].EPIDEMY.__cooldown_days = 3391`, `o_data.__game_director_events_data.__params.group_cooldown_days.EPIDEMY = 3391` (메뉴와 게임 안 모두) |
| 4. `knowledge\technology\cultural_knowledge\addiction_resist.json` | **그렇다** | `population` 110 → 3767. `ds_map[193].addiction_resist.__available_parameters[0].__population = 3767`, `o_data.__knowledge_data.__array_of_population_tiers[6] = 3767` (메뉴와 게임 안 모두) |
| 5. 언제 읽히는가 | **게임을 켤 때** | 다섯 값 모두 새 게임을 시작하기 전인 메뉴 덤프에 있다. 게임을 시작하면 일부가 게임 오브젝트(`o_debug`, `o_province_controller`)로 옮겨진다 |
| 6. "게임 안"을 무엇으로 알아볼 수 있는가 | **오브젝트의 유무와 전역 값으로 알아볼 수 있다. 룸으로는 안 된다** | 아래 "게임 안을 알아볼 수 있는 신호" |

**"런타임에 있다"와 "게임 규칙이 그 값을 쓴다"는 다르다.** 이번에 효과까지 본 것은 하나다.

- `initial_budget`: AI 도시의 초기 재산이 745가 됐다. 효과가 보인다.
- `budget_money`: `default_budget_money`가 2345가 됐다. 그런데 사용자가 화면에서 본 시작 금화는 **3000**이다.
  바꾸기 전 값(2000)도 바꾼 값(2345)도 아니다. 이 값이 시작 금화를 그대로 정하지는 않는다. 3000의 출처는 확인하지 못했다.
- 나머지 셋: 값이 게임 오브젝트나 데이터 구조에 올라온 것까지만 봤다. 그 값으로 게임이 달라지는지는 재지 않았다.

## 어느 덤프를 게임 안의 것으로 봤는가

세 가지가 서로 맞았다.

- 사용자: 새 게임을 시작해 게임 화면에 들어갔고 시작 금화가 3000으로 보였다고 알렸다.
- 상태 기록: `o_character`, `o_building`, `o_creature`가 t=163.1(09:01:03)에 생겼다.
- 덤프: 순번 4(09:01:41), 5(09:02:28), 6(09:03:16)이 그 뒤에 시작했다. 셋 모두 `present`에 `o_character:4`, `o_building:2`가 있고
  `o_main_menu`가 없다. 게임은 09:03:42에 꺼졌다.

실행 1에서는 사용자가 새 게임을 시작하지 않았다고 확인했고, 기록도 그랬다(t=46.5 뒤로 상태 변화가 없고, 새로 실행된 이벤트 코드가
종료 때의 것뿐이며, 뒤 덤프의 방문 수가 메뉴 덤프와 같았다).

## 확인의 결과

`dump_tool.py controls`를 실행 2의 덤프 넷에 돌렸다. 넷 모두 전부 `ok`다. 메뉴 덤프와 게임 안 덤프(`late1`)의 출력:

```
menu
ok   expect global.__gameplay_vars.global_map_ai_economy_initial_budget: want 745.0
ok   selfcheck: {'types': True, 'made': True, 'ds_map': True, 'ds_list': True}
ok   ds_range: highest id=332 scanned below 100000
ok   truncated: sections=[]
ok   hits_cut: sections=[]
ok   enumeration: (failed, short) by section={}
ok   instances: objects with members=10

late1
ok   expect global.__gameplay_vars.global_map_ai_economy_initial_budget: want 745.0
ok   selfcheck: {'types': True, 'made': True, 'ds_map': True, 'ds_list': True}
ok   ds_range: highest id=1467 scanned below 100000
ok   truncated: sections=[]
ok   hits_cut: sections=[]
ok   enumeration: (failed, short) by section={}
ok   instances: objects with members=25
```

## 게임 안을 알아볼 수 있는 신호

실행 2의 기록이다. 시각 t는 모듈이 적재된 뒤의 초다.

| t | 일어난 일 | 기록 |
|---|---|---|
| 0.0 | 로딩 시작 | `room=rm_game`, `o_game_launch`, `o_menu_background_render`, `o_sounds_controller`, `o_debug_timer`, `imgui` |
| 13.2 | | `+o_steam_controller +o_data` |
| 50.4 | **메인 메뉴** | `+o_camera_controller +o_time_controller +o_main_menu -o_game_launch`. `__game_load_operator.__game_is_loading` 1 → 0 |
| 85.8 | **새 게임 설정 화면** | `__new_game_initializer.__is_active` 0 → 1. `+o_global_map_camera -o_menu_background_render` |
| 87.8~95.0 | 설정 단계 | `__new_game_initializer.__current_step` 1 → 4 |
| 102.5 | **게임 만들기 시작** | `-o_main_menu`(Destroy 이벤트). `+o_debug +o_game_map_controller +o_construction_controller +o_production_manager +o_service_manager +o_dialogue_controller` 등. `__game_is_loading` 0 → 1, `__game_load_type` -4 → 0 |
| 103.1 | | `+o_province_controller +o_local_map_camera +o_global_map` |
| 139.2 | 맵 만들기 끝 | `-o_menu_background_render`. `__current_step` 5. `__game_is_loading` 1 → 0 |
| 163.1 | **게임 화면** | `+o_character +o_dummy +o_parent_actor +o_building +o_creature` |
| 163.6 | 새 게임 초기화 끝 | `__new_game_initializer.__is_active` 1 → 0, `__current_step` 0 |
| 그 뒤 | 게임 진행 | `o_gui_popup_message`, `o_gui_popup_icon`, `obj_muffel`이 몇 초마다 생겼다 사라진다 |

후보마다의 값:

| 신호 | 메인 메뉴 | 설정 화면 | 게임 만드는 중 | 게임 화면 |
|---|---|---|---|---|
| 룸 이름 | `rm_game` | `rm_game` | `rm_game` | `rm_game` |
| `o_time_controller`, `o_camera_controller` | 있음 | 있음 | 있음 | 있음 |
| `o_main_menu` | 있음 | 있음 | 없음 | 없음 |
| `o_province_controller`, `o_debug` | 없음 | 없음 | 있음 | 있음 |
| `o_character`, `o_building` | 없음 | 없음 | 없음 | 있음 |
| `__new_game_initializer.__is_active` | 0 | 1 | 1 | 0 |
| `__game_load_operator.__game_is_loading` | 0 | 0 | 1 | 0 |

- **룸은 신호가 아니다.** 처음부터 끝까지 `rm_game`이다.
- `o_time_controller`와 `o_camera_controller`는 메뉴에서 이미 있다. 신호가 아니다.
- **게임 화면**: `o_main_menu`가 없고 `o_character`가 있다. 메뉴, 설정 화면, 게임 만드는 중에는 거짓이고 t=163.1부터 참이다.
- **값이 게임 오브젝트로 옮겨지는 때**: `o_debug`와 `o_province_controller`가 생기는 t=102.4~103.1이다. 게임 화면보다 약 60초 이르다.
  하위 프로젝트 2가 값을 쓸 때 어느 쪽을 기준으로 할지는 쓰려는 값이 어디 있느냐에 달렸다.
- 자주 바뀌는 것(`o_gui_popup_message`, `o_gui_popup_icon`, `obj_muffel`, `o_gui_deep_social_bubble`)은 조건에 쓰지 않는다.

**세이브를 불러올 때의 흐름은 재지 않았다.** 위는 새 게임 한 번의 기록이다.

## 파일 키와 런타임 경로의 대응

값을 바꿔서 확인한 것은 굵게 적었다. 나머지는 이름과 값이 파일과 같다는 것까지만 봤다.

| 파일의 키 | 게임을 켠 뒤 (메뉴) | 게임을 시작한 뒤 |
|---|---|---|
| `debug_params.json` **`budget_money`** | `ds_map[150].budget_money` | `o_province_controller.default_budget_money` |
| `debug_params.json` `production_cost` (35개) | `ds_map[150].production_cost` → `ds_map[143]` | `o_province_controller.production_cost` (배열 39) |
| `debug_params.json` `product_count` (3개) | `ds_map[150].product_count` → `ds_map[149]` | `o_province_controller.default_resource_count` (배열 39) |
| `debug_params.json` `building_resources` (101개) | `ds_map[150].building_resources` → `ds_map[144]` | 찾지 못했다(이름이 달라졌거나 긴 배열 안이다) |
| `debug_params.json` `building_duration_factor` | `ds_map[150].building_duration_factor` | `o_debug.debug_building_duration_factor` |
| `debug_params.json` `fair_trade` | `ds_map[150].fair_trade` → `ds_map[148]` → `fair_trade_purchase` `ds_map[146]`, `fair_trade_sale` `ds_map[147]` (39개씩) | `o_game_map_controller.__trade_manager.__fair_trade_default_price_buy`, `…_sell` (배열) |
| `debug_params.json` `romantic_slowdown_factor`, `romantic_decrease_factor`, `matching_by_opinion`, `dialogue_chitchat_count`, `dialogue_random_threshold` | `ds_map[150]` | `o_debug.debug_romantic_slowdown_factor`, `debug_romantic_decrease_factor`, `debug_matching_by_opinion_value`, `debug_dialogue_chitchat_count`, `debug_dialogue_random_threshold` |
| `battle_params.json` **`battle_dodge_base`** | `ds_map[285].battle_dodge_base` | `o_debug.battle_dodge_base` |
| `battle_params.json` `battle_dodge_shift_better`, `battle_dodge_shift_worse`, `soldier_hiring_price_skill_factor`, `number_of_most_painful_mind` | `ds_map[285]` | `o_debug`의 같은 이름 |
| `gameplay_variables.json` **`global_map.ai_economy.initial_budget`** | `ds_map[80].initial_budget`, `global.__gameplay_vars.global_map_ai_economy_initial_budget` | AI 도시의 `__cached_town.__economy.__initial_wealth` |
| `director_params.json` **`group_cooldown_days.EPIDEMY`** | `ds_map[257].EPIDEMY.__cooldown_days`, `o_data.__game_director_events_data.__params.group_cooldown_days.EPIDEMY` | 같은 자리 |
| 지식 파일 **`available_parameters[0].population`** | `ds_map[193].addiction_resist.__available_parameters[0].__population`, `o_data.__knowledge_data.__array_of_population_tiers[6]` | 같은 자리 |

- ds_map의 번호(150, 285, 80, 257, 193)는 실행 1과 실행 2에서 같았다. 게임 버전이 바뀌면 달라질 수 있다(재지 않았다).
  번호에 기대지 말고 키 이름으로 찾아야 한다.
- 데이터 파일은 ds_map으로 읽힌다. 단계 0의 찾기가 ds_map 안을 보지 않아서 `debug_params.json`과 `battle_params.json`을
  "확인하지 못함"으로 적었던 것이다.
- `o_debug`에는 설정값 말고도 스위치가 많다(변수 127개). 예: `is_debug_enabled` = 0, `is_instant_build_buildings` = 0,
  `is_can_build_all_buildings` = 0, `is_resources_edit_mode` = 0, `is_save_disabled` = 0, `is_disable_dodge` = 0.
  이름과 값만 봤다. 바꾸면 어떻게 되는지는 재지 않았다. 하위 프로젝트 2와 3의 단서다.

## 찾기가 본 범위

| | 메뉴 (`menu`) | 게임 안 (`late1`) |
|---|---|---|
| 전역 변수 | 4,737 | 4,739 (`__debug_path_result`, `is_gc_collect_after_save_enabled`가 늘었다) |
| 전역 찾기: 방문 / 구조체·배열 / 깊이 한도에서 멈춤 | 40,171 / 16,286 / 2,029 | 62,145 / 16,432 / 23,810 |
| ds 찾기: 방문 / 구조체·배열 / 깊이 한도에서 멈춤 | 1,145,502 / 269,777 / 985 | 1,722,916 / 394,503 / 61,603 |
| ds_map 수 (가장 큰 번호) | 333 (332) | 1,468 (1,467) |
| ds_list 수 (가장 큰 번호) | 277 (276) | 307 (306) |
| 인스턴스 찾기: 방문 / 들어간 인스턴스 | 10,015 / 11 | 83,279 / 38 |
| 맞은 것 (전역 / ds / 인스턴스) | 8 / 232 / 11 | 8 / 262 / 20 |
| 길어서 들어가지 않은 것 (전역 / ds / 인스턴스) | 200개 넘음 / 32 / 6 | 200개 넘음 / 32 / 47 |
| 한도에 걸림, 히트를 다 못 적음, 열거가 끊김 | 없음 | 없음 |

보지 않는 것: 길이 64를 넘는 배열과 ds_list, 키가 2만을 넘는 ds_map, ds_grid·stack·queue·priority, 오브젝트마다 16개를 넘는
인스턴스, 깊이 6 아래(게임 안에서 깊이 한도에서 멈춘 것이 많다), 비활성 인스턴스, 문자열로 저장된 수.
값으로 맞은 것에는 우연한 일치가 섞여 있다(스프라이트 번호, 좌표, 구독자 번호). 경로의 이름으로 가렸다.

## 이번에 처음 잰 것

스펙 §3.7의 "모른다"에 대한 결과다.

| 모르던 것 | 결과 |
|---|---|
| 메뉴와 게임 안에서 어떤 오브젝트가 있는가. 룸이 바뀌는가 | 위의 표. 룸은 바뀌지 않는다 |
| 값이 든 자리. 수 하나로 된 전역이 ds_map 번호인가 | 값은 ds_map과 `o_data` 인스턴스에 있다. 전역 `__map_of_cached_loaded_files__`(19) 등이 ds_map 번호인지는 직접 확인하지 않았다 |
| `CCode`의 배치가 이 러너와 맞는가 | 맞는다. 코드 이름 121개를 읽었고 못 읽은 것은 0이다 |
| 덤프 한 번에 걸리는 시간 | 메뉴 1.6초, 게임 안 2.0~2.1초 |

그 밖에:

- ds 형 상수 1(map)과 2(list)는 이 러너에서 맞다. 만들면 `ds_exists`가 참이고 지우면 거짓이었다.
- ds 번호는 0부터 빈틈없이 배정돼 있었다(가장 큰 번호 + 1 = 개수).
- 룸 이름(`GetBuiltin("room")` + `room_get_name`), 인스턴스 수와 변수(`instance_number`, `instance_find`,
  `variable_instance_get_names`, `variable_instance_get`), ds 함수들이 모두 동작했다. 게임이 죽지 않았다.
- 멤버 열거가 끊긴 구조체는 없었다(러너가 말한 멤버 수와 본 수가 모두 같았다).
- `instance_number`가 자식을 포함한다는 것이 보였다: `o_parent_actor` 11 = `o_character` 4 + `o_dummy` 7.
- 런타임의 오브젝트는 65개다. `data.win`의 `OBJT`에서 센 것은 64개였다. **하나가 어디서 오는지는 확인하지 못했다.**
- 사용자가 게임을 끄기 직전에 메인 메뉴로 나갔다(게임 로그에 09:03:38 `Menu Opened`).

## 세이브 폴더

실행 전 사본: `backups\saves\20261005-033145` (67개). 두 실행 뒤의 차이:

- 새로 생김 31개: `generator\debug\generation_001 d2026-10-05 t오전 9.00.18 v0.5588.9777.0\` 아래의 지도 생성 디버그 그림과 `data.json`.
- 바뀜 27개: `generator\debug\raw\` 아래의 같은 종류.
- 바뀜 3개: `catched_errors_0.5588.9777.0.txt`, `encyclopedia_save.json`, `game_settings.json`.
- **새 세이브는 없다.** `saves\`에는 `steam_autocloud.vdf`뿐이고, 게임 로그에 `Save game:` 줄이 없다.

아무것도 지우지 않았다.

## 단계 1에 주는 결론

- **다섯 파일 모두 오버레이의 대상이 된다.** 값이 게임에 읽힌다.
- **프리셋을 입힌 뒤에는 게임을 다시 켜야 한다.** 값은 게임을 켤 때 읽힌다. 게임이 켜진 채 파일을 바꾸면 다시 읽는지는 재지 않았다.
- 카탈로그에는 "런타임에 올라오는 것을 확인했다"와 "게임이 달라지는 것을 확인했다"를 따로 적어야 한다.
  지금 뒤쪽에 드는 것은 `initial_budget` 하나다.
- `budget_money`는 시작 금화가 아니다(화면에는 3000이 보였다). MVP의 "Gold"를 이 키로 풀 수 있는지는 3000의 출처를 찾은 뒤에 정한다.

## 확인하지 못한 것

- 시작 금화 3000이 어디서 오는가. 요청에 3000을 찾는 줄이 없었다. 덤프의 한 단계 목록에는 3000이 없다.
- 바닐라에서 시작 금화가 얼마인가(2000인지 3000인지).
- 세이브를 불러올 때의 흐름과 신호.
- 게임이 켜진 채 파일을 바꾸면 다시 읽는가.
- 바뀐 값이 세이브에 굳는가.
- `battle_params.json`, `director_params.json`, 지식 파일, `debug_params.json`의 값이 게임을 실제로 바꾸는가.
- `o_debug`와 `o_province_controller`의 변수를 런타임에 바꾸면 게임이 그 값을 쓰는가.
- `building_resources`가 게임 안에서 어디로 가는가.
- 런타임의 65번째 오브젝트.
- 길이 64를 넘는 배열과 깊이 6 아래에 무엇이 있는가.
