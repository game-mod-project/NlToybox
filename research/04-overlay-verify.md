# 04 — 프리셋의 값이 런타임에 올라오는가 (데이터 오버레이 단계 1의 실측)

조사일 2026-10-05. 대상: Norland `0.5588.9777.0`. 게임을 켠 횟수: 1. **메인 메뉴까지만 갔다**(새 게임을 시작하지 않았다).

오버레이 도구(`tools/overlay.ps1`)로 값 30개를 바꾼 프리셋(`presets/verify-stage1.json`)을 입히고, 게임을 켜서 메뉴의 런타임 덤프에서
그 값들을 찾았다. 덤프와 출력은 `refs/runtime/stage1-verify.*`에 있다(추적 안 함).

다시 재려면(게임이 꺼져 있고 exe가 패치돼 있고 모듈이 배포돼 있을 때):

    pwsh -File tools/overlay.ps1 apply -Preset presets\verify-stage1.json
    pwsh -File tools/overlay.ps1 probe-request -Preset presets\verify-stage1.json -Out tools\probes\stage1-verify.txt
    pwsh -File tools/probe.ps1 -Request tools\probes\stage1-verify.txt -Out refs\runtime\stage1-verify.json
    py -3.14 tools/re/dump_tool.py controls refs\runtime\stage1-verify.menu.json
    pwsh -File tools/overlay.ps1 verify -Preset presets\verify-stage1.json -Dump refs\runtime\stage1-verify.menu.json
    pwsh -File tools/overlay.ps1 restore

## 답

**바꾼 값 30개가 모두 메인 메뉴의 런타임에 있었다.** 없는 것(`absent`)은 0개다.

| 판정 | 수 | 뜻 |
|---|---|---|
| `anchored` | 29 | 그 값이 있고, 런타임 경로의 이름 조각 하나가 키 이름과 같다 |
| `value-only` | 1 | 값은 있으나 경로에 이름이 없다. 이름 히트에서 리스트 번호를 따라가 확인했다(아래) |
| `absent` | 0 | |

이 판정을 믿는 근거:

- **값이 겹칠 수 없다.** 30개는 게임 폴더의 JSON 4,701개(수 254,251개) 어디에도 없는 수다. 입히기 전에 훑어 확인했다.
  같은 프리셋을 단계 0b의 덤프(이 값들을 입히지 않은 실행)에 돌리면 30개 모두 `absent`다.
- **양성 대조가 맞았다.** 이미 두 번 확인한 `global_map.ai_economy.initial_budget`을 7450319로 바꿨고, 알던 두 자리
  (`global.__gameplay_vars.global_map_ai_economy_initial_budget`, `ds_map[80].initial_budget`)에서 그 값이 나왔다.
- **덤프가 온전하다.** `dump_tool.py controls`의 출력:

  ```
  ok   selfcheck: {'types': True, 'made': True, 'ds_map': True, 'ds_list': True}
  ok   ds_range: highest id=332 scanned below 100000
  ok   truncated: sections=[]
  ok   hits_cut: sections=[]
  ok   enumeration: (failed, short) by section={}
  ok   instances: objects with members=10
  ```

**"런타임에 있다"는 "게임이 그 값을 쓴다"가 아니다.** 이 실행은 메뉴에서 값이 메모리에 올라온 것까지만 봤다. 효과는 하나도 재지 않았다.
특히 `debug_params.json`과 `battle_params.json`의 값은 메뉴에서는 그 파일을 읽은 ds_map에만 있다. 게임 오브젝트(`o_debug`,
`o_province_controller`)는 새 게임을 시작해야 생긴다(`research/02`).

## 판정표

값은 "바닐라 → 바꾼 값"이다. 첫 열의 `…/`는 `knowledge/technology/`다. ds_map의 번호는 이 실행의 것이다(번호에 기대지 않는다).

| 파일 | 키 | 값 | 판정 | 런타임 경로 | 카탈로그의 등급 |
|---|---|---|---|---|---|
| `gameplay_variables.json` | `global_map.ai_economy.initial_budget` (양성 대조) | 700 → 7450319 | anchored | `global.__gameplay_vars.global_map_ai_economy_initial_budget`<br>`ds_map[80].initial_budget` | VERIFIED |
| `gameplay_variables.json` | `bribe.give_rings` | 10 → 1019923 | anchored | `global.__gameplay_vars.bribe_give_rings`<br>`ds_map[104].give_rings` | SEEN → VERIFIED |
| `gameplay_variables.json` | `skill.increase_in_use.command` | 2 → 2053807 | anchored | `global.__gameplay_vars.skill_increase_in_use_command`<br>`ds_map[105].command` | SEEN → VERIFIED |
| `gameplay_variables.json` | `free_lord.stay_duration` | 10 → 1031269 | anchored | `global.__gameplay_vars.free_lord_stay_duration`<br>`ds_map[109].stay_duration` | SEEN → VERIFIED |
| `gameplay_variables.json` | `church.max_capacity` | 50 → 5023441 | anchored | `global.__gameplay_vars.church_max_capacity`<br>`ds_map[111].max_capacity` | SEEN → VERIFIED |
| `gameplay_variables.json` | `tavern.max_capacity` | 30 → 3037583 | anchored | `global.__gameplay_vars.tavern_max_capacity`<br>`ds_map[125].max_capacity` | SEEN → VERIFIED |
| `gameplay_variables.json` | `paper["messenger_cost "]` | 1 → 1049317 | anchored | `global.__gameplay_vars.paper_messenger_cost`<br>`ds_map[128].messenger_cost ` (키에 뒤 공백) | SEEN → VERIFIED |
| `gameplay_variables.json` | `soldier.cost_for_combat_level` | 20 → 2029753 | anchored | `global.__gameplay_vars.soldier_cost_for_combat_level`<br>`ds_map[139].cost_for_combat_level` | SEEN → VERIFIED |
| `gameplay_variables.json` | `bribe.cooldown` | 12 → 1213087 | anchored | `ds_map[104].cooldown` **뿐이다** | UNSEEN → VERIFIED (실험) |
| `gameplay_variables.json` | `prestige.for_population` | 1 → 1061119 | anchored | `ds_map[91].for_population` **뿐이다** | UNSEEN → VERIFIED (실험) |
| `debug_params.json` | `production_cost.ale` | 2 → 2311747 | anchored | `ds_map[143].ale` | SEEN → VERIFIED |
| `debug_params.json` | `fair_trade.fair_trade_purchase.ale` | 40 → 4127339 | anchored | `ds_map[146].ale` | SEEN → VERIFIED |
| `debug_params.json` | `fair_trade.fair_trade_sale.ale` | 30 → 3119561 | anchored | `ds_map[147].ale` | SEEN → VERIFIED |
| `debug_params.json` | `fair_trade.migrants_limit_dummy` | 10 → 1013983 | anchored | `ds_map[148].migrants_limit_dummy` | SEEN → VERIFIED |
| `debug_params.json` | `product_count.wood` | 300 → 3073111 | anchored | `ds_map[149].wood` | SEEN → VERIFIED |
| `debug_params.json` | `building_duration_factor` | 0.5 → 0.573119 | anchored | `ds_map[150].building_duration_factor` | SEEN → VERIFIED |
| `debug_params.json` | `bet.dummy` | 5 → 5099413 | anchored | `ds_map[145].dummy` | UNSEEN → VERIFIED |
| `debug_params.json` | `building_resources.woodcutter_lvl_1[0][1]` | 15 → 1517377 | value-only | `ds_list[245][1]` | UNSEEN → VERIFIED |
| `battle_params.json` | `battle_dodge_shift_better` | 11 → 1117649 | anchored | `ds_map[285].battle_dodge_shift_better` | SEEN → VERIFIED |
| `battle_params.json` | `limit.cut` | 5 → 5021857 | anchored | `ds_map[261].cut`<br>`ds_map[258].cut[9]` | UNSEEN → VERIFIED |
| `battle_params.json` | `attack.sword.cut` | 280 → 2805133 | anchored | `ds_map[272].cut` | UNSEEN → VERIFIED |
| `battle_params.json` | `pain.cut` | -6 → -6131299 | anchored | `ds_map[274].cut`<br>`ds_map[192].cut.__moral_modify` | UNSEEN → VERIFIED |
| `battle_params.json` | `defence.shield.bruise` | 20 → 2039771 | anchored | `ds_map[275].bruise` | UNSEEN → VERIFIED |
| `director_params.json` | `group_cooldown_days.RAID` | 4 → 4057213 | anchored | `ds_map[257].RAID.__cooldown_days`<br>`o_data.__game_director_events_data.__params.group_cooldown_days.RAID` | SEEN → VERIFIED |
| `director_params.json` | `group_min_spawn_days.RAID` | 2 → 2063627 | anchored | `ds_map[257].RAID.__min_spawn_day`<br>`o_data.…__params.group_min_spawn_days.RAID` | UNSEEN → VERIFIED |
| `director_params.json` | `group_super_priority_days.RAID` | 10 → 1069841 | anchored | `ds_map[257].RAID.__super_priority_days`<br>`o_data.…__params.group_super_priority_days.RAID` | UNSEEN → VERIFIED |
| `director_params.json` | `events.vassal_player_demand.tickets` | 1000 → 1009373 | anchored | `o_data.…__params.events.vassal_player_demand.tickets` | UNSEEN → VERIFIED |
| `director_params.json` | `events.vassal_player_demand.cooldown_days.max` | 0 → 7307159 | anchored | `o_data.…__params.events.vassal_player_demand.cooldown_days.max` | UNSEEN → VERIFIED |
| `…/economic/07!building_internal_trade.json` | `available_parameters[0].population` | 40 → 4273691 | anchored | `ds_map[193].internal_trade.__available_parameters[0].__population` | UNSEEN → VERIFIED |
| `…/textbooks/skill_combat_1.json` | `upgrade_skill[0].value` | 16 → 1613477 | anchored | `ds_map[193].skill_combat_1.__upgrade_skill[0].value` | UNSEEN → VERIFIED |

카탈로그의 수 1,437개를 등급으로 다시 세면 VERIFIED 34(앞서 5), SEEN 219(234), UNSEEN 902(916), 카탈로그 밖 282다.

### `value-only` 하나를 어떻게 판단했나

`building_resources.woodcutter_lvl_1[0][1]`은 배열 안의 값이라 런타임 경로에 이름이 없다(`ds_list[245][1]`). 요청에 넣어 둔
이름 찾기(`find_name=woodcutter_lvl_1`)로 이었다.

- `ds_map[144].woodcutter_lvl_1 = 246` — `ds_map[144]`는 `building_resources`다(`research/02`). 값 246은 ds_list의 번호다.
- 그 리스트의 한 단계: `0 = 245` — 첫 원소가 ds_list 245번이다.
- `ds_list[245][1] = 1517377` — 그 안의 둘째 값이 바꾼 값이다.

파일의 `"woodcutter_lvl_1": [["wood", 1517377]]`과 모양이 같다. 그래서 `VERIFIED`로 올렸다.

## 처음 알게 된 것

- **`global.__gameplay_vars`에 이름이 없던 키는 파일을 읽은 ds_map에는 있고, `__gameplay_vars`에는 여전히 없다.**
  `bribe.cooldown`과 `prestige.for_population`이 그렇다. 같은 파일의 다른 키 여덟은 두 곳 모두에 있었다.
  파일은 통째로 읽히지만 게임이 `__gameplay_vars`로 옮기는 키는 따로 정해져 있다. 이 둘과 같은 처지인 키가 54개 더 있다(`research/03`).
  게임이 이 값들을 ds_map에서 바로 읽는지, 쓰지 않는지는 모른다. 카탈로그에서는 등급을 올리되 실험 키로 남겼다.
- **게임은 종료할 때 데이터 파일을 다시 쓰지 않았다.** 게임이 꺼진 뒤 여섯 파일의 해시가 입힌 그대로였다
  (`overlay.ps1 status`: `applied: 6`, `unknown: 0`). 메뉴까지만 간 실행에서 본 것이다.
- **큰 값 30개를 넣어도 게임은 메뉴까지 갔다.** 메뉴는 모듈이 적재된 뒤 27.7초에 떴다. 값의 크기가 로딩을 막지 않았다.
  게임을 시작한 뒤에 이 값들이 무엇을 하는지는 보지 않았다.
- `pain.cut`의 값은 `ds_map[192].cut.__moral_modify`에도 있다. 고통 값이 사기 보정으로 쓰이는 구조에 옮겨진다는 단서다.
  `limit.cut`은 `ds_map[258].cut[9]`에도 있다. 뜻은 모른다.
- 지식 파일의 런타임 이름은 파일 이름이 아니라 파일 안의 `name`이다(`07!building_internal_trade.json` → `internal_trade`).
  `upgrade_skill`의 `value`는 런타임에서도 `value`다(`__upgrade_skill[0].value`. 앞에 밑줄이 붙지 않는다).
- `"messenger_cost "`는 ds_map에서는 뒤 공백이 붙은 키 그대로이고, `__gameplay_vars`에서는 공백이 없는 이름이다.
- 덤프에 걸린 시간은 1.6초, 방문 수는 단계 0b의 메뉴 덤프와 같다(전역 40,171, ds 1,145,502, 인스턴스 10,015).

## 도구에 대해 확인한 것

- `apply`가 여섯 파일을 쓰고 스냅샷 둘을 새로 떴다(지식 파일 둘. 넷은 단계 0의 것이 있었다).
- `restore` 뒤 카탈로그의 125개가 모두 바닐라다(해시). 여섯 파일의 수정 시각도 2026-10-04 16:20으로 돌아왔다
  (게임 폴더의 JSON 가운데 그 시각이 아닌 것 0개).
- 게임 폴더에 임시 파일(`*.nltoybox-tmp`)이 남지 않았고, 상태 파일이 지워졌다.
- 끝난 뒤 게임은 바닐라다: exe SHA256 `609C17CF…863E`, `mods\`와 `aurie.log` 없음.
- 모듈의 적재 판정 줄(`NlToyBox 0.2.0 loaded`, `builtin code_is_compiled = true`,
  `script gml_Script_command_line_parameters_init = found`, `probe done`)이 이 실행의 로그에 모두 있다.

## 세이브 폴더

실행 전 사본: `backups\saves\20261005-105134`. 실행 뒤 달라진 것은 둘이다: `catched_errors_0.5588.9777.0.txt`(게임의 로그),
`game_settings.json`. 새 세이브는 없다. 아무것도 지우지 않았다.

## 확인하지 못한 것

- **효과.** 30개 가운데 게임이 달라지는 것을 본 값은 없다. 양성 대조인 `initial_budget`만 앞 단계에서 효과를 봤다.
- 게임을 시작한 뒤 이 값들이 어디로 가는가(`o_debug`, `o_province_controller`). 이번 값으로는 재지 않았다.
- 같은 객체의 다른 키. `production_cost.ale`을 확인했다고 `production_cost.*`의 나머지 34개를 확인한 것은 아니다.
  같은 ds_map에 함께 읽히는 것은 `research/03`에서 이름·값으로 봤다(SEEN).
- 허용 범위. 이 값들이 게임 안에서 문제를 일으키는지 보지 않았다.
- `__gameplay_vars`에 없는 키 56개를 게임이 쓰는가.
- 새 게임을 시작한 실행에서도 게임이 데이터 파일을 다시 쓰지 않는가.
