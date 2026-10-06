# 20 — 영주끼리의 평판, 왕에 대한 충성, 따르는 사람, 특성의 글

게임 0.5588.9777.0. 세이브 "아덴"(`Autosave_Morning_day_3`). 영주 다섯: Barra, Amold, Kira, Shchitka, Daven(왕).

## 실행 1 (모듈 0.20.4, 2026-10-06, `lord-session1`)

답: `refs/runtime/lord-session1.answer.txt`. 4일차 06:00 에서 멈춘 채 조사하고, 09:21 까지 8배속으로 흘린 뒤 다시 멈춰 조사했다. 저장하지 않고 껐다(새 세이브 없음). 적재 판정 통과. 백그라운드.

### 도구: 구조체의 주소

0.20.4 부터 `ask`·`method`의 답과 기록의 표본이 구조체의 주소를 적는다(`struct {...} @1369ca73600`, `struct@1369ca73600`).
인자로 온 구조체가 어느 것인지(영혼인가, 인물 영혼인가, 어느 평판 자료인가)를 추측하지 않고 주소로 맞춰 본다.

### 평판(OpinionMinds)

- 영주의 평판 구조체: `inst:o_character:<n>.__soul.__character_soul.__opinions`. 주민(`o_dummy`)에게는 `__character_soul`이 없다(`undefined`).
- `__opinion_minds[]`의 한 칸: `__character_uuid`(대상의 영혼 uuid), `__generic_opinion_mind`(평판 자료), `__owner`(갖는 쪽의 `__character_soul`), `__lifetime`, `__opinion_modify_factor`(1), `__uuid`.
  평판 자료: `__system_name`, `__caption`(현지화 열쇠. `opinion_mind.parent`), `__opinion_modify`, `__duration`, `__stack_limit`, `__tags`.
- **대상을 가리키는 인자는 상대의 `__soul.__character_soul`이다**(주소로 맞춰 봤다. `Faction.get_king_character_soul()`이 돌려주는 것도 그 구조체다).

| 함수(OpinionMinds) | 게임이 부른 꼴 | 본 것 |
|---|---|---|
| `opinion_attach` | `(대상, 자료, 1, undefined) -> 평판`, `(대상, 자료) -> 평판` | 붙을 때마다 `__opinion_minds`가 하나 늘었다 |
| `detach_generic_opinion_mind` | `(대상, 자료) -> undefined` | 그 자료의 평판 하나를 뗀다(안에서 `detach_opinion_mind(평판, false, false)`) |
| `get_number_of_attached_opinion_minds_for_character` | `(대상, 자료) -> 수` | 붙이기 전에 게임이 스스로 부른다. 붙이면 +1, 떼면 -1 |
| `get_opinion` | `(대상) -> 수`, `(대상, true) -> 수` | 세 시간에 1,084번 |
| `is_enemy`, `is_deadly_enemy` | `(대상) -> 불리언` | |
| `detach_all_generic_opinion_minds` | `(자료) -> undefined` | 그 자료의 평판을 모든 대상에게서 뗀다(모듈은 쓰지 않는다: 왕의 평판에는 외교의 것도 들어 있다) |
| `delete_oldest_mind_about_character` | `(대상, 자료) -> undefined` | |
| `clean_cached_opinion` | `()`, `(대상) -> undefined` | |

`is_friends`, `get_hate_state`는 이 실행에서 한 번도 불리지 않았다(본문은 인자 하나를 읽는다). 부르지 않았다.

- `SoulCharacter`: `get_opinion_mind() -> 평판 구조체`, `get_friends() -> 구조체(__array_of_souls, __context_soul)`, `__refresh_gui_after_opinion_change(대상)`,
  `__is_king_whose_opinion_defines_loyalty(대상) -> 불리언`(영주가 왕을 넘기면 참). 뒤의 둘은 `opinion_attach` 안에서 불린다.
- **영주끼리 직접 붙이고 뗐다**(멈춘 채): Barra → Kira 에 `opinion_attach(Kira, debug_positive, 1, undefined)`: 평판 2 → 7, 평판의 수 17 → 18, 센 수 0 → 1.
  `detach_generic_opinion_mind(Kira, debug_positive)`: 2, 17, 0 으로 돌아왔다.
- **좋은 평판 하나의 크기는 사람에 따라 다르다**: Barra 에게는 ±5, Amold 가 왕을 볼 때는 +6 이었다(13 → 19 → … → 121. 18개). 까닭은 재지 않았다. 100 을 넘어도 오른다.

### 왕에 대한 충성

| 함수(SoulBasic. 모두 인자 없음, 게임이 그 꼴로 불렀다) | 본 것 |
|---|---|
| `get_loyalty_to_king() -> 수` | 충성을 따지는 영주는 왕을 보는 평판과 같은 수(Barra 36, Amold 13, Kira 77). 따지지 않는 사람은 100(Shchitka, 왕 Daven, 주민) |
| `get_loyalty_state() -> 0, 1, 2` | 아래 |
| `is_has_loyalty() -> 불리언` | Barra·Amold·Kira 참, Shchitka·Daven·주민 거짓 |
| `is_loyalist() -> 불리언` | 모두 거짓이었다 |

- 충성 상태: Amold 에게 좋은 평판을 하나씩 붙이자 49 까지 1, **55 에서 2**. Barra 에게 나쁜 평판을 하나씩 붙이자 -14 까지 1, **-19 에서 0**.
  `is_enemy(왕)`은 -24 까지 거짓, **-29 에서 참**. `is_deadly_enemy`는 -44 에서도 거짓이었다.
- 붙인 것은 모두 떼어 처음 값으로 돌렸다(센 수 0, 평판 13 과 36).

### 따르는 사람(fealty)

- `__soul.__fealty`(ComponentFealty): `__loyaled_to_uuid`(충성 대상인 영주의 uuid. 없으면 빈 글), `__is_changed_loyalty_on_global_map`.
  주민 25명 가운데 4명에게 대상이 있었다(Barra 2, Kira 2).
- `get_loyaled_to() -> 영혼(__soul) 또는 undefined`(게임이 세 시간에 303번 불렀다).
- **`reset_loyaled_to()`**: 본문이 인자를 읽지 않는다(기계어). 게임이 부르는 것은 보지 못했다. Barra 를 따르던 주민 한 명에게 실행의 맨 끝에 한 번 불렀다:
  `-> undefined`, `__loyaled_to_uuid`가 빈 글이 되고 `get_loyaled_to()`가 `undefined`가 됐다. 게임은 죽지 않았다.
- `try_to_set_loyaled_to`(인자 하나. 형을 모른다), `gml_Script_rebellion_debug_make_soldier_loyaled_to`(인자 하나)는 부르지 않았다.

### 모르는 것

- 붙인 디버그 평판이 세이브에 남는지, 얼마나 가는지(`__duration` -4).
- 충성 상태가 게임에서 무엇을 바꾸는지(반란), 충성 대상을 지운 것이 반란의 셈에 어떻게 먹는지.
- `__opinon_state.<uuid>`의 수(3 과 1 을 봤다)가 뜻하는 것. 영주끼리의 "친구"의 문턱(`is_friends`를 부르지 않았다).
- 게임의 오류 기록에 `_struct_copy_from is not a struct!` 경고가 두 번 났다(13:36:55, :57). 그때 보낸 요청은 `get_opinion` 20번과
  `__is_king_whose_opinion_defines_loyalty` 6번이었다. 어느 호출의 것인지 가리지 못했다(이 경고는 다른 실행에서도 났다: `research/09`, `10`).
  **모듈은 `__is_king_whose_opinion_defines_loyalty`를 부르지 않는다**(`get_loyalty_to_king`, `is_has_loyalty`로 같은 것을 본다).

## 특성의 글 (게임을 켜지 않고 파일로)

- `<게임>\localization\locale_definition.json`: 언어와 파일의 목록. `main.csv`(화면의 이름들), `hints.csv`·`hints_with_icons.csv`·`hints_tutorial.csv`(힌트).
- **CSV 의 꼴**: 첫 줄이 머리(`Key` 또는 `Code`, 그 뒤로 `Russian, English, Comments, Timestamp, English - final, …, Korean, …` 20칸). 줄은 CRLF 로 끝나고 BOM 이 없다.
  큰따옴표로 싼 칸이 쉼표와 줄바꿈(LF)을 담고 그 안의 `""`는 따옴표 하나다. 같은 열쇠가 두 번 나오는 줄이 있다(main 13건, hints 10건).
- **특성의 화면 이름의 열쇠는 `trait.<게임의 이름>`이다**(`main.csv`에 271개. 한국어 칸이 있는 것 270개). 한국어 이름에는 모드창 글꼴(한글, 라틴-1)에 없는 글자가 없다.
- **특성에서 힌트(설명)의 열쇠로 가는 짝은 파일에 없다.** `hint_trait_<이름>` 18, `hint_talent_<이름>` 59, `hint_<이름>` 150, 둘 이상에 맞는 것 4, 어느 것도 아닌 것 40.
  그 짝은 실행 파일 안에 있다(`gml_Script_trait_hint_name_get`, `trait_hint_name_set`). 이름으로 어림하지 않고 게임에서 얻는다.
- 힌트의 글: 첫 줄이 제목(화면 이름), 그 아래가 설명. 꺾쇠 표식(`<hint=…>…</hint>` 2,504, `<b>`, `<red>`, `<green>`, `<img=…>`, `<nbsp>` …),
  값이 들어가는 자리(`{time}`, `{beauty}`, `{soul_uuid_victim}` …), 다른 힌트의 끼움(`[hint_injury_remain_time]`)이 들어 있다.
  한국어 힌트에서 글꼴에 없는 글자는 U+2014(2번)와 키릴 문자 하나(6번)뿐이다.
- 게임의 스크립트(실행 파일의 직접 호출 수): `gml_Script_trait_get_caption`(인자 하나를 읽는다. 61곳), `trait_hint_name_get`(41곳. 인자의 수를 기계어로 가리지 못했다),
  `trait_get_hint_context`(인자 셋), `trait_is_exists`(하나), `trait_property_get`(둘). 인자의 형은 보지 못했다. 부르지 않았다.
- 그래서 **모듈은 화면 이름을 게임의 함수가 아니라 게임 폴더의 `localization\main.csv`에서 읽는다**(`core/Localization`의 `ReadLocalization`. Korean 칸, 비면 English 칸).
  게임 파일의 글은 레포에 싣지 않는다.
