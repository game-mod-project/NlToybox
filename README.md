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

## 게임 안의 치트 메뉴

모듈을 놓고(`setup-aurie.ps1`, `build.ps1`, `deploy.ps1`) 게임을 켠 뒤 **F8**을 누르면 모드창이 뜬다. 왼쪽에서 영역을 고른다.

- **탐색기**: 게임의 아무 값이나 보고 고친다. 값을 잠그거나(0.1초마다 다시 써 넣는다) 이름·값으로 찾을 수 있다.
- **경제, 건설·생산, 인구·욕구, 군대·전투, 외교, 종교, 월드, 이벤트, 유틸**: 게임에 들어 있는 개발자 스위치와 수.
  이름 옆의 `(?)`는 효과를 아직 확인하지 않았다는 뜻이다.
- **시간**: 게임 속도. 처음 누를 때 어느 값이 속도를 정하는지 시험으로 찾는다(게임 화면에서, 일시정지를 푼 채로).
- **배율**: 건설 비용 같은 값의 배율(0.3.0 의 것).

켠 것은 `mods\Aurie\NlToyBox.cheats.txt`에 저장된다. 위쪽의 "모두 끄기"가 켠 것을 원래 값으로 되돌린다.

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

## 켜 둔 게임에 묻기 (개발용)

    pwsh -File tools/session.ps1 -Action start
    & tools\ask.ps1 -Lines 'state', 'call gml_Script_budget_money_get', 'shot hud'
    pwsh -File tools/session.ps1 -Action stop -Name my-session

게임을 켜 둔 채 값을 읽고, 쓰고, 이름·값으로 찾고, 화면을 뜨고, 스크립트의 호출을 기록하고, 게임의 함수를 부른다.
명령은 `src/core/RemoteCommand.hpp`에 있다. 훅을 걸기 전에 그 함수가 잡히는 함수인지 센다:

    py -3.14 tools/re/script_calls.py <Norland.exe> budget_money_change time_hour

## 상태 보기

    pwsh -File tools/game-status.ps1

게임 경로가 기본값과 다르면 환경변수 `NORLAND_GAME_DIR`로 지정한다.

## 문서

- `docs/superpowers/specs/` — 설계
- `research/00-game-structure.md` — 게임 구조와 실측 기록
- `research/03-data-files.md` — 데이터 파일의 모양과 키별 근거
- `research/04-overlay-verify.md` — 프리셋의 값이 런타임에 올라오는지 잰 결과
- `research/05-mod-window.md` — 게임 안의 모드창
- `research/06-cheat-menu.md` — 치트 메뉴에서 잰 것
- `research/07-remote.md` — 원격 질의와 호출 기록기, 게임 화면에서 찾은 자리와 함수(금화, 자원)
- `CLAUDE.md` — 레포 규칙
