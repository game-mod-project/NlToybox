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
(`research/01-data-overlay.md`). 새 게임에 들어간 상태에서 다시 잰다.

**이 단계는 기록만 한다.** "게임에 들어갔다"를 무엇으로 알아볼지는 아직 재 본 적이 없다. 그래서 조건을 미리
정하지 않고, 상태가 바뀌는 것을 기록하면서 일정 간격으로 덤프한다. 감지 로직은 이 기록에서 나온 신호로
다음 계획에서 만든다. 추측한 조건을 넣고 틀리면 대비하는 방식은 쓰지 않는다.

#### 근거

계획과 코드가 기대는 사실과 그 출처다. 출처가 없는 것은 "모른다"에 적고 이번 실행에서 잰다.

| 사실 | 출처 |
|---|---|
| 러너 버전 문자열은 `2023.4.0.113` 하나다 | `refs/exe_strings.txt` |
| 계획이 쓰는 빌트인의 이름이 exe에 있다: `ds_exists`, `ds_map_keys_to_array`, `ds_map_is_map`, `ds_map_is_list`, `ds_map_find_value`, `ds_map_size`, `ds_map_create`, `ds_map_add`, `ds_map_destroy`, `ds_list_create`, `ds_list_add`, `ds_list_destroy`, `ds_list_size`, `ds_list_find_value`, `variable_instance_get_names`, `variable_instance_get`, `instance_number`, `instance_find`, `object_exists`, `object_get_name`, `room_get_name`, `array_length` | `refs/exe_strings.txt`에 이름마다 한 번씩 있다 |
| `ds_type_map` = 1, `ds_type_list` = 2 (stack 3, queue 4, grid 5, priority 6) | 공개된 공식 러너 소스 `YoYoGames/GameMaker-HTML5`의 `scripts/functions/Function_YoYo.js` 36~41행. 이 러너에서도 맞는지는 실행 때 자가 점검으로 다시 잰다 |
| `ds_exists(ind, type)`은 불리언을 돌려준다. 같은 번호가 형이 다른 여러 자료구조에 쓰일 수 있다 | GameMaker 매뉴얼 `ds_exists` (Context7 `/yoyogames/gamemaker-manual`) |
| `instance_number(obj)`는 자식 오브젝트의 인스턴스를 포함하고 비활성 인스턴스는 세지 않는다 | 매뉴얼 `instance_number` |
| `instance_find(obj, n)`은 없으면 `noone`을 돌려준다. `noone`은 -4다. 부모 오브젝트를 주면 자식의 인스턴스도 돈다 | 매뉴얼 `instance_find`, `Instance Keywords` |
| `variable_instance_get_names(id)`는 인스턴스 변수 이름의 배열을, `variable_instance_get(id, name)`은 값을(없으면 `undefined`) 돌려준다 | 매뉴얼 `variable_instance_get_names`, `variable_instance_get` |
| `object_exists(obj)`는 불리언을(잘못된 번호면 거짓), `object_get_name(obj)`과 `room_get_name(index)`는 문자열을 돌려준다. 매뉴얼의 예가 `room_get_name(room)`이다 | 매뉴얼 `object_exists`, `object_get_name`, `room_get_name` |
| `ds_map_keys_to_array(id)`는 키의 배열을, `ds_map_is_map`·`ds_map_is_list(id, key)`는 불리언을 돌려준다 | 매뉴얼 |
| `ds_map_size(id)`·`ds_list_size(id)`는 수를, `ds_map_find_value(id, key)`는 값을(키가 없으면 `undefined`), `ds_list_find_value(id, pos)`는 값을 돌려준다 | 매뉴얼. **없는 번호에 부르면 어떻게 되는지는 매뉴얼에 없다.** 그래서 `ds_exists`로 있는 것을 본 번호에만 부른다 |
| `json_decode`는 중첩된 ds_map·ds_list를 만든다 | 매뉴얼 `json_decode` |
| `EnumInstanceMembers`의 콜백이 거짓을 돌려주면 열거가 이어진다. 끝까지 돌면 `AURIE_OBJECT_NOT_FOUND`를 돌려주고, 멤버 하나를 얻지 못하면 그 자리에서 돌아온다. 멤버 수는 러너의 `StructGetKeys`로 얻는다 | YYToolkit 위키 `EnumInstanceMembers` (Context7 `/aurieframework/yytoolkit`), 서브모듈 `Module Interface/MI_Public.cpp` 315~416행, 단계 0의 실행(전역 4,737개가 열거됐다) |
| 없는 이름으로 `GetInstanceMember`를 부르면 YYToolkit이 구조체에 대고 `variable_instance_exists`를 부른다. `GetInstanceMemberCount`는 `variable_instance_names_count`를 부른다. 이 러너에서 그 빌트인들이 구조체를 받는지는 **모른다.** 그래서 둘 다 쓰지 않고, 이름은 열거로 찾고 멤버 수는 `StructGetKeys`로 얻는다 | 서브모듈 `MI_Public.cpp` 255~313행, 859~900행 |
| `GetBuiltin`으로 전역 빌트인 변수(`room`)를 읽을 수 있다. 이 게임에서 YYToolkit의 빌트인 변수 표 초기화가 성공했다 | YYToolkit 위키 `GetBuiltin`, `refs/phase0/aurie.pass.log`의 `Zeus::YYC::GetBuiltinInformation => AURIE_SUCCESS` |
| 오브젝트 이벤트 콜백의 인자는 (Self, Other, CCode, Arguments, Flags) 순서다. 코드 이름은 `CCode::m_Name`이다 | 서브모듈 `Module Internals/Hooks/Hooks.cpp` 122행, `Private Interface/PI_Public.cpp` 646행 |
| 게임의 첫 룸은 `rm_game`이고 거기 놓인 인스턴스는 `o_game_launch` 하나다. 룸은 18개, 오브젝트는 64개다 | `data.win`의 `GEN8` 룸 순서(첫 값 0)와 `ROOM`·`OBJT` 청크. 필드 배치는 UndertaleModTool `UndertaleGeneralInfo.cs` |
| 데이터 파일을 읽는 스크립트로 보이는 이름: `file_get_nested_value`, `file_get_nested_value_cached`, `file_set_nested_value`. 전역 `__map_of_cached_loaded_files__` = 19, `____cache_file_nested_value` = 18 | `refs/strg.txt`(이름), 단계 0의 메뉴 덤프(값). **동작은 모른다** |
| 게임은 `%LOCALAPPDATA%\Strategy\catched_errors_<버전>.txt`에 `Menu Opened`, `Save game: <이름>`, `Load game: <이름>`을 시각과 함께 적는다. 자동 저장의 이름은 `…_Autosave_Morning_…`, `…_Autosave_Evening_…`다 | 그 파일의 2026-09-24 기록 |
| 세이브 폴더는 67개 784KB이고 `saves\`에는 `steam_autocloud.vdf`뿐이다 | 2026-10-04에 읽음 |

모른다 (이번 실행에서 잰다):

- 메인 메뉴와 게임 안에서 어떤 오브젝트의 인스턴스가 있는가. 룸이 바뀌는가.
- 값이 든 자리. 수 하나로 된 전역(19, 18 등)이 ds_map 번호인가.
- `CCode`의 배치가 이 러너와 맞는가(이름을 읽을 수 있는가).
- 덤프 한 번에 걸리는 시간.

커뮤니티에서 본 것 (확인하지 않았다. 이 단계에서는 쓰지 않는다):

- Steam 토론 "Any way to edit stats (cheat/hack)?"(2024-08-10): `knowledge\technology\textbooks`의 JSON을 고쳐
  효과를 봤다는 글. 같은 글타래의 3월 14일 글: `-debug`로 켜고 인물을 누른 뒤 Ctrl+D를 누르면 디버그 창이 열린다.
  `-debug`는 exe 문자열에서 GameMaker 러너의 옵션 목록(`-trace`, `-noaudio` 등) 사이에 있다. Norland가 그것으로
  디버그 창을 켜는지는 재 보지 않았다. 하위 프로젝트 3에서 잴 것이다.
- Steam 토론 "Cheats"(2025-05-22): 개발사 계정이 "디버그 모드는 다음 패치에 들어간다, 37 패치에는 치트가 없다"고 썼다.

#### 답할 질문

4. 새 게임 상태에서 `debug_params.json`, `battle_params.json`, `director_params.json`,
   `knowledge\technology\*.json`의 바꾼 값이 런타임에 있는가. 어디에 있는가.
5. 그 값은 언제 읽히는가. 메인 메뉴에서 이미 있는가, 새 게임에서 생기는가.
6. "게임 안"을 무엇으로 알아볼 수 있는가(룸, 오브젝트의 유무, 전역 값). 기록에서 뽑는다.

#### 모듈

요청 파일에 키를 더한다. 기존 키의 뜻은 그대로이되 `find_name`은 **부분 일치**로 바뀐다
(평평한 이름 `…_battle_dodge_base`를 찾으려고).

```
repeat_seconds=45        # 첫 덤프 뒤로, 앞 덤프가 끝난 때부터 이만큼 지나면 다시 덤프한다. 없으면 첫 덤프만 한다
keep_last=3              # 되풀이 덤프는 마지막 이만큼만 남긴다 (late0, late1, late2 를 돌려 쓴다)
watch=global.__new_game_initializer.__is_active   # 값이 바뀔 때마다 로그에 적는다
trace_events=1           # 처음 보는 오브젝트 이벤트 코드의 이름을 로그에 적는다
skip=ds                  # 구역을 건너뛴다 (ds, instances, room)
max_hits=2000            # 찾기의 한도. 요청으로 바꿀 수 있다: max_depth(6), max_visited(2000000), max_array(64),
                         # max_hits(2000), max_ds_keys(20000), max_ds_id(100000), max_instances(16)
```

- **상태 재기.** 0.5초마다 룸 이름, 오브젝트 64개의 인스턴스 수(`instance_number`), `watch`의 값을 잰다.
  룸이나 인스턴스가 있는 오브젝트의 집합이 바뀌면 `state t=<초> room=… +이름 -이름`을, `watch` 값이 바뀌면
  `watch t=<초> …`을 `NlToyBox.log`에 적는다. 이 기록이 질문 6의 답이다.
- **덤프.** `delay_seconds`에 한 번(`NlToyBox.dump.menu.json`). `repeat_seconds`가 있으면 그 뒤로 되풀이한다
  (`NlToyBox.dump.late<k>.json`). 덤프마다 순번, 시각, 룸, 그때 있던 오브젝트를 적는다. 로그에는 시작·끝 시각과
  걸린 시간을 적는다. 메뉴 덤프와 뒤의 덤프를 비교하면 질문 5의 답이 나온다.
- **찾기를 고친다.** 단계 0에서 드러난 한계를 없앤다.
  - 너비 우선으로 내려가고, 구조체·배열마다 닿은 가장 얕은 깊이를 적어 둔다. 깊은 길로 먼저 닿아서 빠지는 것이 없다.
  - ds_map과 ds_list를 번호 0부터 한도까지 모두 본다(`ds_exists`). 가장 큰 번호를 덤프에 적는다.
    이름으로 찾은 항목이 중첩된 map·list이면 그 속을 한 단계 적는다.
  - 오브젝트 인스턴스의 변수를 본다(`variable_instance_get_names`). 오브젝트마다 16개까지.
  - 형 상수(1, 2)가 이 러너에서 맞는지 먼저 잰다: ds_map과 ds_list를 하나씩 만들어 `ds_exists`가 참인지,
    지운 뒤 거짓인지 본다. 맞지 않으면 ds를 훑지 않는다(없는 번호에 ds 함수를 부르지 않으려고).
  - 자가 점검: 표식을 넣은 ds_map과 ds_list를 하나씩 만들어 찾기가 그 키와 값을 보는지 확인한 뒤 지운다.
    **이 둘이 §3.2의 "읽기만 한다"의 유일한 예외다.** 게임의 값은 바꾸지 않는다.
  - 한도와 통계는 구역(전역, ds, 인스턴스)마다 따로 센다. 한 구역이 한도에 걸려도 다음 구역은 제 몫을 본다.
    한도는 요청으로 바꿀 수 있다.
  - 구역마다 적는다: 방문한 수, 맞은 수(적지 못한 것도 센다), 히트를 다 적지 못했는가(`hits_cut`), 한도에
    걸렸는가(`truncated`), 멤버 열거가 오류로 끝난 구조체의 수(`enum_failed`), 러너가 말한 멤버 수보다 적게 본
    구조체의 수(`enum_short`), 길어서 들어가지 않은 배열·ds의 경로와 길이(`too_long`, 200개까지).
  - 러너 내부 구조체의 배치에 기대는 호출(`GetInstanceObject`, `CRoom`)은 쓰지 않는다.
    코드 이름(`CCode`)만 예외이고, 잘못 읽어도 게임이 죽지 않게 감싼다. 읽은 수와 못 읽은 수를 덤프에 적는다.
- 첫 표본에서는 호출마다 앞에 로그 한 줄을 적는다(`sample: objects`, `sample: room`, `sample: counts`,
  `sample: watch`, `sample: ok`). 처음 쓰는 호출이 게임을 죽이면 어느 호출이었는지 남는다.
- 덤프의 순서는 덜 위험한 것부터다: 전역 목록, 전역 찾기, ds 찾기, 인스턴스, 스크립트 호출. 구역이 끝날 때마다
  로그에 한 줄을 적는다. 도중에 죽으면 어느 구역이었는지 남는다.
- `script=`는 되풀이 덤프와 함께 쓸 수 없다(같은 스크립트를 여러 번 부르지 않는다).
- 요청 파일은 모듈이 읽은 뒤 스스로 지운다. 덤프에는 모듈 버전과 한도를 적는다. 요청에 모르는 키나 잘못된 값이
  있으면 `dump failed: bad request`를 적고 아무것도 하지 않는다.
- 러너에 기대지 않는 부분(요청 읽기, 덤프 일정, 글 처리)은 `src/core/`로 나누고 게임 없이 시험한다.

#### 도구

- `tools/probe.ps1`: 덤프를 이름대로 가져온다(`-Out x.json` → `x.menu.json`, `x.late0.json` …).
  로그에 `dump failed`가 보이면 기다리지 않고 실패한다. 되풀이 요청이면 새 게임을 시작하라고 알리고,
  **사용자가 게임을 끌 때까지** 기다린다(게임 안에서 도구가 강제로 끄지 않는다).
  덤프는 다시 켜야만 얻으므로, 복사한 것만 게임 폴더에서 지운다. 복사하지 못한 덤프는 남겨 두고 실패로 끝난다.
- `tools/saves-backup.ps1`: 세이브 폴더의 사본을 `backups\saves\<시각>\`에 뜬다. `-Diff <사본>`은 사본과 지금의
  차이를 보여 준다. **세이브 폴더에는 쓰지 않는다.**
- `tools/test-native.ps1`: `src/core`의 시험을 돌린다. `tools/probes/*.txt`가 오류 없이 읽히는지도 본다.
- `tools/re/dump_tool.py`: 새 덤프 형식을 읽는다. 옛 형식도 읽는다.

#### 실행

게임을 한 번 켜고, 한 번을 예비로 둔다. **사용자가 할 일이 있다**: 메인 메뉴가 뜨고 약 1분 뒤 화면이 잠깐 멈췄다
풀리면(메뉴 덤프) 새 게임을 기본 설정으로 시작한다. 게임 화면이 나오면 2분쯤 그대로 둔다(그 사이 화면이 두 번쯤
멈췄다 풀린다). 그런 다음 평소처럼 게임을 끈다. 게임 화면에 들어간 때와 끈 때를 대강 알려 주면 그것이 기준이 된다.

| 파일 | 바꾸는 값 | 찾는 법 |
|---|---|---|
| `debug_params.json` | `budget_money` 2000 → 2345 | 값 2345, 이름 `budget_money` |
| `gameplay_variables.json` | `initial_budget` 700 → 745 | 값 745. **양성 대조**: `global.__gameplay_vars`에서 찾아져야 한다(단계 0에서 확인한 자리) |
| `battle_params.json` | `battle_dodge_base` 20 → 23 | 이름 `dodge_base` |
| `director_params.json` | `group_cooldown_days.EPIDEMY` 7 → 3391 | 값 3391, 이름 `EPIDEMY` |
| `knowledge\technology\cultural_knowledge\addiction_resist.json` | `population` 110 → 3767 | 값 3767, 이름 `addiction_resist` |

다섯 문자열이 파일마다 정확히 한 번 나오는 것은 2026-10-04에 세어 확인했다.
값을 바꾸지 않고 이름으로만 찾는 것: `production_cost`, `building_resources`, `building_duration_factor`,
`product_count`, `fair_trade`, `group_cooldown_days`.

"없다"를 답으로 쓰기 전에 확인할 것:

1. 그 덤프가 게임 안에서 뜬 것이다(사용자가 알려 준 때와 덤프의 시각, 상태 기록이 서로 맞는다).
2. 값 745가 `global.__gameplay_vars.global_map_ai_economy_initial_budget`에서 찾아졌다.
3. ds 형 상수 확인과 자가 점검이 모두 참이고, 가장 큰 ds 번호가 본 범위 안에 있다.
4. 어느 구역도 한도에 걸리거나(`truncated`), 히트를 다 적지 못하거나(`hits_cut`), 멤버 열거가 끊기거나
   (`enum_failed`, `enum_short`), 건너뛰지 않았다.
5. 인스턴스 구역에 변수가 적힌 오브젝트가 하나 이상 있다.

예비 실행은 위 다섯 가운데 하나가 어긋났고 요청 파일을 고쳐 바로잡을 수 있을 때만 쓴다.

#### 산출물과 완료 기준

- `research/02-new-game-state.md`: 질문 4~6의 답과 근거, 상태 기록, 감지에 쓸 수 있는 신호, 확인하지 못한 것.
- 완료 기준: (1) 질문 4~6 각각에 "그렇다 / 아니다 / 확인하지 못함"과 근거가 있다. (2) 위 다섯 확인의 결과가 적혀 있다.
  (3) 끝난 뒤 게임이 바닐라다(§3.6의 4와 같다). (4) 세이브 폴더에 생긴 파일을 사용자에게 알렸다.
- 이 단계 뒤에 할 일: 기록에서 나온 신호로 "게임에 들어간 것을 알아보는" 로직을 만들고 한 번 켜서 확인한다.
  따로 계획을 쓴다.

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
| (0b) 사용자가 게임에 들어가기 전에 끈다, 또는 덤프가 게임 안에서 한 번도 돌지 않는다 | 덤프마다 시각과 그때의 상태를 적는다. 게임 안의 덤프가 없으면 답을 "확인하지 못함"으로 적고 예비 실행을 쓴다 |
| (0b) 새로 쓰는 빌트인이 게임을 죽인다 | 덜 위험한 구역부터 쓰고 구역마다 로그를 남긴다. 죽은 구역은 `skip=`으로 빼고 예비 실행을 쓴다 |
| (0b) 되풀이 덤프가 게임을 자주 멈춘다 | 측정용 실행이라 받아들인다. 걸린 시간을 로그에 적어 다음 설계의 근거로 쓴다 |
| (0b) 새 게임이 세이브를 만든다 | 실행 전에 세이브 폴더의 사본을 뜬다. 끝나고 생긴 파일을 사용자에게 알린다. 지우는 것은 사용자가 정한다. 게임 로그로 보면 자동 저장은 게임 안의 아침·저녁에 생긴다 |
| (0b) 게임 안에서 도구가 강제로 꺼서 저장을 끊는다 | 되풀이 요청에서는 사용자가 게임을 끈다. 도구는 기다린다. 제한 시간이 지났을 때만 끈다 |
