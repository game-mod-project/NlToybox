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
- CI가 없다. 코드가 바뀌면 머지 전에 `tools/build.ps1` 성공과
  `tools/check-load.ps1` 종료 코드 0을 확인한다. 문서만 바뀌면 생략해도 된다.
- 머지한 브랜치는 지운다(`git branch -d`).

## 경로

- 게임 경로는 환경변수 `NORLAND_GAME_DIR` 우선, 없으면 `tools/common.ps1`의 기본값.
  **경로를 스크립트에 직접 적지 않는다.** `Get-NlGameDir`를 쓴다.
- 게임 경로에 공백이 있다. PowerShell에서는 `-LiteralPath`를 쓴다.
- 세이브·설정은 `%LOCALAPPDATA%\Strategy`. 도구는 이 폴더를 건드리지 않는다.
- 작업 디렉토리가 게임 폴더로 열려 있어도 소스와 git은 `E:\NlToyBox`에 있다.
  git 명령은 `git -C E:\NlToyBox`로 쓴다.

## 저작물 경계

`refs/`(게임 파일에서 뽑은 문자열, 실행 로그)와 `backups/`(`Norland.exe` 원본)는 Long Jaunt의
저작물이거나 그 파생물이다. `downloads/`는 제3자 배포물이다. **커밋하지 않는다.** 커밋 전에 확인한다:

    git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"

결과가 비어 있어야 한다. 도구로 다시 만든다(`tools/re/*.py`, `tools/setup-aurie.ps1`).

## 개발 루프

    pwsh -File tools/build.ps1         # build\NlToyBox.dll
    pwsh -File tools/deploy.ps1        # → <게임>\mods\Aurie\
    pwsh -File tools/check-load.ps1    # 게임을 켜서 NlToyBox.log 로 판정, 끝나면 끈다

- 스크립트는 `pwsh`(PowerShell 7)로 실행한다. Windows PowerShell 5.1은 BOM 없는 UTF-8의
  한글을 깨뜨린다.
- 모듈 변경은 게임을 다시 켜야 적용된다. 배포는 게임을 끈 상태에서 한다.
- `check-load.ps1`이 찾는 줄 형식은 `src/ModuleMain.cpp`가 쓴다. 한쪽을 바꾸면 다른 쪽도 바꾼다.
- 게임을 켜는 확인은 한 번에 몰아서 하고 끝나면 바로 끈다. 켜기 전에 사용자에게
  게임 창을 누르지 말라고 알린다. 켜는 횟수는 사용자에게 승인받은 만큼만 쓴다.
- 판정이 `FAIL`이면 게임을 강제 종료하기 전에 게임 창(클래스 `YYGameMakerYY`)에 `WM_CLOSE`를 보내
  정상 종료시킨다. 그래야 `aurie.log`가 채워진다.

## 모듈을 쓸 때

- 러너를 건드리는 호출은 게임 스레드의 콜백 안에서 한다. `ModuleInitialize`는 Aurie의 스레드에서 돈다.
- 게임 스레드 진입점은 `EVENT_OBJECT_CALL`이다. **`EVENT_FRAME`은 불리지 않는다**
  (YYToolkit v5.0.0c가 Present 훅을 걸지 않는다. `research/00-game-structure.md` 참고).
- 출력은 Aurie의 `DbgPrintEx`로 한다. v5 인터페이스에는 `Print` 계열이 없다.

## 게임에 가하는 변경

- `tools/setup-aurie.ps1`: `Norland.exe` 패치(원본은 `backups/`에 보관),
  `mods\Native\AurieCore.dll`, `mods\Aurie\YYToolkit.dll`.
- `tools/restore-game.ps1`: 같은 버전의 백업으로 exe를 덮어쓰고, `mods\`에서 이 레포가 놓은
  파일과 `aurie.log`를 지운다.
- 게임 갱신이나 Steam 무결성 검사 뒤에는 `tools/game-status.ps1`로 패치가 남았는지 보고
  `tools/setup-aurie.ps1`을 다시 돌린다. 그 뒤 `tools/check-load.ps1`로 다시 확인한다.

## 의존

- Aurie v2.0.2, YYToolkit v5.0.0c 릴리스 바이너리. 출처·크기·SHA256은 `tools/pins.json`.
- 헤더는 서브모듈 `external/YYToolkit` (`experimental` 브랜치 `d5cc0078`).
  **git 태그 `v5.0.0c`를 체크아웃하지 않는다.** 그 태그는 v4 헤더를 가리킨다.
  새로 클론했으면 `git submodule update --init` 먼저.
- 서브모듈 안의 파일은 고치지 않는다.
- Aurie와 YYToolkit은 AGPL-3.0이다. 레포 공개나 모듈 배포 전에 다시 검토한다.

## 도구

- Python은 `py -3.14`. `WindowsApps\python.exe`는 스토어 스텁이라 멈춘다.
- cmake·ninja는 PATH에 없다. `tools/build.ps1`이 Build Tools 동봉본을 찾아 쓴다.
  빌드 출력의 `'vswhere.exe' is not recognized` 한 줄은 `vcvars64.bat` 안에서 나오는 것으로 무해하다.

## 작업 규칙

- 원인은 실측(로그·해시·덤프)으로 확인한 뒤 보고한다. 추정을 결론처럼 쓰지 않는다.
- 게임 버전이 바뀌면 `research/00-game-structure.md`의 수치를 다시 잰다.
