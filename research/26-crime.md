# 26. 범죄: 부랑자, 죄, 범죄 혐의

2026-10-06 · 게임 0.5588.9777.0 · 모듈 0.26.2(조사), 0.27.x(만든 것) · 세이브: 아침 자동 저장(`…Morning_day_5…19_11`. 불러오기만 했다. 게임의 저장은 꺼 두었다)

**요약**: 게임에서 범죄자("부랑자")는 주민의 범죄 구성요소(`c_criminal`)의 깃발이다. 게임은 저녁 18:00 에 주민 몇을 범죄자로 만들려 하는데(하지 않는 저녁도 있다), 그 직전에 "면책인가"를 묻는다:
그 답을 참으로 바꾼 시도 셋은 모두 범죄자를 만들지 못했다. 지정은 게임의 같은 함수로 풀 수 있다. 죄와 영주의 범죄 혐의는 특성이라 특성 떼기로 지워진다
(혐의는 내가 붙인 특성을 떼어 본 것뿐이다. 게임이 건 혐의는 보지 못했다).

자료는 `refs\runtime\crime-session*.answer.txt`, `crime-session*.modlog.txt`, `crime-session1.argc.txt`(추적 안 함).

## 구조

- 영지(`inst:o_game_map_controller.__province`)의 관리자들: `__criminal_manager`(CriminalManager), `__characters_punishment_manager`(ProvinceCharacterPunishmentManager),
  `__hire_patrol_manager`(HirePatrolManager), `__slaves_manager`, `__daily_statistic`(`get_criminal_average_hapines` 들).
- **범죄자 관리자**의 칸: `__today_criminal_events`, `__last_crimes`(배열: `time`, `x`, `y`), `__today_can_investigate`, `__investigate_cases`, `__taken_actors`, `__array_of_burglary_targers`,
  `__prince_of_thieves_share_gold`·`_runes`, `__gallows_punishment_options_data`(선택지 넷), `__gallows_terror_options_data`(선택지 열. `__terror_options.criminals` 들).
  메서드(이름): 조사(`request_investigate_case`, `is_harsh_judge_investigation_available`), 겁주기(`get_criminal_for_intimidation`, `intimidate_finish`), 공포(`__start_terror`),
  교수대(`__get_criminal_dummy_for_gallow`, `is_fair_punishment`), 하루의 끝(`__cycle_end_event`: 하루에 한 번 불렸다).
- **사람마다의 범죄 구성요소** `inst:o_dummy:<n>.c_criminal`(ComponentCriminal. 영주 `o_character`에게도 있다):
  `__is_dummy_criminal`, `__is_dummy_thug`, `__is_dummy_criminal_immunity`, `__is_dummy_criminal_work_done`, `__is_criminal_rolled_today`, `__is_criminal_will_today`,
  `__is_can_contraband_today`, `__criminal_begin_time`(-4), `__stolen_gold`, `__stolen_resources`, `__dark_order`. **깃발들은 처음에는 수 0 이고 지정된 뒤로는 불리언이다.**
- 살인의 구성요소 `c_murder`는 이 세이브의 사람들에게 없었다.
- 게임의 디버그 오브젝트(`o_debug`)에는 범죄에 관한 깃발이 없다.

## 인자의 수 (본문의 기계어)와 게임이 부르는 꼴

- 인자 없음: `is_criminal_immunity`, `is_criminal_work_will_today`, `is_criminal_work_done`, `is_criminal_scum`, `is_thug`, `is_can_leave_town_by_happiness`, `is_can_contraband`,
  `roll_contraband_chance`, `criminal_work_done`, `push_dummy_crime`, `return_back_stolen_to_player_warehouse`. 하나: `add_stolen_gold`, `add_stolen_resources`.
  들머리의 인자 맞춤: `become_thug` 1, `set_criminal_scum` 2. 관리자의 것은 `refs\runtime\crime-session1.argc.txt`.
- 기록(게임의 호출): `is_criminal_scum() -> 0 | 불리언`(자주: 열두 시간에 9천 번), `is_thug() -> 불리언`, `is_criminal_immunity() -> false`,
  `set_criminal_scum(true, true) -> undefined`, `set_criminal_scum(true) -> undefined`, `is_criminal_work_will_today() -> false`, `is_criminal_work_done() -> false`,
  `is_can_leave_town_by_happiness() -> false`, `roll_contraband_chance() -> undefined`, `return_back_stolen_to_player_warehouse() -> undefined`,
  관리자의 `__cycle_end_event()`, `__try_pay_prince_of_thieves_share()`, `__get_prince_of_thieves_candidates() -> 배열`.

## 범죄자가 생기는 때

- 이 세이브에서 하루 밤낮(6일차 06:00 ~ 7일차 06:00)을 돌리는 동안 범죄자는 없었고 범죄의 함수는 하나도 불리지 않았다.
- 일자리 있는 주민 12명에게 디버그 "슬픔" 생각(기분 -100, 하루)을 붙였다: 열 시간 동안 아무 일도 없었다. 디버그 소환기로 만든 주민 여덟(일자리 없음)에게도 붙였다: 범죄자가 되지 않았다
  (그동안 주민의 수가 줄었다. 떠난 것으로 보인다).
- **7일차 18:00 에 게임이 다른 주민 다섯을 범죄자로 만들었다**(기분 70~85 의 플레이어의 주민. `__criminal_begin_time` 669607). 한 사람마다 `is_criminal_immunity()`를 묻고
  `set_criminal_scum(true, true)`를 불렀다. 9일차 18:00 에도 한 사람(`set_criminal_scum(true)`: 인자 하나). 무엇이 그들을 고르는지는 재지 못했다.
- **같은 세이브의 같은 저녁이라도 게임의 시도는 실행마다 다르다**: 7일차 18:00 에 실행 1 은 다섯 번, 실행 3 은 0번(면책의 함수도 `set_criminal_scum`도 불리지 않았다).
- 범죄자가 된 뒤에도 진영은 `player`이고 `person list`에 그대로 나온다. **화면 왼쪽 위의 넷째 수가 범죄자의 수다**(직접 만든 범죄자의 수를 따라 0 → 5, 4 → 2 → 0 으로 바뀌었다. 실행 3·4).
  범죄자가 있으면 게임이 화면 위에 과제의 알림을 띄운다(실행 4).
- **이틀 동안 범죄자들은 범죄를 저지르지 않았다**(`push_dummy_crime` 0번, `__last_crimes` 빈 채). 깡패(`become_thug`)가 된 사람도 없었다. 그래서 도둑질, 훔친 것, 깡패, 도적의 것은 재지 못했다.

## 지정 풀기

- 범죄자 한 사람에게 `c_criminal.set_criminal_scum(false, true)`를 불렀다: `__is_dummy_criminal` 거짓, `__criminal_begin_time` -4, `is_criminal_scum()` 거짓. 오류 없음.
  그 사람은 그 뒤 이틀 동안 다시 지정되지 않았다(하루는 면책 훅이 켜져 있었다).
- **시험용 범죄자는 게임이 부르는 꼴 그대로 만든다**: 플레이어의 주민(갈래 1)의 `c_criminal.set_criminal_scum(true, true)`. 실행 3 에서 다섯, 실행 4 에서 넷을 만들었다(기능이 아니라 시험의 준비다).
  목록에 그대로 나오고, 줄의 단추·원격 명령·"모두" 단추로 풀렸다(한 틱에 셋까지). 푼 뒤 넷 모두 깃발 거짓, 시작 시각 -4, 진영 `player`.
- 범죄자가 아닌 사람과 영주의 uuid 는 거절한다("그 사람은 플레이어의 부랑자가 아닙니다").
- **깡패는 되돌리지 않는다**: 깡패의 지정을 풀면 어떻게 되는지 재지 못했다(게임이 만든 깡패를 본 적이 없다). 시험으로 한 범죄자의 `__is_dummy_thug`에 1 을 쓰자
  게임의 `is_thug()`가 참을 돌려줬다(그 함수는 이 깃발을 읽는다). 목록에 "깡패"가 붙고 줄의 단추가 없어지고, "모두"는 그 사람을 남기고 까닭을 적었다. 깃발을 0 으로 되돌린 뒤에는 풀렸다(실행 4).
- **훔친 것 되돌리기**(`return_back_stolen_to_player_warehouse()`. 인자 없음)는 훔친 금화가 0 인 범죄자에게 넷 번 불렀다: 오류 없음, 훔친 금화 0 에서 0. **훔친 것이 있는 사람에게서는 보지 못했다**(확인 전).

## 면책 판정을 바꾸기 (`no_new_criminals`)

| 저녁 | `is_criminal_immunity`의 답 | 게임의 시도 | 새 범죄자 |
|---|---|---|---|
| 실행 1, 7일차 | 그대로(거짓) | 5 (`set_criminal_scum(true, true)`) | 5 |
| 실행 1, 8일차 | 참으로 바꿈 | 2 (`set_criminal_scum(true)`) | 0 |
| 실행 1, 9일차 | 그대로 | 1 (`set_criminal_scum(true)`) | 1 |
| 실행 2, 7일차(같은 세이브의 같은 저녁) | 참으로 바꿈(표의 항목) | 1 (`set_criminal_scum(true, true)`) | 0 |
| 실행 3, 7일차(같은 저녁) | 그대로 | 0 | 0 |

- **저녁끼리 견주지 않고 시도마다 센다**(같은 저녁의 시도가 5, 1, 0번으로 다르다): 답을 그대로 둔 시도 여섯은 모두 범죄자를 만들었고, 참으로 바꾼 시도 셋은 하나도 만들지 못했다.
- 훅은 `o_character`의 주소(`inst:o_character:0.c_criminal.is_criminal_immunity`)로 건다: 스크립트는 모든 사람이 함께 쓰고(주민의 주소로 건 것과 같은 번호였다) 게임 화면에는 `o_character`가 언제나 있다.
  그래서 훅은 누구의 호출이든 바꾼다. 그 함수가 저녁의 지정 말고 다른 데서도 불리는지는 모른다(두 실행에서는 저녁에만 불렸다).

## 죄와 범죄 혐의

- **죄는 특성이다.** 게임의 특성 281개 가운데 이름이 `sin_`으로 시작하는 것이 열둘: `sin_begin_terror`, `sin_forbidden_sex`, `sin_forbidden_child`, `sin_abuse`, `sin_criminal`,
  `sin_unfair_attack`, `sin_murder`, `sin_fight`, `sin_dark_action`, `sin_seduce`, `sin_divorce`, `sin_evil_joke`.
- **영주의 범죄 혐의도 특성이다**: `character_crime`, `character_crime_blamed_by_bishop`, `character_crime_blamed_by_fanatics`. 주민의 것은 `dummy_crime`.
  그 밖의 범죄 쪽 특성: `criminal_intimidated`(겁먹음), `fear_of_punishment`, `roped_by_thugs`, `prince_of_thieves`, `harsh_judge`, `__guest_prisoner__`, `slave`, `slave_roped`, `__criminal_surrender__`.
- 영주 한 사람의 `sin_forbidden_child`를 인물 모듈의 특성 떼기(`person <uuid> trait_remove name=…`. 게임의 `trait_detach`)로 뗐다: 특성이 없어지고 생각의 합이 11.18 → 20.65.
- 7일차 저녁에는 영주 둘에게 죄가 있었다(게임이 붙인 것): `crime absolve <uuid>`가 하나, `crime absolve lords`가 둘을 뗐다(실행 3).
  주민(`o_dummy`)도 죄의 특성을 가질 수 있다(한 사람의 `sin_forbidden_child`). 죄 지우기는 플레이어의 영주에게만 한다(주민의 uuid 는 거절한다).
- **게임이 건 혐의는 보지 못했다.** 실행 4 에서 영주 한 사람에게 `character_crime`을 직접 붙였다(`person <uuid> trait_add name=character_crime`. 오류 없음): 목록에 나오고
  줄의 단추가 그 특성을 뗐다. 본 것은 특성이 떼어지는 것까지다: 혐의를 지운 뒤 게임의 처벌 메뉴와 주교·광신도 쪽이 어떻게 되는지는 보지 못했다. 그래서 창의 단추에 "(확인 전)"을 달았다.

## 게임 변수

`global.__gameplay_vars`의 이름과 값(뜻은 이름에서 읽은 것이다. 게임이 따르는지는 재지 않았다):

| 이름 | 값 |
|---|---|
| `dummy_turn_to_bandit_chance`, `dummy_turn_to_bandit_chance_peaceful`, `dummy_turn_to_bandit_limit_per_day` | 35, 20, 3 |
| `dummy_criminal_days_to_thug`, `dummy_join_to_thug_when_punish`, `dummy_join_to_thug_when_enslave` | 2, 1, 1 |
| `dummy_thug_combat_level_min`·`max` | 2, 4 |
| `dummy_storage_steal_minimal_val`·`maximal_val` | 10, 20 |
| `dummy_criminal_chance_to_contraband` | 0.5 |
| `mind_crime_not_punished_modify`, `mind_crime_victim_modify` | -7, -18 |
| `punishment_mind_<blindness·enslavement·execution·mask_of_shame>_strength`·`_duration` | 30·20·40·20, 1 |
| `executor_work_intimidation_duration` | 2 |
| `actor_patrol_view_cells`, `distance_to_patrol_around_sign_small`·`mid`·`big` | 8, 3·11·25 |

- 표의 넷(`no_bandit_turn`, `thug_days`, `crime_minds_off`, `theft_none`)은 그 가운데 일곱 값을 쓴다. 켜면 0·0, 20, 0·0, 0·0 이 되고 끄면 35·20, 2, -7·-18, 10·20 으로 돌아왔다(실행 2). 효과는 확인 전이다.
- **세이브에 남는 것**(세이브 파일을 읽어 봤다. 열쇠는 앞의 `__`가 없다): 그 일곱 게임 변수의 열쇠는 없다. 사람마다의 범죄의 깃발(`is_dummy_criminal`, `criminal_begin_time`, `stolen_gold`, `is_dummy_thug` 들)은 있다.
  그러니 범죄자의 지정과 그것을 푼 결과는 저장하면 세이브에 들어간다.
- 게임 자료(`o_data`)의 범죄 쪽 생각: `mind_crime_criminal_not_punished`, `mind_crime_in_city`, `mind_crime_punished`, `mind_crime_unfairly_punished`, `mind_crime_victim`, `mind_night_murder`,
  `mind_terror_after_punishment`, `mind_was_in_patrol`, 그리고 `opinion_mind_criminal` 들. 디버그 생각은 `mind_debug_totally_happy`·`totally_sad`(-100, 하루).

## 실행

| 실행 | 모듈 | 한 것 | 끝 |
|---|---|---|---|
| 1 `crime-session1` | 0.26.2 | 구조와 함수의 꼴, 범죄자가 생기는 저녁 셋, 지정 풀기, 면책의 답 바꾸기와 대조, 죄 떼기 | 판정 통과. 새 세이브 없음 |
| 2 `crime-session2` | 0.27.0 | 명령(`crime …`)과 패널, 게임 변수 넷의 쓰기·되돌리기, 표의 항목으로 건 면책 훅(같은 세이브의 같은 저녁) | 판정 통과. 새 세이브 없음 |
| 3 `crime-session3` | 0.27.0 | 훅 없이 같은 저녁(시도 0번), 영주의 죄 지우기, 직접 만든 범죄자 다섯의 목록·줄의 단추·"모두" 단추·원격 명령, 훔친 것 되돌리기의 호출 | 판정 통과. 새 세이브 없음 |
| 4 `crime-session4` | 0.27.1 | 검토 뒤 고친 판: 읽지 못한 수를 따로 적는 요약, `return_stolen <uuid\|all>`(금화의 앞뒤), 깡패 제외, 줄의 단추 셋(되돌리기, 죄 지우기, 혐의 지우기), 붙인 혐의의 특성 떼기. 시간을 흘리지 않았다 | 판정 통과. 새 세이브 없음. 게임의 오류 파일에 새 오류 없음 |

게임의 오류 파일(`catched_errors_…txt`)은 실행 2 에서 16.6 → 23.4 MB 로 자랐다(매복의 오류. 게임의 것이다: `research/25`의 대조 실행). 그래서 그 뒤로는 하루를 더 돌리지 않았다.

## 하지 않은 것

- 순찰(순찰 표지, 고용), 교수대의 처벌과 공포, 조사, 겁주기: 구조만 봤다.
- 영주를 처벌하는 것(게임의 처벌 메뉴), 포로.
- 범죄자가 실제로 저지르는 범죄(도둑질, 강도, 밤의 살인), 훔친 것을 되돌리는 게임의 함수의 효과, 깡패와 도적으로 넘어가는 것, 게임이 만든 깡패의 지정 풀기.
- 게임이 영주에게 건 범죄 혐의(주교, 광신도)와 그것을 지운 뒤의 일.
