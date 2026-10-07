# 29. 이벤트 — 목록, 감독, 강제 발동·예약 취소, 진행 중인 것의 자리 (2026-10-07)

이벤트 탭(강제 발동·해제)을 설계하기 위한 조사. 0.29.1(리팩토링 C 뒤). 아덴 4일차 저녁 세이브(`Evening_day_4`)에서 4번째 켜기(`refs/runtime/events-research1.*`)로 읽은 것과, 게임 파일 `director_params.json`·`localization\main.csv` 에서 이름과 열쇠만 옮긴 것이다.
값(`tickets`, `cooldown_days`, 인구 문턱)은 레포에 옮기지 않는다(게임의 자료). 읽는 길: `pwsh` 로 그 파일을 `ConvertFrom-Json` 해서 본다.

## 이벤트 61개, 묶음 11개

`director_params.json` 의 `events` 열쇠(이벤트마다 `category`, `group`, `tickets`, `min_population`, `max_population`, `cooldown_days.min/max`), `group_cooldown_days`·`group_min_spawn_days`·`group_super_priority_days`(묶음마다).
런타임에는 `inst:o_data.__game_director_events_data.__events_by_name`(ds_map 256번. 이름 → 이벤트 구조체)에 같은 61개가 있다. 구조체의 칸 10개:
`__system_name`, `__type`(int64. 0 BIG_THREAT, 1 SMALL_THREAT, 2 NEUTRAL, 3 GOOD — 파일의 `category` 와 하나씩 맞았다), `__weight`(파일의 `tickets`), `__min_population`·`__max_population`,
`__cooldown_days`(구조체 `x`·`y` = 파일의 min·max), `__delayed_spawn_time`(수. 그 시각에 생긴다. -1 이면 바로), `__group`(구조체: `__name`, `__cooldown_days`, `__min_spawn_day`, `__super_priority_days`, `__is_can_spawn_events`),
`__is_available`·`__spawn_method`(메서드). **화면 이름의 열쇠는 구조체에 없다.**

| 묶음 | 이벤트(`__type`) |
|---|---|
| GUEST(15) | `u_guest_assassin`·`mountain_knight`·`escaped_lord`·`witch`·`bandit_and_slave`·`cultists`·`joker`·`dog_seller`·`bard`·`nectar_trader`·`piligrims`·`incognito_philosoph`·`incognito_bishop`(2), `incognito_bandit`·`incognito_maniac`(1). 모두 18시에 생긴다 |
| RAID(11) | `raid_bums_in_rags`·`yellow_fanatics`·`flagellants`·`faceless`·`dogheads`·`deserters`·`bandits`, `forest_bandits_attack_player_village`·`_town`, `mountain_bandits_attack_player_town`, `prophecy_wolf_attack`(모두 0) |
| POLITICAL(9) | `desire_politician_for_power_make_me_heir_low`(2)·`_middle`(1)·`_high`(0), `politician_for_power_make_me_king_low`(2)·`_middle`(1)·`_high`(0): 21시. `politician_bribe`(1)·`dark_actions`(1): 18시. `conspiracy`(0): 20시 |
| REWARD(7) | `reward_hostage_lord`·`neutralise_lord`·`weak_bandits_camp`·`rich_migrants`·`trade_request`·`enemy_squad_on_march`(3), `ai_preach`(3. 18시) |
| GLOBAL_MAP(6) | `vassal_player_demand`·`demand_forcing_neutrality`·`town_king_tribute_light`·`faceless_attack_player_town`·`attack_player_village`·`player_vassal_rebellion`(모두 0) |
| ECONOMICAL(5) | `prophecy_tree_bug`·`rats_invasion`(1), `reward_increased_prices`·`farm_grow_boost`·`raw_collect_boost`(2) |
| UPRISING(4) | `rebellion_slaves`·`culture`·`religiosity`·`loyalty`(0) |
| DISTORTION(2) | `prophecy_xenophoby`(1), `migrants_bandits`(1. 18시) |
| EPIDEMY(1) | `prophecy_cholera_epidemy`(0) |
| CAMP(1) | `prophecy_bandits_camp`(1) |

적지 않은 `__delayed_spawn_time` 은 -1 이다.

## 화면 이름의 열쇠(`localization\main.csv`)

`event.<이름>` 꼴은 없다. 가족마다 다르다(열쇠만 적는다. 글은 모듈이 시작할 때 파일에서 읽는다):
- 손님: `guest.<u_guest_ 뒤>`(`guest.assassin`, `guest.bard`, `guest.dog_seller` …). 익명 넷은 `guest.incognito` 하나.
- 습격 일곱: `raid.<raid_ 뒤>`(`raid.bandits` …). 숲·산 도적의 공격과 늑대 습격 예언은 그 꼴이 없다(`prophecy.wolf_attack` 은 있다).
- 예언: `prophecy.<prophecy_ 뒤>`(`prophecy.cholera_epidemy`, `prophecy.xenophoby`, `prophecy.tree_bug`, `prophecy.rats_invasion`, `prophecy.wolf_attack`. 도적 기지는 게임의 오타 `prophecy.bandits_capm`).
- 보상 넷: `map.reward_rich_migrants`, `map.reward_farm_grow_boost`, `map.reward_raw_collect_boost`, `map.reward_trade_request`. 음모: `map.conspiracy`.
- 나머지(정치, 반란, 세계 지도, 인질·중립화·약한 기지·행군 보상, `ai_preach`, `migrants_bandits`)는 이름의 열쇠를 찾지 못했다(알림 글 `display_event.*` 366개는 이벤트의 이름이 아니라 그때그때의 알림이다).

## 감독(`inst:o_game_map_controller.__game_director`, GameDirector)

- 상태: `__current_phase`(구조체: `__current_day_in_phase`, `__generic`(`__type`, `__duration_days`, `__base_tickets[8]`, `__auto_success_event`), `__events_weighted_random`(`__array_of_elements[8]`, `__total_weight`), `__returned_events_history`),
  `__events_cooldowns`(이벤트 이름 → 남은 날. `u_guest_dog_seller 18`), `__events_groups_cooldowns`(`GUEST 1`), `__events_groups_super_priority_counters`(묶음 8개의 수), `__delayed_events`(배열. 비어 있었다), `__current_reward`(undefined), `__invoke_event_random`, `__adaptation_factor`.
- 함수: `__try_to_determine_and_start_event`(하루 한 번 오후. research/28), `get_super_priority_events_or_available_events`, `is_event_cooldown_is_ready`·`get_event_cooldown_days_left`·`apply_event_cooldowns`, `is_event_group_cooldown_is_ready`·`get_event_group_cooldown_days_left`,
  `__register_delayed_spawn`, `get_current_reward`·`set_current_reward`·`clear_current_reward`·`__clear_current_reward_if_invalid`, `get_current_phase`·`get_current_phase_type`·`is_current_phase_threat`·`is_current_phase_rest`·`__set_current_phase`, `on_next_day`·`on_new_hour`·`handle_cooldowns`.
- 자료(`inst:o_data.__game_director_events_data`, GameDirectorEventsData): `get_debug_forced_event`·`reset_debug_forced_event`(인자 없음), `get_event_by_system_name`, `get_available_events_by_type`·`get_events_by_type`, `get_group`·`get_groups`, `__get_event_type`·`__get_event_group`·`__get_event_tickets`·`__get_event_cooldown_days`·`__get_event_min/max_population`, `add_event`, `init_events`, `reload_params_from_json`.

## 강제 발동과 예약 취소 — 확인

- 발동(0.29.0 부터): `__events_by_name` 의 구조체를 `__debug_forced_event` 에 쓴다. 감독이 그날 오후에 뽑아 쿨다운에 올리고 `reset_debug_forced_event()` 로 지운다(research/28).
- **예약 취소 — 확인**: `u_guest_joker` 를 써 둔 뒤 `method inst:o_data.__game_director_events_data.reset_debug_forced_event`(인자 없음. 게임이 그 꼴로 부르는 것을 record 로 봤다) → `-> undefined`, `__debug_forced_event` 가 undefined 로 돌아왔다.
- 뽑힌 뒤 생기기 전의 자리는 `__delayed_events`(지연 생성. 손님 18시, 정치가 21시, 음모 20시)로 보인다(추정. 비어 있어 원소의 꼴을 못 봤다).

## 진행 중인 이벤트의 자리와 끝내는 후보 함수(이름만 봤다. 인자의 꼴은 아직 모른다 — 부르지 않았다)

| 가족 | 관리자 | 상태 | 끝내는 후보 | 판정 함수 |
|---|---|---|---|---|
| 습격 | `…__raids_manager`(RaidsManager) | `__current_raid`(undefined) | `try_to_remove_raid` | `get_current_raid`, `is_raids_enabled`, `is_can_set_raid`(시작은 `try_to_set_raid`) |
| 예언 | `…__province.__prophecy_manager`(ProphecyManager) | `__current_prophecy`(-4), `__completed_prophecy_list` | `__reset_prophecy` | `is_prophecy_active`, `is_has_prophecy`(시작은 `try_to_start_prophecy`·`__set_prophecy`) |
| 음모 | `…__province.__conspiracy`(ConspiracyManager) | `__current_conspiracy`(undefined) | `__reset_conspiracy` | `is_has_conspiracy`(시작은 `try_to_set_conspiracy`) |
| 정치(소요) | `…__province.__politics_manager`(PoliticsManager) | `__is_unrest_active_struct`, `__strategy_map`, `__political_followers` | `__all_unrest_finish`, `stop_being_politician`, `stop_being_follower`, `__reset_strategy` | `is_unrest_active`, `is_has_politician_not_king` |
| 반란 | `…__province.__rebellions_manager`(RebellionsManager): `__culture/__loyalty/__religiosity/__slaves_rebellion_manager` | (안쪽 넷은 아직 안 읽었다) | `clear_announcements`; 안쪽의 함수는 다음 조사 | `is_can_start_rebellion`, `is_can_announce_rebellion`, `is_actor_in_rebellion` |
| 매복·격분 | `…__ambush_manager`(AmbushManager) | `__array_of_ambush`([0]) | (끝내는 함수가 안 보인다. 원소의 꼴을 봐야 한다) | `is_any_ambush_exists`. **시작 함수들은 부르지 않는다**(research/14, 16) |
| 손님 | `…__province.__unique_guests`(UniqueGuestsManager) | `__current_guest`(undefined), `__guest_results` | `remove_guest_result`(뜻 추정) | `is_has_guest`, `get_current_guest`, `is_guest_available_to_spawn` |
| 어두운 행동 | `…__dark_actions_manager`(DarkActionManager) | `__orders`([0]) | — | `is_can_make_dark_actions`, `is_faction_has_dark_order` |
| 전역 허용(온보딩) | `…__onboard_manager`(GameOnboardingManager) | `__debug_force_allowed_rebellions`, `__debug_is_player_can_be_attacked`, `__debug_is_force_allow_tantrum`, `__debug_is_force_allow_to_give_desire`, `__attack_player_cooldown*` | — | `is_allow_to_get_guests`, `is_allow_to_attack_player_villages`, `is_any_rebellion_allowed`, `is_allow_to_dark_actions`, `is_politicians_active`, `is_allow_to_vassal_request`, `is_allow_to_king_tribute_request`, `is_allow_to_spawn_bandit_camp` … |

- 영지에는 `__is_debug_new_day_events_disabled`(bool) 와 `__random_events_manager`(아직 안 읽었다)도 있다. 44개의 구조체 멤버 가운데 이벤트와 닿아 보이는 것: `__conspiracy`, `__politics_manager`, `__prophecy_manager`, `__rebellions_manager`, `__unique_guests`, `__usurper_manager`, `__tribute_from_village`, `__caravan_random`, `__overpopulation_manager`.
- 이 세이브에는 진행 중인 것이 하나도 없었다(습격·예언·음모·손님 모두 비어 있다). 끝내는 함수의 인자와 효과는 **그 이벤트가 진행 중인 세이브**에서 record 로 봐야 한다.

## InspectionManager(사용자의 게임이 끝난 자리. 2026-10-07 12:50)

- `…__province.__inspection_manager`(InspectionManager)의 `step` = 스크립트 `…_485212661`, `check_hall_inspection` = `…_531112662`. 둘 다 **인자 없이 매 프레임** 불린다(90초에 6,692번·6,715번. 모두 `-> undefined`). 그래서 기록으로는 무엇이 undefined 였는지 못 본다.
- 오류는 `check_hall_inspection` 의 154행에서 정수 인자가 undefined 였다는 것뿐이다. 그때 사용자의 게임에서 모드가 하던 것: 건설비 0, 창고 용량 x10, 생산량 x2, 작업 능률 훅, 건물 지식 훅, 전투력 훅, 욕구·신앙심 채우기, 자원 바닥 +50. 어느 것도 undefined 를 쓰지 않는다. 원인은 못 가렸다(홀의 시찰이 보는 건물·사람이 없어진 상태로 추정만 한다).

## 구현(0.30.0. 이벤트 탭)

- `src/Events.cpp` + `core/EventPlan`. 표 61줄(이름·묶음·갈래·열쇠·지은 이름·가족), 예약과 취소, 묶음·찾기의 표, 가족 다섯의 진행 중, 끝내기(확인 전 다섯은 꺼져 있다), 쿨다운 지우기.
- 확인(Task 7 의 켜기. `refs/runtime/events-tab-run1.*`, `refs/ui/events-forced.png`·`events-after.png`·`events-world.png`. 아덴 4일차 저녁, 적재 판정 PASS):
  `world events` 가 61 of 61(표에 없는 게임의 이름 0, 게임에 없는 표의 줄 0). 파일에서 읽은 화면 이름 29개(손님 11, 습격 7, 예언 6, 보상 4, 음모 1), 손님 15줄 모두 한국어(익명 넷은 지은 이름).
  `world events group=GUEST find=bard` → `GUEST  u_guest_bard  음유시인  보통  cd -` 한 줄(1 of 61). `world event name=u_guest_joker` → 예약 `u_guest_joker (지연 0)`, 창에 "예약된 이벤트: 어릿광대 (u_guest_joker)"와 [예약 취소].
  `world event_cancel` → "예약을 지웠습니다: u_guest_joker"(로그 `world call reset_debug_forced_event() (forced: u_guest_joker)` → `forced event after reset: (none)`), 다시 → "예약된 이벤트가 없습니다"(함수를 부르지 않았다: 로그에 그 줄이 한 번뿐).
  `world event_end kind=raid` → "습격 끝내기는 확인 전이라 부르지 않습니다 …", `kind=rebellion` → `world event_end needs kind=<…>`. `world cooldowns_clear` → "이벤트 쿨다운 1개와 묶음 쿨다운 1개를 0 으로 썼습니다". 월드 패널에 이벤트의 글이 섞이지 않았다. 게임의 오류 파일에 새 ERROR 없음.
  **본 문제**: 표가 패널의 너비를 넘어 "묶음 쿨다운" 칸이 잘리고 "일으키기" 단추가 보이지 않았다(`events-forced.png`). 단추 칸을 맨 앞으로 옮기고 표에 가로·세로 스크롤을 주었다.
- 둘째 켜기(`refs/runtime/events-tab-run2.*`, `refs/ui/events2-forced.png`·`events2-filter.png`·`events2-after.png`. 적재 판정 PASS): 고친 표에서 "일으키기" 단추가 줄마다 맨 앞에 보이고 묶음·이름·갈래·쿨다운 칸이 보인다("묶음 쿨다운"은 가로 스크롤 뒤에).
  찾기 칸에 `bard` 를 쳐 넣자(`ui click`·`ui type`) 표가 `GUEST  음유시인  u_guest_bard  보통  -  1일` 한 줄이 됐다(묶음 쿨다운 GUEST 1일). 최종 리뷰의 고침 뒤에도 `world event name=u_guest_joker` → 예약 `u_guest_joker`, `world event_cancel` → "예약을 지웠습니다: u_guest_joker"(로그 `forced event after reset: 없음`), 다시 → "예약된 이벤트가 없습니다".
- 최종 리뷰(fable)에서 고친 것: 끝내기는 `Verified` 이고 인자의 꼴이 빈 글(인자 없음)일 때만 허용(`EventEndAllowed`. 헤더에 규약을 적었다), 예약의 자리를 읽지 못한 것과 없는 것을 가른다(`ForcedEventText`: "읽지 못함"·"없음"·"(이름을 읽지 못함)". 취소는 못 읽으면 부르지 않고 'f'), 표에 없는 줄 수는 이름을 읽은 뒤에만 센다.

## 남은 것

- 끝내는 후보 함수의 인자·효과(진행 중인 이벤트가 있는 세이브에서). 반란 안쪽 관리자 넷과 `__random_events_manager`, `__delayed_events` 원소의 꼴.
- 이름의 열쇠가 없는 이벤트 25개의 화면 이름(모듈이 지은 한국어 이름을 쓸지, 원시 이름을 보일지).
- 쿨다운 중인 이벤트를 강제로 써 두면 오는가(아직).
