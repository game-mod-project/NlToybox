# 03 — 데이터 파일의 모양과 키별 근거 (데이터 오버레이 단계 1의 준비)

조사일 2026-10-05. 대상: Norland `0.5588.9777.0` (Steam buildid `25211575`). **게임을 켜지 않았다.**
게임 폴더는 읽기만 했고, 런타임 쪽은 단계 0·0b의 덤프(`refs/runtime/`)를 다시 읽었다.

단계 1(오버레이 도구)의 설계가 기대는 사실을 여기 모은다. 수정기의 문법, 카탈로그의 등급, 프리셋의 경로 식이 모두 이 기록에서 나왔다.

## 게임 폴더의 JSON

`*.json`은 **4,701개**다.

| 폴더 | 수 |
|---|---|
| `building_constructor\` | 4,099 |
| `actor_constructor\` | 269 |
| `sounds\` | 201 |
| `knowledge\` | 121 |
| 최상위 | 6 (`audio_settings`, `battle_params`, `debug_params`, `director_params`, `gameplay_variables`, `road_tileset`) |
| `generator\` | 2 |
| `localization\`, `maps\` | 1씩 |
| `.omc\` | 1 (게임의 것이 아니다. 아래) |

스펙 §2의 "4,499개"는 `sounds\`의 201개와 `.omc\`의 1개를 뺀 수다.

`.omc\state\last-tool-error.json`은 게임 폴더를 작업 폴더로 열어 둔 Claude Code 플러그인(oh-my-claudecode)이 2026-10-04 18:04에 만든 파일이다.
이 레포의 도구가 놓은 것이 아니고 게임이 읽는 파일도 아니다. 지우지 않았다.

## 문법 (4,701개 전부)

읽을 때 쓴 것은 닫는 괄호 앞의 쉼표만 봐주는 스캐너다. 4,701개가 모두 읽혔다. 표준 파서가 읽는 4,698개에서는
스캐너가 찾은 스칼라 값이 모두 표준 파서(Python `json`)의 값과 같았다.

| 항목 | 결과 |
|---|---|
| 인코딩 | 전부 UTF-8. BOM이 붙은 파일은 없다. 4,698개는 ASCII만 쓴다 |
| 뿌리 | 전부 객체. 가장 깊은 중첩은 8 |
| 줄바꿈 | 한 줄짜리 4,449개, CRLF 251개, LF 1개. 섞인 파일은 없다 |
| 표준에서 벗어나는 것 | **닫는 괄호 앞의 쉼표뿐이다.** `debug_params.json`(객체 4곳), `gameplay_variables.json`(객체 1곳), `localization\locale_definition.json`(배열 1곳) |
| 한 번도 없는 것 | 주석, 작은따옴표 문자열, 따옴표 없는 키, `NaN`·`Infinity`, 지수 표기, `+`로 시작하는 수, 한 객체 안의 같은 키, 값 뒤의 군더더기 |
| 문자열의 이스케이프 | `\"`, `\\`, `\n`, `\r`뿐이다 (합쳐 12번). `\u`는 없다 |
| 리터럴 | `true` 8,185, `false` 8,745, `null` 604 |
| 키 | 점, 대괄호, 별표, 따옴표가 든 키는 없다. 공백이 든 키는 둘: `"messenger_cost "`(뒤 공백, `gameplay_variables.json`)와 `"<unknown built-in variable>"`(`building_constructor\`의 672개 파일) |
| 수로 된 키 | 있다. `battle_params.json`의 `unit_skill_stage`가 `"0"`, `"1"`을 키로 쓴다 |

수는 254,251개다.

| 표기 | 수 | 예 |
|---|---|---|
| `.0`으로 끝나는 소수 | 230,727 | `5.0`, `-3.0` |
| 정수 | 22,277 | `2000`, `-6` |
| 그 밖의 소수 | 1,247 | `0.5`, `0.20000000298023224` |

소수 가운데 509개는 가장 짧은 왕복 표기가 아니다(`0.90000000000000002`, `0.20000000000000000`). 파싱해서 다시 쓰면 글자가 달라지는 자리다.

**게임의 JSON 파서가 무엇을 받는지는 재지 않았다.** 지수 표기나 주석을 읽는지 모른다. 그래서 도구는 게임 파일에 실제로 있는
표기(부호, 숫자, 소수점)만 쓴다.

## 다섯 대상 파일

카탈로그가 다루는 파일은 125개다: 최상위 넷과 `knowledge\technology\` 아래의 121개.

| 파일 | 수(number)의 개수 | 모양 |
|---|---|---|
| `debug_params.json` | 291 | 탭 들여쓰기, CRLF. `production_cost`(35), `building_resources`(건물 101개, 값은 `[["wood", 20], …]`, 수 163개), `fair_trade`(스칼라 3 + `fair_trade_purchase` 39 + `fair_trade_sale` 39), `product_count`(3), `bet`(2), 스칼라 7 |
| `gameplay_variables.json` | 150 | 범주 37개. 범주 안이 한두 단계 더 중첩된다 |
| `battle_params.json` | 203 | `attack.<무기>.<부상>`, `defence.<방어구>.<부상>`, `pain`, `limit`, `unit_skill_stage`, `debug_battle`, `patrol_setup`, 스칼라 8 |
| `director_params.json` | 338 | 2칸 들여쓰기. `group_cooldown_days`·`group_min_spawn_days`·`group_super_priority_days`(그룹 11개씩), `events`(61개. `tickets`, `min_population`, `max_population`, `cooldown_days.min/max`) |
| `knowledge\technology\**\*.json` | 455 | 한 줄. `cultural_knowledge` 52, `economic` 41, `textbooks` 28개. 수가 든 자리: `available_parameters[n].population`(86), `available_parameters[n].type`(102), `tag`(121), `upgrade_skill[n].value`(24개 파일), `technology_effects[n].value`(31), `allow_to_upgrade[n].value`(32), `hint_fields.*`(59) |

`economic\`의 파일 이름에는 느낌표가 있다(`07!building_internal_trade.json`).

## 스냅샷이 바닐라라는 근거

`backups\data\0.5588.9777.0\`의 다섯 파일을 바닐라로 본다. 근거:

- Steam 매니페스트(`appmanifest_1857090.acf`)의 `LastUpdated`가 2026-10-04 16:20:28이고 `buildid`가 `25211575`다.
- 게임 폴더의 JSON 4,701개의 수정 시각이 모두 2026-10-04 16:20대다. Steam이 그때 쓴 것이다.
- 스냅샷은 같은 날 20:17에 떴고, 그때 원본의 수정 시각은 16:20:18 그대로였다(스냅샷이 그 시각을 물려받았다).
  Steam이 쓴 뒤 누가 고쳤다면 수정 시각이 달라졌을 것이다.
- 지금 게임 파일 다섯 개는 스냅샷과 바이트가 같다.

네 파일의 SHA256:

| 파일 | SHA256 |
|---|---|
| `debug_params.json` | `D12254C3311A003134FE217B082F768A53B2F3F377E5DBE975FE414A8955580A` |
| `gameplay_variables.json` | `F8E3ADECAD955F2808FBF6F9699E242A418C8BB640B3A2AC4B8B3DBC8AD227F3` |
| `battle_params.json` | `4F75F4123EE5B1D600C8042DEBA2FF4666C2B016D5489E7FC78EF8B9C77EE74B` |
| `director_params.json` | `100626D17B936B3094263183D561FE71958EF85898244F8B2F13A7C5B4E12FA4` |

## 키별 런타임 근거

등급은 셋이다.

- **VERIFIED** — 파일의 값을 바꿔서 런타임에서 그 값을 봤다.
- **SEEN** — 바닐라 값이 같은 이름으로 런타임에 있는 것을 봤다. **코드의 기본값이 우연히 같은 경우와 가려지지 않는다.**
  `global.__gameplay_vars`에는 파일에 없는 이름이 326개 있고 그 값은 파일에서 오지 않았다(`research/01`).
- **UNSEEN** — 런타임에서 본 적이 없다. 찾지 않았거나, 찾았는데 없었다.

| 파일 | 키 | 등급 | 근거 |
|---|---|---|---|
| `debug_params.json` | `budget_money` | VERIFIED | `research/02` |
| | `production_cost.*` (35) | SEEN | 메뉴 덤프의 `ds_map[150].production_cost`에 35개가 이름·값이 같게 있다 |
| | `product_count.*` (3) | SEEN | 같은 방법. 3개 |
| | `fair_trade.fair_trade_purchase.*`, `fair_trade_sale.*` (39씩) | SEEN | 같은 방법. 39개씩 |
| | `fair_trade.migrants_*` (3) | SEEN | `ds_map[150].fair_trade`의 멤버 5개 가운데 스칼라 3개 |
| | `building_duration_factor`, `romantic_slowdown_factor`, `romantic_decrease_factor`, `matching_by_opinion`, `dialogue_chitchat_count`, `dialogue_random_threshold` | SEEN | 게임 안 덤프의 `o_debug.debug_<이름>`에 같은 값(`matching_by_opinion`은 `debug_matching_by_opinion_value`) |
| | `building_resources.*[*][1]` (163) | UNSEEN | 묶음은 `ds_map[150].building_resources`에 건물 101개로 있다. 값은 ds_list 번호이고 그 안은 보지 않았다 |
| | `bet.*` (2) | UNSEEN | 찾지 않았다 |
| `battle_params.json` | `battle_dodge_base` | VERIFIED | `research/02` |
| | `battle_dodge_shift_better`, `battle_dodge_shift_worse`, `soldier_hiring_price_skill_factor`, `number_of_most_painful_mind` | SEEN | 게임 안 덤프의 `o_debug`에 같은 이름·값 |
| | `debug_right_team_mind_modify`, `debug_left_team_mind_modify`, `is_hide_extra_traits` | UNSEEN | `o_debug`의 변수 127개에 그 이름이 없다 |
| | `attack.*.*`, `defence.*.*`, `pain.*`, `limit.*`, `unit_skill_stage.*`, `debug_battle.*`, `patrol_setup.*` | UNSEEN | 찾지 않았다 |
| `director_params.json` | `group_cooldown_days.EPIDEMY` | VERIFIED | `research/02` |
| | `group_cooldown_days.*` (나머지 10) | SEEN | `o_data.__game_director_events_data.__params.group_cooldown_days`에 11개가 있고 10개의 값이 파일과 같다(EPIDEMY는 바꿔 둔 3391) |
| | `group_min_spawn_days.EPIDEMY`, `group_super_priority_days.EPIDEMY` | SEEN | `ds_map[257].EPIDEMY`의 `__min_spawn_day` = 15, `__super_priority_days` = 30 |
| | 나머지 그룹의 두 값, `events.*.*` | UNSEEN | 찾지 않았다 |
| 지식 | `addiction_resist.json`의 `available_parameters[0].population` | VERIFIED | `research/02` |
| | 그 밖의 파일과 키 | UNSEEN | 찾지 않았다 |
| `gameplay_variables.json` | `global_map.ai_economy.initial_budget` | VERIFIED | `research/01`, `research/02`. 효과도 봤다(AI 도시의 초기 재산) |
| | 93개 | SEEN | `global.__gameplay_vars`에 키 경로를 `_`로 이은 이름으로 같은 값 |
| | 56개 | UNSEEN | `global.__gameplay_vars`에 그 이름이 없다 |

카탈로그 파일 125개의 수 1,437개를 등급으로 세면 VERIFIED 5, SEEN 234, UNSEEN 916이다.
나머지 282개(지식 파일의 `tag`, `available_parameters[n].type`, `hint_fields.*`)는 카탈로그에 올리지 않는다. 번호나 표시용 값으로 보이고 뜻을 모른다.

### `gameplay_variables.json`의 범주별

| 범주 | 런타임에 있음 / 없음 | | 범주 | 있음 / 없음 |
|---|---|---|---|---|
| `actor` | 7 / 2 | | `knowledge` | 0 / 2 |
| `bandits` | 0 / 1 | | `loyalty` | 0 / 5 |
| `battle` | 3 / 2 | | `migration` | 0 / 3 |
| `book` | 0 / 1 | | `mind` | 2 / 9 |
| `bribe` | 1 / 4 | | `moral` | 0 / 1 |
| `building` | 1 / 1 | | `paper` | 1 / 0 |
| `bush` | 2 / 0 | | `pregnancy` | 2 / 0 |
| `camera` | 0 / 1 | | `prestige` | 0 / 6 |
| `church` | 1 / 0 | | `render` | 5 / 0 |
| `combat_training` | 2 / 0 | | `skill` | 6 / 1 |
| `dialogue` | 0 / 1 | | `slave` | 10 / 0 |
| `dummy` | 6 / 2 | | `soldier` | 2 / 1 |
| `executor_work` | 1 / 3 | | `squad` | 2 / 2 |
| `farm` | 8 / 0 | | `tavern` | 1 / 0 |
| `free_lord` | 2 / 0 | | `trait` | 0 / 1 |
| `gallows` | 0 / 1 | | `trees` | 0 / 2 |
| `game` | 1 / 0 | | `wolves` | 1 / 1 |
| `genius_king` | 7 / 0 | | | |
| `global_map` | 17 / 3 | | | |
| `inspection` | 3 / 0 | | 합계 | 94 / 56 |

런타임에 없는 56개의 이름은 `catalog/0.5588.9777.0/keys.json`의 마지막 묶음에 있다. `bribe`는 `give_rings`만 있고
`cooldown`, `leave_chance`, `add_opinion`, `loss_loyalty`는 없다.

`paper`의 `"messenger_cost "`는 런타임에서 `paper_messenger_cost`(공백 없음)다. 값은 둘 다 1이다.

## 값 3000 (시작 금화의 출처를 찾아서)

게임 폴더의 `*.json`, `*.csv`, `*.ini`, `*.txt`에서 수 3000을 찾았다. 네 곳뿐이고 어느 것도 금화가 아니다.

- `building_constructor\NewWorld.json`: 지도 경계의 좌표 `{"x":5514,"y":3000}`.
- `sounds\scripts\buildings\general\ambience_bellows.json`, `ambience_crash.json`, `ambience_water.json`: `"falloff_ref":3000.0`.

이름에 `money`, `gold`, `budget`이 든 키는 최상위 데이터 파일에서 `budget_money`(2000), `global_map.bandit_camp.gold_for_one_bandit`(25),
`global_map.ai_economy.initial_budget`(700), `army_budget_factor` 둘이 전부다. `NewWorldParams.csv`에는 그런 열이 없다.

**시작 금화 3000은 데이터 파일에 그 수로 적혀 있지 않다.** 코드에 있거나, 다른 값에서 계산되거나, 난이도 같은 선택에 달렸다. 어느 쪽인지는 재지 않았다.

## 참조 문서와의 대조

사용자가 준 `Norland_Modding_Reference.md`(기준일 2026-10-05)의 주장을 이 설치본과 맞춰 봤다. 문서는 커뮤니티 실험 결과를 모은 것이고
스스로 "현재 게임 버전에서 다시 확인해야 한다"고 적는다.

| 문서의 주장 | 이 설치본 |
|---|---|
| `battle_settings\` 폴더에 전투 변수 | **없다.** 전투 수치는 `battle_params.json`에 있다 |
| `maps\map_1.json`, `map_2.json` | **없다.** `maps\`에는 `flat_210x175.json` 하나와 `.map_template` 6개가 있다 |
| `building_constructor\map.json` | **없다.** `NewWorld.json`과 `old_map.json`이 있다 |
| `graphics\`, `audio\`, `subtitles\`, `localization\text\` | **없다.** `sounds\`, `videos\`, `fonts\`, `localization\`이 있다 |
| `options.ini`에 `debug=Auto` | 있다. `[Steamworks]` 절 아래다. Steamworks 확장의 설정이지 게임의 디버그 모드라는 근거는 없다 |
| `gameplay_variables.json`의 `bribe` 다섯 값 | 같다 (12, 0.25, 0, 10, 25) |
| `free_lord` (`stay_duration` 10, `ring_by_n_levels` 3) | 같다 |
| `church`의 `preach_conversion_factor` 1.5, `max_capacity` 50 | `max_capacity` 50만 있다. `preach_conversion_factor`는 **없다** |
| `tavern.max_capacity` 30 | 같다 |
| `paper.messenger_cost` 1 | 키가 `"messenger_cost "`(뒤 공백)다 |
| `chancellery.charge_paper_cost` 9 | **없다** |
| `soldier` (`bow.prepare_time` 5, `cost_for_combat_level` 20, `distance_to_patrol` 3) | 같다 |
| `slave`의 11개 값 | `cost_player_faction_factor`가 **없다.** 나머지 10개는 같다 |
| `debug_params.json`의 `church_lvl_1`이 `[["wood",150],["iron",10]]` | `[["stone", 30], ["clay_roof_tile", 30]]`이다 |
| `fair_trade_purchase`의 값(`food` 9, `flour` 4 …) | 값이 다르다(`food` 8, `ale` 40 …). `flour`는 없다 |
| 지식 파일의 `dialect`, `available_parameters`, `upgrade_skill`, `hint_fields` | 있다. `available_parameters`는 객체가 아니라 `{"type", "population" 또는 "knowledge_name"}`의 배열이다. `upgrade_skill`은 24개 파일에 있다(예: `skill_combat_1.json`의 `[{"value":16,"name":"combat"}]`) |
| 세이브가 `%LOCALAPPDATA%\Strategy\saves`의 `.NORLAND` | 폴더는 맞다. 지금 그 폴더에 세이브가 없어 확장자와 내용은 보지 못했다 |

문서의 예시는 2024년 빌드의 것으로 보인다(아래의 원문 날짜). **키 이름과 값은 문서가 아니라 이 설치본의 파일에서 가져온다.**

### 커뮤니티 원문

| 글 | 확인한 내용 |
|---|---|
| Steam 토론 [Tested some Modding](https://steamcommunity.com/app/1857090/discussions/1/4522261213598868108/) (DexteR, 2024-07-25) | 참조 문서 §4~§12의 출처다. 바꿔서 통했다고 적은 것: `bribe.give_rings` 10 → 1, `free_lord.ring_by_n_levels` 3 → 50, `church.max_capacity` 50 → 100, `tavern.max_capacity` 30 → 50, `product_count`(시작 자원), `pregnancy.from_dummy_chance` → 1, `maps\`의 광산 추가, 세이브의 `"budget"`. "파일을 고치고 게임을 켠 뒤 세이브를 불러오면 바뀐 값이 그 세이브에도 통한다"고 적었다. 개발사의 답글은 없다 |
| Steam 토론 [Any way to edit stats (cheat/hack)?](https://steamcommunity.com/app/1857090/discussions/1/4522262793486097538) (2024-08-10) | `knowledge\technology\textbooks`의 `upgrade_skill`의 `value`를 99로 올려 책으로 얻는 경험치를 늘렸다는 글. 2025-03-14의 글(사용자 MBBasar): `-debug`로 켜고 인물을 누른 뒤 Ctrl+D를 누르면 디버그 메뉴가 열린다. **이 글은 개발사 표시가 없는 사용자의 글이다** |
| Steam 토론 [Cheats](https://steamcommunity.com/app/1857090/discussions/0/603031150125937562) (2025-05-19) | 개발사 계정 NorlandCM: "디버그 모드는 다음 패치에 들어간다. 37 패치에는 치트 코드도 디버그 메뉴도 없다" |

- 글의 내용은 작은 모델이 요약한 것을 읽었다. 원문의 문장을 하나하나 대조하지는 않았다.
- 모두 옛 빌드에서의 보고다. **이 빌드에서 효과를 다시 잰 것은 없다.** 카탈로그에는 `effect: TESTED`, `effect_by: community`로 적고 출처를 단다.
- 참조 문서는 `-debug` + Ctrl+D를 "개발자 안내"라고 적었으나, 찾은 원문은 사용자의 글이다. 개발사가 따로 안내했는지는 찾지 못했다.
- 참조 문서가 `slave`에 대해 적은 "값을 크게 바꾸면 충돌했다는 보고"의 원문은 찾지 못했다. 문서의 방침대로 실험 키로 둔다.

## 도구의 프로토타입으로 확인한 것

계획에 넣을 코드를 먼저 스크래치 폴더에서 돌려 봤다(레포와 게임 폴더에는 쓰지 않았다).

- 스캐너가 4,701개를 모두 읽고, 수 254,251개의 자리에 같은 글을 다시 써 넣으면 4,701개 모두 바이트가 그대로다.
- 카탈로그의 125개 파일을 스크래치로 복사해 값 29개(파일 6개)를 바꾸는 프리셋을 입히면 정확히 29줄이 달라지고,
  되돌리면 125개 모두 원본과 바이트가 같다.
- 래퍼(`overlay.ps1`)는 콘솔 코드 페이지가 65001일 때와 949일 때 모두 한글을 깨뜨리지 않았다.

## 확인하지 못한 것

- 게임의 JSON 파서가 받는 문법(지수 표기, 주석, 같은 키의 중복).
- SEEN인 키가 정말 파일에서 읽히는지(코드 기본값과 가려지지 않는다). UNSEEN인 키가 읽히는지.
- 값이 게임을 바꾸는지. 이 빌드에서 본 것은 `initial_budget` 하나다.
- 허용 범위. 어떤 값이 게임을 죽이는지 재지 않았다.
- 시작 금화 3000의 출처.
- 바뀐 값이 세이브에 굳는지, 진행 중인 세이브에 통하는지(2024년의 글은 통한다고 적었다).
- 지식 파일의 `technology_effects`, `allow_to_upgrade`, `hint_fields`의 뜻.
