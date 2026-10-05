# 14. 외교·종교·이벤트·월드

치트 메뉴 6단계의 조사. 게임 버전 `0.5588.9777.0`, 모듈 0.13.1, 2026-10-06.
실행 묶음 `stage6-session1`(답: `refs/runtime/stage6-session1.answer.txt`). 세이브 "아덴"(4일차 06:00). 멈춘 채 읽기만 했다.
이 실행은 내가 인자 없이 부른 충성도 함수 때문에 GML 오류로 끝났다(`research/13`의 끝). 기록(`record`)은 받지 못했다.

아래에서 `gm` = `inst:o_game_map_controller`, `pv` = `gm.__province`. 인자 수는 본문의 기계어로 읽었다(`research/11`의 방법):
"없음"은 본문이 `argc`를 옮기지 않는 것, "N 까지"는 들머리의 인자 맞춤이 N 인 것이다(**생략해도 되는지는 모른다**).

## 관리자와 메서드

| 자리 | 생성자 | 제 메서드 가운데 쓸 만한 것 (인자 수) |
|---|---|---|
| `gm.__factions_manager` | `FactionsManager` | `get_array_of_factions`(2 까지), `get_faction`(1), `get_agreement_matrix`·`get_allies_matrix`·`get_factions_gui_struct`(없음) |
| `…__factions_manager.__player_faction` | `Faction` | `get_relation_with`, `is_enemy_with`, `is_friend_with`, `is_deadly_enemy_with`, `force_neutrality`, `attach_opinion_about_faction`, `detach_opinion_about_faction`, `is_declared_peace_with`, `add_to_alliance_with_leader`, `vassalise_by_faction`, `get_strength`, `order_66` (모두 재지 않았다) |
| `pv.__religiosity_manager` | `ReligiosityManager` | `debug_force_send_bishop`·`get_bishop_opinion`·`get_fanatics_ratio`·`is_has_bishop`(없음), `attach_opinion_to_bishop`(인자를 받는다) |
| `pv.__prophecy_manager` | `ProphecyManager` | `is_has_prophecy`(없음), `try_to_start_prophecy`·`is_prophecy_available_to_start`(1) |
| `pv.__ceremony_manager` | `CeremonyManager` | `add_ceremony`, `set_today_ceremony`, `is_wedding_today` … |
| `gm.__game_director` | `GameDirector` | `get_current_phase_type`·`is_current_phase_threat`·`on_next_day`·`handle_cooldowns`(없음), `is_event_cooldown_is_ready`·`is_event_group_cooldown_is_ready`·`get_event_cooldown_days_left`·`get_event_group_cooldown_days_left`(1), `__try_to_determine_and_start_event`(4 까지) |
| `gm.__game_conditions` | `GameConditions` | `get_difficulty`·`get_speed_div`·`is_ironman`·`get_chancellery_charge_cost`·`get_inspection_paper_cost`·`try_convert_rings_to_gold`(없음), `set_difficulty`·`set_speed_div`·`set_crisis`·`is_crisis_active`(1) |
| `gm.__ambush_manager` | `AmbushManager` | `debug_start_ambush_archers`·`start_ambush_wolves`·`is_any_ambush_exists`(없음), `debug_start_ambush_professional`·`debug_start_ambush_faceless`·`start_slaves_rebellion`·`start_culture_rage`·`start_loyalist_rage`(1), `start_ambush_tutorial`(2 까지), `start_religious_rebellion`(3 까지), `start_ambush`(3) |
| `gm.__raids_manager` | `RaidsManager` | `try_to_set_raid`, `is_can_set_raid`, `is_raids_enabled`, `get_current_raid`, `try_to_remove_raid`. 자료: `__current_raid`(undefined), `__debug_override_raid_budget`(0) |
| `pv.__army_loyalty` | `ArmyLoyaltyManager` | `on_battle_win`(없음), `get_army_loyalty`·`get_summary_army_loyalty`·`get_people_loyalty`·`get_summary_people_loyalty`(2 까지. **인자 없이 부르면 게임이 끝난다**), `get_summary_loyalty`(1), `debug_change_people_loyalty`(3) |
| `gm.__endgame_manager` | `EndgameManager` | `get_num_of_province_under_control`·`is_endgame`·`declare_conquest`(없음. 뒤의 것은 부르지 않는다) |
| `gm.__tantrum_manager` | `TantrumManager` | `choose_random_tantrum`(1) |
| `pv.__politics_manager` | `PoliticsManager` | `unrest_start`, `is_unrest_active`, `make_all_politicians_puppets_followers`, `get_politicians` … (60여 개) |
| `pv.__conspiracy` | `ConspiracyManager` | `try_to_init_conspiracy`, `is_has_conspiracy`, `__reset_conspiracy` … |
| `pv.__criminal_manager`, `pv.__puppet_manager`, `pv.__usurper_manager`, `gm.__reinforcements`, `gm.__trade_agreements_manager`, `gm.__salary_manager_new`, `gm.__unlocker_manager` | | 이름만 받았다(답 파일) |
| `gm.__current_local_map` | | `get_weather`, `get_extreme_season_manager`, `get_wolf_manager`, `get_mines`, `get_trees` … 자료: `__season_manager`, `__weather_manager`, `__wolf_manager`, `__mines_manager`, `__sun` (안은 아직 보지 않았다) |

## 읽은 값 (인자 없는 읽기 함수를 불렀다)

| 함수 | 돌려준 것 |
|---|---|
| `ReligiosityManager.get_bishop_opinion()` / `get_fanatics_ratio()` / `is_has_bishop()` | 0 / 0 / false (주교가 아직 없다. `__bishop_uuid` "") |
| `GameDirector.get_current_phase_type()` / `is_current_phase_threat()` | 0 / true |
| `GameConditions.get_difficulty()` / `get_speed_div()` / `is_ironman()` | 1 / 2 / 0 |
| `GameConditions.get_chancellery_charge_cost()` / `get_inspection_paper_cost()` | 1 / 0.5 |
| `AmbushManager.is_any_ambush_exists()`, `ProphecyManager.is_has_prophecy()` | false, false |
| `EndgameManager.get_num_of_province_under_control()` / `is_endgame()` | 0 / 0 |

## 자료의 자리

- 세력: `gm.__factions_manager.__array_of_factions[57]`. 한 칸은 `Faction`: `__system_name`(플레이어는 `"player"`, 나머지는 `"faction.new.name.13"` 같은 이름), `__uuid`, `__king_uuid`, `__heir_uuid`,
  `__number_of_titles`, `__tags`(수), `__caption`. 플레이어의 세력은 `…__player_faction`.
  `__agreement_matrix.__matrix.<세력의 uuid>`(구조체), `__allies_matrix.__matrix`, `__allies_matrix.__is_faction_enemy_for_all.<uuid>`(11칸이 1 또는 true).
- 다른 세력과의 관계의 캐시: `pv.__other_faction_relation_cache.<1|2|3>.<세력의 uuid>` = 수(24칸. 하나만 −7 이고 나머지는 0). 무엇의 캐시인지, 쓰면 남는지는 재지 않았다.
- 이벤트 쿨다운: `gm.__game_director.__events_cooldowns.<이벤트 이름>` = 남은 날(`u_guest_dog_seller` 19), `__events_groups_cooldowns.<묶음>`(`GUEST` 2).
  지금의 국면: `__current_phase.__generic.__type` 0, `__duration_days` 4, `__current_day_in_phase` 1. `__adaptation_factor.__adaptation_factor` 0.3.
- 게임 조건: `gm.__game_conditions.__difficulty` 1, `__speed_div` 2, `__is_ironman` 0, `__crisis_name` "no_crisis", `__scenario_name` "no_scenario", `__mutators` 0칸.
- 끝내기: `gm.__endgame_manager.__is_conquest_declared` 0, `__is_endgame_achieved` 0.
- 시간: `inst:o_time_controller.time_speed_variants[4]`, `time_warp_max` 100, `is_important_notification_pause_enabled` 1(이야기 창이 뜰 때 멈추는 것으로 보인다. 추정).

## 둘째 실행 (모듈 0.13.1, 2026-10-06)

실행 묶음 `stage6-session2`(답: `refs/runtime/stage6-session2.answer.txt`). 06:00 ~ 10:40. **이 실행도 내가 부른 함수 때문에 GML 오류로 끝났다**(아래 "궁수 매복"). 저장하지 않는 실행이었다.

- 게임이 부른 꼴(기록. 06:00 ~ 10:18): `Faction.get_relation_with(세력 구조체) -> 수`(280번: 0, 2 …), `Faction.is_enemy_with(세력 구조체) -> 불리언`(270번: true 와 false 가 모두 나왔다),
  `ReligiosityManager.get_bishop_opinion() -> 0`(5번). 같은 동안 0번: `is_friend_with`, `attach_opinion_about_faction`, `force_neutrality`, `get_strength`,
  감독의 `is_event_cooldown_is_ready`·`get_event_cooldown_days_left`·`apply_event_cooldowns`·`__try_to_determine_and_start_event`, 습격의 `is_raids_enabled`·`is_can_set_raid`·`try_to_set_raid`,
  충성도의 넷, `attach_opinion_to_bishop`, `is_crisis_active`(하루의 다른 시각에 불리는지는 모른다).
- 게임의 세계 지도(아래 단추 줄의 맨 오른쪽)에 이웃 영지 옆의 "−7"이 보였다(`refs/ui/c2-map.png`). `pv.__other_faction_relation_cache`의 −7 과 같은 수다(같은 것인지는 재지 않았다).
- **이벤트 쿨다운은 써진다**: `write …__events_cooldowns.u_guest_dog_seller=0`(19 → 0), `…__events_groups_cooldowns.GUEST=0`(2 → 0). 다시 읽어도 0 이었다. 이벤트가 더 일찍 오는지는 보지 못했다.
- **주교 부르기**: `ReligiosityManager.debug_force_send_bishop()`(인자 없음) → `-> undefined`, `o_character` 5 → 6, `is_has_bishop()` false → true, `__bishop_uuid` "" → 그 인물의 uuid.
  온 사람: `o_character`, 진영 `holy_synod`, 갈래 3("Setptakh"). 20분(게임 시간) 뒤에도 그대로였다.
- **궁수 매복은 게임을 끝낸다**: `AmbushManager.debug_start_ambush_archers()`(본문이 `argc`를 옮기지 않는다)를 인자 없이 부르자
  `REAL argument incorrect type undefined`(`gml_Script_battle_squad_generate` ← 그 메서드). 인자가 없는 함수도 안에서 쓰는 값이 비어 있으면 죽는다. 다시 부르지 않는다.
- 기계어로 읽은 인자 수(부르지 않았다): 날씨 `get_days_without_rain`·`is_rain_started`(없음), `__start_rain`(3), `__start_snow`(1), `__start_dust_storm`·`__start_snow_storm`(2 까지).
  계절(`ExtremeSeasonManager`. `gm.__current_local_map.__season_manager`: `__current_phase` 3, `__start_phase_time` 28800) `is_extreme`·`get_extreme_season`·`get_current_phase`·`get_remain_time_to_extreme_season`·`get_snow_progress`(없음), `__set_phase`(3), `override_biome`(1).
  늑대 `get_max_number_of_wolves`·`night_call`(없음), `spawn_wolf`(1). 습격 `is_raids_enabled`·`get_current_raid`·`get_budget_for_raid`(없음), `is_can_set_raid`(1), `try_to_set_raid`(2 까지), `try_to_remove_raid`(1).
  충성도 `on_battle_win`(없음). 정치 `unrest_start`(가변), `is_unrest_active`(1). 감독 `get_adaptation`(없음).
- 이벤트의 자료: `inst:o_data.__game_director_events_data`: `__debug_forced_event`(undefined), `__events`·`__events_by_name`·`__groups`(ds_map), `__params`.
  지금 국면의 뽑기: `gm.__game_director.__current_phase.__events_weighted_random.__array_of_elements[8]` = 0, 200, 1, 60, 2, 80, 3, 70(번호와 무게가 번갈아 든 것으로 보인다. 추정).

## 확인 실행 (모듈 0.14.1, 2026-10-06)

실행 묶음 `stage6-session3`(답: `refs/runtime/stage6-session3.answer.txt`). 멈춘 채와 48분(게임 시간). 저장하지 않고 껐다. 적재 판정 통과. 게임 창과 마우스는 건드리지 않았다(백그라운드).

| 한 것 | 본 것 |
|---|---|
| `world cooldowns_clear` | "이벤트 쿨다운 1개와 묶음 쿨다운 1개를 0 으로 썼습니다". `u_guest_dog_seller` 19 → 0, `GUEST` 2 → 0 |
| 한 번 더 | "지울 쿨다운이 없습니다 (0 보다 큰 칸이 없습니다)" |
| `world ambush` | 거부 |
| `world bishop` | "주교가 왔습니다"(같은 틱에 `is_has_bishop()`이 참으로 읽혔다). `o_character` 5 → 6. 게임의 영주 줄에 주교가 올라왔다(`refs/ui/d1-religion2.png`) |
| 한 번 더 | "주교가 이미 있습니다" |
| `cheat no_enemies on` 뒤 48분 | `override -> false`, 40번 불렸다(표본은 원래도 false 였다). 끄자 바꾸기가 풀렸다. 효과는 보지 못했다 |
| `person spawn_soldier amount=1`, `economy gold_add amount=10` | 25 → 26, 1901 → 1911 (앞 단계의 것이 그대로 된다) |

## 세이브에 남는 것 (세이브 파일을 읽기만 했다)

- **세이브의 열쇠에는 런타임 이름의 앞 `__`가 없다.** "아덴" 세이브에서 `"events_cooldowns"` 1곳, `"__events_cooldowns"` 0곳. `"next_day_migrants_bonus"`도 밑줄 없이 1곳이다.
  열쇠를 찾을 때는 밑줄을 떼고 찾는다(밑줄째로만 찾으면 있는 것을 "없다"고 읽는다).
- 있는 열쇠: `events_cooldowns`, `events_groups_cooldowns`, `difficulty`, `speed_div`, `bishop_uuid`. 이벤트 쿨다운과 게임 조건에 쓴 값은 저장하면 남는다(쓴 0 이 저장을 거쳐 남는 것 자체는 재지 않았다).
- 없는 열쇠(밑줄이 있든 없든 0곳): `other_faction_relation_cache`, `faction_update_relations`, `debug_override_raid_budget`, `debug_is_can_die_of_old_age`,
  그리고 앞 단계에서 "없다"고 적은 것들(`capacity_in_categories`, `generic_warehouses`, `fair_trade_default_price_buy`·`_sell`, `map_of_production`, `construction_cost`, `production_points_cost`)도 두 이름 모두 0곳이었다.

## 확인하지 못한 것

- 이벤트 쿨다운을 0 으로 쓴 효과(이벤트가 일찍 오는가). 이벤트를 골라 일으키는 길(`__debug_forced_event`에 무엇을 쓰는가).
- `is_enemy_with`를 false 로 바꾼 효과. 관계의 수(−7)를 바꾸는 길, 동맹·전쟁·평화를 강제하는 길(`force_neutrality`, `add_to_alliance_with_leader`, `vassalise_by_faction`의 꼴).
- 주교의 평판을 바꾸는 길(`attach_opinion_to_bishop`의 꼴), 신앙·설교·예언.
- 계절·날씨를 바꾸는 길(인자가 있는 함수들의 꼴), 지도 공개, 난이도(`__difficulty` 1)와 게임 조건을 쓴 효과.
- 싸움을 붙이는 길: `start_ambush_wolves()`(인자 없음)는 부르지 않았다(궁수 매복이 죽은 뒤라 넘겼다).
