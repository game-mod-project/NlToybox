# NlToyBox

Norland용 네이티브 코드 모드 작업 공간. Aurie + YYToolkit 위에 C++ 모듈을 올린다.

## 필요한 것

- Norland (Steam), Windows x64
- Visual Studio Build Tools 2022 (C++ 도구, 동봉 CMake·Ninja)
- PowerShell 7 (`pwsh`), git
- Python 3.14 (데이터 오버레이와 조사 도구)

## 처음 한 번

    git submodule update --init
    pwsh -File tools/setup-aurie.ps1     # Aurie·YYToolkit 을 받아 게임에 놓고 exe 를 패치한다

`setup-aurie.ps1`은 `Norland.exe` 원본을 `backups/`에 보관한 뒤 패치한다.

## 한 바퀴

    pwsh -File tools/build.ps1
    pwsh -File tools/deploy.ps1
    pwsh -File tools/check-load.ps1

`check-load.ps1`은 게임을 켜고 `mods\Aurie\NlToyBox.log`를 읽어 `PASS`/`FAIL`을 낸 뒤 게임을 끈다.

## 데이터 파일에 프리셋 입히기

모듈 없이도 된다(exe를 패치하지 않는다). 게임을 끈 상태에서:

    pwsh -File tools/overlay.ps1 check -Preset presets\example.json
    pwsh -File tools/overlay.ps1 apply -Preset presets\example.json
    pwsh -File tools/overlay.ps1 restore

프리셋은 카탈로그(`catalog/<게임 버전>/keys.json`)에 있는 키의 수만 바꾼다. 입힌 뒤에는 게임을 다시 켠다.

## 바닐라로 되돌리기

    pwsh -File tools/overlay.ps1 restore     # 데이터 파일에 입힌 프리셋
    pwsh -File tools/restore-game.ps1        # exe 와 mods\

`restore-game.ps1`과 `game-status.ps1`은 exe와 `mods\`만 본다. 데이터 파일의 상태는 `tools/overlay.ps1 status`로 본다.

## 상태 보기

    pwsh -File tools/game-status.ps1

게임 경로가 기본값과 다르면 환경변수 `NORLAND_GAME_DIR`로 지정한다.

## 문서

- `docs/superpowers/specs/` — 설계
- `research/00-game-structure.md` — 게임 구조와 실측 기록
- `research/03-data-files.md` — 데이터 파일의 모양과 키별 근거
- `research/04-overlay-verify.md` — 프리셋의 값이 런타임에 올라오는지 잰 결과
- `CLAUDE.md` — 레포 규칙
