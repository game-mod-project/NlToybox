# 24. 임신·출생·성장

2026-10-06 · 게임 0.5588.9777.0 · 모듈 0.24.2(조사), 0.25.0 ~ 0.25.2(만든 것) · 세이브: 사용자의 `1_day_5_…`(불러오기만 했다. 저장하지 않았다)

**요약**: 임신의 단계는 특성이다. 다음 단계는 게임의 디버그 함수로 넘기고(출산까지), 임신은 아버지를 적고 1/3기의 특성을 붙여 시작한다
(게임의 `begin_pregnant()`는 게임을 끝낸다). 다음 단계 함수는 게임의 유산 확률을 그대로 타고, 그 확률의 게임 변수를 0 으로 쓰면 유산이 나지 않는다.

자료는 `refs\runtime\preg-session*.answer.txt`(추적 안 함).

## 구조

- 임신의 단계는 **특성**으로 보인다: `pregnant_st1`, `pregnant_st2`, `pregnant_st3`. 3/3기에는 `state_immobilization`도 붙었다.
  출산 뒤 어머니에게 `pregnant_forbid`가 붙는다(이름으로 보아 다시 임신하지 못한다. 그 판정을 재지는 않았다). 난산의 특성은 `hard_childbirth`, 아이는 `kid`.
- 임신 구성요소: `inst:o_character:<n>.__soul.__pregnancy`(ComponentPregnancy). 칸은 `__father_soul_uuid`(아버지의 uuid. 임신 중이 아니면 빈 글), `__sex_in_row`,
  `__debug_is_must_die_from_pregnancy`. 단계나 날짜의 칸은 없다. 주민(`o_dummy`)의 영혼에도 이 구성요소가 있다.
  영혼의 `get_pregnancy()`가 그것을 돌려주는 것으로 보인다(부르지 않았다).
- 정적 메서드(이름): `is_can_pregant`(철자 그대로), `is_pregnant`, `begin_pregnant`, `miscarriage`, `debug_pregnancy_next_stage`, `spawn_kid`, `get_trait_context`, `pub_sub_perform`, `imgui_draw`.
  구성요소는 `global_map_before_next_day` 이벤트를 받는다(하루가 넘어갈 때 단계가 나아가는 것으로 보인다. 재지 않았다).
- 성별: `SoulBasic.get_gender() -> 1 | 0`(게임이 28분 사이에 334번 불렀다). 임신한 영주가 1, 그 아이의 아버지가 0 이었다. 그래서 1 을 여성, 0 을 남성으로 읽는다.
  `is_kid() -> 불리언`은 그 사이에 3,544번 불렸다(기록을 걸어 두지 않는다).

## 인자의 수 (본문의 기계어)

`begin_pregnant`, `debug_pregnancy_next_stage`, `spawn_kid`, `miscarriage`, `is_pregnant`, `is_can_pregant`, `get_trait_context`: 본문이 `argc`를 어디에도 옮기지 않는다 → 인자를 읽지 않는다.
`pub_sub_perform`은 넷까지 읽는다. 스크립트 이름은 `gml_Script_anon_ComponentPregnancy_gml_GlobalScript_ComponentPregnancy_<수>_…`
(`begin_pregnant` 794120877, `debug_pregnancy_next_stage` 976220882, `spawn_kid` 883520879, `miscarriage` 914020880, `is_pregnant` 965620881, `is_can_pregant` 861020878).

28분을 흘리는 동안 임신의 함수들은 한 번도 불리지 않았다.

## 다음 단계 함수: 출산까지

임신 1/3기의 영주(아버지의 uuid 가 적혀 있다)에게 `debug_pregnancy_next_stage()`를 세 번 불렀다(실행의 맨 끝에, 하나씩, 부를 때마다 다시 읽으며).

| 호출 | 뒤의 특성 | 그 밖 |
|---|---|---|
| 1 | `pregnant_st1` → `pregnant_st2` | `-> undefined` |
| 2 | → `pregnant_st3`, `state_immobilization` | 생각의 합 88.83 → 87.62 |
| 3 | 임신의 특성이 없어지고 `pregnant_forbid` | **아이가 생겼다**: `o_character` 5 → 6, 플레이어의 영주로. 게임이 `spawn_kid() -> 구조체`를 한 번 불렀다. `__father_soul_uuid`가 빈 글이 됐다. 생각의 합 +10 |

- 태어난 아이는 **5세**였고(`age 5`), `kid`와 부모의 특성 일부를 가졌고, 능력치는 전투·명령·교육만 있었다(나머지 다섯은 칸이 없다).
- 게임의 오류 파일에 남은 것이 없다.

## 성장

여섯 살 아이의 나이를 `set_age`로 올렸다: 15, 16 에서는 `kid` 그대로이고 `is_kid()`도 참. **18 에서 게임이 `kid`를 떼고 `untitled_lord`를 붙였다**(`is_kid()` 거짓).
진영(`__faction.__system_name`)이 `player`에서 `player_untitled`로 바뀌었다. 17 은 재지 않았다.
그래서 "어른으로"는 나이를 18 로 맞춘다. 그렇게 자란 사람은 플레이어의 영주 목록(진영 `player`)에서 빠진다.

## 게임 변수

`global.__gameplay_vars`에 있다(값은 이 빌드의 것): `pregnancy_chance`, `pregnancy_from_dummy_chance`, `pregnancy_miscarriage_chance`, `pregnancy_mother_die`,
`trait_death_in_childbirth`, `slave_pregnant_chance`. 게임이 그 값을 실행 중에 다시 읽는지는 모른다. 표의 항목 셋(임신 확률 배율, 유산 없음, 출산 중 사망 없음)은 확인 전이다.

## 실행 2: 만든 명령으로 (모듈 0.25.0)

- `person <uuid> pregnancy_next`: 1/3기 → 2/3기. `person lords birth`: 임신한 영주 하나에게 두 번 불러 임신이 끝났다. 다시 보내면 "임신한 영주가 없습니다".
  임신 중이 아닌 사람에게는 함수를 부르지 않고 "임신 중이 아닙니다". `grow_up`: 아이가 소영주가 됐다. 아이가 아니면 "아이가 아닙니다".
- **3/3기에서 한 번 더 부르면 출산이 아니라 유산일 수 있다.** 이번에는 영주의 수가 그대로였고(6 → 6) 어머니에게 생각 `pregnancy_miscarriage`(기분 −24)가 붙었다
  (실행 1 에서는 아이가 태어나 5 → 6 이 됐고 생각의 합이 +10 이었다). 게임 변수 `pregnancy_miscarriage_chance`가 0.2 다: 다음 단계 함수는 게임의 확률을 그대로 탄다.
  그런데 모듈은 "출산했습니다"라고 적었다(단계 3 → 0 만 봤다). **틀린 글이다.** 아이가 생겼는지는 `o_character`의 수가 하나 늘었는지로 본다.
  어머니가 출산 중에 죽을 확률(`pregnancy_mother_die` 0.1)도 같은 길을 탈 것으로 보인다(보지 못했다).
- 생각의 자료: `__soul.__minds.__array_of_minds[n].__generic_mind.__system_name`(`"pregnancy_miscarriage"`), `__moral_modify`, 그리고 `…[n].__lifetime`.
- 표의 항목 셋: 켜자 `pregnancy_chance` 0.5 → 1, `pregnancy_from_dummy_chance` 0.3 → 0.6, `pregnancy_miscarriage_chance` 0.2 → 0, `pregnancy_mother_die` 0.1 → 0,
  `trait_death_in_childbirth` 0.1 → 0. 끄자 모두 원래 값으로 돌아왔다. 게임이 그 값을 따르는지는 아직이다.

## 임신 시작: `begin_pregnant()`는 게임을 끝낸다

여성 영주(19세, 임신 중이 아님)에게: `is_can_pregant()`를 불렀고(참으로 읽혔다), `__father_soul_uuid`에 남성 영주의 uuid 를 적고, `begin_pregnant()`를 불렀다. **게임이 끝났다**:

    ERROR in action number 1 of Step Event1 for object o_actors_transfer_manager:
    I32 argument is undefined
    gml_Script_anon_ComponentPregnancy_…_794120877_… (line 205)

- 본문이 인자를 읽지 않는 것(기계어)과 아버지의 uuid 가 구성요소에 있다는 것만으로는 모자랐다. 무엇이 비어 있었는지는 모른다(부르는 쪽의 `other`일 수도, 다른 칸일 수도 있다. 추측이다).
- **`begin_pregnant()`를 다시 이 꼴로 부르지 않는다.** 실행의 맨 끝에 하나만 불렀고 저장하지 않았다(세이브 폴더에 새 파일 없음. 사용자의 설정은 되돌렸다).
- 다른 길(다음 실행에서 잰다): 단계가 특성이므로, 아버지의 uuid 를 적고 `Traits.trait_attach("pregnant_st1")`(본 꼴)로 1/3기를 붙인 뒤 다음 단계 함수로 넘긴다.
  그 뒤 출산에서 아이가 생기는지, 하루가 넘어갈 때 스스로 나아가는지를 본다.

## 실행 3: 새 방식의 임신 시작, 유산 없음 (모듈 0.25.1)

- 화면: 인물 탭의 "임신·성장"에서 "임신 다음 단계" 단추(모드창의 입력 큐에 누름을 넣었다) → 2/3기, "바로 출산" → 두 번 불러 아이가 태어났다(영주 6 → 7. 게임의 "아이 탄생" 알림이 떴다).
- **임신 시작이 된다**: `is_can_pregant()`가 참 → `__father_soul_uuid`에 남성 영주의 uuid 를 적음 → `trait_attach("pregnant_st1")`. 칸은 적은 대로 남았다.
  다음 단계를 세 번 넘기자 2/3기, 3/3기(`state_immobilization`), 출산: 아이가 생기고, 게임이 `spawn_kid() -> 구조체`를 부르고, 아버지의 칸이 빈 글이 되고, 어머니에게 `pregnant_forbid`가 붙었다.
- 되풀이: `pregnant_forbid`를 떼고 → 임신 시작 → 바로 출산. 어머니 둘에게 번갈아 48번. **임신 시작은 한 번도 실패하지 않았다**(`pregnant_forbid`를 뗀 바로 뒤에도 `is_can_pregant`가 참이었다).

| 표의 항목 | 게임 변수(유산, 출산 중 사망 둘) | 임신 | 출산 | 유산 | 어머니의 죽음 |
|---|---|---|---|---|---|
| `no_miscarriage`·`safe_childbirth` 켬 | 0, 0, 0 | 25 | 25 | 0 | 0 |
| 끔 | 0.2, 0.1, 0.1 | 27(이 실행 25, 앞의 실행 둘) | 22 | 5 | 0 |

- **유산 없음은 게임이 따른다.** 유산 확률이 0.2 일 때 25번에 한 번도 나지 않을 확률은 0.8^25 = 0.4% 다. 끈 쪽의 5/27(19%)은 0.2 와 맞는다.
  게임은 `pregnancy_miscarriage_chance`를 그때그때 읽는다. `no_miscarriage`를 확인으로 올렸다.
- **출산 중 사망은 가리지 못했다.** 끈 채 22번의 출산(난산 특성의 어머니 것 포함)에서 어머니가 한 번도 죽지 않았다. 확률이 0.1 이면 그럴 확률이 10% 다.
  다음 단계 함수의 길이 그 추첨을 하지 않을 수도 있다. `safe_childbirth`는 확인 전으로 둔다.
- `pregnancy_chance`(임신 확률)는 재지 못했다: 모듈의 임신 시작은 그 확률을 타지 않는다.
- 한 실행에서 아이 44명이 태어나 영주가 52명이 됐다(멈춘 채). 탈이 없었고 게임의 오류 파일에 남은 것이 없다.
- 세는 법: 모듈의 로그의 `people: birth on <uuid>: …` 줄. 원격의 답은 첫 줄이 `running person …`이고 결과는 그다음 줄이다(첫 줄을 결과로 읽어 한 번 헛셌다).

## 재지 않은 것

- 하루가 넘어갈 때 단계가 스스로 나아가는 것, 며칠이 걸리는지. 모듈이 붙인 1/3기가 게임의 시간으로도 나아가는지.
- `pregnant_forbid`가 얼마나 가는지(떼면 모듈의 임신 시작은 된다).
- 주민(`o_dummy`)에게 다음 단계 함수를 부르는 것.
- 유산(`miscarriage()`), 출산 중 사망.
