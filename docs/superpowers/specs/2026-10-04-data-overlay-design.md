# 하위 프로젝트 1 — 데이터 오버레이 설계

- 작성일: 2026-10-04
- 상위 문서: `docs/superpowers/specs/2026-10-04-configurable-rules-roadmap.md`
- 대상: Norland `0.5588.9777.0`
- 상태: 단계 0, 0b 완료(`research/01-data-overlay.md`, `research/02-new-game-state.md`). 단계 1 완료
  (계획 `docs/superpowers/plans/2026-10-05-data-overlay-stage1.md`, 근거 `research/03-data-files.md`, 실측 결과 `research/04-overlay-verify.md`).
  단계 2(효과 실측)는 계획을 썼다(`docs/superpowers/plans/2026-10-05-data-overlay-stage2-effects.md`, §5)

## 1. 목적

프리셋에 적은 값을 게임의 데이터 파일에 입히고, 언제든 바닐라로 되돌릴 수 있게 한다.
그 전에, 파일의 값이 게임에 실제로 반영되는지를 잰다.

두 단계로 나눈다.

- **단계 0 — 실측.** 무엇이 반영되는지 모르는 채로 도구를 만들지 않는다.
- **단계 1 — 오버레이 도구.** 카탈로그에 올린 키만 다루고, 키마다 무엇이 확인됐는지를 등급으로 적는다(§4.3).

구현 계획은 단계마다 따로 쓴다. 단계 1의 카탈로그가 단계 0의 결과에 달려 있다.

단계 1 뒤에 **단계 2 — 효과 실측**(§5)을 더했다: 값을 바꾸면 게임이 달라지는지를 잰다.

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
- JSON 4,499개(`sounds\`의 201개를 빼고 센 수다. 다 세면 4,701개. `research/03-data-files.md`) 가운데 엄격한 파서가 못 읽는 것은 3개이고 모두 trailing comma다:
  `debug_params.json`, `gameplay_variables.json`, `localization\locale_definition.json`.
- `gameplay_variables.json`에 뒤 공백이 붙은 키 `"messenger_cost "`가 있고 exe에도 그대로 있다.
- 4,249개 파일은 한 줄로 붙어 있고 250개는 여러 줄(CRLF)이다. 숫자 표기가 섞여 있다
  (`0.0`, `0`, `0.20000000298023224`). 4,701개 전체의 집계는 `research/03-data-files.md`에 있다.

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

계획을 쓸 때는 몰랐고 실행에서 잰 것 (2026-10-05, `research/02-new-game-state.md`):

- 룸은 바뀌지 않는다. 처음부터 끝까지 `rm_game`이다. 게임 화면은 `o_main_menu`가 없고 `o_character`가 있는 것으로 알아본다
  (새 게임에서 잰 것이다. 세이브를 불러올 때는 재지 않았다). `o_time_controller`와 `o_camera_controller`는 메뉴에서 이미 있다.
- 데이터 파일의 값은 게임을 켤 때 ds_map으로 읽히고, 게임을 시작하면 일부가 `o_debug`와 `o_province_controller`의 변수로 옮겨진다.
- ds 형 상수 1(map)과 2(list)는 이 러너에서 맞다. ds 번호는 0부터 빈틈없이 배정돼 있었다.
- `CCode`의 배치가 이 러너와 맞는다. 코드 이름 121개를 읽었고 못 읽은 것은 없다.
- 덤프 한 번에 메뉴에서 1.6초, 게임 안에서 2.0~2.1초가 걸린다.
- 런타임의 오브젝트는 65개다(`data.win`에서 센 것은 64개). 하나가 어디서 오는지는 모른다.

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

단계 0과 0b의 결과: `research/01-data-overlay.md`, `research/02-new-game-state.md`. 요약하면 값을 바꿔 본 다섯 파일
(`debug_params.json`, `gameplay_variables.json`, `battle_params.json`, `director_params.json`, 지식 파일 하나) 모두
바꾼 값이 런타임에 올라왔다. 값은 게임을 켤 때 ds_map으로 읽힌다(단계 0에서 못 찾은 것은 ds_map 안을 보지 않아서였다).
파일에 없는 키를 넣는 것은 시험한 한 경로(`game.speed_slower_div`)에서 통하지 않았다.
그래서 다섯 파일 모두 단계 1의 대상이 되고, 프리셋을 입힌 뒤에는 게임을 다시 켜야 한다.
다만 "런타임에 올라온다"와 "게임이 달라진다"는 다르다. 효과까지 본 것은 `initial_budget` 하나이고,
`budget_money`는 `default_budget_money`로 옮겨지지만 화면의 시작 금화는 3000이었다. 카탈로그는 이 둘을 따로 적는다(§4.3).

단계 0에서 스펙 §3과 달라진 점: 실행 1은 요청의 스크립트 호출(인자 형 오류)로 게임이 끝나 실패했고 다시 뜨지 않았다.
실행 3에서 추가한 키는 §3.4의 규칙 대신 사용자 승인을 받아 `game.speed_slower_div`로 했다. 찾기의 한도는
방문 200만, 배열 64 이하로 바꿨다. 모듈은 찾기를 스크립트 호출보다 먼저 한다.

2026-10-05에 사용자가 `Norland_Modding_Reference.md`(커뮤니티 실험을 모은 참조 문서)를 주었다. 그 문서의 주장을 설치본과
맞춰 본 결과와, 설계가 기대는 그 밖의 실측은 `research/03-data-files.md`에 있다. 문서에서 받아들인 것: 정보 확정도의 네 등급(§4.3),
항목마다 근거를 적는 것, 위험한 키를 실험 설정으로 가르는 것, 파일별 키 목록을 뽑는 것(`keys` 명령).
받아들이지 않은 것: 영역별 설정 파일 묶음(`config/gameplay/bribe.json` …). 로드맵 v0.2에서 정한 프리셋 + 카탈로그를 그대로 쓴다.
문서의 파일 이름과 값은 2024년 빌드의 것이 섞여 있어, 키 이름과 값은 모두 이 설치본의 파일에서 가져온다.

### 4.0 근거

| 사실 | 출처 |
|---|---|
| 게임 폴더의 JSON은 4,701개이고 전부 UTF-8(BOM 없음), 뿌리는 객체다 | `research/03` |
| 표준 JSON에서 벗어나는 것은 닫는 괄호 앞의 쉼표뿐이다(세 파일). 주석, 작은따옴표, 따옴표 없는 키, `NaN`, 지수 표기, 한 객체 안의 같은 키는 한 번도 없다 | `research/03` |
| 수 254,251개 가운데 509개는 가장 짧은 왕복 표기가 아니다(`0.90000000000000002`). 파싱해서 다시 쓰면 글자가 달라진다 | `research/03` |
| 키에 점·대괄호·별표가 든 것은 없다. 공백이 든 키가 둘 있고(`"messenger_cost "`), 수로 된 키가 있다(`unit_skill_stage`의 `"0"`, `"1"`) | `research/03` |
| 게임의 JSON 파서가 지수 표기나 주석을 읽는지는 **모른다** | 재지 않았다. 그래서 도구는 게임 파일에 있는 표기만 쓴다 |
| 값은 게임을 켤 때 읽힌다. 메인 메뉴에서 이미 런타임에 있다 | `research/02` |
| `tools/data-restore.ps1`은 스냅샷 폴더 아래의 모든 파일을 게임 폴더로 복사한다 | 그 스크립트. 그래서 상태 파일을 스냅샷 폴더에 두지 않는다 |
| 지금의 스냅샷 다섯 개는 바닐라다. Steam이 2026-10-04 16:20에 쓴 뒤 수정 시각이 바뀌지 않은 파일에서 떴다 | `research/03` |
| `gameplay_variables.json`의 값 150개 가운데 94개가 `global.__gameplay_vars`에 같은 이름·값으로 있고 56개는 없다 | `research/01`, `research/03` |
| 같은 이름·값이 런타임에 있다는 것은 파일에서 읽혔다는 증거가 아니다. `__gameplay_vars`에는 파일에 없는 이름이 326개 있다 | `research/01` |
| 커뮤니티가 바꿔서 통했다고 적은 키(2024년 빌드): `bribe.give_rings`, `free_lord.ring_by_n_levels`, `church.max_capacity`, `tavern.max_capacity`, `pregnancy.from_dummy_chance`, `product_count`, 교본의 `upgrade_skill[].value` | Steam 토론 두 글. `research/03`에 주소와 날짜. **이 빌드에서 다시 재지 않았다** |
| 값의 허용 범위 | **모른다.** 어떤 값이 게임을 죽이는지 재지 않았다 |

### 4.1 구성

```
tools/overlay/                 Python 3.14. 표준 라이브러리만 쓴다
  jsonedit.py                  값의 자리를 찾고 그 자리의 글자만 바꾼다
  paths.py                     경로 식(키, 와일드카드, 배열 첨자)
  catalog.py                   카탈로그 읽기와 검증
  preset.py                    프리셋 읽기와 검증
  compose.py                   프리셋 + 카탈로그 + 바닐라 글 → 새 글 (파일을 읽고 쓰지 않는다)
  store.py                     게임 폴더에 쓰기: 스냅샷, 적용, 복원, 상태
  verify.py                    런타임 확인: 요청 파일 만들기, 덤프 판정
  cli.py                       명령
  tests/                       unittest
catalog/<게임 버전>/keys.json   지원하는 키와 등급 (손으로 쓴다)
catalog/<게임 버전>/files.json  파일마다 바닐라의 SHA256 (`pin` 명령이 만든다)
presets/*.json                 프리셋
tools/overlay.ps1              게임 상태를 검사하고 경로를 채워 cli.py 를 부른다
backups/data/<게임 버전>/       바닐라 스냅샷 (단계 0의 것을 그대로 쓴다. 추적 안 함)
backups/overlay/<게임 버전>/state.json   마지막에 입힌 프리셋과 그때 쓴 파일의 해시 (추적 안 함)
```

게임의 데이터 파일을 쓰는 코드는 `store.py` 하나다. 나머지는 글과 자료구조만 다룬다.

### 4.2 프리셋

```json
{ "name": "Easy",
  "game_version": "0.5588.9777.0",
  "allow_experimental": false,
  "changes": [
    { "file": "debug_params.json", "path": "budget_money", "set": 5000 },
    { "file": "debug_params.json", "path": "building_resources.*[*][1]", "mul": 0.5, "round": "ceil" },
    { "file": "knowledge/technology/textbooks/*.json", "path": "upgrade_skill[*].value", "mul": 2 } ] }
```

- 연산은 `set`(수를 지정)과 `mul`(바닐라 값에 곱함) 둘이다. `mul`에는 `round`(`ceil`, `floor`, `nearest`)를 줄 수 있다.
  `nearest`는 반을 올린다. 1을 곱하면 그대로이고 정수끼리의 곱은 정확하다. 그 밖의 곱은 유효숫자 15자리로 다듬는다
  (0.1 × 3이 0.30000000000000004가 되지 않게).
- **수만 바꾼다.** 문자열, 불리언, `null`, 객체, 배열에 닿는 변경은 오류다.
- 경로: 점으로 키를 잇고, `*`는 객체의 모든 키, `[n]`과 `[*]`는 배열 첨자다. 점·공백·대괄호·별표·따옴표가 든 키는
  `paper["messenger_cost "]`처럼 JSON 문자열로 적는다. 점 뒤의 수는 키다(`unit_skill_stage.1`).
- `file`은 게임 폴더 기준 상대경로이고 `/`로 나눈다. 한 조각 안에서 `*`를 쓸 수 있다.
- 한 변경이 아무 값에도 닿지 않으면 오류다. 조용히 넘어가지 않는다. 파일 패턴이 여러 파일에 맞을 때는 그 가운데 하나에만 닿아도 된다.
- 한 값에 변경이 둘 닿으면 뒤의 것이 이긴다. 둘 다 바닐라 값에서 계산한다.
- 새 값이 바닐라 값과 같으면 그 자리는 건드리지 않는다.
- 모르는 항목이 있으면 오류다(오타가 조용히 무시되지 않게).

### 4.3 카탈로그

카탈로그는 이 도구가 바꿀 수 있는 키의 목록이고, 키마다 무엇을 실측했는지를 적는다. **프리셋은 카탈로그에 있는 키만 바꿀 수 있다.**

`keys.json`은 묶음의 목록이다. 묶음 하나는 파일(패턴), 경로 식들, 등급, 근거를 적은 글을 가진다. 한 값에 맞는 묶음이 여럿이면 앞의 것이다.

```json
{ "file": "debug_params.json", "paths": ["budget_money"],
  "runtime": "VERIFIED", "effect": "UNKNOWN",
  "note": "2345 로 바꾸자 … (research/02)" }
```

등급은 두 축이다.

| `runtime` — 이 레포가 이 빌드에서 잰 것 | |
|---|---|
| `VERIFIED` | 파일의 값을 바꿔서 런타임에서 그 값을 봤다. 어디서 봤는지는 `note`에 적는다. 그 파일을 읽은 ds_map에서만 본 것도 여기 든다(값이 메모리에 올라왔다는 뜻이지 게임이 쓴다는 뜻이 아니다) |
| `SEEN` | 바닐라 값이 같은 이름으로 런타임에 있는 것을 봤다. 코드의 기본값이 같은 경우와 가려지지 않는다 |
| `UNSEEN` | 런타임에서 본 적이 없다 |

| `effect` — 값이 게임을 바꾸는가 (참조 문서 §16의 등급) | |
|---|---|
| `OFFICIAL` | 개발진이나 공식 문서가 말했다 |
| `TESTED` | 게임에서 바꿔 보고 달라진 것을 봤다. `effect_by`에 누가 봤는지 적는다(`nltoybox`, `community`) |
| `INFERRED` | 이름이나 구조로 미루어 본 것 |
| `UNKNOWN` | 모른다 |

- `runtime`이 `UNSEEN`이거나 묶음에 `"experimental": true`가 있으면 **실험 키**다. 프리셋에 `"allow_experimental": true`가 있어야 바꿀 수 있다.
- `min`, `max`는 허용 범위다. 지금은 잰 것이 없어 아무 묶음에도 적지 않는다. 범위를 재면 적는다.
- 세이브에 굳는지, 새 게임이 필요한지는 잰 것이 없다. 칸을 두지 않고, 재면 더한다.
- 지금의 카탈로그는 파일 125개의 수 1,437개 가운데 1,155개를 다룬다: VERIFIED 34, SEEN 219, UNSEEN 902
  (단계 1의 실측 전에는 5, 234, 916이었다. `research/03`, `research/04`).
  지식 파일의 `tag`, `available_parameters[n].type`, `hint_fields.*`는 올리지 않는다.
- 카탈로그에는 키의 이름과 등급, 파일의 해시만 둔다. 게임 파일의 값은 두지 않는다(저작물 경계). 값까지 든 표는
  `keys` 명령으로 `refs\registry\`에 뽑는다(추적 안 함).

`files.json`은 파일마다 바닐라의 SHA256을 적는다. 도구는 이것으로 "이 파일이 바닐라인가"를 판정한다.
게임 파일이 바닐라일 때(Steam 설치나 무결성 검사 직후) `pin` 명령으로 만든다.

등급을 올리는 길: 그 키를 흔치 않은 값으로 바꾸는 프리셋을 입히고, `probe-request`로 요청 파일을 만들어 `tools/probe.ps1`로
메인 메뉴의 덤프를 받고, `verify`로 판정한다. 값이 런타임에 있고 그 경로의 이름 조각 하나가 키 이름과 같으면(`anchored`.
더 긴 이름의 일부인 것은 치지 않는다) `VERIFIED`로 올리고 근거를 `research/`에 적는다. 값은 게임 JSON 어디에도 없는 수로 고르고,
이미 `VERIFIED`인 키 하나를 양성 대조로 함께 바꾼다. 양성 대조가 `anchored`가 아니면 그 실행의 `absent`를 믿지 않는다.

### 4.4 적용과 복원

- 파일의 상태는 넷 가운데 하나다: **바닐라**(해시가 `files.json`과 같다), **입힌 것**(해시가 `state.json`에 적힌 것과 같다),
  **모르는 것**(둘 다 아니다), **없음**.
- 카탈로그의 파일 가운데 하나라도 모르는 것이거나 없으면 적용도 복원도 하지 않는다. 게임이 갱신됐거나 밖에서 고친 것이다.
  사용자에게 Steam 무결성 검사나 `tools/data-restore.ps1`을 안내한다.
- 적용 전에 확인한다: 게임이 꺼져 있다(래퍼), 카탈로그와 프리셋의 `game_version`이 지금 exe 버전과 같다, 프리셋 전체가 오류 없이 계산된다.
  하나라도 어긋나면 아무것도 쓰지 않는다.
- 스냅샷은 프리셋 전체의 계산이 끝난 뒤, 실제로 바뀌는 파일만 뜬다. 바닐라일 때만 뜨고, 읽은 바이트의 해시를 `files.json`과 다시 견준다.
  스냅샷이 이미 있으면 그 해시가 `files.json`과 같은지 본다(고친 파일에서 뜬 스냅샷을 믿지 않는다). 스냅샷은 원본의 수정 시각을 물려받는다.
- **적용은 항상 바닐라에서 출발한다.** 차례: (1) 앞서 입힌 파일을 모두 바닐라로 되돌린다, (2) 새 상태를 `state.json`에 적는다,
  (3) 새 내용을 쓴다. 프리셋을 바꿔도 변경이 쌓이지 않는다. 이 차례로 하면 어느 쓰기에서 죽어도 게임 파일은
  바닐라 아니면 `state.json`에 적힌 해시다(모르는 것이 되지 않는다).
- 파일은 임시 파일에 쓴 뒤 바꿔치기한다(`<이름>.nltoybox-tmp`). 반쯤 쓰인 파일이 남지 않는다.
- 복원은 입힌 파일을 스냅샷으로 되돌리고(수정 시각도), 다시 읽어 대조하고, `state.json`을 지운다.
- 쓰는 도중에 실패하면(읽기 전용 파일, 다른 프로그램이 잡은 파일) 무엇을 할지 알린다: 다시 `apply`하거나 `restore`한다.
  그때 `status`는 "부분 적용"이라고 알리고 종료 코드 1을 낸다. 모르는 파일이 있을 때는 프리셋을 "알 수 없다"고 적는다.
- 상태는 `backups\overlay\<게임 버전>\state.json`에 둔다. 스냅샷 폴더에 두면 `data-restore.ps1`이 게임 폴더로 복사한다.
- 게임이 갱신돼 exe 버전이 바뀌면 그 버전의 카탈로그가 없어 래퍼가 거부한다.

### 4.5 수정기의 요구

- 바꾸는 값의 글자 범위만 바꾼다. 나머지 바이트는 그대로다(닫는 괄호 앞의 쉼표, 공백, 줄바꿈, 키의 뒤 공백, 다른 수의 표기, BOM).
- 아무것도 바꾸지 않는 적용은 파일을 바이트 단위로 그대로 둔다.
- 새 값의 표기: 정수는 정수로, 실수는 가장 짧은 왕복 표기로 쓴다. 그 자리에 있던 글에 소수점이 있으면 정수에도 `.0`을 붙인다.
  지수 표기는 쓰지 않는다(`0.00001`로 푼다). 유한하지 않은 수는 오류다.
- 게임 파일에 한 번도 없던 문법(주석, 작은따옴표, 따옴표 없는 키, `NaN`, 한 객체 안의 같은 키)을 만나면 오류다.
  게임이 그것을 어떻게 읽는지 모르므로 추측해서 고치지 않는다. UTF-8로 읽히지 않는 파일도 오류다.

### 4.6 명령

`tools/overlay.ps1 <명령>`이 게임 폴더, exe 버전, 카탈로그·스냅샷·상태 폴더를 채워 `cli.py`를 부른다.
`apply`와 `restore`는 게임이 켜져 있으면 거부한다.

| 명령 | 하는 일 | 게임 파일을 쓰는가 |
|---|---|---|
| `check -Preset <파일>` | 입히면 무엇이 바뀌는지 보여 준다 | 아니다 |
| `apply -Preset <파일>` | 바닐라로 되돌린 뒤 프리셋을 입힌다 | 쓴다 |
| `restore` | 바닐라로 되돌린다 | 쓴다 |
| `status` | 파일마다의 상태와 입혀진 프리셋의 이름 | 아니다 |
| `keys [-Out <파일>] [-File <패턴>]` | 카탈로그 파일에 든 수를 모두 적는다(경로, 바닐라 값, 등급). 카탈로그에 없는 키도 나온다 | 아니다 |
| `pin` | `keys.json`의 파일 패턴을 게임 폴더에서 찾아 지금의 해시로 `files.json`을 만든다. 프리셋이 입혀져 있으면(이 버전이든 옛 버전이든) 거부한다. 지금의 파일이 바닐라인지는 알 수 없으므로 Steam 무결성 검사 직후에만 돌린다 | 아니다 (레포에 쓴다) |
| `scan` | 게임 폴더의 모든 `*.json`을 읽어 본다: 읽히는가, 같은 글을 다시 쓰면 바이트가 같은가, 값이 표준 파서와 같은가 | 아니다 |
| `probe-request -Preset <파일> -Out <파일>` | 프리셋이 쓰는 값을 런타임에서 찾는 요청 파일을 만든다 | 아니다 |
| `verify -Preset <파일> -Dump <파일>` | 덤프에서 프리셋의 값을 찾아 `anchored` / `value-only` / `absent`로 판정한다. 덤프에 찾기 구역이 없거나 끊겼으면 `absent` 대신 `unknown`을 적는다 | 아니다 |

### 4.7 시험

- 단위 시험(`tools/overlay/tests/`): 수정기(닫는 괄호 앞의 쉼표, 뒤 공백 키, 한 줄 파일, CRLF, 중첩 배열, 문자열 안의 따옴표·역슬래시,
  없던 문법의 거부), 경로 식, 카탈로그와 프리셋의 검증, 계산, 임시 폴더의 가짜 게임으로 적용·복원 왕복, 쓰기마다 죽여 보는 시험.
- 래퍼(`tools/tests/safety.tests.ps1`): 가짜 게임 폴더(`NORLAND_GAME_DIR`)로. 켜져 있으면 거부, 밖에서 바뀐 파일이 있으면 거부,
  복원 뒤 바이트가 원본과 같다.
- 실제 게임 파일(읽기 전용): `scan`이 4,701개 모두에서 통과한다. 레포의 카탈로그와 프리셋이 읽힌다.
- 실제 게임에서의 반영 확인은 `tools/probe.ps1`을 다시 쓴다(§4.8).

### 4.8 실측 실행과 완료 기준

도구를 만든 뒤 게임을 한 번 켠다(예비 한 번). **메인 메뉴까지만 간다.** 값은 게임을 켤 때 읽히므로 새 게임을 시작하지 않아도 되고,
사용자가 할 일이 없다(게임 창을 누르지 않는다).

- 검증 프리셋(`presets/verify-stage1.json`)이 아직 `VERIFIED`가 아닌 묶음마다 값 하나씩 29개와, 양성 대조 하나(이미 `VERIFIED`인
  `initial_budget`)를 바꾼다. 값 30개는 게임 폴더의 JSON 4,701개 어디에도 없는 수다(2026-10-05에 훑어 확인했다).
- `apply` → `status` → `probe-request` → `tools/probe.ps1` → `status`(게임이 데이터 파일을 다시 썼는지) → `dump_tool.py controls` → `verify` → `restore` → `tools/restore-game.ps1`.
- `anchored`인 키는 카탈로그에서 `VERIFIED`로 올린다. `value-only`는 덤프의 이름 히트로 사람이 판단하고 근거를 적는다.
  `absent`는 `UNSEEN`으로 두고 "메뉴에서 값으로 찾았으나 없었다"고 적는다.
- 게임이 메뉴에 닿기 전에 끝나면 그것이 결과다(어떤 값이 게임을 죽였다). 되돌리고 보고한다. 값을 나눠 다시 재는 것은 따로 승인받는다.

완료 기준:

1. 단위 시험, 안전 시험, 기존 시험이 모두 통과한다.
2. `scan`이 실제 게임 폴더에서 실패 0으로 끝난다.
3. 검증 프리셋의 값마다 판정이 `research/04-overlay-verify.md`에 있고, 카탈로그의 등급이 그 판정과 맞는다.
4. 끝난 뒤 게임이 바닐라다: `overlay.ps1 status`가 125개 모두 바닐라, exe 해시가 원본, `mods\`와 `aurie.log`가 없다.

**실측 결과 (2026-10-05, `research/04-overlay-verify.md`):** 게임을 한 번 켰다. 값 30개가 모두 메인 메뉴의 런타임에 있었다
(`anchored` 29, `value-only` 1을 이름 히트로 이어 확인, `absent` 0). 양성 대조가 맞았고 덤프의 점검이 모두 `ok`였다. 29개를 `VERIFIED`로 올렸다.
`global.__gameplay_vars`에 이름이 없는 키 둘(`bribe.cooldown`, `prestige.for_population`)은 파일을 읽은 ds_map에서만 보여 실험 키로 남겼다.
게임은 종료할 때 데이터 파일을 다시 쓰지 않았고, 복원 뒤 125개 파일이 해시와 수정 시각까지 바닐라였다.

효과(`effect`)를 재는 것은 이 단계의 일이 아니다. 새 게임을 시작해 화면에서 봐야 하고 키마다 방법이 다르다.
도구가 선 뒤에 키 묶음별로 따로 잰다.

## 5. 단계 2 — 효과 실측

단계 1까지는 "파일의 값이 런타임의 메모리에 올라온다"를 쟀다(`research/04-overlay-verify.md`). 이 단계는 그 다음을 잰다:
**값을 바꾸면 게임이 달라지는가.** 지금까지 효과를 본 키는 `initial_budget` 하나다. `budget_money`는 런타임에 올라오지만 화면의 시작 금화는
그 값이 아니었다(`research/02`).

한 번의 실행으로 잴 수 있는 것부터 잰다: 새 게임의 첫 화면에서 보이는 것(시작 금화, 시작 자원)과, 게임을 시작한 뒤 값이 옮겨지는 자리.
건물 비용, 거래 가격, 전투, 이벤트 빈도처럼 플레이해야 보이는 효과는 이 단계의 결과(값이 게임 안 어디에 앉는가)를 보고 따로 계획한다.

### 5.1 근거

| 사실 | 출처 |
|---|---|
| 새 게임의 설정에 지역과 난이도의 선택이 있다 | `refs/strg.txt`의 `gui_main_menu_choose_province_difficulty_settings`, `GAME_CONDITIONS.get_difficulty()`. 전역 `__new_game_initializer`의 멤버 `__choosed_province`, `__game_conditions`(단계 0b의 덤프) |
| 지역은 25곳이고 지역마다 난이도(Easy, Normal, Hard, Extreme)와 마을의 자원·수량이 있다. 금화의 열은 없다 | `NewWorldParams.csv` |
| 화면 위의 자원 표시에 금화 요소가 있다 | `refs/strg.txt`의 `gui_hud_resources_element_money_create` |
| 금화를 다루는 스크립트: `budget_money_get`, `budget_money_change`, `budget_default_money_get`, `budget_default_money_set` | `refs/strg.txt`. 인자의 형을 모른다. **부르지 않는다** |
| 게임 안의 덤프(깊이 6)에서 전역과 인스턴스의 한 단계 목록에 값 3000이 없다. `o_province_controller`의 변수는 여섯이고 그 가운데 `default_budget_money`, `production_cost`(배열 39), `default_resource_count`(배열 39)가 있다 | `refs/runtime/stage0b-run2.late1.json` |
| 그 덤프에서 깊이 한도에 걸려 들어가지 못한 구조체·배열이 전역 23,810개, ds 61,603개다 | `research/02` |
| 덤프 한 번에 깊이 6, 방문 약 186만에서 2.0~2.1초가 걸렸다. 더 깊이 볼 때의 시간은 **모른다** | `research/02` |
| 게임 JSON에는 0~999의 정수가 972와 987을 빼고 모두 있다. 1000~1999에는 없는 정수가 369개 있다 | 2026-10-05에 4,701개 파일을 훑음. 작은 정수는 값으로 찾으면 우연히 맞는다 |
| 이번 프리셋의 값 열 개(1006, 1031, 1057, 1101, 2767, 2.3719, 41.3719, 31.2917, 0.5719, 11.4373)는 게임 JSON 어디에도 없다 | 같은 날 `probe-request`의 프로토타입으로 확인 |
| 단계 0b의 실행에서 게임 화면은 모듈이 적재된 뒤 약 163초에 나왔고, 2분 반 동안 새 세이브가 생기지 않았다 | `research/02` |
| 메뉴까지만 간 실행에서 게임은 데이터 파일을 다시 쓰지 않았다. 새 게임을 시작한 실행에서는 **모른다** | `research/04` |

### 5.2 무엇을 "효과를 봤다"고 하는가

카탈로그의 `effect`를 `TESTED`(`effect_by: nltoybox`)로 올리는 기준이다.

| 본 것 | 카탈로그에 적는 것 |
|---|---|
| 화면의 수치가 바꾼 값과 같다(사용자가 읽는다) | `TESTED`. `note`에 "화면에서" |
| 게임이 만든 개체의 상태(도시의 재산, 창고의 재고처럼 플레이로 변하는 값)에서 바꾼 값을 봤다 | `TESTED`. `note`에 그 경로 |
| 설정을 들고 있는 자리에서만 봤다(`o_debug`, `o_province_controller`의 `default_*`와 배열, 기본 가격표) | `effect`는 그대로. `note`에 "게임을 시작하면 …로 옮겨진다" |
| 화면의 수치가 바꾼 값과 다르다 | `effect`는 그대로. `note`에 "화면의 …은 이 값이 아니었다"와 본 수치 |
| 게임 안의 덤프 어디에도 없다(점검이 모두 `ok`이고 양성 대조가 맞았을 때) | `effect`는 그대로. `note`에 그 사실 |

어느 줄에 드는지 가리기 어려우면 올리지 않고 본 것을 그대로 적는다.

### 5.3 답할 질문

7. 시작 금화는 어디서 오는가. 화면의 금화는 얼마이고, 런타임의 어느 변수에 있으며, `budget_money`를 바꾼 값은 어디에 있는가.
   고른 지역과 난이도는 무엇이었는가.
8. `product_count`를 바꾸면 시작 자원이 달라지는가(나무, 당근. 바꾸지 않는 약은 그대로인가).
9. 게임을 시작하면 값이 어디로 옮겨지는가: `production_cost`, `fair_trade`의 가격, `building_resources`, `building_duration_factor`, `battle_dodge_shift_better`.
10. 새 게임을 시작한 실행에서도 게임이 데이터 파일을 다시 쓰지 않는가.

### 5.4 프리셋과 요청

`presets/effect-stage2.json`. 값은 모두 게임 JSON에 없는 수다. 화면에서 읽을 것은 정수로, 옮겨지는 자리만 볼 것은 원래 값과 비슷한 크기의 소수로 골랐다
(게임을 시작해 몇 분 돌아야 하므로 터무니없는 크기를 피한다).

| 키 | 값 | 무엇을 보려는가 |
|---|---|---|
| `gameplay_variables.json` `global_map.ai_economy.initial_budget` | 700 → 1006 | **양성 대조.** `global.__gameplay_vars.…initial_budget`와 AI 도시의 `__initial_wealth`에서 나와야 한다(`research/02`) |
| `debug_params.json` `budget_money` | 2000 → 2767 | 질문 7 |
| `debug_params.json` `product_count.wood`, `product_count.carrot` | 300 → 1031, 200 → 1057 | 질문 8. `product_count.medicine`은 바꾸지 않는다(같은 실행 안의 대조) |
| `debug_params.json` `production_cost.ale` | 2 → 2.3719 | 질문 9 |
| `debug_params.json` `fair_trade.fair_trade_purchase.ale`, `fair_trade_sale.ale` | 40 → 41.3719, 30 → 31.2917 | 질문 9 |
| `debug_params.json` `building_resources.woodcutter_lvl_1[0][1]` | 15 → 1101 | 질문 9 |
| `debug_params.json` `building_duration_factor` | 0.5 → 0.5719 | 질문 9 |
| `battle_params.json` `battle_dodge_shift_better` | 11 → 11.4373 | 질문 9 |

요청 파일은 앞머리(`tools/probes/stage2-effect.head.txt`)에 프리셋의 값을 찾는 줄을 붙여 만든다(`overlay.ps1 probe-request -Header`).
앞머리가 정하는 것:

- 메뉴에서 한 번, 그 뒤로 60초마다 덤프하고 마지막 셋을 남긴다.
- 깊이 8까지 본다(깊이 6에서는 금화가 든 변수가 한 단계 목록에 없었다). 방문 한도는 구역마다 800만, 히트는 2만.
  **덤프에 걸리는 시간은 이번에 잰다.** 방문 한도가 시간을 묶는다.
- 새 게임의 흐름을 기록한다(`__new_game_initializer`의 `__is_active`, `__current_step`, `__choosed_province`, `__game_load_operator.__game_is_loading`).
- 금화의 출처: 값 3000, 이름에 `budget`, `money`, `difficulty`, `game_conditions`가 든 것. 시작 자원: `default_resource_count`.

게임 스크립트는 부르지 않는다.

### 5.5 도구의 고침

- `probe-request`가 앞머리 파일을 받는다(`-Header`). 주지 않으면 지금처럼 메뉴에서 한 번 덤프하는 요청을 만든다.
- `probe-request`가 프리셋의 값 가운데 게임 JSON에 이미 있는 수를 알려 준다(경고일 뿐 거부하지 않는다).
  프리셋이 입혀져 있어도 카탈로그의 파일은 바닐라의 내용으로 센다.

### 5.6 실행

게임을 한 번 켠다(예비 한 번은 그때 다시 승인받는다). **사용자가 할 일이 있다.**

1. 메인 메뉴가 뜨고 약 1분 뒤 화면이 한 번 멈췄다 풀리면(메뉴 덤프) 새 게임을 시작한다. 단계 0b와 같은 차례다.
   메뉴에서 뜬 덤프가 게임 안의 덤프와 견줄 기준이 된다. 설정 화면에서 고른 **지역과 난이도**를 기억해 둔다.
2. 게임 화면이 나오면 화면의 **금화, 나무, 당근, 약**의 수치를 읽는다. 보이면 건설 메뉴의 벌목꾼 건물(woodcutter)의 나무 비용도 읽는다(선택).
3. 게임 화면에서 3분쯤 둔다. 그 사이 60초마다 화면이 멈췄다 풀린다(덤프). 얼마나 멈출지는 이번에 잰다.
4. 평소처럼 게임을 끈다. 읽은 수치와, 게임 화면에 들어간 때와 끈 때를 대강 알려 준다.

판정에는 **게임 안에서 뜬 덤프만** 쓴다: `present`에 `o_character`가 있고 `o_main_menu`가 없는 덤프(`research/02`의 신호). 그런 덤프가 없으면
답을 "확인하지 못함"으로 적는다. "없다"를 쓰려면 그 덤프에서 `dump_tool.py controls`가 모두 `ok`이고 양성 대조가 맞아야 한다.

끝나면 `overlay.ps1 status`로 게임이 데이터 파일을 다시 썼는지 보고(질문 10), 되돌린다. 다시 썼으면(`unknown`) `tools/data-restore.ps1`로 되돌린다.

### 5.7 산출물과 완료 기준

- `research/05-effects.md`: 질문 7~10의 답과 근거, 사용자가 읽은 수치, 값마다의 런타임 경로, 덤프에 걸린 시간, 확인하지 못한 것.
- 카탈로그: §5.2의 기준대로 `effect`와 `note`를 고친다.
- 완료 기준: (1) 질문마다 "그렇다 / 아니다 / 확인하지 못함"과 근거가 있다. (2) 판정에 쓴 덤프가 게임 안의 것임을 적었다.
  (3) 끝난 뒤 게임이 바닐라다(`overlay.ps1 status`가 125개 모두 바닐라, exe 해시가 원본, `mods\`와 `aurie.log`가 없다).
  (4) 세이브 폴더에서 달라진 파일을 사용자에게 알렸다.

## 6. 범위 밖

- 런타임에서 값을 쓰는 것 (하위 프로젝트 2).
- 편집 UI (하위 프로젝트 3).
- 새 건물·이벤트·규칙, Mod API (하위 프로젝트 4).
- 세이브 파일을 읽거나 고치는 것.
- `.map_template`(바이너리가 붙어 있다)과 CSV의 수정.
- 수가 아닌 값(문자열, 불리언)을 바꾸는 것과 키를 더하거나 빼는 것. 지식의 선행 조건(`knowledge_name`)이나 이벤트의 분류가 여기 든다.
  필요가 확인되면 다시 정한다.
- 플레이해야 보이는 효과(건물 비용, 거래 가격, 전투, 이벤트 빈도)를 재는 것. 단계 2(§5)가 새 게임의 첫 화면에서 보이는 것까지 잰 뒤에 따로 계획한다.
- 카탈로그 밖의 파일(`building_constructor\`, `generator\`, `maps\`).

## 7. 위험

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
| (1) 고친 파일에서 뜬 스냅샷으로 "복원"한다 | 바닐라인지는 카탈로그의 SHA256으로 판정한다. 스냅샷도 그 해시와 같을 때만 쓴다 |
| (1) 게임 갱신이나 무결성 검사 뒤에 옛 스냅샷으로 덮어쓴다 | 바닐라도 아니고 마지막에 쓴 것도 아닌 파일이 하나라도 있으면 아무것도 쓰지 않는다. exe 버전이 바뀌면 그 버전의 카탈로그가 없어 거부한다 |
| (1) 적용 도중에 죽는다 | 되돌리기 → 상태 적기 → 새 내용 쓰기의 차례와 임시 파일 바꿔치기. 어느 쓰기에서 죽어도 파일은 바닐라 아니면 상태에 적힌 해시다(시험으로 확인한다) |
| (1) 프리셋의 값이 게임을 죽인다 | 허용 범위는 잰 것이 없다. `restore`로 되돌린다. 죽인 값을 알게 되면 카탈로그에 범위를 적는다 |
| (1) `SEEN`을 "반영된다"로 읽는다 | 등급의 뜻을 카탈로그와 `check`의 출력에 적는다. 실측 실행으로 `VERIFIED`를 늘린다 |
| (1) 커뮤니티의 보고(2024년 빌드)를 이 빌드의 사실로 쓴다 | `effect_by: community`와 출처를 따로 적는다. `runtime` 등급은 이 레포가 잰 것만 올린다 |
| (2) 바꾼 값 때문에 새 게임이 시작되지 않거나 도중에 끝난다 | 값을 원래 값과 비슷한 크기로 고른다. 그래도 끝나면 그것이 결과다. 되돌리고 보고한다 |
| (2) 깊이 8의 덤프가 게임을 오래 멈춘다 | 방문 한도(구역마다 800만)가 시간을 묶는다. 사용자에게 미리 알리고, 걸린 시간을 재서 적는다 |
| (2) 게임 안의 덤프를 하나도 얻지 못한다 | 덤프마다 `present`로 상태를 가린다. 없으면 "확인하지 못함"으로 적고 예비 실행을 승인받는다 |
| (2) 설정을 들고 있는 자리에서 본 것을 효과로 적는다 | §5.2의 표로만 올린다. 가리기 어려우면 올리지 않는다 |
| (2) 새 게임이 세이브를 만든다 | 실행 전에 세이브 폴더의 사본을 뜬다. 생긴 파일을 알리고, 지우는 것은 사용자가 정한다 |
