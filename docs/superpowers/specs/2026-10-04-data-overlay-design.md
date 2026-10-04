# 하위 프로젝트 1 — 데이터 오버레이 설계

- 작성일: 2026-10-04
- 상위 문서: `docs/superpowers/specs/2026-10-04-configurable-rules-roadmap.md`
- 대상: Norland `0.5588.9777.0`
- 상태: 단계 0 완료(`research/01-data-overlay.md`). 단계 0b(§3.7)는 검토 대기

## 1. 목적

프리셋에 적은 값을 게임의 데이터 파일에 입히고, 언제든 바닐라로 되돌릴 수 있게 한다.
그 전에, 파일의 값이 게임에 실제로 반영되는지를 잰다.

두 단계로 나눈다.

- **단계 0 — 실측.** 무엇이 반영되는지 모르는 채로 도구를 만들지 않는다.
- **단계 1 — 오버레이 도구.** 단계 0에서 반영이 확인된 키만 다룬다.

구현 계획은 단계마다 따로 쓴다. 단계 1의 카탈로그가 단계 0의 결과에 달려 있다.

## 2. 실측 근거 (2026-10-04)

- 게임 수치가 든 파일: `debug_params.json`, `gameplay_variables.json`, `battle_params.json`,
  `director_params.json`, `knowledge\technology\**\*.json`(121개), `building_constructor\NewWorld.json`,
  `NewWorldParams.csv`.
- 키 이름 `budget_money`, `building_duration_factor`, `production_cost`, `building_resources`,
  `fair_trade_purchase`, `product_count`, `battle_dodge_base`, `group_cooldown_days`가 exe 문자열에
  그대로 있다. exe가 이 키들을 읽는다는 정황이다. **값이 실제로 쓰이는지는 확인하지 않았다.**
- exe에 `default_budget_money`, `default_production_cost`, `debug_building_duration_factor` 같은
  이름이 있다. 코드 기본값이 따로 있을 수 있다.
- 값을 돌려주는 게임 스크립트가 있다: `gml_Script_budget_default_money_get`,
  `gml_Script_resource_default_count_get`.
- JSON 4,499개 가운데 엄격한 파서가 못 읽는 것은 3개이고 모두 trailing comma다:
  `debug_params.json`, `gameplay_variables.json`, `localization\locale_definition.json`.
- `gameplay_variables.json`에 뒤 공백이 붙은 키 `"messenger_cost "`가 있고 exe에도 그대로 있다.
- 4,249개 파일은 한 줄로 붙어 있고 250개는 여러 줄(CRLF)이다. 숫자 표기가 섞여 있다
  (`0.0`, `0`, `0.20000000298023224`).

## 3. 단계 0 — 실측

### 3.1 답할 질문

1. 로딩이 끝난 뒤 런타임의 전역 변수는 어떻게 생겼는가 (이름, 형, 값).
2. 파일의 값을 바꾸면 런타임의 값이 바뀌는가. 세 파일에서 하나씩 본다.
3. 파일에 없는 키를 넣으면 읽히는가.

### 3.2 모듈: 덤프

`NlToyBox.dll`에 덤프 기능을 더한다.

- 요청 파일 `mods\Aurie\NlToyBox.probe.txt`가 있을 때만 동작한다. 없으면 Phase 0과 똑같이 동작한다.
- 요청 파일은 줄 단위 `키=값`이다(모듈에 JSON 파서를 넣지 않으려고). `#`으로 시작하는 줄은 주석이다.

  ```
  delay_seconds=60
  script=gml_Script_budget_default_money_get
  script=gml_Script_resource_default_count_get|wood
  find=2345
  find_name=battle_dodge_base
  ```

- 모듈이 적재된 뒤 `delay_seconds`가 지나고 처음 오는 `EVENT_OBJECT_CALL`에서(게임 스레드) 한 번 실행한다.
  기다리는 이유: 모듈은 게임 창이 뜬 직후 적재되고, 데이터 로딩은 그 뒤 약 30초 동안 이어진다(실측).
- 하는 일:
  1. **전역 목록.** 전역 변수의 이름을 모두 얻는다. 이름마다 형을 적고, 수·문자열·불리언이면 값을 적는다.
     구조체면 멤버를 한 단계만 적는다. 배열이면 길이를 적는다.
  2. **스크립트.** `script=` 줄의 스크립트를 차례로 부르고 반환값을 적는다(`|` 뒤는 인자. 수로 읽히면 수, 아니면 문자열).
  3. **찾기.** 전역에서 출발해 구조체와 배열을 따라 내려가며(깊이 6, 방문 200만 개, 길이 64 이하의 배열.
     처음 값은 40만·2,048이었고 실측 뒤 바꿨다. 탐색의 한계는 `research/01-data-overlay.md`)
     `find=`의 수와 같은 값, `find_name=`의 이름과 같은 멤버를 찾아 경로와 값을 적는다. 이름으로 찾은 것이
     구조체면 그 멤버를 한 단계 적는다. 파일의 값이 메모리 어디에 앉는지 한 번의 실행으로 찾기 위한 것이다.
- 결과를 `mods\Aurie\NlToyBox.dump.json`에 쓴다. 다 쓴 뒤 `NlToyBox.log`에 `dump done`을 남긴다.
- 문자열 값은 200바이트에서 자른다. 전역 하나를 적을 때마다 파일을 비운다(도중에 죽어도 어디까지 왔는지 남는다).

덤프는 읽기만 한다. 게임의 값을 바꾸지 않는다.

### 3.3 도구

- `tools/probe.ps1 [-Request <파일>] [-Out <파일>] [-TimeoutSec 240]`
  - 요청 파일을 `mods\Aurie\`에 놓고, 게임을 켜고, `dump done`을 기다리고, 덤프를 `-Out`으로 복사하고,
    게임 창에 `WM_CLOSE`를 보내 정상 종료시킨다. 15초 안에 끝나지 않으면 강제 종료한다.
  - 거부 조건은 `check-load.ps1`과 같다(실행 중, 미패치, DLL 누락). 두 도구가 같은 함수
    (`Assert-NlReadyToLaunch`)를 쓴다.
  - 요청 파일과 덤프는 끝난 뒤 게임 폴더에서 지운다(실패해도 지운다). `restore-game.ps1`의 삭제 목록에도 넣는다.
  - 게임을 끄는 함수(`Stop-NlGame`: `WM_CLOSE` 뒤 기한이 지나면 강제 종료)는 `check-load.ps1`도 쓴다.
- `tools/data-edit.ps1 -File <상대경로> -Find <글> -Replace <글>`
  - 스냅샷이 있는 파일에서 찾는 글이 정확히 한 번 나올 때만 바꾼다. 아니면 거부하고 파일을 그대로 둔다.
- `tools/data-snapshot.ps1 -Files <상대경로…>`
  - 게임 파일을 `backups\data\<게임 버전>\<상대경로>`로 복사한다. 이미 있고 해시가 같으면 건너뛴다.
    해시가 다르면 거부한다(스냅샷은 바닐라여야 한다).
- `tools/data-restore.ps1`
  - 스냅샷에 있는 모든 파일을 게임 폴더로 되돌리고 해시를 대조한다. 스냅샷에 없던 파일은 건드리지 않는다.
- 단계 0에서 값을 바꾸는 일은 손으로 정한 문자열 치환 몇 개뿐이다. 치환 대상이 파일에 정확히 한 번
  나오는지 확인하고 바꾼다. 범용 수정기는 단계 1에서 만든다.

### 3.4 실행 순서

게임을 세 번 켠다. 켜기 전에 사용자에게 알린다.

| 실행 | 파일 상태 | 얻는 것 |
|---|---|---|
| 1 | 바닐라 | 기준 덤프 `refs\runtime\dump-vanilla.json`. 원래 값(2000, 700)과 이름으로 찾기 |
| 2 | 세 값을 바꿈: `debug_params.json`의 `budget_money` 2000 → 2345, `gameplay_variables.json`의 `global_map.ai_economy.initial_budget` 700 → 745, `battle_params.json`의 `battle_dodge_base` 20 → 23 | 덤프 `dump-changed.json`. 2345와 745를 값으로 찾고, `battle_dodge_base`를 이름으로 찾는다 |
| 3 | 2의 상태에 더해, `gameplay_variables.json`에 파일에 없던 키를 하나 추가 | 덤프 `dump-added.json`. 추가한 키가 반영됐는지 본다 |

- 2345와 745는 흔치 않은 값이라 메모리에서 값으로 찾으면 파일 값이 앉은 자리가 드러난다.
- 실행 3의 키는 이렇게 고른다: 실행 2에서 `initial_budget`이 앉은 런타임 구조체(`ai_economy`에 대응)의
  멤버 가운데 파일의 `ai_economy`에 없는 수 멤버가 있으면, 그 이름을 파일에 추가하고 값을 원래 값 + 45로 준다.
  그런 멤버가 없으면 `gameplay_variables.json`의 다른 범주에 대응하는 런타임 구조체에서 같은 방법으로 찾는다
  (실행 2의 덤프에 이름으로 찾은 구조체들의 멤버가 있다). 어디에도 없으면 실행 3은 하지 않고
  "런타임 구조체가 파일과 같은 키만 가진다"로 적는다. 파일 값이 메모리에서 아예 안 보였어도 실행 3은 하지 않는다.
- 끝나면 `data-restore.ps1`과 `restore-game.ps1`로 되돌리고, 게임 파일이 스냅샷과 같은지 확인한다.

### 3.5 산출물

- `research/01-data-overlay.md`: 세 질문의 답, 반영이 확인된 키와 확인되지 않은 키, 덤프의 구조 요약,
  파일 키와 런타임 이름의 대응 규칙(보이면).
- `refs\runtime\dump-*.json` (추적하지 않는다).
- 덤프 기능이 든 모듈과 `tools/probe.ps1`, `data-snapshot.ps1`, `data-restore.ps1`.

### 3.6 단계 0의 완료 기준

1. 덤프 기능을 넣은 뒤에도 Phase 0의 판정 줄(`loaded`, `builtin … = true`, `script … = found`, `probe done`)이
   실행 1의 `NlToyBox.log`에 모두 있다. 요청 파일이 없을 때의 동작은 게임을 따로 켜서 보지 않는다(실행 횟수를
   아끼려고. 요청 파일이 없으면 덤프 코드는 초기화에서 바로 돌아간다).
2. 실행 1의 덤프가 생기고, 전역 변수 항목이 하나 이상 있으며, `gml_Script_budget_default_money_get`의
   반환값이 적혀 있다.
3. 세 질문 각각에 "그렇다 / 아니다 / 확인하지 못함"과 그 근거가 `research/01-data-overlay.md`에 있다.
4. 끝난 뒤 게임이 바닐라다: exe 해시가 원본과 같고, 스냅샷한 데이터 파일의 해시가 스냅샷과 같고,
   `mods\`와 `aurie.log`가 없다.

질문 2의 답이 세 파일 모두 "아니다"이면 단계 1을 시작하지 않는다. 로드맵으로 돌아가 하위 프로젝트 2를
먼저 할지 정한다.

### 3.7 단계 0b — 새 게임 상태에서 다시 잰다

단계 0은 메인 메뉴에서만 쟀고 `debug_params.json`과 `battle_params.json`은 "확인하지 못함"으로 남았다
(`research/01-data-overlay.md`). 새 게임에 들어간 상태에서 다시 잰다. 새 게임은 사용자가 메뉴에서 시작하고,
**모듈이 게임에 들어간 것을 알아보고 덤프한다.**

#### 근거 (2026-10-04, 게임을 켜지 않고 얻은 것)

- `data.win`의 `ROOM` 청크는 18개이고 첫 항목이 `rm_game`이다. 거기 놓인 인스턴스는 `o_game_launch` 하나다.
  메인 메뉴만의 룸은 없다(`rm_main_menu_test`는 인스턴스가 없다). **룸 이름으로는 메뉴와 게임을 가릴 수 없다.**
- 오브젝트는 64개다. `o_main_menu`의 Step 이벤트가 메인 메뉴에서 돈다(단계 0 실행 1의 오류 창).
  게임 안의 것으로 보이는 이름: `o_character`, `o_building`, `o_time_controller`, `o_province_controller`.
  이들이 메뉴에서 이미 있는지는 모른다.
- 새 게임 설정 화면은 `gui_main_menu_choose_province*` 스크립트와 `global.__main_menu_manager`의 멤버
  (`__choose_province`, `__family_editor`, `__conditions_editor`)에 있다. 메뉴의 일부로 보인다(이름뿐, 동작은 모른다).
- 상태 변수 후보(메인 메뉴 덤프의 값): `global.__new_game_initializer.__is_active` = 0, `.__current_step` = 0,
  `global.__game_load_operator.__main_menu_state` = 0, `.__game_is_loading` = 0.
- 수 하나로 된 전역이 많다(`__map_of_cached_loaded_files__` = 19, `__map_of_buffers` = 326 등). ds_map 번호로
  보인다(추정). 단계 0의 찾기는 ds_map·ds_list 안을 보지 않았다. JSON을 `json_decode`로 읽는 게임이라면
  파일 값은 거기에 있다.
- 세이브 폴더 `%LOCALAPPDATA%\Strategy`는 67개 784KB이고 `saves\`에는 `steam_autocloud.vdf`뿐이다(세이브 없음).

#### 답할 질문

4. 새 게임 상태에서 `debug_params.json`, `battle_params.json`, `director_params.json`,
   `knowledge\technology\*.json`의 바꾼 값이 런타임에 있는가. 어디에 있는가.
5. 그 값은 언제 읽히는가. 메인 메뉴에서 이미 있는가, 새 게임에서 생기는가.
6. "게임 안"을 무엇으로 알아보는가(오브젝트의 유무, 상태 변수).

#### 모듈

요청 파일에 키를 더한다. 기존 키의 뜻은 그대로이되 `find_name`은 **부분 일치**로 바뀐다
(평평한 이름 `…_battle_dodge_base`를 찾으려고).

```
trigger=!o_main_menu&o_character     # 조건 한 줄. 항은 오브젝트 이름(인스턴스가 있다) 또는 !이름(없다). & 는 "그리고"
trigger=!o_main_menu&o_building      # 여러 줄이면 어느 하나
settle_seconds=20                    # 조건이 이만큼 이어서 참이면 덤프한다
trigger_timeout_seconds=600          # 적재 뒤 이때까지 안 걸리면 그대로 덤프하고 timeout 이라고 적는다
watch=global.__new_game_initializer.__is_active   # 값이 바뀔 때마다 로그에 적는다
trace_events=1                       # 처음 보는 오브젝트 이벤트 코드의 이름을 로그에 적는다
skip=ds                              # 찾기의 구역을 건너뛴다 (ds, instances)
```

- **상태 재기.** 0.5초마다 오브젝트 64개의 인스턴스 수(`instance_number`)와 `watch`의 값을 잰다.
  인스턴스가 있는 오브젝트의 집합이 바뀌면 `state t=<초> +이름 -이름`을, `watch` 값이 바뀌면 `watch t=<초> …`을
  `NlToyBox.log`에 적는다. 이 기록이 질문 6의 답이다.
- **조건.** 한 번이라도 거짓이었던 조건이 참이 되어 `settle_seconds` 동안 이어지면 걸린다. 처음부터 참인 조건은
  걸리지 않는다. 모르는 오브젝트 이름이 있으면 `dump failed: …`를 적고 그만둔다.
- **덤프 두 번.** `trigger=`가 있으면 `delay_seconds`에 한 번(`NlToyBox.dump.menu.json`), 조건이 걸리면 한 번
  (`NlToyBox.dump.game.json`) 쓴다. `trigger=`가 없으면 앞의 것만 쓴다. 다 끝나면 `dump done`을 적는다.
  같은 실행의 두 덤프를 비교하면 질문 5의 답이 나온다.
- **찾기를 고친다.** 단계 0에서 드러난 한계를 없앤다.
  - 너비 우선으로 내려가고, 구조체·배열마다 닿은 가장 얕은 깊이를 적어 둔다. 깊은 길로 먼저 닿아서 빠지는 것이 없다.
  - ds_map과 ds_list를 번호 0부터 훑는다(`ds_exists`). 이름으로 찾은 항목이 중첩된 map·list이면 그 속을 한 단계 적는다.
  - 오브젝트 인스턴스의 변수를 본다(`variable_instance_get_names`). 오브젝트마다 16개까지.
  - 자가 점검: 표식을 넣은 ds_map과 ds_list를 하나씩 만들어 찾기가 그것을 보는지 확인한 뒤 지운다.
    "없다"가 "보지 못했다"가 아님을 보이기 위한 것이다. **이것이 §3.2의 "읽기만 한다"의 유일한 예외다.** 게임의 값은 바꾸지 않는다.
  - 러너 내부 구조체의 배치에 기대는 호출(`GetInstanceObject`, `GetInstanceMemberCount`, `CRoom`)은 쓰지 않는다.
    빌트인만 쓴다. 멤버 열거가 중간에 끊기는지는 여전히 알 수 없다.
- 덤프의 순서는 덜 위험한 것부터다: 전역 목록, 전역 찾기, ds 찾기, 인스턴스, 스크립트 호출. 구역이 끝날 때마다
  로그에 한 줄을 적는다. 도중에 죽으면 어느 구역이었는지 남는다.
- 요청 파일은 모듈이 읽은 뒤 스스로 지운다. 덤프에는 모듈 버전과 한도를 적는다. 요청에 모르는 키나 잘못된 값이
  있으면 `dump failed: bad request`를 적고 아무것도 하지 않는다.
- 러너에 기대지 않는 부분(요청 읽기, 조건 판정, 글 처리)은 `src/core/`로 나누고 게임 없이 시험한다.

#### 도구

- `tools/probe.ps1`: 덤프를 단계별 이름으로 가져온다(`-Out x.json` → `x.menu.json`, `x.game.json`).
  로그에 `dump failed`가 보이면 기다리지 않고 실패한다. 요청에 `trigger=`가 있으면 새 게임을 시작하라고 알린다.
  끌 때 기다리는 시간을 `-GraceSec`로 받는다.
- `tools/saves-backup.ps1`: 세이브 폴더의 사본을 `backups\saves\<시각>\`에 뜬다. `-Diff <사본>`은 사본과 지금의
  차이를 보여 준다. **세이브 폴더에는 쓰지 않는다.** 새 게임이 세이브를 만들 수 있어서 실행 전에 사본을 뜬다.
- `tools/test-native.ps1`: `src/core`의 시험을 돌린다. `tools/probes/*.txt`가 오류 없이 읽히는지도 본다
  (요청 파일의 오타로 게임 실행 한 번을 버리지 않으려고).
- `tools/re/dump_tool.py`: 새 덤프 형식을 읽는다. 옛 형식도 읽는다.

#### 실행

게임을 한 번 켜고, 한 번을 예비로 둔다. **사용자가 할 일이 있다**: 메인 메뉴가 뜨고 약 1분 뒤 화면이 잠깐 멈췄다
풀리면(메뉴 덤프) 새 게임을 기본 설정으로 시작한다. 게임 화면이 나온 뒤에는 기다리면 도구가 덤프를 받고 게임을 끈다.

| 파일 | 바꾸는 값 | 찾는 법 |
|---|---|---|
| `debug_params.json` | `budget_money` 2000 → 2345 | 값 2345, 이름 `budget_money` |
| `gameplay_variables.json` | `initial_budget` 700 → 745 | 값 745. **양성 대조**: `global.__gameplay_vars`에서 찾아져야 한다 |
| `battle_params.json` | `battle_dodge_base` 20 → 23 | 이름 `dodge_base` |
| `director_params.json` | `group_cooldown_days.EPIDEMY` 7 → 3391 | 값 3391, 이름 `EPIDEMY` |
| `knowledge\technology\cultural_knowledge\addiction_resist.json` | `population` 110 → 3767 | 값 3767, 이름 `addiction_resist` |

값을 바꾸지 않고 이름으로만 찾는 것: `production_cost`, `building_resources`, `building_duration_factor`,
`product_count`, `fair_trade`, `group_cooldown_days`.

"없다"를 답으로 쓰기 전에 확인할 것:

1. 게임 덤프의 조건이 `timeout`이 아니고, 그때 있던 오브젝트에 게임 안의 것이 있다.
2. 값 745가 두 덤프 모두 `global.__gameplay_vars.global_map_ai_economy_initial_budget`에서 찾아졌다.
3. ds 자가 점검이 둘 다 참이다.
4. 어느 구역도 한도에 걸리지 않았다(`truncated` 거짓).
5. 인스턴스 구역에 변수가 적힌 오브젝트가 하나 이상 있다.

예비 실행은 위 다섯 가운데 하나가 어긋났고 요청 파일을 고쳐 바로잡을 수 있을 때만 쓴다.

#### 산출물과 완료 기준

- `research/02-new-game-state.md`: 질문 4~6의 답과 근거, 상태 기록, 확인하지 못한 것.
- 완료 기준: (1) 질문 4~6 각각에 "그렇다 / 아니다 / 확인하지 못함"과 근거가 있다. (2) 위 다섯 확인의 결과가 적혀 있다.
  (3) 끝난 뒤 게임이 바닐라다(§3.6의 4와 같다). (4) 세이브 폴더에 생긴 파일을 사용자에게 알렸다.

## 4. 단계 1 — 오버레이 도구

단계 0의 결과: `research/01-data-overlay.md`. 요약하면 `gameplay_variables.json`은 반영이 확인됐고
(런타임에 이름이 있는 키 94개), `debug_params.json`과 `battle_params.json`은 메인 메뉴에서는 확인하지 못했으며,
파일에 없는 키를 넣는 것은 시험한 한 경로(`game.speed_slower_div`)에서 통하지 않았다. 그래서 단계 1의 첫 범위는 `gameplay_variables.json`의 94개 키다.
다른 파일은 새 게임을 시작한 상태에서 반영을 잰 뒤에 카탈로그에 올린다.

단계 0에서 스펙 §3과 달라진 점: 실행 1은 요청의 스크립트 호출(인자 형 오류)로 게임이 끝나 실패했고 다시 뜨지 않았다.
실행 3에서 추가한 키는 §3.4의 규칙 대신 사용자 승인을 받아 `game.speed_slower_div`로 했다. 찾기의 한도는
방문 200만, 배열 64 이하로 바꿨다. 모듈은 찾기를 스크립트 호출보다 먼저 한다.

아래는 계획을 쓰기 전에 위 결과에 맞춰 다시 고칠 뼈대다.

### 4.1 구성

```
tools/overlay/            Python 3.14. 표준 라이브러리만 쓴다
  jsonedit.py             값의 자리만 고치는 수정기
  paths.py                경로 식(키, 와일드카드, 배열 첨자)
  catalog.py              카탈로그 읽기와 검증
  preset.py               프리셋 읽기와 검증
  cli.py                  명령: apply, restore, status, check
  tests/                  unittest
catalog/<게임 버전>.json   지원하는 키의 목록
presets/*.json            프리셋
tools/apply-preset.ps1    게임 상태를 검사하고 cli.py 를 부른다
```

### 4.2 프리셋

```json
{ "name": "Easy",
  "game_version": "0.5588.9777.0",
  "changes": [
    { "file": "debug_params.json", "path": "budget_money", "set": 5000 },
    { "file": "debug_params.json", "path": "building_resources.*[*][1]", "mul": 0.5, "round": "ceil" } ] }
```

- 연산은 `set`(값 지정)과 `mul`(수에 곱함) 둘이다. `mul`에는 `round`(`ceil`, `floor`, `nearest`, 없음)를 줄 수 있다.
- 경로: 점으로 키를 잇고, `*`는 모든 키, `[n]`과 `[*]`는 배열 첨자다. 점·공백·대괄호가 든 키는
  `["messenger_cost "]`처럼 적는다.
- 한 경로가 아무 값에도 닿지 않으면 오류다. 조용히 넘어가지 않는다.

### 4.3 카탈로그

키마다 파일, 경로, 형, 허용 범위, 실측 여부(단계 0에서 반영이 확인됐는가), 세이브에 굳는지를 적는다.
프리셋은 카탈로그에 있고 실측된 키만 바꿀 수 있다.

### 4.4 적용과 복원

- 적용은 항상 스냅샷에서 출발한다: 스냅샷을 게임 폴더로 되돌린 뒤 프리셋을 입힌다. 프리셋을 바꿔도
  변경이 쌓이지 않는다.
- 적용 전에 확인한다: 게임이 꺼져 있다, 프리셋의 `game_version`이 지금 exe 버전과 같다, 바꿀 파일의
  바닐라 스냅샷이 있다(없으면 만든다), 게임 파일이 스냅샷과도 마지막 적용 결과와도 다르지 않다
  (다르면 게임이 갱신된 것이므로 거부한다).
- 적용 결과(프리셋 이름, 바뀐 파일의 해시)를 `backups\data\<게임 버전>\state.json`에 적는다.
- 복원은 스냅샷을 되돌리고 해시를 대조한다.

### 4.5 수정기의 요구

- 바꾸는 값의 글자 범위만 바꾼다. 나머지 바이트는 그대로다(trailing comma, 공백, 줄바꿈, 키의 뒤 공백,
  다른 숫자의 표기).
- 아무것도 바꾸지 않는 적용은 파일을 바이트 단위로 그대로 둔다.
- 새 값의 표기: 정수는 정수로, 실수는 가장 짧은 왕복 표기로 쓴다. 원래 값이 `2.0` 꼴이고 새 값이
  정수이면 `.0`을 붙인다.

### 4.6 시험

- `jsonedit`: 단위 시험. trailing comma, 뒤 공백 키, 한 줄 파일, CRLF 파일, 중첩 배열, 문자열 안의 따옴표·역슬래시.
- 실제 게임 JSON 전부에 "모든 값의 자리를 찾을 수 있다"와 "아무것도 안 바꾸면 바이트가 같다"를 읽기 전용으로 확인한다.
- 적용·복원 왕복: 가짜 게임 폴더(`NORLAND_GAME_DIR`)로. 복원 뒤 바이트가 원본과 같아야 한다.
- 실제 게임에서의 반영 확인은 단계 0의 `probe.ps1`을 다시 쓴다.

## 5. 범위 밖

- 런타임에서 값을 쓰는 것 (하위 프로젝트 2).
- 편집 UI (하위 프로젝트 3).
- 새 건물·이벤트·규칙, Mod API (하위 프로젝트 4).
- 세이브 파일을 읽거나 고치는 것.
- `.map_template`(바이너리가 붙어 있다)과 CSV의 수정. CSV는 단계 1에서 필요가 확인되면 다시 정한다.

## 6. 위험

| 위험 | 대응 |
|---|---|
| 덤프가 게임을 느리게 하거나 죽인다 (전역이 많다) | 한 번만 실행한다. 구조체는 한 단계만 본다. 죽으면 범위를 줄여 다시 잰다 |
| `delay_seconds` 안에 로딩이 끝나지 않는다 | 요청 파일로 조절한다. 덤프에 시각을 적어 로딩 로그와 대조한다 |
| 전역 변수 이름을 얻는 빌트인이 YYC에서 기대대로 동작하지 않는다 | 단계 0의 첫 실행에서 드러난다. 안 되면 요청의 스크립트 호출만으로 질문 2를 본다 |
| 메인 메뉴에서는 값이 아직 로드되지 않는다 (새 게임을 시작해야 한다) | 덤프로 드러난다. 그러면 사용자가 새 게임을 시작한 뒤 덤프하도록 절차를 바꾸고 다시 승인받는다 |
| 게임 실행 횟수 | 세 번. 더 필요하면 멈추고 묻는다 |
| (0b) 조건이 너무 일찍 걸린다 (설정 화면에서 `o_main_menu`가 사라지는 경우) | 조건에 게임 안의 오브젝트를 함께 넣는다. 걸린 때의 오브젝트 집합을 덤프와 로그에 적어 판별한다. 일찍 걸렸으면 기록을 보고 조건을 고쳐 예비 실행을 쓴다 |
| (0b) 조건이 끝내 걸리지 않는다 | `trigger_timeout_seconds`에 그대로 덤프한다. 사용자가 그때 게임 안에 있었으면 그 덤프를 쓴다 |
| (0b) 새로 쓰는 빌트인이 게임을 죽인다 | 덜 위험한 구역부터 쓰고 구역마다 로그를 남긴다. 죽은 구역은 `skip=`으로 빼고 예비 실행을 쓴다 |
| (0b) 새 게임이 세이브를 만든다 | 실행 전에 세이브 폴더의 사본을 뜬다. 끝나고 생긴 파일을 사용자에게 알린다. 지우는 것은 사용자가 정한다 |
| (0b) 게임 안에서 `WM_CLOSE`가 확인 창에 막혀 강제 종료된다 | 끄는 기한을 30초로 늘린다. 강제 종료로 끊긴 자동 저장은 이번 실행이 만든 것뿐이다 |
