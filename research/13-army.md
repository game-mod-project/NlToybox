# 13. 군대

치트 메뉴 5단계의 나머지(군대·전투)의 조사. 게임 버전 `0.5588.9777.0`, 모듈 0.11.1, 2026-10-06.
실행 묶음 `stage5b-session1`(답: `refs/runtime/stage5b-session1.answer.txt`). 세이브 "아덴"(4일차 06:00. 병영도 병사도 없다).

## 병사를 만든다

- `gml_Script_rebellion_debug_spawn_player_soldier()`: 인자를 하나까지 받고 생략할 수 있다(기계어: 들머리의 인자 맞춤 N=1). **인자 없이 부르자 병사가 생겼다**(`-> 인스턴스`).
  안에서 `gml_Script_rebellion_debug_get_spawn_node() -> 구조체`와 `gml_Script_actor_spawn(영혼 구조체, x, y) -> 인스턴스`가 불렸다(기록).
  `rebellion_debug_make_soldier_loyaled_to`는 불리지 않았다(생략한 인자가 충성할 영주로 보인다. 추정).
- 세 번 부르자: `o_dummy` 25 → 28, `…__soldiers_barracks_manager.get_array_of_soldiers()`가 0 → 3칸, **게임의 군대 창에 전사 3명**(전투 10, 7, 6. `refs/ui/a1-army.png`), HUD 의 병사 수 3.
- 생긴 병사: `o_dummy`, 진영 `player`, `__soul.__social_strata` 2(병사), 능력치는 전투·지휘만, 무기는 단검(`__equipment.__cached_first_arm.__resource` 14), 갑옷 없음, 병과 `__soldier_class.__system_name` `"unknown"`.
  반란 시험용 자리(지도 가장자리. x 15802, y 22455)에 나타나 네 시간쯤(게임 시간) 사이에 마을(x 7423, y 4923)까지 스스로 걸어왔다.
- 디버그 소환기 `inst:o_debug.debug_spawner`(`CreatureSpawner`)의 `__spawn_wolf`·`__spawn_bandit`·`__spawn_thug`·`__spawn_soldier`·`__spawn_knight`·`__spawn_criminal`·`__spawn_peasant`·`__spawn_lord`는 인자가 없다(기계어).
  `__spawn_bandit()`을 부르자 도적 하나가 생겼다(`o_dummy`, 진영 `forest_bandits`, 갈래 2. x 2362, y 495). 그 도적은 네 시간 동안 제자리에 서 있었다(싸움이 붙지 않았다).

## 모집

- 고용 창(군대 창의 "전사 고용")의 값은 `SoulBasic.get_soldier_cost()`가 돌려주는 수다(인자 없이 178번: 100, 120, 140, 160. 풀려난 수감자는 42~52).
  스크립트 이름: `gml_Script_anon_SoulBasic_gml_GlobalScript_SoulBasic_11987516072_SoulBasic_gml_GlobalScript_SoulBasic`.
- **`override … x:0.1 whole`을 걸고 창을 다시 열자 값이 16·14·12·10, 수감자 5·5·4 가 됐고**(`refs/ui/a1-hire2.png`), 160짜리 농민 출신을 고용하자 금화가 1,901 → 1,885 가 됐다(16).
- 고용: `…__solders_barracks_hiring_manager.try_to_hire(구조체) -> true`(게임이 한 번). 농민 출신은 바로 병영에 들어왔다(병사 3 → 4. 있던 주민의 갈래가 2 가 됐다).
- `get_hiring_time(구조체) -> undefined`(7번). 수가 아니라 배율을 걸 수 없다.

## 전투 (재지 못했다)

- 싸움이 일어나지 않았다: `SoulBasic.take_damage`·`battle_hit` 0번. 치트 표의 전투 스위치(`o_debug`)도 확인하지 못했다.
- 읽은 것: `SoulBasic.get_mortal_pain_threshold() -> 40`(인자 없음. 계속 불린다), `get_combat_level_in_battle()`(인자 없음. 0번), `battle_hit(인자 5)`,
  `take_damage(글, 구조체, 불리언) -> true`(`research/11`의 실행에서 80번: `"cut"`, `"light_cut"`, `"bruise_light"`).
  수를 돌려주는 둘에 배율을 걸면 양쪽 모두에게 걸린다(영혼의 메서드라 적의 영혼도 같은 함수를 쓴다).
- 아군에게만 걸려면 훅이 `self`(영혼 구조체)가 플레이어의 것인지 가려야 한다. 그 길은 만들지 않았다.

## 이 실행이 게임에 남긴 것

- 저장하지 않고 껐다(게임 시각 11시쯤. 자동 저장 시각을 넘기지 않았다). 실행 중에 병사 셋과 도적 하나를 만들고 농민 출신 하나를 16 금화에 고용했다.

## 확인하지 못한 것

- 전투의 모든 것(피해, 사기, 승패), 전투 스위치들의 효과. 병사를 내보내는 길(`SoldiersBarracksManager.fire(인자 3)`의 꼴).
- 소환한 병사의 임금과 계약(용병이 아니다. 무엇으로 치는지 재지 않았다). 충성할 영주를 넘겼을 때의 동작.
