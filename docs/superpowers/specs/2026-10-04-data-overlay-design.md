# 하위 프로젝트 1 — 데이터 오버레이 설계

- 작성일: 2026-10-04
- 상위 문서: `docs/superpowers/specs/2026-10-04-configurable-rules-roadmap.md`
- 대상: Norland `0.5588.9777.0`
- 상태: 설계 승인(대화) → 문서 검토 대기

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

- 요청 파일 `mods\Aurie\NlToyBox.probe.json`이 있을 때만 동작한다. 없으면 Phase 0과 똑같이 동작한다.
- 요청 파일의 내용:

  ```json
  { "delay_seconds": 60,
    "scripts": [ { "name": "gml_Script_budget_default_money_get", "args": [] },
                 { "name": "gml_Script_resource_default_count_get", "args": ["wood"] } ] }
  ```

- 모듈이 적재된 뒤 `delay_seconds`가 지나고 처음 오는 `EVENT_OBJECT_CALL`에서(게임 스레드) 한 번 실행한다.
  기다리는 이유: 모듈은 게임 창이 뜬 직후 적재되고, 데이터 로딩은 그 뒤 약 30초 동안 이어진다(실측).
- 하는 일:
  1. 전역 변수의 이름을 모두 얻는다. 이름마다 형을 적고, 수·문자열·불리언이면 값을 적는다.
     구조체면 멤버 이름과 수·문자열·불리언 멤버의 값을 한 단계만 적는다. 배열이면 길이를 적는다.
  2. 요청의 스크립트를 차례로 부르고 반환값을 적는다. 부르지 못하면 오류 상태를 적는다.
- 결과를 `mods\Aurie\NlToyBox.dump.json`에 쓴다. 다 쓴 뒤 `NlToyBox.log`에 `dump done`을 남긴다.
- 문자열 값은 200자에서 자른다. 값을 읽다 실패한 항목은 건너뛰지 않고 `"error"`로 적는다.

덤프는 읽기만 한다. 게임의 값을 바꾸지 않는다.

### 3.3 도구

- `tools/probe.ps1 [-Request <파일>] [-Out <파일>] [-TimeoutSec 240]`
  - 요청 파일을 `mods\Aurie\`에 놓고, 게임을 켜고, `dump done`을 기다리고, 덤프를 `-Out`으로 복사하고,
    게임 창에 `WM_CLOSE`를 보내 정상 종료시킨다. 15초 안에 끝나지 않으면 강제 종료한다.
  - 거부 조건은 `check-load.ps1`과 같다(실행 중, 미패치, DLL 누락).
  - 요청 파일과 덤프는 끝난 뒤 게임 폴더에서 지운다. `restore-game.ps1`의 삭제 목록에도 넣는다.
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
| 1 | 바닐라 | 기준 덤프 `refs\runtime\dump-vanilla.json` |
| 2 | 세 값을 바꿈: `debug_params.json`의 `budget_money` 2000 → 2345, `gameplay_variables.json`의 값 하나, `battle_params.json`의 `battle_dodge_base` 20 → 23 | 덤프 `dump-changed.json`. 기준과 비교해 어느 값이 움직였는지 본다 |
| 3 | 2의 상태에 더해, `gameplay_variables.json`에 exe에만 있는 이름에 대응하는 키를 하나 추가 | 덤프 `dump-added.json`. 추가한 키가 반영됐는지 본다 |

- 실행 2에서 바꿀 `gameplay_variables.json`의 값과 실행 3에서 추가할 키는 실행 1의 덤프를 보고 고른다.
  덤프에서 위치를 찾을 수 있는 것이어야 비교가 된다. 고른 값과 이유를 조사 기록에 적는다.
- 실행 3에서 키를 어디에 어떤 이름으로 넣을지는, 덤프의 변수 이름과 파일의 키 경로가 어떻게 대응하는지
  (예: `farm.rye.duration`과 전역 이름) 본 뒤에 정한다. 대응 규칙이 보이지 않으면 실행 3은 하지 않고
  "확인하지 못함"으로 적는다.
- 끝나면 `data-restore.ps1`과 `restore-game.ps1`로 되돌리고, 게임 파일이 스냅샷과 같은지 확인한다.

### 3.5 산출물

- `research/01-data-overlay.md`: 세 질문의 답, 반영이 확인된 키와 확인되지 않은 키, 덤프의 구조 요약,
  파일 키와 런타임 이름의 대응 규칙(보이면).
- `refs\runtime\dump-*.json` (추적하지 않는다).
- 덤프 기능이 든 모듈과 `tools/probe.ps1`, `data-snapshot.ps1`, `data-restore.ps1`.

### 3.6 단계 0의 완료 기준

1. 모듈이 요청 파일 없이 켜졌을 때 Phase 0과 같은 로그를 낸다(`tools/check-load.ps1`의 판정 기준 유지).
2. 실행 1의 덤프가 생기고, 전역 변수 항목이 하나 이상 있으며, `gml_Script_budget_default_money_get`의
   반환값이 적혀 있다.
3. 세 질문 각각에 "그렇다 / 아니다 / 확인하지 못함"과 그 근거가 `research/01-data-overlay.md`에 있다.
4. 끝난 뒤 게임이 바닐라다: exe 해시가 원본과 같고, 스냅샷한 데이터 파일의 해시가 스냅샷과 같고,
   `mods\`와 `aurie.log`가 없다.

질문 2의 답이 세 파일 모두 "아니다"이면 단계 1을 시작하지 않는다. 로드맵으로 돌아가 하위 프로젝트 2를
먼저 할지 정한다.

## 4. 단계 1 — 오버레이 도구

단계 0의 결과로 이 절을 고친 뒤 계획을 쓴다. 아래는 지금 정할 수 있는 뼈대다.

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
