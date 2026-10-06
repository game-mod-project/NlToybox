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

## 실행 2 (모듈 0.23.1, 2026-10-06, `rel-session2`)

답: `refs/runtime/rel-session2.answer.txt`, 화면: `refs/ui/w1-rel*.png`. 06:00 에서 확인하고 한 시간과 40분을 흘렸다. 저장하지 않고 껐다(새 세이브 없음). 적재 판정 통과. 백그라운드.

| 한 것 | 본 것 |
|---|---|
| 주교가 오기 전 `court bishop like about=king` | "주교가 없습니다 (종교 패널의 '주교 부르기')" |
| `world bishop` 뒤 `court list` | 주교 줄(`bishop`)이 따로 있고 "(5 lords, 1 bishop, …)". `get_bishop_opinion()` 0 |
| `court bishop opinion about=king amount=3` | "Osortep -> Daven: 평판 0 -> 15 (좋은 평판 3개 붙임)". `get_bishop_opinion()` 15 |
| `court bishop like about=king` | 15 → 100(17개). `get_bishop_opinion()` 100 |
| `opinion amount=-2`, `clear` | 100 → 90(좋은 평판 2개 뗌) → 0(18개 뗌). **주교의 평판에서도 떼기가 듣는다.** 게임의 함수도 90, 0 |
| `court bishop like about=king queue=1`(틱이 한다) | 6초 안에 100. 그 뒤 `court lords loyal queue=1`: 일 3개(주교는 들지 않는다) |
| `cheat holy_defence on`, 자료를 0 으로 쓰고 `is_under_holy_defence()` | 1(기록: `() -> 0 => 1`). 끄면 0. 자료는 1 로 되돌렸다 |
| `cheat no_religious_riot on` | 훅이 걸린다(`() -> false => false`) |
| `cheat religion_free on` | 13칸이 0(게임 변수 여섯, 0 이 아닌 설교 비용 일곱). 끄면 3·5·250·300·200·50 과 50·30·50·50·50·50·150 |
| `cheat piety_restore 3` | 15·15·30·20 → 45·45·90·60. 끄면 처음 값 |
| `cheat preach_conversion 5` | 1 → 5. 끄면 1 |
| `cheat piety_decrease 0` 으로 한 시간(06:00 → 07:20) | 기도하지 않은 14명의 신앙심이 그대로(20 → 20 …). 둘은 기도로 +5. 끄자 변수가 0.83 으로 돌아갔다 |
| `cheat piety_full on` 으로 40분(07:20 → 08:00) | **플레이어의 사람 14명이 모두 100.** 플레이어의 사람이 아닌 둘(주교, 손님)은 57 → 56.45, 69 → 68.45(평소의 감소) |

- 그래서 `piety_decrease`와 `piety_full`은 확인으로 둔다. `holy_defence`·`no_religious_riot`는 함수의 답까지, `religion_free`·`piety_restore`·`preach_conversion`은 값이 써지고 되돌려지는 것까지 봤다(게임이 그 값을 따르는지는 보지 못했다).
- 화면(`w1-rel3.png`): 종교 패널에 표의 항목, 배율, "주교 부르기", "주교와의 평판"(`Osortep -> Daven (왕): 평판 100`과 단추 여섯)이 그려진다.
- **시간을 흘리는 스크립트는 배속이 0 으로 떨어지면 다시 걸어야 한다**: 이 세이브는 4일차 08:00 에 이야기 창이 떠 시간이 멈춘다. 다시 걸지 않은 스크립트는 거기서 멈춰 한 시간이 40분이 됐다.

## 실행 3 (모듈 0.23.2, 2026-10-06, `rel-session3`)

답: `refs/runtime/rel-session3.answer.txt`. 06:00 에서 멈춘 채로만 했다. 저장하지 않고 껐다(새 세이브 없음). 적재 판정 통과(마지막 판). 백그라운드.

- `cheat piety_full on` 뒤 4초: Barra 의 신앙심 0 → 100.

### 재능을 몇 개까지 붙일 수 있는가 (인물 프리셋의 검토를 위한 측정)

사용자가 인물 탭의 역할 프리셋(왕, 내정, 학자, 교관, 장군, 결투, 정치가, 음모, 관계, 종교, 무역, 생산, 교육)을 검토해 달라고 했다. 그 프리셋이 붙일 재능(talent) 특성이 한 사람에게 여럿 붙는지를 쟀다.

- 처음: 영주마다 재능은 하나였다(Amold `armorer`, Kira `gifted`, Daven `flatterer`. Barra 는 없음).
- `person <uuid> trait_add name=<이름>`으로 **71번 붙여 71번 모두 붙었다**(거부 0): Barra 에게 12개(장군·결투·교관), Amold 에게 30개(학자·교육·종교·관계·음모. 이미 있던 `empath`는 그대로),
  Kira 에게 20개(무역·생산·내정과 그 밖), 왕 Daven 에게 9개(통치자의 재능 넷과 `charisma`, `calm`, `longlive`, `typh_immunity`, `addiction_resist`).
  게임의 재능 58개가 모두 붙었고, 한 사람이 30개를 가져도 거부되지 않았다. 서로 밀어내는 조합을 보지 못했다.
- 붙일 때 게임이 스스로 부른 것: `Traits.is_limit_reached("이름") -> false`(70번 모두 false), `Traits.trait_attach("이름") -> uuid`(모듈이 부른 꼴 그대로).
  `gml_Script_trait_load_limit_value`는 한 번도 불리지 않았다.
- 떼기: Barra 에게서 `leader`, `terrifying`, `cutter`를 뗐다(목록에서 사라졌다).
- 보지 못한 것: 붙인 재능이 게임에서 실제로 듣는지(재능마다의 효과), 게임의 인물 창에 어떻게 보이는지, 세이브에 남는지(특성은 영혼의 자료라 남을 것으로 보이지만 재지 않았다).

