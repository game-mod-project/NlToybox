# 32. 거주 칸(정원), 건물의 효과와 범위, 이벤트 바로 일으키기 (2026-10-08)

질문 셋: 주택과 병영의 정원을 늘릴 수 있는가. 서비스 건물(교회·제단 같은)이 주변에 미치는 범위를 넓히고, 나쁜 효과는 없앨 수 있는가. 이벤트를 예약하지 않고 바로 일으킬 수 있는가.
방법: 켜기 2번(0.31.0. 아덴 5일차 아침 세이브 `아덴_Autosave_Morning_day_5`). 첫 번은 재기(읽기·`record`·시험 호출), 둘째는 구현의 확인.
답은 `refs/runtime/living-range-events-run1.*`·`-run2.*`, 화면은 `refs/ui/instant-rich-migrants.png`·`instant-prophecy.png`·`build-panel.png`·`events-panel.png`·`events-filter.png`·`events-now.png`·`people-panel.png`·`army-panel.png`(추적 안 함).
두 실행 모두 모듈 로그에 오류 줄이 없고, 불러온 뒤 게임의 오류 파일에 새 줄이 없고, 게임은 정상 종료했다. 둘째 실행의 로그는 적재 판정(`Get-NlLoadFailures`)을 통과했다.

## 거주 칸

- 자리: 건물 종류(`GenericBuilding`)의 `__number_of_living_places`. 게임의 건물 자료(`building_constructor\buildings\*.json` 85개)의 열쇠 `number_of_living_places`가 옮겨진 것이다(값은 옮겨 적지 않는다).
  런타임에서 본 것: `hut_8x8` 2, `barrack_6x10` 6, `lord_house_10x6` 2, `castle` 2, `slaves_barrack_bottom` 6. 세이브 파일에 그 열쇠는 없다(거주자의 목록은 있다).
- 게임이 부르는 꼴(기록): 건물 인스턴스의 `get_number_of_living_places() -> 수`(`o_building` Create 의 메서드. 묶여 있다),
  `GenericBuilding.get_number_of_living_places(0) -> 6`, `get_raw_number_of_living_places() -> 6`.
  등급 3~5 의 `hut_8x8` 건물은 6 을 답했다(종류 `hut_8x8` 의 칸은 2 다: 등급의 종류를 본다).
- 거주 구성요소 `inst:o_building:<n>.c_residence`(`BuildingComponentResidence`): `__array_of_residents`(지금 사는 사람의 배열. 길이가 그 수다. 자리의 수가 아니다),
  `is_full() -> 불리언`, `get_number_of_residents() -> 수`, `is_enought_places_for(구조체) -> 불리언`, `add_resident(구조체) -> true`, `add_resident_with_priority(구조체) -> undefined`, `remove_resident(구조체) -> true`.
- **배율 2 를 건 결과**(치트 표의 `housing_capacity`·`barrack_capacity`. `src/Jobs` 의 엔진): 주택 25종과 병영 6종에 써졌다. 건물의 `get_number_of_living_places()` 가 바로 6 → 12(오두막, 병영, 노예 막사), 2 → 4(영주 저택, 영주관)가 됐다.
  - 집의 창이 "거주민(6 / 12)"를 보였다(`instant-rich-migrants.png` 의 왼쪽 아래).
  - 병사 20명을 만들자(`person spawn_soldier amount=20`. 게임이 `add_resident_with_priority` 를 20번 불렀다) 병영 여섯의 거주자가 6·6·6·6·2·0 → 12·12·12·10·2·0 이 됐다(합 26 → 48. 원래 정원의 합은 36).
    게임은 앞의 건물부터 정원까지 채운다.
  - 이주민과 만든 농민이 들어와 오두막 셋(b5, b9, b52)이 12명이 됐다(원래 6).
- **끈 뒤**: 병영의 정원은 6 으로 돌아왔고, 12명이 사는 병영 셋은 아홉 시간(20:41 → 05:37) 뒤에도 12명이었다. 그 뒤로 더 오래는 보지 않았다.

## 건물의 효과

- 건물 종류의 `__effect` = `{ __caption, __name, __effect(수), __range(칸), __type }`. 효과를 가진 종류는 14개이고 구조체는 종류마다 따로다(모듈이 처음 걸을 때 로그에 적는다: `build: effect …`).

| 효과(`__name`) | 값 | 범위 | 종류 |
|---|---|---|---|
| `religious_nearby` | +3 / +5 / +7 | 12 | `altar` / `church_small` / `church` |
| `public_place_nearby` | +3 | 8 | `market`, `gallows`, `tavern`, `castle`, `drug_deg` |
| 종류의 이름 그대로 | -4 | 6 | `armorsmith`, `coal_furnace`, `forge`, `melting_furnace`, `woodcutter` |
| `training_ground` | -8 | 8 | `training_ground` |

- 구성요소 `inst:o_building:<n>.c_effect`(`BuildingComponentEffects`).
  - 내는 쪽: `__effect_current`(= `__effect_default`. 같은 구조체) = `{ __building, __generic(종류의 __effect), __x1, __x2, __y1, __y2 }`. **사각형은 건물이 만들어질 때 굳는다**: 건물의 칸(`map_cell_x`·`map_cell_y`, 크기)에 범위를 더한 것이다
    (교회: 칸 18·76, 범위 12 → 6..39 × 64..99. 범위 24 로 만들어지면 0..51 × 52..111: 0 아래로는 가지 않는다).
  - 받는 쪽: `__applied_effects`(받는 효과의 구조체들. 내는 건물의 `__effect_current` 와 같은 주소다)와 `__applied_effects_value`(합). 그 합이 집의 창의 "안락도"다(창의 "+12"와 값 12 가 같았다).
    건물 58채 가운데 28채가 내는 건물의 것이 아닌 효과를 하나 갖고 있었고(제 등급의 것으로 보인다: 집의 창의 "품질의 안락도"), 교회의 것은 둘(b8, b12), 영주관의 것은 둘(b1, b3)이 받고 있었다.
    용광로·훈련장·선술집의 것을 받는 건물은 없었다.
- **게임 안에서 범위만 바꾸면 먹지 않는다**: 종류의 `__range` 를 두 배로 써 둔 아홉 시간 반 동안 교회의 사각형과 집 28채의 합이 그대로였다.
  - `c_effect.__init_my_effect()` 를 불러도 사각형은 그대로였다(이미 만들어져 있으면 다시 만들지 않는 것으로 보인다).
  - 사각형을 직접 12칸씩 넓혀 쓰고 `c_effect.__update_applied_effects()` 를 집 28채와 교회에 불러도 합이 그대로였다: **이 함수는 받는 효과의 목록을 다시 찾지 않는다.**
  - 종류의 값(`__effect.__effect`)을 7 → 0 으로 쓰고 `__update_applied_effects()` 를 부르자 교회의 효과를 받던 집의 합이 19 → 12, 되돌리고 부르자 19: **합은 목록의 값으로 다시 낸다.** 값만 쓰고 부르지 않으면 그대로다.
- **메인 메뉴에서 써 두면 먹는다**(둘째 실행): 메뉴에서 `building_generic_get_array_of_all_buildings()` 가 171개를 줬다(건물 종류는 게임이 켜질 때 만들어진다).
  메뉴에서 `effect_range` 를 2 로 켜자 좋은 8종에 써졌고(나쁜 6종은 그대로), 세이브를 불러오니
  - 교회 6..39 × 64..99 → 0..51 × 52..111, 선술집과 영주관도 범위 16 으로 넓어졌고, 용광로(62..81 × 158..177)와 훈련장은 그대로였다.
  - 집 28채의 합(오두막 17, 병영 6, 영주 저택 4, 영주관): 원래 `12 12 19 19 19 19 11 19 16 16 12 12 12 19 16 16 16 | 20 20 23 23 23 20 | 13 10 13 10 | 10`
    → `19 19 19 19 19 19 11 19 16 16 19 19 19 19 16 16 16 | 23 23 23 23 23 20 | 13 10 13 10 | 10`. 오두막 다섯이 +7(교회), 병영 둘이 +3(공공장소).
  - 게임 안에서 끄면 종류의 범위는 12 로 돌아왔고 교회의 사각형과 집의 합은 넓은 채였다(만들어진 건물은 그대로다).
- 나쁜 효과 없애기(`bad_effects_off`): 켜자 6종의 값이 0 이 됐고(용광로 -4, 훈련장 -8 → 0. 교회 7, 선술집 3 은 그대로) 건물 58채에 `__update_applied_effects()` 를 불렀다. 끄자 원래 값으로 돌아왔다.
  **나쁜 효과를 받는 집이 이 세이브에 없어 합이 바뀌는 것은 보지 못했다**(길은 위의 교회로 봤다).
- 인자를 읽는지(기계어. `dumpbin /disasm`. 본문이 `argc`(r9d)를 레지스터나 스택에 옮기는가):
  `__init_my_effect`(`0x141C0EBD0`), `reset_effect`(`0x141C0F8A0`), `__update_applied_effects`(`0x141C112C0`)는 옮기지 않는다(인자 없음). `set_applied_effects`(`0x141C12790`)와 `set_effect`(`0x141C0F0B0`)는 옮긴다(인자를 받는다. 꼴은 모른다).
- 관리자 `inst:o_game_map_controller.__current_local_map.__building_effects_manager`(`BuildingEffectsManager`): 멤버 `__cache`, `__cells`, `__water_effect`, `__water_effect_registered`.
  메서드 `register_building`, `unregister_building`, `update_buildings_for_effect`, `get_affected_buildings`, `get_affected_buildings_by_effect`, `get_buildings_in_zone`, `get_buildings_in_range`, `get_cell_effects`, `get_cell_effect_value`,
  `get_area_effects`, `get_building_effects`, `is_building_in_effect_zone`, `effects_are_stackable`, `register_water_effects`, `__clear_cache`. **두 실행에서 한 번도 불리지 않았다**(건물이 지어지지 않았다). 꼴을 모른다.
- 단서(재지 않았다): 게임 변수 `building_effect_water_area_distance`·`building_effect_water_area_value`(물 근처 효과로 보인다), 현지화의 효과 이름 `building.effect.{burned, living_zone, no_church, public_place_nearby, quality_work, water_nearby}`.
  건물 자료의 `displayed_circle_radius` 는 85개 모두 0 이다.

## 이벤트 바로 일으키기

- **예약은 그날 오지 않을 수 있다.** 감독(`…__game_director`)의 하루: `on_new_hour(시)`(매시), `on_next_day()`(하루 한 번. 인자 없음) → `__try_to_determine_and_start_event(종류|undefined, 종류|undefined, true, 구조체)`.
  6일차의 뽑기는 `(undefined, undefined, true, 구조체) -> undefined` 였고 `get_debug_forced_event()` 는 한 번도 불리지 않았다: 08:16 에 써 둔 예약(`u_guest_bard`)이 17:44 까지 그대로였다.
  넷째 인자의 구조체는 호출마다 주소가 다르다(감독·국면·자료·영지의 구조체 멤버 317개와 견줬다. 맞는 것이 없다).
- **게임이 이벤트를 생기게 하는 꼴**(기록): `on_next_day()` 를 게임과 같은 꼴로 한 번 부르자
  `__try_to_determine_and_start_event(3, undefined, true, 구조체) -> 이벤트`, 그 안에서 `get_debug_forced_event() -> 이벤트`, `이벤트.__is_available() -> true`,
  `apply_event_cooldowns(이벤트, true)`(쿨다운 15일, 묶음 2일), `__register_delayed_spawn(이벤트, 18)`. 18시의 `on_new_hour(18)` 때 **`이벤트.__spawn_method()`(인자 없음)** 가 불렸고 손님이 왔다(영주·손님 8 → 9).
  `on_next_day()` 는 쿨다운의 날을 하루 줄이고 국면의 날을 넘긴다(국면이 새로 시작됐다). 그래서 바로 일으키기에 쓰지 않는다.
- `__spawn_method` 의 스크립트는 가족마다 다르고(손님끼리는 같다) 모두 묶여 있다. `__is_available()` 의 답(6일차 18:16, 손님이 와 있을 때):
  보상 다섯(`reward_rich_migrants`, `reward_farm_grow_boost`, `reward_raw_collect_boost`, `reward_increased_prices`, `reward_trade_request`)·`prophecy_wolf_attack`·`migrants_bandits` 는 true,
  `raid_bandits`·`raid_bums_in_rags`·`prophecy_tree_bug`·`conspiracy`·`rebellion_slaves`·`politician_bribe` 는 false, 손님 둘은 undefined.
  5일차 17:31(손님이 없을 때)에는 `u_guest_bard` 가 true, `u_guest_witch` 가 false 였다: 손님마다 조건이 다르다.
- **직접 불러 본 것**(게임이 부르는 꼴 그대로. 인자 없음):
  - `reward_rich_migrants.__spawn_method()` → 감독의 `__current_reward` 가 undefined → 구조체, 화면에 "새로운 기회!". 두 시간 반쯤 뒤 주민이 85 → 103 이었고 오두막 b5 가 6 → 11명이었다(그 이주민인지는 가리지 않았다).
  - `prophecy_wolf_attack.__spawn_method()` → 예언 관리자의 `__current_prophecy` 가 -4 → 구조체, 화면에 "총대주교: 새로운 예언". 그 뒤 `__is_available()` 은 false.
  - `apply_event_cooldowns(이벤트, true)` → 쿨다운에 `reward_rich_migrants 14`, 묶음 `REWARD 2`.
- **구현의 확인**(둘째 실행. 치트 없는 실행 묶음):
  - 창의 찾기 칸에 `trade_request` 를 쳐 넣고(`ui click`·`ui type`) 줄의 "지금"을 눌렀다(`ui click`): 로그에 `world call reward_trade_request.__is_available()`, `…__spawn_method()`, `apply_event_cooldowns(reward_trade_request, true)`,
    `spawned now, cooldown 15 day(s)`. `__current_reward` 가 구조체가 됐고 줄의 쿨다운이 "15일", 묶음 쿨다운이 "2일", 화면에 "새로운 기회!".
  - `world event_now name=raid_bandits` → "게임의 조건이 지금 맞지 않아 일으키지 않았습니다 (__is_available 의 답: false)". `u_guest_witch` 도 같다. 없는 이름 → "게임에 그 이름의 이벤트가 없습니다".
  - 17:31 에 `world event_now name=u_guest_bard` → "바로 일으켰습니다 (쿨다운 20일)". 손님 관리자의 `__current_guest` 가 바로 구조체가 됐고(지연 생성 0건), 18:06 에 영주·손님이 7 → 8 이었다.
    다시 보내자 "조건이 맞지 않아 … (답: undefined)"(손님이 이미 있다).
- 습격·정치·반란·세계 지도·전염병·기지의 이벤트는 조건이 참인 때가 없어 직접 일으켜 보지 못했다(호출의 자리와 꼴은 같다).

## 구현(0.31.0)

- `core/BuildingPlan`(`IsBarracksName`, `EffectSideOf`, `EffectSides`), `src/Build.cpp`(`WalkLiving`, `WalkEffects`, `RefreshEffects`, 효과의 목록을 로그에), `src/Jobs`(`JobDef::AnyScreen`).
  치트 표: `housing_capacity`(인구), `barrack_capacity`(군대), `effect_range`(건설. 좋은 효과의 범위 배율), `bad_effects_off`(건설. 확인 전).
- `core/EventPlan`(`EventAvailable`, `InstantEventReport`), `core/WorldPlan`(`WorldAct::EventNow`, 낱말 `event_now`), `src/Events.cpp`(`SpawnEventNow`, 줄의 "지금"·"예약" 단추, 청을 이름과 함께 쌓는다).
- 화면에서 본 것: 건설·인구·군대 패널의 새 항목과 상태 줄, 이벤트 패널의 "지금"·"예약" 단추와 찾기.
- 셋째 켜기(마지막 빌드 `559da9f` 의 적재 판정. `refs/runtime/living-range-events-run3.*`, `refs/ui/events-result-top.png`·`build-final.png`): 적재 판정 PASS, 모듈 로그에 오류 줄 없음, 게임의 오류 파일에 새 ERROR 없음.
  둘째 켜기에서 결과의 줄이 패널의 맨 아래에 있어 거절한 까닭이 스크롤해야 보였다. 표의 위로 옮긴 뒤 `world event_now name=raid_bandits` 의 답
  "마지막 한 일: raid_bandits: 게임의 조건이 지금 맞지 않아 일으키지 않았습니다 (__is_available 의 답: false)"가 "예약된 이벤트" 바로 아래에 보였다.
  건설 패널의 "좋은 효과의 범위 배율"에는 확인 전 표시가 없고 "나쁜 효과 없애기"에는 있다.

## 프리셋(0.31.1)

- 확인된 셋(`housing_capacity`, `barrack_capacity`, `effect_range`)을 샌드박스와 신 묶음에 잰 배율 x2 로 넣었다(2026-10-08 에 사용자가 넣기로 했다. 다른 배율은 재지 않았다). 쉬움에는 넣지 않았다.
  `bad_effects_off` 는 확인 전이라 어느 묶음에도 없다(`CheckPreset` 이 막는다).
- 범위 배율은 건물이 만들어질 때 먹으므로 묶음을 건 뒤 세이브를 다시 불러와야 이미 지은 건물에 보인다(프리셋 패널의 위쪽 글에 적었다).

## 남은 것

- 나쁜 효과를 받는 집이 있는 세이브에서 없애기의 합. 켠 뒤에 새로 짓는 건물의 범위. 이미 지은 건물에 범위를 바로 먹이는 길(건물을 지을 때 관리자 함수를 `record`).
- 물 근처 효과(게임 변수 둘). 좋은 효과의 범위를 넓힌 채 저장한 세이브를 배율 없이 불러오면 원래대로인가(사각형이 세이브에 드는가. 열쇠로는 없어 보였다).
- 정원을 끈 뒤 넘친 거주자가 며칠 뒤에도 남는가. 정원을 넘긴 집의 침대(`c_residence` 의 `try_to_take_bed`, `get_any_free_bed`)와 잠.
- 습격·정치·반란·세계 지도 이벤트의 바로 일으키기(조건이 참인 세이브에서). 조건이 거짓일 때 억지로 일으키는 길은 만들지 않았다.
