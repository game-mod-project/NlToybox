# 11. 인물·영주·인구·욕구

치트 메뉴 4단계의 조사. 게임 버전 `0.5588.9777.0`, 모듈 0.9.1, 2026-10-06.
실행 묶음 `stage4-session1`(답: `refs/runtime/stage4-session1.answer.txt`). 세이브 "아덴"(4일차, 영주 5, 주민 25).
게임 없이 읽은 것은 세이브 파일(평문 JSON)과 exe 의 기계어다.

## 누가 있는가

- 영주·손님은 `o_character`, 주민·노예·상단의 일꾼은 `o_dummy`다. 둘 다 `__soul`(생성자 `SoulBasic`)을 가진다.
- 플레이어의 사람은 `__soul.__faction.__system_name == "player"`로 가린다. 도착한 상인(`o_character`)은 `"unique_guests"`였다.
- `__soul.__social_strata`: 영주 3(상인도 3), 주민 0·1. 세이브의 291명은 0: 6, 1: 19, 2: 22, 3: 237, 4: 7(뜻은 3 = 영주 말고는 재지 않았다).
- `__soul.__uuid`(글)가 인물의 고유한 이름이다. `inst:o_character:<n>`의 n 은 인물이 드나들면 바뀐다(상인이 오자 5번이 생겼다).
- `__soul.get_name()` → 화면의 이름(`"Barra"`). `__soul.__name`은 번역 열쇠(`"m_name.gwelts.450"`)다.
- 죽음: `c_status.__is_dead`(불리언), `__soul.is_alive()`.
- 세이브의 꼴: `souls.souls[#].basic_soul`(`skills.levels`·`skills.points`, `moral`, `born`, `motive.array_of_motive`, `inventory.money`, `fealthy.loyaled_to`, `look.gender`, `social_strata`, `faction_uuid`)과
  `character_soul`(`nature`, `family`, `opinion`, `romantic`). 플레이어의 진영은 `faction.player_uuid`.

## 값의 자리와 게임의 함수

꼴은 게임이 스스로 부른 것을 기록한 것이다(`record`). "인자 없음"은 기록 또는 기계어(본문이 `argc`를 어디에도 두지 않는다)로 봤다.

| 것 | 자리 | 게임의 함수 (기록한 꼴) |
|---|---|---|
| 능력치 8종 | `__soul.__skills.__level.<combat\|command\|education\|knowledge\|management\|manners\|negotiation\|oratory>`(수), 다음 등급까지의 점수 `__points.<이름>` | `Skills.set_level(기술 구조체, 수)`, `add_level(구조체, 불리언)`, `add_points(구조체, 수[, 불리언])`, `get_level(구조체) -> 수`, `get_max_level(구조체) -> 20` |
| 나이 | `__soul.__born`(날. 나이 = (지금 날 − born) / 2) | `set_age(나이) -> undefined`, `get_age() -> 나이` |
| 기분 | `__soul.__moral`(0~100) | `set_moral(수)`, `get_moral()`, `instantly_update_moral()`. `c_basic_personality.__moral_update()`가 10분쯤마다 다시 셈한다 |
| 생각 | `__soul.__minds.__array_of_minds` | `Minds.attach_generic_mind(생각 구조체) -> 구조체`, `get_total_modify() -> 수`, `detach_all_minds()`(인자 없음. 부르지 않았다) |
| 욕구 6칸 | `__soul.__motive.__motive[6]`(0~100), 상한 `__motive_limit[6]`(100) | `ComponentMotive.get(번호) -> 수`, `set(번호, 수)`, `change(번호, 변화량)`, `restore()`(인자 없음. 부르지 않았다) |
| 특성 | `__soul.__traits.__list_of_traits`(이름의 배열) | `Traits.trait_attach("이름"[, …]) -> uuid`, `trait_detach("이름") -> 뗀 수`, `trait_is_attached("이름") -> 불리언` |
| 통증·병 | 특성으로 든다(`bruise_light`, `cut`, `pneumonia_st_1`, `bleeding_light` …) | `get_pain() -> 수`, `kill_pain()`, `Traits.cure_all_disease()`, `cure_bleeding()`(셋 다 인자 없음) |
| 소지금 | `__soul.__inventory.__money`(493) | `gml_Script_character_gold_add`(직접 호출 18곳. 꼴은 재지 않았다) |
| 노화로 죽는가 | `__soul.__aging.__old.__debug_is_can_die_of_old_age`(인물마다 `true`) | — |
| 임신 | `__soul.__pregnancy`(`__debug_is_must_die_from_pregnancy`, `__father_soul_uuid`) | `is_pregnant() -> 불리언`, `begin_pregnant(구조체)`, `debug_pregnancy_next_stage()`(인자 없음. 부르지 않았다) |
| 충성 | `__soul.__fealty.__loyaled_to_uuid` | `try_to_set_loyaled_to(인자 1)`, `SoulBasic.get_people_loyalty`·`get_loyalty_to_king`(재지 않았다) |

- **욕구의 번호**(`gml_Script_motive_get_caption(번호)`가 돌려준 이름): 0 수면, 1 음식, 2 휴식, 3 신앙심, 4 성관계, 5 돌봄.
- **기술 구조체는 찾지 못했다.** `set_level`·`get_level`이 받는 첫 인자는 구조체인데 전역에서 이름(`negotiation`, `skill`)으로 잡히지 않았다(생성자의 정적 변수로 보인다. 추정).
  그래서 능력치는 `__level.<이름>`에 바로 쓴다.
- 특성의 이름 282개는 `inst:o_data.game_trait_list`에 있다. 생각의 종류는 `inst:o_data.mind_*`(구조체. `__moral_modify`, `__duration`, `__system_name`)다.
  `inst:o_data.mind_debug_totally_happy`는 기분 +100, 하루(86,400초) 동안이다(`mind_debug_totally_sad`도 있다).
- 경험이 쌓이는 빠르기: `global.__gameplay_vars.skill_increase_in_use_<management 0.5|education 2|oratory 4|manner 2|knowledge 0.25|command 2|negotiation 6|training_ground 3>`(읽기만 했다).

## 그 자리에서 해 본 것

| 한 것 | 본 것 |
|---|---|
| `write …:0.__soul.__motive.__motive[3]=100`(신앙심 0) | 남았다. `get(3)` → 100. 그 뒤 게임이 평소대로 줄였다(99.4 → 95.5) |
| `write …:0.__soul.__skills.__level.management=12`(원래 2) | 남았다. **그 사이의 자동 저장에 12 로 들어갔다**(세이브 파일을 읽었다). 인물 창으로는 보지 못했다. 2 로 되돌렸다 |
| `…:3.__soul.__minds.attach_generic_mind p:inst:o_data.mind_debug_totally_happy` | `get_total_modify` 8.07 → 108.07. 기분이 43 → 100 이 됐다(게임 시간 40분쯤 뒤. `instantly_update_moral()`은 바로 올리지 않았다) |
| `…:0.__soul.set_age n:30` 뒤 `n:40` | `get_age` 30, `__born` −76 → −56. 다시 40, −76 |
| `…__traits.trait_attach s:brave` 뒤 `trait_detach s:brave` | 목록에 생겼다 사라졌다(`-> uuid`, `-> 1`) |
| `trait_attach s:bruise_light` 뒤 `kill_pain()` | 통증 0 → 1 → 0.33. `kill_pain`은 `painkiller` 특성을 붙인다(통증을 3분의 1로). 둘 다 `trait_detach`로 뗐다 |
| `cure_all_disease()`(병이 없는 인물) | 오류 없이 지나갔다. 병이 낫는 것은 보지 못했다(병든 인물이 없었다) |
| `write …__migration_manager.__next_day_migrants_bonus=3` | 18:15 에 "3명의 이주자가 도착했습니다". 게임이 `__try_to_spawn_migrants(undefined, 구조체)`를 한 번, `SoulsData.request_soul_for_dummy(구조체, undefined, 구조체)`를 세 번 불렀다. 값은 0 으로 돌아가고 `__active_migrants_bonus`가 3 이 됐다. 주민 31 → 34 |

## 인구

- 이주는 `inst:o_game_map_controller.__province.__migration_manager`(`MigrationManager`)가 한다. 저녁(이 실행에서는 18시쯤)에 한 번 돈다.
  `__happiness_thresholds`는 (평균 행복, 그날의 이주민 수)의 짝 10개다: 10 → −4, 20 → −3, 30 → −2, 40 → −1, 50 → +1, 60 → +2, … 100 → +6.
- `__next_day_migrants_bonus`에 쓴 수만큼 다음 이주 때 더 온다(위 표). 이 열쇠는 세이브에 있다(`next_day_migrants_bonus` 1건).
- `MigrationManager.add_next_day_migrants_bonus(인자 1)`, `set_migration_bonus(인자 2)`, `toggle_migration_allowing()`는 부르지 않았다.
- `Province.spawn_new_player_dummy(자리?, 문화?)`는 이주민이 올 때 불리지 않았다(0번). 본문은 `spawn_new_player_dummy_soul(둘째)`의 결과와 첫째를 `__spawn_new_player_dummy_actor`에 넘긴다(기계어). 인자의 형을 몰라 부르지 않았다.
- 게임의 디버그 소환기 `inst:o_debug.debug_spawner`(`CreatureSpawner`): `__spawn_peasant()`·`__spawn_lord()`(인자 없음)·`__spawn_create(인자 2)`. 소환 자리 같은 상태를 스스로 든다. 부르지 않았다.
- `gml_Script_rebellion_debug_spawn_player_dummy(첫째, [불리언])`은 반란 시험용이다(안에서 `rebellion_debug_get_spawn_node`·`rebellion_debug_make_actor_unhappy`를 부른다). 첫째의 형을 몰라 부르지 않았다.
- 인구의 수: `…__province.__overpopulation_manager.__cached_population`(24), 문화별 `…__migration_manager.__current_people_count.<gwelts|makha|tanaya|voruns>`.

## 처음 잰 것

- **인자가 없는 함수는 기계어로 가릴 수 있다.** YYC 의 스크립트 함수는 `argc`(r9d)를 레지스터나 스택(`mov dword ptr [rsp+20h],r9d`)에 옮겨 둔 뒤 견준다.
  어디에도 옮기지 않는 본문은 인자를 읽지 않는다. 기록으로 꼴을 아는 함수 여덟(`get_age`, `is_alive`, `set_moral`, `set_age`, `Motive.get`·`change` …)에서 맞았다.
  들머리에서 `0x14018A9B0`을 `r8d = N`으로 부르는 함수는 인자를 N 개까지 받는다(생략할 수 있는 인자가 있다. `trait_attach` N=4, `add_points` N=3).
- **훅 자리 64개는 이번에도 바닥났다.** 기록 63개 + 사용자가 켠 치트의 훅 2개. 그 뒤로는 훅 치트(거래 배율)를 새로 켤 수 없었다. 상인이 왔지만 거래 배율을 확인하지 못했다.
- 세이브에 남는 것: 능력치·욕구·기분·나이(인물의 자료라 남는 것이 맞다), `next_day_migrants_bonus`. 남지 않는 것: `is_can_die_of_old_age`(열쇠 0건).
- 3나-3 에서 넘어온 임금: 이 세이브에서도 읽지 않았다(다음 조사에서 본다).

## 이 실행이 게임에 남긴 것

- 03:48 의 자동 저장(`아덴_Autosave_Evening_day_4 …`)에 그때의 시험 값이 들어 있다: Barra 의 관리 12(원래 2).
- 저장하지 않고 껐다. 실행 중에 준 것: Barra 의 신앙심 100, Shchitka 의 디버그 행복 생각(하루), 이주민 3명.

## 확인 실행 (모듈 0.10.0 → 0.10.1, 2026-10-06)

실행 묶음 `stage4-session2`(0.10.0)와 `stage4-session3`(0.10.1). 세이브는 사람이 누르지 않고 불러왔다(아래 "세이브를 직접 불러오기").

| 한 것 (인물 패널과 같은 길: 원격 `person …`) | 본 것 |
|---|---|
| 능력치 쓰기: 관리 12 → −3, 전투 99, 지휘 −5 | 9, 20(당겨짐), 0. **게임의 인물 창이 관리 9, 전투 20, 명령 0 을 보였다**(초상을 눌러 연 창. `refs/ui/v4-sheet1.png`) |
| 욕구 100, 500 | 100, 100(상한으로 당겨짐) |
| 나이 30, 39 | `get_age` 30, 39 |
| 특성 붙이기·떼기(`brave`) | 목록에 생겼다 사라졌다. 없는 이름(`not_a_trait`)은 거부 |
| 한 영주에게 `pneumonia_st_1`·`bleeding_light`·`bruise_light`·`cut`을 붙이고 "치료" | 넷 다 사라졌다(통증 7 → 0). 병은 `cure_all_disease()`, 출혈은 `cure_bleeding()`, 부상 둘은 `trait_detach`가 뗐다 |
| "행복하게"(아이, 기분 35) | 게임 시간 20분쯤 뒤 기분 98. 게임의 인물 창에는 그 생각이 `mind.debug_totally_happy`로 보인다(번역 없음) |
| 일괄: 영주 전원 능력치 20·욕구·행복, 사람 전원 치료·욕구 | 5/5, 29/29. 손님(진영 `unique_guests`)은 대상에 들지 않았다 |

| 표의 항목 | 본 것 |
|---|---|
| 모든 욕구 채워 두기 | 플레이어의 사람 29명의 여섯 칸이 100 으로 유지됐다. 손님의 칸은 그대로(59, 34, 85 …)였다 |
| 배고픔 없음 / 피로 없음 | 네 칸을 50 으로 쓴 주민에서 음식 칸만 / 수면·휴식 칸만 100 이 됐다 |
| 언제나 행복 | 첫 바퀴에 28명에게 생각을 붙였다. 기분 35 → 98, 70 → 98 |
| 노화로 죽지 않음 | 깃발이 `false`가 되고(손님은 `true` 그대로) 끄자 다시 `true`가 됐다. **늙어도 죽지 않는지는 보지 못했다** |
| 날마다 추가 이주민 2 | `__next_day_migrants_bonus` 2, 끄자 0. 실제 도착은 조사 실행에서 봤다(3 → 3명) |

- **`c_status.__is_dead`는 게임의 캐시다.** 평소 `false`이고, 특성이 바뀌면 `-4`(아직 셈하지 않음)로 비워졌다가 게임이 다음에 물을 때 다시 채운다.
  0.10.0 은 `-4`를 죽음으로 읽어, 특성을 붙인 바로 뒤(같은 틱)의 명령이 그 사람을 놓쳤다("대상이 없습니다", 치료 뒤의 일괄이 29 → 27명).
  0.10.1 은 `true`·`false`만 믿고 그 밖에는 `__soul.is_alive()`(인자 없음 → 불리언)로 묻는다. 고친 뒤 같은 차례가 29/29 였다.
- 갈래 0 은 노예다(주민 Flora: `__social_strata` 0, 특성 `slave`, 진영 `player`). 플레이어의 사람으로 친다.
- 욕구를 채워 두면 식량이 줄지 않는가: 켠 2.6시간(게임 시간) 동안 음식 다섯 종의 수가 그대로였다. 끈 대조는 재지 못했다(한 번 채우면 하루쯤 먹지 않는다. 하루를 돌려야 가려진다).

## 세이브를 직접 불러오기

- 불러오기는 `global.__game_load_operator`(`GameLoadOperator`)가 한다. 세이브의 목록은 `__saves[n]`(구조체: `__file_name`, `__file_path`, `__metadata_game_day` …),
  가장 최근 것의 이름은 `__last_game_save_name`.
- `load_save(세이브 구조체)`: 본문이 인자 하나를 구조체로 읽는다(기계어: 인자의 갈래가 6 인지 보고 멤버를 셋 읽는다). `get_last_game_save()`(인자 없음)가 그 구조체를 돌려주고
  `is_can_load_save()`가 `true`였다. 메인 메뉴에서 `method global.__game_load_operator.load_save p:global.__game_load_operator.__saves[3]`로 불러 세이브가 불러와졌다(두 번).
  게임의 로그(`catched_errors_…txt`)에는 사람이 불러올 때와 같은 `Load game: <이름>` 줄이 남는다.
- `tools/load-save.ps1 -Name <이름의 일부>`가 이것을 한다(메인 메뉴를 기다리고, 이름으로 고르고, 게임 화면이 될 때까지 기다린다).
- 게임 창을 눌러야 할 때(이야기 창의 "계속하기", 초상): 게임 창이 앞에 있으면 `SetCursorPos` + `mouse_event`(왼쪽 누름·뗌)가 먹는다. 자리는 `shot`으로 본다.
  4일차 08:00 에 이야기 창 "신제국"이 떠 게임이 멈춘다(`time_warp` 0). "계속하기"는 1920x1080 에서 (960, 1001)이었다.
- 자동 저장은 게임 시각 06시(아침)와 18시(저녁)에 생기고, 같은 종류의 앞 자동 저장을 갈아 끼운다(저녁 것이 앞의 저녁 것을 지웠다).
  시험 값이 든 채 그 시각을 넘기면 사용자의 자동 저장이 시험 값으로 바뀐다. 확인 실행 둘은 그 시각을 넘기지 않았다.

## 확인하지 못한 것

- "노화로 죽지 않음"이 실제로 죽음을 막는가. 욕구를 채워 두면 식량 소비가 멈추는가(하루를 돌린 대조).
- 치료가 떼는 부상 19개 가운데 `bruise_light`·`cut` 말고의 것. `Motive.restore()`가 무엇을 하는가.
- 어른을 아이 나이로, 아이를 어른 나이로 바꿀 때 `kid` 특성이 따라오는가.
- 충성·관계(호감)의 자리와 함수. 영입 비용. 인구를 줄이는 길. 임신·출생을 바로 일으키는 길. 임금·세금·유지비.
- 주민이 40명을 넘는 세이브에서 표 항목의 순회(묶음이 여러 틱에 걸친다. 시험으로만 봤다).
