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

## 확인하지 못한 것

- 게임이 위의 함수들을 부르는 꼴(기록을 걸었지만 받기 전에 게임이 끝났다).
- 인자 없는 디버그 함수(`debug_start_ambush_archers`, `start_ambush_wolves`, `debug_force_send_bishop`, `on_battle_win`)를 불렀을 때 일어나는 일.
- 이벤트 쿨다운과 게임 조건의 값을 썼을 때의 효과. 계절·날씨 관리자의 꼴.
