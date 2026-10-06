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

## 전투 (첫 조사에서는 재지 못했다. 아래 "전투"에서 쟀다)

- 싸움이 일어나지 않았다: `SoulBasic.take_damage`·`battle_hit` 0번. 치트 표의 전투 스위치(`o_debug`)도 확인하지 못했다.
- 읽은 것: `SoulBasic.get_mortal_pain_threshold() -> 40`(인자 없음. 계속 불린다), `get_combat_level_in_battle()`(인자 없음. 0번), `battle_hit(인자 5)`,
  `take_damage(글, 구조체, 불리언) -> true`(`research/11`의 실행에서 80번: `"cut"`, `"light_cut"`, `"bruise_light"`).
  수를 돌려주는 둘에 배율을 걸면 양쪽 모두에게 걸린다(영혼의 메서드라 적의 영혼도 같은 함수를 쓴다).
- 아군에게만 걸려면 훅이 `self`(영혼 구조체)가 플레이어의 것인지 가려야 한다. 0.13.0 에서 만들었다(아래 "확인 실행 (모듈 0.13.0)").

## 이 실행이 게임에 남긴 것

- 저장하지 않고 껐다(게임 시각 11시쯤. 자동 저장 시각을 넘기지 않았다). 실행 중에 병사 셋과 도적 하나를 만들고 농민 출신 하나를 16 금화에 고용했다.

## 확인 실행 (모듈 0.12.0, 2026-10-06)

실행 묶음 `stage5b-session2`(답: `refs/runtime/stage5b-session2.answer.txt`). 같은 세이브. 게임 시각 06:00 ~ 16:52, 저장하지 않고 껐다.

| 한 것 | 본 것 |
|---|---|
| `person spawn_soldier amount=3`(멈춘 채) | 주민·병사 25 → 28, 병영의 목록 3칸, 게임의 군대 창에 전사 3(`refs/ui/b2-hire-on.png`), 모드창 "플레이어의 병사 3명"(`refs/ui/b2-army1.png`) |
| `person spawn_soldier amount=5`(시간이 흐르는 채, 한 틱에) | 32 → 37, 병영 11칸. 오류 없음 |
| `amount=0` | 거부 |
| `cheat hire_cost on` 뒤 고용 창 | 16·14·12·10, 수감자 5·5·4 |
| `cheat hire_cost off` 뒤 고용 창을 다시 열기 | 160·140·120·100, 수감자 45·52·42(`refs/ui/b2-hire-off.png`) |

## 디버그 소환기

`inst:o_debug.debug_spawner`(`CreatureSpawner`): 게임의 디버그 창에서 종류를 고르고 지도를 눌러 소환하는 도구다(`__spawn_functions`: wolf, pig, dog / lord, knight / peasant, slaves, soldier / criminal, thug / forest bandit / slave kid).
정적 메서드 `__spawn_wolf`·`pig`·`dog`·`lord`·`knight`·`peasant`·`slave`·`soldier`·`criminal`·`thug`·`bandit`·`slave_kid`와 `__spawn_create`, `imgui_control`. `__faction_name`(""), `__is_change_faction`(false)이 있다.

- **인자 없이 부르면 마우스가 가리키는 지도의 자리에 하나를 만든다.** 커서를 화면 (700, 650)에 두고 `__spawn_soldier()` → `o_dummy`가 (1942.5, 909)에, (780, 650)에 두고 `__spawn_bandit()` → (2047.5, 909)에 생겼다.
- 불러서 본 것(각각 한 번 이상. 모두 `-> 인스턴스`):

| 메서드 | 생긴 것 |
|---|---|
| `__spawn_soldier()` | `o_dummy`, 진영 `player`, 갈래 2. 경갑(6)과 나무 창(11). 병영의 목록에 오른다 |
| `__spawn_knight()` | `o_dummy`, 진영 `player`, 갈래 2("Krasa 부인") |
| `__spawn_peasant()` | `o_dummy`, 진영 `player`, 갈래 1 |
| `__spawn_slave()` | `o_dummy`, 진영 `player`, 갈래 0 |
| `__spawn_lord()` | `o_character`, 진영 `player`, 갈래 3("Nara". 특성 `__wolves_wont_attack__`, gambler, brave) |
| `__spawn_bandit()` | `o_dummy`, 진영 `forest_bandits`. 싸움을 걸지 않았다: 하나는 (8117, 6327)로 달려갔고, 뒤의 실행에서는 셋도 열둘도 (27800, 16700) 쪽으로 가서 사라졌다 |
| `__spawn_wolf()` | `o_dummy`가 아니다. 다섯 마리를 풀었지만 세 시간 동안 싸움이 늘지 않았다 |
| `__spawn_thug()`, `__spawn_criminal()` | `o_dummy`. 사람 목록에 다른 진영으로 나오지 않았다(플레이어의 진영으로 생긴 것으로 보인다. 추정). 한 시간 동안 싸움이 없었다 |

- 기계어: `gml_Script_debug_spawn_army`는 여덟 곳에서 인자 여섯으로 불린다(소환기의 메서드들은 그것을 부르지 않았다: 기록 0번).
  `gml_Script_rebellion_debug_spawn_player_dummy`(인자 둘까지)·`_character`(없음)·`make_*_rebellion_possible`(없음)·`allow_rebellions`(없음)은 부르지 않았다.

## 장비 (되지 않았다)

- 병사의 소지품에 중갑(7)·검(12)·방패(15)를 하나씩 넣자(`person <uuid> item_add`) 그 자리에서 착용됐다(`__equipment.__cached_armor.__resource` 7, `__cached_first_arm` 12, `__cached_second_arm` 15).
- `__equipment.give_possible_equipment()`를 인자 없이 부르자 원래대로 돌아갔다(단검, 갑옷 없음. 소지품에서도 사라졌다). 영지 창고에 같은 것을 셋씩 넣고 다시 불러도 그대로였다.
- 다시 넣어 두고 세 시간 반을 돌리자 게임이 `give_possible_equipment(구조체)`를 17번 불렀고 장비는 다시 단검이 됐다. **소지품에 넣은 장비는 남지 않는다.** 그 구조체가 무엇인지는 모른다.

## 전투

- **싸움을 붙이는 길은 찾지 못했다.** 아래의 싸움들은 게임이 스스로 건 것이고 무엇이 맞았는지는 보지 못했다. 두 실행 모두 4일차 14:20 쯤에 `take_damage`가 몰려 불렸다
  (0.13.0 의 실행: 14:18 ~ 14:45 에 162번, 그 뒤로 0번. 그동안 `o_dummy`의 수는 그대로였고 플레이어의 사람의 상처도 그대로였다. 사냥처럼 사람이 아닌 것이 맞는 일로 보인다. 추정).
  16시쯤에는 노예 상단(`traders_slaves` 13명과 상인 하나)이 왔다.
- 도적 하나를 풀고 세 시간 반 뒤: `gml_Script_battle_start(배열, 정수, 구조체, 배열, 정수, 구조체) -> 구조체` 3번((65570, 2)가 둘, (128, 4096)이 하나), `SoulBasic.take_damage(글, 구조체, 불리언) -> true` 26번,
  `SoulBasic.battle_hit(구조체, 배열, undefined, 정수[, 수]) -> 구조체` 45번, `get_combat_level_in_battle() -> 10, 7` 150번.
  주민 둘의 상처가 늘었다(Keelan: 세이브의 `light_cut`, `cut`×4, `inflamed_wound`, `wound_deep`에 `lost_eye`, `bleeding_middle`, `scars`×3 이 더해졌다. 뒤에 목록에서 사라졌다.
  Marna: `cut`×2 → 6개). 도적도 사라졌다. 누가 때렸는지는 보지 못했다.
- 상처의 이름(`take_damage`의 첫 인자): `injury_face`, `bruise_light`, `bruise`, `lost_eye`, `light_cut`, `cut`, `stun`, `wound_deadly`. 사람의 특성으로 붙는다.
- **`take_damage`를 건너뛰면 상처가 생기지 않는다.** `override … b:0 skip`(모두에게) 뒤 도적 셋을 풀자 두 시간 반 동안 6,371번 불렸고(`wound_deadly` 포함. 도적이 14명으로 불어났다)
  57명의 상처 특성은 8개 그대로였다. 들어올 때 `Result`는 `undefined`였다(`(skipped, Result came as undefined) => false`). 아무도 쓰러지지 않아 싸움이 끝나지 않았다.
- 군대의 관리자: `inst:o_game_map_controller.battle_squads_direct_controller`(`order_attack`, `order_move`, `order_follow`, `get_selected_squad`),
  `battle_squads_manager`(`__array_of_squads` 0칸. 분대는 군대 창의 "분대 만들기"로 만든다), `battle_debugger`(`__array_of_battles`),
  `__raids_manager`(`try_to_set_raid`, `is_can_set_raid`, `is_raids_enabled`, `__debug_override_raid_budget`. 이 실행에서 0번), `…__province.__rebellions_manager`(`is_can_start_rebellion`: 0번).

## 확인 실행 (모듈 0.13.0, 2026-10-06)

실행 묶음 `stage5b-session3`(답: `refs/runtime/stage5b-session3.answer.txt`). 같은 세이브. 06:00 ~ 16:46, 저장하지 않고 껐다. 적재 판정 통과.

| 한 것 | 본 것 |
|---|---|
| `person spawn_soldier amount=20`(한 틱에) | 25 → 45, 병영 20칸. 오류 없음 |
| `person spawn soldier`·`knight`·`peasant`·`slave`·`lord` | 하나씩 생겼다(주민 45 → 49, 영주 5 → 6). 새 영주가 게임의 영주 줄에 올라왔다(`refs/ui/b3-scene.png`) |
| `person spawn bandit` | 거부(플레이어의 사람만 만든다) |
| 군대·인구 패널 | 단추와 안내 글이 그려진다(`refs/ui/b3-shield.png`, `b3-people.png`) |
| `cheat ally_invincible on` | 훅이 걸렸다: `override -> false (skip …) (only when self is one of the player's souls)`. 옆의 글 "플레이어의 사람 54명" |
| 켠 채 14:18 ~ 14:45 의 싸움 | `take_damage` 162번이 모두 지나갔다(막은 호출 0). 플레이어의 사람의 상처 특성은 9개 그대로(맞은 것이 플레이어의 사람이 아니었다) |
| 켠 채 `method inst:o_dummy:25.__soul.take_damage s:bruise_light p:<다른 병사의 영혼> b:0`(플레이어의 병사) | `-> false`, 막은 호출 1, 그 병사의 특성 그대로(`wet_feet homeless human`) |
| 같은 호출을 상인(진영 `traders`)에게 | `-> true`, 지나간 호출 163, 상인에게 `bruise_light`가 붙고 통증 0 → 1, 생각의 합 −6.61 → −7.61 |

- 그래서 아는 것: 훅의 self 는 영혼 구조체의 주소와 같다(직접 부른 호출에서). `take_damage`는 self 의 영혼에 상처를 입히고, 둘째 인자로 영혼을 받는다.
- **모르는 것: 게임이 스스로 건 싸움에서 플레이어의 사람이 맞을 때**(그런 호출이 이 실행에 없었다). 그래서 치트 표의 `ally_invincible`은 확인 전이다. (0.16.1 에서 확인됨으로 올렸다: 게임의 호출에서 self 가 영혼이라는 것을 봤다. `research/16`)
  다음에 잴 때는 켠 채 `records <take_damage>`의 "applied to N call(s)"와 표본의 `[self in, …]`을 본다(0.13.1 부터 표본에 적힌다).

## 0.13.1 의 되짚기와 적재 판정 (2026-10-06)

실행 묶음 `stage6-session1`의 앞부분(6단계의 조사 실행에 얹었다). 적재 판정 통과.

- `person spawn soldier` → 하나. `cheat ally_invincible on` 뒤 그 병사에게 `take_damage`를 직접 부르자 `-> false`, 표본에
  `("bruise_light", struct, false) [self in, other out, arg1 in] -> (skipped, Result came as undefined) => false`, "applied to 1 call(s), let 0 pass". 끄자 바꾸기가 풀렸다.
- **이 실행은 내가 부른 함수 때문에 GML 오류로 끝났다**(저장하지 않는 실행이었다): `…__province.__army_loyalty.get_summary_army_loyalty()`를 인자 없이 불렀다.
  기계어의 인자 맞춤(N=2)을 "생략할 수 있다"로 읽은 것이 틀렸다. 오류: `I32 argument is undefined`(`get_summary_army_loyalty` → `get_army_loyalty` → 안쪽 함수).
  충성도 함수들(`get_army_loyalty`, `get_summary_army_loyalty`, `get_people_loyalty`, `get_summary_people_loyalty`)은 인자 둘을 받는 함수로 보고, 게임이 부르는 꼴을 본 뒤에만 부른다.

## 확인하지 못한 것

- 전투의 사기와 승패, 분대의 싸움(이 실행의 싸움은 분대 없이 붙었다), 전투 스위치들(`o_debug`)의 효과. 병사를 내보내는 길(`SoldiersBarracksManager.fire(인자 3)`의 꼴).
- 소환한 병사의 임금과 계약(무엇으로 치는지 재지 않았다). 충성할 영주를 넘겼을 때의 동작. 병영의 정원.
- 장비를 남게 주는 길(`give_possible_equipment(구조체)`의 구조체). 모집 시간(`get_hiring_time`이 `undefined`를 준다).
