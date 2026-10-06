# 21 — 종교: 주교의 평판, 신앙심, 성스러운 보호, 종교의 비용

게임 0.5588.9777.0. 세이브 "아덴"(`Autosave_Morning_day_3`). `pv` = `inst:o_game_map_controller.__province`.

## 실행 1 (모듈 0.22.2, 2026-10-06, `rel-session1`)

답: `refs/runtime/rel-session1.answer.txt`. 4일차 06:00 에서 멈춘 채 조사하고 15:32 까지 세 번에 나눠 흘렸다. 저장하지 않고 껐다(새 세이브 없음). 적재 판정 통과. 백그라운드.

### 관리자와 자료

| 자리 | 자료 | 메서드(기계어로 본 인자의 수) |
|---|---|---|
| `pv.__religiosity_manager`(ReligiosityManager) | `__bishop_uuid`(없으면 빈 글), `__cached_matriarch_opinion`, `__preach_roll`, `__punishment_group` | `get_bishop_opinion`·`get_fanatics_ratio`·`is_has_bishop`·`get_bishop`·`get_bishop_soul`·`get_bishop_soul_character`·`__control_bishop_opinion`·`__try_to_set_preach`(0), `attach_opinion_to_bishop`(인자를 받는다) |
| `pv.__prophecy_manager`(ProphecyManager) | `__current_prophecy`(-4), `__completed_prophecy_list`, `__prophecy_completed_count` | `is_has_prophecy`·`is_allow_to_set_prophecy`(0), `is_prophecy_active`·`try_to_start_prophecy`·`is_prophecy_available_to_start`(1) |
| `inst:o_game_map_controller.__onboard_manager`(GameOnboardingManager) | `__is_under_holy_defence`(1), `__population_to_remove_holy_defence`(65), `__holy_defence_will_removed_by_population_day`(-4), `__temp_holy_defence_time`(0), `__removed_holy_defence_reason`(-4), `__debug_is_player_can_be_attacked`(false) … | `is_under_holy_defence`·`is_allow_to_religious_riot`·`is_allow_to_spawn_bishop`·`is_temp_church_defence`·`get_remain_temp_defence_hours`·`__is_player_can_be_attacked`·`is_allow_to_attack_player_villages`(0), `set_temp_holy_defence`·`__disable_holy_defence`(1) |
| `inst:o_data.__preach_data.__preach_list[10]` | 설교의 종류: `__name`, `__caption`, `__cost`, `__donate_to_listeners`, `__start_hour`(8), `__hint` … | |
| `inst:o_building:<n>.c_church`(BuildingComponentChurch. 교회가 아니면 -4) | `__preach`(없으면 undefined), `__ai_preach_history`, `__is_repeat_preach` | `set_preach`, `reset_preach`, `preach_complete`, `is_has_preacher`, `get_preacher` …(꼴을 보지 못했다) |

- 건물의 종류는 `inst:o_building:<n>.raw_caption`(`"building.stone_church"`)으로 가린다. 이 세이브의 건물 48채 가운데 석조 교회가 하나(제단과 목조 교회는 없다).
- 설교 열 가지(`__name`: `__cost`): `preach_of_hope` 50, `faith` 30, `doubt` 50, `anti_bandits` 0, `preach_loyalty` 50, `preach_loyalty_more_effective` 50, `support_preach` 0,
  `sacrificer_preach` 50, `inspirer_preach` 150, `preach_after_political_demand_refuse` 0.
- 종교의 게임 변수(`global.__gameplay_vars`. 32개 가운데): `religiosity_confession_cost` 3, `religiosity_divorce_cost` 5, `religiosity_begging_cost` 250,
  `religiosity_canonization_cost_gold` 300, `religiosity_canonization_cost_per_province` 200, `religiosity_sacrificer_cost_gold` 50,
  `church_pray_piety_restore` 15, `altar_pray_piety_restore` 15, `church_pray_morning_service_restore` 30, `trait_saint_piety_talk_restore` 20,
  `church_preach_conversion_factor` 1, `church_max_capacity` 50, `religiosity_population_for_bishop` 40, `religiosity_rebellion_min_fanatics_ratio` 0.3,
  `mind_fanatic_modify` 12, `migration_fanatics_chance` 0.15, `prophecy_cooldown_days_min`·`max` 12·14. **뜻은 이름에서 읽은 것이다.**
- `inst:o_debug`: `church_donation_runes` 1, `church_donation_runes_fanatic` 2, `church_donation_runes_threshold` 3, `debug_piety_decrease_per_hour` 0.83.

### 주교의 평판

- 주교 부르기(`debug_force_send_bishop()`) 뒤: `__bishop_uuid`가 그 인물의 uuid, 인물은 `o_character`(진영 `holy_synod`), `is_has_loyalty()` 거짓.
- **`get_bishop_opinion()`은 주교가 우리 왕을 보는 평판이다.** 주교의 평판 구조체(`inst:o_character:<n>.__soul.__character_soul.__opinions`)에
  `opinion_attach(왕의 __character_soul, opinion_mind_debug_positive, 1, undefined)`를 부르자 `get_opinion(왕)`과 `get_bishop_opinion()`이 함께 0 → 5 → 15.
  영주의 호감(`research/20`)과 같은 함수가 주교에게도 듣는다.
- 게임이 6시간에 부른 것: `get_bishop_opinion() -> 15`(15번), `get_bishop_soul() -> 구조체`(134번), `get_bishop() -> ref`(3번), `__control_bishop_opinion() -> undefined`(1번),
  `get_fanatics_ratio() -> 0`(17번). `attach_opinion_to_bishop`은 한 번도 불리지 않았다.
- `__cached_matriarch_opinion`: 주교가 오기 전 0, 온 뒤 3, 6시간 뒤 2. 무엇의 수인지 재지 않았다.

### 신앙심

- 신앙심은 욕구 3번(`__soul.__motive.__motive[3]`)이다.
- **`inst:o_debug.debug_piety_decrease_per_hour`를 게임이 따른다.** 사람 18명(영주 5, 주교, 주민 12)의 신앙심을 세 번 읽었다:

  | 구간 | 그 변수 | 기도하지 않은 14명 |
  |---|---|---|
  | 12:22 → 14:10 (1.8시간) | 0.83(게임의 값) | 시간당 −0.81(8명) 또는 −0.40(6명) |
  | 14:10 → 15:32 (1.37시간) | 0 으로 썼다 | 14명 모두 0.00 |

  나머지 넷: 신앙심이 0 인 한 명은 그대로, 셋은 올랐다(기도. 시간당 +0.4 ~ +5.8).
  절반(−0.40)으로 주는 사람이 있는 까닭은 재지 않았다. 잰 값은 0 뿐이다.

### 성스러운 보호와 종교 반란

- `is_under_holy_defence()`는 `__is_under_holy_defence`의 수를 그대로 돌려준다: 자료 1 → 1, 자료를 0 으로 쓰자 0. 게임이 6시간에 1번 불렀다(`() -> 1`).
- **훅으로 `n:1`을 걸면 자료를 0 으로 써도 1 을 돌려준다**(`#5 () -> 0 => 1`). 풀면 다시 자료의 값이 나온다. 쓴 자료는 1 로 되돌렸다.
- `is_allow_to_religious_riot() -> false`(게임이 1번. 광신도가 없는 세이브다. 참일 때를 보지 못했다).
- 보호가 실제로 무엇을 막는지, 인구 65 를 넘기면 어떻게 풀리는지는 보지 못했다.

### 설교

- 설교·헌금의 스크립트 아홉(`get_preach_mind_strength`·`_by_place`, `get_chance_to_religious_transform`, `get_donate_money_count`·`_opinion_mind_strength`·`_negative_mind_value`,
  `get_faith_preach_bishop_relation_mult`, `get_preach_oratory_skill_level`, `try_to_set_preach_somewhere`)에 기록을 걸고 06:00 → 12:22 를 흘렸다: **모두 0번.**
  교회는 있지만 설교가 정해져 있지 않다(`c_church.__preach` undefined). 설교를 정하는 것은 게임의 창이다.
- 실행의 마지막 호출 `ReligiosityManager.__try_to_set_preach()`(인자 없음. 게임이 부르는 것을 보지 못했다): `-> undefined`, 설교는 정해지지 않았고 게임은 죽지 않았다.
- 그래서 **설교의 효과(강도, 전환 확률, 헌금)는 재지 못했다.** 돌려주는 형을 모르는 함수에는 배율을 걸지 않았다.

### 써지는가, 세이브에 남는가

- `global.__gameplay_vars.religiosity_confession_cost` 3 → 0 → 3, `inst:o_data.__preach_data.__preach_list[1].__cost` 30 → 0 → 30: 써지고 되돌려진다.
- 세이브 파일(읽기만. 밑줄을 뗀 이름으로도): `gameplay_vars`, `religiosity_confession_cost`, `church_preach_conversion_factor`, `preach_list`, `"cost"`, `debug_piety_decrease_per_hour` 0건.
  `is_under_holy_defence` 1건, `bishop_uuid` 1건, `opinion_minds` 237건(평판은 세이브에 들어간다).

### 주민의 신앙

- 주민 24명의 특성에 `religiosity_fanatic`·`religiosity_doubting`이 없다(광신도 비율 0). 광신도·의심하는 사람이 있을 때의 것은 재지 못했다.
