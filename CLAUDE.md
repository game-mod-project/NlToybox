# NlToyBox

Norland (Steam appid 1857090) 네이티브 코드 모드 작업 공간. Aurie + YYToolkit 위에
C++ 모듈 `NlToyBox.dll`을 올린다.

- 설계: `docs/superpowers/specs/2026-10-04-nltoybox-phase0-workspace-design.md`
- 조사 기록: `research/00-game-structure.md` (먼저 읽을 것)

## 브랜치 전략

`main`은 보호 브랜치다. 직접 커밋·푸시하지 않는다.

    main ← develop ← feat/* | fix/* | chore/* | docs/*

- 작업 브랜치는 `develop`에서 분기하고 `develop`으로 통합한다.
- `main`에는 릴리스 시점에 `develop`에서만 들어간다.
- 원격 저장소가 없다. PR 대신 로컬 merge commit(`git merge --no-ff`)을 쓴다.
  원격을 만들면 2단계 PR로 바꾸고 이 절을 고친다.
- CI가 없다. 코드가 바뀌면 머지 전에 `tools/build.ps1` 성공, `tools/test-native.ps1`·
  `tools/tests/safety.tests.ps1` 통과, `py -3.14 -m unittest discover -s tools/re/tests`와 `py -3.14 -m unittest discover -s tools/overlay/tests` 통과,
  `tools/check-load.ps1` 종료 코드 0을 확인한다. 문서만 바뀌면 생략해도 된다.
  `check-load.ps1`을 뺀 나머지는 게임을 켜지 않는다(`safety.tests.ps1`은 임시 폴더의 가짜 게임으로 돈다).
- 머지한 브랜치는 지운다(`git branch -d`).

## 경로

- 게임 경로는 환경변수 `NORLAND_GAME_DIR` 우선, 없으면 `tools/common.ps1`의 기본값.
  **경로를 스크립트에 직접 적지 않는다.** `Get-NlGameDir`를 쓴다.
- 게임 경로에 공백이 있다. PowerShell에서는 `-LiteralPath`를 쓴다.
- 세이브·설정은 `%LOCALAPPDATA%\Strategy`. 도구는 이 폴더에 쓰지 않는다(`tools/saves-backup.ps1`이 읽어서 사본을 뜰 뿐이다).
- 작업 디렉토리가 게임 폴더로 열려 있어도 소스와 git은 `E:\NlToyBox`에 있다.
  git 명령은 `git -C E:\NlToyBox`로 쓴다.

## 저작물 경계

`refs/`(게임 파일에서 뽑은 문자열, 실행 로그)와 `backups/`(`Norland.exe` 원본)는 Long Jaunt의
저작물이거나 그 파생물이다. `downloads/`는 제3자 배포물이다. **커밋하지 않는다.** 커밋 전에 확인한다:

    git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"

결과가 비어 있어야 한다. 도구로 다시 만든다(`tools/re/*.py`, `tools/setup-aurie.ps1`).

## 개발 루프

    pwsh -File tools/build.ps1         # build\NlToyBox.dll
    pwsh -File tools/test-native.ps1   # src/core 의 시험. 게임을 켜지 않는다
    pwsh -File tools/deploy.ps1        # → <게임>\mods\Aurie\
    pwsh -File tools/check-load.ps1    # 게임을 켜서 NlToyBox.log 로 판정, 끝나면 끈다

- 스크립트는 `pwsh`(PowerShell 7)로 실행한다. Windows PowerShell 5.1은 BOM 없는 UTF-8의
  한글을 깨뜨린다.
- 모듈 변경은 게임을 다시 켜야 적용된다. 배포는 게임을 끈 상태에서 한다.
- `check-load.ps1`이 찾는 줄 형식은 `src/ModuleMain.cpp`가 쓴다. 한쪽을 바꾸면 다른 쪽도 바꾼다.
- 게임을 켜는 확인은 한 번에 몰아서 하고 끝나면 바로 끈다. 켜기 전에 사용자에게
  게임 창을 누르지 말라고 알린다(사용자가 새 게임을 시작해야 하는 실행이면 할 일을 차례대로 알린다).
  켜는 횟수는 사용자에게 승인받은 만큼만 쓴다.
- `check-load.ps1`과 `probe.ps1`은 게임 창에 `WM_CLOSE`를 보내 정상 종료시키고(그래야 `aurie.log`가
  채워진다), 15초 안에 끝나지 않을 때만 강제 종료한다(`Stop-NlGame`).

## 모듈을 쓸 때

- 러너를 건드리는 호출은 게임 스레드의 콜백 안에서 한다. `ModuleInitialize`는 Aurie의 스레드에서 돈다.
- 게임 스레드 진입점은 `EVENT_OBJECT_CALL`이다. **`EVENT_FRAME`은 불리지 않는다**
  (YYToolkit v5.0.0c가 Present 훅을 걸지 않는다. `research/00-game-structure.md` 참고).
- 출력은 Aurie의 `DbgPrintEx`로 한다. v5 인터페이스에는 `Print` 계열이 없다.
- **게임 스크립트를 인자가 틀린 채 부르면 게임이 GML 오류로 끝난다**(정수를 받는 스크립트에 문자열을 넘겨 실측).
  인자의 형을 모르는 스크립트는 부르지 않는다. 위험한 호출은 다른 결과를 파일에 쓴 뒤 맨 마지막에 한다.
- `gameplay_variables.json`의 값은 런타임에서 `global.__gameplay_vars.<키 경로를 _ 로 이은 이름>`에 앉는다
  (`research/01-data-overlay.md`).
- 데이터 파일의 값은 게임을 켤 때 ds_map으로 읽히고, 게임을 시작하면 일부가 `o_debug`와 `o_province_controller`의
  변수로 옮겨진다. ds_map은 번호가 아니라 키 이름으로 찾는다. **런타임에 있다는 것과 게임이 그 값을 쓴다는 것은 다르다**
  (`budget_money`는 `default_budget_money`가 되지만 화면의 시작 금화는 달랐다. `research/02-new-game-state.md`).
- 메인 메뉴와 게임은 같은 룸(`rm_game`)이다. 게임 화면은 `o_main_menu`가 없고 `o_character`가 있는 것으로 알아본다
  (새 게임에서 잰 것이다. 세이브를 불러올 때는 재지 않았다). `o_time_controller`는 메뉴에서 이미 있다.
- 러너에 기대지 않는 로직은 `src/core/`에 두고 `tests/native/`에서 시험한다. 러너에 닿는 호출은
  `src/Game.cpp`를 거친다. 빌트인과 문서·소스·실행으로 확인한 인터페이스만 쓴다. `GetInstanceObject`와
  `CRoom`은 러너 내부 구조체의 배치에 기대므로 쓰지 않는다. 없는 이름으로 부르는 `GetInstanceMember`와
  `GetInstanceMemberCount`는 확인하지 못한 빌트인을 구조체에 대고 부르므로 쓰지 않는다(이름은 열거로 찾는다).
- 정적 저장 기간의 `RValue`를 두지 않는다. 함수 안에서만 든다.
- 빌트인을 새로 쓸 때는 먼저 이름이 `refs/exe_strings.txt`에 있는지, 인자와 반환값이 매뉴얼
  (Context7 `/yoyogames/gamemaker-manual`)에 어떻게 적혀 있는지 확인한다. GML 상수는 C++에서 이름으로 쓸 수
  없으니 값의 출처를 주석에 적는다. ds 함수는 `ds_exists`로 있는 것을 본 번호에만 부른다.

## 게임에 가하는 변경

- `tools/setup-aurie.ps1`: `Norland.exe` 패치(원본은 `backups/`에 보관),
  `mods\Native\AurieCore.dll`, `mods\Aurie\YYToolkit.dll`.
- `tools/restore-game.ps1`: 같은 버전의 백업으로 exe를 덮어쓰고, `mods\`에서 이 레포가 놓은
  파일과 게임 폴더 최상위의 `aurie.log`를 지운다. 백업의 내용이 이름의 해시와 다르면 거부한다.
- 프리셋은 `tools/overlay.ps1`로 입히고 되돌린다(아래 "데이터 오버레이"). `data-edit.ps1`은 실측용으로 남겨 둔다.
  둘을 섞어 쓰지 않는다: `data-edit.ps1`로 고친 파일은 `overlay.ps1`이 "모르는 것"으로 보고 거부한다(`data-restore.ps1`로 먼저 되돌린다).
- 데이터 파일은 `tools/data-snapshot.ps1`으로 바닐라 사본을 뜬 뒤에만 고친다. 되돌릴 때는
  `tools/data-restore.ps1`. 스냅샷은 `backups\data\<게임 버전>\`에 있다(추적 안 함).
- `tools/probe.ps1`은 요청 파일(`tools/probes/*.txt`)을 놓고 게임을 켜서 런타임 덤프를 받아 온다.
  요청에 `repeat_seconds`가 있으면 메인 메뉴에서 한 번, 그 뒤로 일정 간격으로 덤프하고, 사용자가 새 게임을
  시작해 잠시 둔 뒤 직접 게임을 끈다(메뉴 덤프가 끝났을 때의 알림음은 `-Beep`을 줄 때만 낸다). 덤프는 `refs\runtime\`에 둔다(추적 안 함). 분석은
  `py -3.14 tools/re/dump_tool.py`, "없다"를 믿어도 되는지는 `dump_tool.py controls`로 본다.
  요청 파일을 고치면 `tools/test-native.ps1`로 읽어 본다. 찾으려는 값은 요청에 미리 넣어야 한다
  (덤프는 요청한 값과 이름만 깊이 찾는다).
- 새 게임을 시작하는 실행 전에는 `tools/saves-backup.ps1`으로 세이브 폴더의 사본을 뜬다. 게임이 만든 파일을
  지우는 것은 사용자가 정한다.
- 게임 갱신이나 Steam 무결성 검사 뒤에는 `tools/game-status.ps1`로 패치가 남았는지 보고
  `tools/setup-aurie.ps1`을 다시 돌린다. 그 뒤 `tools/check-load.ps1`로 다시 확인한다.

## 데이터 오버레이

    pwsh -File tools/overlay.ps1 check -Preset presets\example.json   # 무엇이 바뀌는지 본다. 쓰지 않는다
    pwsh -File tools/overlay.ps1 apply -Preset presets\example.json   # 바닐라로 되돌린 뒤 입힌다
    pwsh -File tools/overlay.ps1 restore                              # 바닐라로 되돌린다
    pwsh -File tools/overlay.ps1 status

- 프리셋(`presets/*.json`)은 카탈로그(`catalog/<게임 버전>/keys.json`)에 있는 키의 **수**만 바꾼다. 형식은 스펙
  (`docs/superpowers/specs/2026-10-04-data-overlay-design.md`) §4.2, 등급의 뜻은 §4.3.
- 입힌 뒤에는 게임을 다시 켜야 반영된다(값은 게임을 켤 때 읽힌다). 게임이 켜져 있으면 `apply`와 `restore`는 거부한다.
- 게임의 데이터 파일을 쓰는 코드는 `tools/overlay/store.py` 하나다. 바닐라인지는 `catalog/<게임 버전>/files.json`의 SHA256으로
  판정한다. 바닐라도 아니고 이 도구가 마지막에 쓴 것도 아닌 파일이 하나라도 있으면 아무것도 쓰지 않는다.
- 상태는 `backups\overlay\<게임 버전>\state.json`, 스냅샷은 `backups\data\<게임 버전>\`에 있다(추적 안 함).
- **`restore-game.ps1`과 `game-status.ps1`은 exe와 `mods\`만 본다.** 데이터 파일이 바닐라인지는 `overlay.ps1 status`로 본다.
  바닐라로 되돌릴 때는 `overlay.ps1 restore`와 `restore-game.ps1`을 둘 다 돌린다.
- `apply`가 쓰는 도중에 실패하면(읽기 전용 파일 등) `status`가 "부분 적용"이라고 알린다. 원인을 없애고 다시 `apply`하거나 `restore`한다.
- 카탈로그의 `runtime` 등급은 이 레포가 이 빌드에서 잰 것만 올린다: 흔치 않은 값을 쓰는 프리셋을 입히고
  `overlay.ps1 probe-request` → `tools/probe.ps1` → `overlay.ps1 verify`. 값은 게임 JSON 어디에도 없는 수로 고르고, 이미 `VERIFIED`인
  키 하나를 양성 대조로 함께 바꾼다(`presets/verify-stage1.json`이 본이다). `verify`의 `anchored`만 올린다. **`SEEN`은 "반영된다"가 아니다**(같은 이름·값이 런타임에
  있었을 뿐이고 코드의 기본값과 가려지지 않는다). 커뮤니티의 보고는 `effect_by: community`와 출처로 따로 적는다.
- 카탈로그와 프리셋에는 게임 파일의 값을 옮겨 적지 않는다(키 이름, 등급, 해시만). 값이 든 표는
  `overlay.ps1 keys -Out refs\registry\<버전>.tsv`로 뽑는다(추적 안 함).
- 수정기(`tools/overlay/jsonedit.py`)는 값의 글자만 바꾼다. 게임 파일에 한 번도 없던 문법(주석, 작은따옴표, `NaN`, 같은 키의 중복)은
  추측해서 읽지 않고 오류를 낸다. 문법의 근거는 `research/03-data-files.md`.
- 게임이 갱신되면 그 버전의 카탈로그가 없어 `overlay.ps1`이 거부한다. **Steam 무결성 검사 직후에** `catalog/<새 버전>/keys.json`을 놓고
  `overlay.ps1 pin`으로 `files.json`을 만든 뒤(프리셋을 입혀 둔 채 갱신됐으면 고친 파일이 남아 있을 수 있다. `pin`은 상태 파일이 남아 있으면 거부한다) `overlay.ps1 scan`과 `check`로 키가 그대로인지 본다. 등급은 새 빌드에서 다시 잰다.

## 의존

- Aurie v2.0.2, YYToolkit v5.0.0c 릴리스 바이너리. 출처·크기·SHA256은 `tools/pins.json`.
- 헤더는 서브모듈 `external/YYToolkit` (`experimental` 브랜치 `d5cc0078`).
  **git 태그 `v5.0.0c`를 체크아웃하지 않는다.** 그 태그는 v4 헤더를 가리킨다.
  새로 클론했으면 `git submodule update --init` 먼저.
- 서브모듈 안의 파일은 고치지 않는다.
- Aurie와 YYToolkit은 AGPL-3.0이다. 레포 공개나 모듈 배포 전에 다시 검토한다.

## 도구

- Python은 `py -3.14`. `WindowsApps\python.exe`는 스토어 스텁이라 멈춘다.
- 오버레이 도구는 표준 라이브러리만 쓴다. 시험은 `py -3.14 -m unittest discover -s tools/overlay/tests`(게임을 켜지 않는다. 임시 폴더로 돈다).
- cmake·ninja는 PATH에 없다. `tools/build.ps1`이 Build Tools 동봉본을 찾아 쓴다.
  빌드 출력의 `'vswhere.exe' is not recognized` 한 줄은 `vcvars64.bat` 안에서 나오는 것으로 무해하다.

## 작업 규칙

- 추측으로 작업하지 않는다. 계획과 코드에 쓰는 사실은 실측(로그·해시·덤프·게임 파일), 공식 문서, 서브모듈 소스로
  확인하고 출처를 적는다. 확인하지 못한 것은 "모른다"로 두고 먼저 그것을 재는 단계를 만든다.
  추정을 결론처럼 쓰지 않는다.
- 게임 버전이 바뀌면 `research/00-game-structure.md`의 수치를 다시 잰다.
