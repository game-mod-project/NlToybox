# NlToyBox Phase 0 — 모드 작업 구성 설계

- 작성일: 2026-10-04
- 대상: Norland (Steam appid 1857090), 게임 버전 `0.5588.9777.0`, buildid `25211575`
- 상태: 문서 승인(2026-10-04). 같은 날 두 번 정정했다
  - 계획 작성 중: §3.1·§4.3·§4.4·§4.5·§9 (YYToolkit 태그 `v5.0.0c`가 v5 소스를 가리키지 않는다)
  - 실행 중, 사용자 승인: §4.2·§4.3 (`EVENT_FRAME`이 불리지 않아 프로브 시점을 게임 스레드
    콜백으로 바꿨다. Aurie가 게임 폴더에 `aurie.log`를 쓴다)

## 1. 목적

`E:\NlToyBox`에서 Norland용 네이티브 코드 모드를 개발할 수 있는 작업 구성을 갖춘다.
Phase 0이 끝나면 다음 한 바퀴가 스크립트만으로 돈다.

    빌드 → 게임에 배치 → 게임을 켜서 모듈이 붙었는지 확인 → 바닐라로 복원

모드의 기능(치트·편집 도구 등)은 이 문서의 범위가 아니다. Phase 0의 모듈은 "붙었다"는
증거만 남긴다.

## 2. 실측한 사실 (2026-10-04)

### 2.1 설치·빌드

| 항목 | 값 | 근거 |
|---|---|---|
| 설치 경로 | `E:\SteamLibrary\steamapps\common\Norland Story Generating Strategy` | `appmanifest_1857090.acf` |
| buildid | `25211575` | 같은 파일 |
| 게임 버전 | `0.5588.9777.0` | `Norland.exe` 버전 정보, `game_settings.json` |
| 빌드 날짜 | 2026-09-09 | `data.win` GEN8 타임스탬프, `catched_errors_*.txt` |
| 빌드 구성 | `SteamRelease` | GEN8 config |
| `Norland.exe` | 129,225,728 B, SHA256 `609C17CFD812E90C7D3E84D3EB730EC559EA3976F30A239395922F604516863E` | `Get-FileHash` |
| 바닐라 여부 | depot 파일은 전부 16:20 시각. 그 뒤 바뀐 것은 `imgui.ini`, `Norland_DLL_log.txt`뿐 | 파일 시각 대조 |

### 2.2 엔진

- GameMaker, **YYC 네이티브 컴파일**. `data.win`에 `CODE`·`VARI`·`FUNC` 청크가 없다.
  UndertaleModTool로 GML을 디컴파일하거나 패치할 수 없다.
- `data.win`의 `SCPT` 항목은 40,813개, `OBJT`는 64개이고 이름이 남아 있다
  (`gml_Script_*`). 이름으로 스크립트를 찾을 수 있다.
- exe에 `2023.4.0.113` 문자열이 있다. GameMaker 런타임 버전으로 보이나 **확인하지 않았다.**
- x64 실행 파일이다(`options.ini`의 `Usex64=True`, 동봉 DLL이 `*_x64.dll`).

### 2.3 모드 지원

- 공식 모드 지원이 없다. `mods` 폴더, 워크숍 구독(`appworkshop_1857090.acf`),
  exe 안의 모드 로더 문자열이 모두 없다.
- `data.win` 문자열 표의 `steam_ugc_*`는 Steamworks 확장이 내보내는 함수 이름이다.
  게임이 그것을 쓴다는 근거는 아니다.

### 2.4 내장 디버그 도구 (Phase 1 후보)

릴리스 빌드에 ImGui 확장(`Imguigml_x64.dll`)과 디버그 스크립트가 들어 있다.
`imgui_debug_actor`, `imgui_debug_building`, `imgui_debug_province`,
`imgui_debug_managers`, `imgui_debug_system_window`, `imgui_debug_main_menu_window`,
`imgui_debug_prices_setup`, `debug_spawn_army`, 문자열 `Variables Editor` 등이다.
`is_debug_forced`, `current_debug_mode`, `command_line_parameters_init`,
`CommandLineParameter`가 게이트로 보인다. **켜는 방법은 확인하지 않았다.**

### 2.5 세이브·설정

- 위치: `%LOCALAPPDATA%\Strategy` (GEN8의 프로젝트 이름이 `Strategy`).
- `saves\`에는 `steam_autocloud.vdf`만 있다. 로컬 세이브 파일이 없다.

### 2.6 로컬 도구

| 도구 | 상태 |
|---|---|
| git 2.52, gh | 있음 (gh 로그인됨) |
| MSVC 14.44.35207 (v143), Windows SDK 10.0.26100.0 | VS Build Tools 2022 17.14 |
| cmake, ninja | PATH에는 없음. Build Tools 동봉본이 `...\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\` 아래에 있음 |
| Python | `py -3.14` (`%LOCALAPPDATA%\Python\pythoncore-3.14-64\python.exe`). `WindowsApps\python.exe`는 스토어 스텁 |
| .NET SDK 8.0.425, Node 24.12 | 있음 (Phase 0에서는 쓰지 않음) |

## 3. 접근

### 3.1 결정

**Aurie v2.0.2 + YYToolkit v5.0.0c**를 공식 릴리스 바이너리로 고정해 쓴다.
모듈(`NlToyBox.dll`)만 이 레포에서 빌드한다.

근거:
- YYToolkit(Next)의 지원 표는 GM 2023.x, x64 러너를 "지원"으로 둔다 (README).
- Aurie v2는 게임 exe를 패치해 붙는다
  (`AuriePatcher.exe <exe> <AurieCore.dll> install|remove`, `AuriePatcher/source/main.cpp`).
  레지스트리와 관리자 권한을 쓰지 않는다.
- v5.0.0c의 릴리스 노트는 Aurie 헤더를 v2.0.2로 올렸다고 적는다. 둘이 짝이다.

알려진 약점 두 가지:
- v5.0.0c는 릴리스 노트가 "Beta & unstable … FOR TESTING ONLY"라고 적은 사전 릴리스다.
- **git 태그 `v5.0.0c`는 v5 소스를 가리키지 않는다.** 태그는 `stable` 브랜치의
  `86c133dc`(2025-03-20, 공유 헤더가 `YYTK_MAJOR 4`)에 붙어 있다. v5 소스는 `experimental`
  브랜치에 있다. 릴리스 바이너리가 어느 커밋에서 빌드됐는지 태그로는 알 수 없으므로
  헤더는 `experimental`의 HEAD에 고정하고(§4.5), 모듈이 YYToolkit 버전을 로그에 찍어
  어긋남을 드러낸다(§4.3).

### 3.2 대안과 실패 시 순서

| 순서 | 구성 | 언제 |
|---|---|---|
| A | Aurie v2.0.2 + YYToolkit v5.0.0c | 먼저 실측한다 |
| B | Aurie v1.2.1 + YYToolkit v4.0.1 | A가 붙지 않을 때. 설치 방식부터 조사한다. 레지스트리 변경이 필요하면 그 단계는 사용자가 실행한다 |
| — | 중단 | B도 붙지 않으면 멈추고 보고한다. 이 문서를 고친 뒤 다시 승인받는다 |

"붙는다"의 판정은 §7의 완료 기준이다.

전부 소스로 빌드하는 안(Aurie·YYToolkit을 MSBuild로 직접 빌드)은 채택하지 않았다.
Phase 0 범위가 커지고, 릴리스 다이제스트 고정으로 무엇을 실행하는지는 이미 특정된다.

## 4. 구성

### 4.1 레포 구조

```
E:\NlToyBox
├─ CLAUDE.md                     레포 규칙 (§4.6)
├─ README.md                     한 바퀴 돌리는 법
├─ .gitignore · .gitmodules
├─ CMakeLists.txt
├─ src/
│   └─ ModuleMain.cpp            Phase 0 모듈 (§4.3)
├─ external/
│   └─ YYToolkit/                서브모듈, experimental 브랜치 커밋 d5cc0078 고정. 헤더만 쓴다
├─ tools/
│   ├─ common.ps1                공용 함수 (§4.4)
│   ├─ pins.json                 받을 파일의 태그·URL·크기·SHA256
│   ├─ game-status.ps1
│   ├─ setup-aurie.ps1
│   ├─ restore-game.ps1
│   ├─ build.ps1
│   ├─ deploy.ps1
│   ├─ check-load.ps1
│   └─ re/
│       ├─ datawin_probe.py      data.win 청크 표·GEN8·문자열 표 추출
│       └─ exe_strings.py        exe ASCII 문자열 추출
├─ research/
│   └─ 00-game-structure.md      §2의 조사 기록과 Phase 0 실측 결과
└─ docs/superpowers/
    ├─ specs/                    이 문서
    └─ plans/
```

추적하지 않는 것 (`.gitignore`):

| 폴더 | 내용 | 이유 |
|---|---|---|
| `refs/` | `tools/re/*.py`의 출력 | Long Jaunt 저작물의 파생물 |
| `downloads/` | 받은 릴리스 바이너리 | 제3자 배포물. `pins.json`으로 재현된다 |
| `backups/` | `Norland.exe` 원본 | 게임 저작물 |
| `build/` · `dist/` | 빌드 산출물 | |
| `.omc/` · `.superpowers/` · `.semgrep/` | 도구 상태 | |

### 4.2 게임 쪽 배치

```
<게임 폴더>
├─ Norland.exe                   AuriePatcher가 패치 (.aurie 섹션 추가)
├─ aurie.log                     Aurie가 쓴다 (실행 중 0바이트, 게임을 끌 때 채워진다)
└─ mods/
    ├─ Native/AurieCore.dll
    └─ Aurie/
        ├─ YYToolkit.dll
        ├─ NlToyBox.dll
        └─ NlToyBox.log          모듈이 쓴다
```

게임 폴더에서 바뀌는 것은 이것이 전부다. `%LOCALAPPDATA%\Strategy`는 건드리지 않는다.

### 4.3 모듈 (`src/ModuleMain.cpp`)

하는 일은 세 가지다.

1. `ModuleInitialize`에서 `YYTK::GetInterface()`를 얻는다. 없으면
   `AURIE_MODULE_DEPENDENCY_NOT_RESOLVED`를 돌려준다. 얻으면 `QueryVersion`으로
   YYToolkit 버전을 읽는다.
2. 게임 스레드에서 도는 콜백 두 개(`EVENT_OBJECT_CALL`, `EVENT_WNDPROC`)를 등록하고,
   **먼저 오는 쪽에서 한 번만** 두 가지를 시험한다. 어느 쪽이 왔는지 로그에 남긴다.
   (`EVENT_FRAME`은 쓰지 않는다. 실측: YYToolkit v5.0.0c는 Present 훅을 걸지 않아
   이 콜백이 불리지 않는다. 소스의 `Hooks::InitializeStage3Hooks`에 호출부가 없다.)
   - 빌트인 호출: 전역 인스턴스로 `CallBuiltinEx(…, "code_is_compiled", …)`.
     YYC이므로 참이어야 한다.
   - 이름으로 스크립트 찾기:
     `GetNamedRoutinePointer("gml_Script_command_line_parameters_init", …)`.
     `AURIE_SUCCESS`여야 한다.
3. 결과를 `NlToyBox.log`(모듈 DLL 옆, `ModulePath` 기준)에 쓰고 Aurie 로그에도 찍는다
   (`DbgPrintEx`. v5 인터페이스에는 `Print` 계열이 없다).

로그 형식 (한 줄에 하나, `check-load.ps1`이 읽는다):

```
NlToyBox 0.0.1 loaded
yytk <major>.<minor>.<patch>
trigger <object_call|wndproc>
builtin code_is_compiled = <true|false|error:<상태>>
script gml_Script_command_line_parameters_init = <found|error:<상태>>
probe done
```

`yytk`와 `trigger` 줄은 판정에 쓰지 않는다. `yytk`는 헤더(`YYTK_MAJOR 5`)와 바이너리가
어긋났는지 보는 단서이고, `trigger`는 이후 기능이 올라갈 게임 스레드 진입점이 어느 것인지
알려 준다.

`ModuleInitialize`에서 바로 시험하지 않는 이유: 이 모듈은 Aurie의 "나머지 모드 적재" 단계에서
Aurie의 스레드로 초기화된다(실측: 게임 창이 뜬 뒤). 그때 러너는 이미 메인 루프를 돌고 있으므로,
러너를 건드리는 호출은 게임 스레드의 콜백 안에서 한다.

로그는 실행마다 새로 쓴다(덮어쓰기). 두 콜백은 시험 뒤 아무 일도 하지 않는다.

빌드:
- CMake + Ninja, 구성 `RelWithDebInfo`, `/std:c++latest`, `/MD`, `UNICODE`.
- 소스: `src/ModuleMain.cpp`,
  `external/YYToolkit/YYToolkit/source/YYTK/Shared/YYTK_Shared_Types.cpp`.
- 포함 경로: `external/YYToolkit/YYToolkit/source/YYTK/Shared`(`YYTK_Shared.hpp`)와
  `external/YYToolkit/YYToolkit/include`(`Aurie/`, `FunctionWrapper/`).
  `ExamplePlugin/include`의 사본은 쓰지 않는다. `experimental`에서도 정본과 내용이 다르다
  (blob 해시가 다르다).
- 산출물: `build/NlToyBox.dll`.

### 4.4 도구 계약

모든 스크립트는 `tools/common.ps1`을 불러 쓴다. 게임 경로에 공백이 있으므로
`-LiteralPath`를 쓴다. 경로를 스크립트에 직접 적지 않는다.

**`common.ps1`**

| 함수 | 돌려주는 것 |
|---|---|
| `Get-NlRepoRoot` | 레포 절대경로 |
| `Get-NlGameDir` | `$env:NORLAND_GAME_DIR`, 없으면 §2.1의 설치 경로. 폴더에 `Norland.exe`가 없으면 오류 |
| `Test-NlGameRunning` | `Norland` 프로세스가 있으면 참 |
| `Get-NlExeInfo` | `Norland.exe`의 버전, 크기, SHA256, PE 섹션 이름 목록, `Patched`(`.aurie` 섹션 유무) |
| `Get-NlPins` | `pins.json`을 읽은 객체 |
| `Get-NlPeSectionNames <경로>` | PE 파일의 섹션 이름 목록. PE가 아니면 오류 |
| `Get-NlBackups <버전>` | `backups/`에서 그 버전의 백업 파일 목록 |
| `Assert-NlGameNotRunning` | 게임이 실행 중이면 PID를 담아 오류 |

**`game-status.ps1`** — 읽기만 한다. buildid, exe 버전·해시·패치 여부, 실행 여부,
`mods\` 내용, 백업 유무와 해시 일치 여부를 한 화면에 찍는다.

**`setup-aurie.ps1`** — 게임에 Aurie와 YYToolkit을 놓고 exe를 패치한다.
1. 게임이 실행 중이면 거부한다.
2. `pins.json`의 세 파일이 `downloads/`에 없으면 받는다. 받은 뒤 크기와 SHA256을
   `pins.json`과 대조하고, 다르면 지우고 중단한다.
3. exe가 패치되지 않은 상태면 `backups/Norland.exe.<버전>.<SHA256 앞 12자>`로 복사한다.
   같은 이름이 이미 있고 **내용의 SHA256이 지금의 exe와 같으면** 다시 복사하지 않는다. 내용이
   다르면(복사가 중간에 끊긴 경우) 다시 만든다. 복사는 임시 이름으로 한 뒤 해시를 확인하고 옮긴다.
   exe가 이미 패치된 상태인데 같은 버전의
   백업이 없으면 거부한다(되돌릴 원본이 없는 상태로 진행하지 않는다).
4. `mods\Native\AurieCore.dll`, `mods\Aurie\YYToolkit.dll`을 놓는다.
5. `AuriePatcher.exe "<exe>" "<mods\Native\AurieCore.dll>" install`을 실행한다.
6. `Get-NlExeInfo`로 `Patched`가 참인지 확인한다. 아니면 오류로 끝낸다.

이미 패치된 상태에서 다시 실행해도 같은 결과가 된다.

**`restore-game.ps1`** — 바닐라로 되돌린다.
1. 게임이 실행 중이면 거부한다.
2. exe가 패치된 상태면, 현재 exe 버전과 같은 버전의 백업을 찾아 덮어쓴다.
   같은 버전의 백업이 없으면 거부한다(Steam 갱신으로 exe가 바뀐 경우다. 이때는 Steam이
   이미 패치를 지웠거나, 무결성 검사로 되돌려야 한다). 같은 버전의 백업이 둘 이상이면
   거부하고 목록을 보여 준다. 백업 내용의 SHA256이 파일 이름의 12자로 시작하지 않으면
   그 백업으로 덮어쓰지 않고 거부한다.
3. `mods\` 안에서 이 레포가 놓은 파일(`AurieCore.dll`, `YYToolkit.dll`, `NlToyBox.dll`,
   `NlToyBox.log`)과 게임 폴더 최상위의 `aurie.log`만 지운다. 폴더가 비면 폴더도 지운다. 다른 파일이 있으면 남기고 알린다.
4. exe의 SHA256이 덮어쓴 백업 파일의 SHA256과 같은지 확인한다.

`AuriePatcher … remove`에 의존하지 않는다. 그것이 바이트 단위로 원본을 되살리는지
확인하지 않았기 때문이다. Phase 0에서 한 번 실측해 `research/`에 적는다.

**`build.ps1`** — Build Tools의 `vcvars64.bat`로 환경을 잡고 동봉 cmake·ninja로
`build/`에 빌드한다. `external/YYToolkit`이 비어 있으면
`git submodule update --init`을 하라고 알리고 끝낸다.

**`deploy.ps1`** — `build/NlToyBox.dll`을 `<게임>\mods\Aurie\`에 복사한다.
다음이면 거부한다: 게임 실행 중, exe가 패치되지 않음, 산출물이 `src/`보다 오래됨.

**`check-load.ps1`** — 게임을 켜서 모듈이 붙었는지 본다.
1. 게임이 이미 실행 중이면 거부한다. exe가 패치되지 않았거나 `AurieCore.dll`·`YYToolkit.dll`·
   `NlToyBox.dll` 가운데 하나라도 없으면 게임을 켜지 않고 거부한다.
2. 이전 `NlToyBox.log`를 지운다.
3. `steam://rungameid/1857090`으로 게임을 켠다.
4. 로그에 `probe done`이 나타날 때까지 기다린다(기본 180초).
5. 로그를 찍고, 게임 프로세스를 끝낸다(`-KeepRunning`이면 두고 나간다).
6. 종료 코드: 로그의 세 줄이 기대값(`loaded`, `= true`, `= found`)이면 0, 아니면 1.
   시간 초과도 1이다.

### 4.5 외부 의존 (`tools/pins.json`)

| 파일 | 출처 | 크기 | SHA256 |
|---|---|---|---|
| `AurieCore.dll` | `github.com/AurieFramework/Aurie` 릴리스 v2.0.2 | 967,680 | `18e3a1de980f487a6b3858b673d2030e96984dd96de3a047b43a263a5ba829ae` |
| `AuriePatcher.exe` | 같은 릴리스 | 260,096 | `4d3aec439dbba5209ad48fb0a40d6324406247fe678541df9031f5b89363d536` |
| `YYToolkit.dll` | `github.com/AurieFramework/YYToolkit` 릴리스 v5.0.0c | 860,672 | `ae7809f136f9222e5f49393d7ce7c3ad4c375c171308e2414660946d4ba0378a` |

SHA256은 GitHub API가 자산마다 내주는 `digest` 값이다(2026-10-04 조회).

서브모듈: `github.com/AurieFramework/YYToolkit` @
`d5cc0078bfbfd5907884ad3fe0b3c6cbaa1c3f54` (`experimental` 브랜치 HEAD, 2026-02-02).

이 커밋을 v5.0.0c의 소스로 보는 근거:
- 이 커밋의 `YYToolkit/include/Aurie/shared.hpp`는 Aurie v2.0.2의
  `Aurie/source/framework/shared.hpp`와 blob 해시가 같다(`e4f0225c7b`).
  v5.0.0c 릴리스 노트의 "Updated Aurie headers to v2.0.2"와 맞는다.
- 릴리스 노트의 나머지 항목(RUNNER_INIT 콜백, RValue 배열 오프셋 알고리즘)이
  `experimental`의 마지막 커밋들이다.
- 공유 헤더가 `YYTK_MAJOR 5`다.

확정은 아니다. 태그가 없으므로 추정이고, Phase 0의 실측(§7)이 판정한다.

### 4.6 `CLAUDE.md`에 적을 것

- 한 줄 소개와 이 문서의 경로.
- 브랜치 전략 (§5).
- 경로: `NORLAND_GAME_DIR`, 공백 경로와 `-LiteralPath`, 세이브 위치.
- 저작물 경계: `refs/`·`backups/`·`downloads/`는 커밋하지 않는다. 커밋 전
  `git ls-files | Select-String "^(refs|backups|downloads)/"`가 비어 있어야 한다.
- 개발 루프: `build.ps1` → `deploy.ps1` → `check-load.ps1`. 모듈 DLL은 게임 실행 중
  잠길 수 있으므로 배포는 게임을 끈 상태에서 한다.
- 게임 갱신 뒤: `game-status.ps1`로 패치가 남았는지 보고 `setup-aurie.ps1`을 다시 돌린다.
- 도구: `py -3.14`, cmake·ninja는 Build Tools 동봉본.
- 작업 디렉토리가 게임 폴더로 열려 있어도 소스와 git은 `E:\NlToyBox`에 있다.
  git 명령은 `-C E:\NlToyBox`로 쓴다.
- 원인은 실측으로 확인한 뒤 보고한다. 추정을 결론처럼 쓰지 않는다.

## 5. Git

```
main ← develop ← feat/* | fix/* | chore/* | docs/*
```

- `main`은 보호 브랜치다. 직접 커밋하지 않는다. 예외는 저장소를 시작한 빈 루트 커밋
  하나다(`b3ea63e`).
- Phase 0 작업은 `chore/workspace-setup`에서 하고 `develop`에 `git merge --no-ff`로 넣는다.
- 원격 저장소는 이번 범위에서 만들지 않는다. 그래서 PR 대신 로컬 merge commit을 쓴다.
  원격을 만들면 형제 레포처럼 2단계 PR로 바꾸고 `CLAUDE.md`를 고친다.
- CI가 없다. 머지 전 확인은 `build.ps1` 성공과 `check-load.ps1` 종료 코드 0이 대신한다.

## 6. 게임에 가하는 변경과 승인

다음 세 가지는 구현 계획을 승인받은 뒤에만 한다.

1. §4.5의 세 파일을 받는다.
2. `Norland.exe`를 패치한다(원본은 먼저 백업한다).
3. 게임을 켜고 끈다. 켜기 전에 게임 창을 누르지 말라고 알린다. 확인은 한 번에 몰아서 하고
   끝나면 바로 끈다.

## 7. 완료 기준

순서대로 모두 성립해야 한다.

1. `tools/build.ps1`이 성공하고 `build/NlToyBox.dll`이 생긴다.
2. `tools/setup-aurie.ps1` 뒤 `Get-NlExeInfo`의 `Patched`가 참이다.
3. `tools/check-load.ps1`이 종료 코드 0으로 끝난다. 즉 `NlToyBox.log`에
   `loaded`, `builtin code_is_compiled = true`,
   `script gml_Script_command_line_parameters_init = found`, `probe done`이 있다.
4. `tools/restore-game.ps1` 뒤 `Norland.exe`의 SHA256이 §2.1의 값과 같고
   `mods\`가 없다.
5. `git ls-files`에 `refs/`·`backups/`·`downloads/`·`build/` 아래 파일이 없다.
6. `research/00-game-structure.md`에 §2의 사실과 Phase 0에서 새로 잰 것
   (붙은 구성, Aurie 로그 위치, `AuriePatcher remove`가 원본을 되살리는지)이 적혀 있다.

4번까지 끝난 뒤 `setup-aurie.ps1`과 `deploy.ps1`을 다시 돌려 개발 가능한 상태로 둘지,
바닐라로 둘지는 Phase 0 끝에서 사용자에게 묻는다.

## 8. 시험

- PowerShell 도구: 실제 게임 폴더에 대고 실행해 확인한다. `Get-NlExeInfo`는 패치 전
  (`Patched` 거짓)과 후(참) 두 상태를 모두 본다. 각 스크립트의 거부 조건은 조건을
  만들어 한 번씩 확인한다(게임 실행 중 거부는 `check-load.ps1 -KeepRunning` 상태에서 본다).
- `tools/tests/safety.tests.ps1`: 임시 폴더의 가짜 게임(`NORLAND_GAME_DIR`)으로 설치·복원·적재
  확인의 안전 동작을 시험한다. 손상된 백업을 믿지 않는지, 이름과 내용이 다른 백업으로 덮어쓰지
  않는지, 미패치·DLL 누락 상태에서 게임을 켜지 않는지 본다. 게임을 켜지 않는다(최종 검토 뒤 추가).
- 모듈: 단위 시험을 두지 않는다. 판정할 로직이 없고, 증거는 게임 안에서 나온다.
- `tools/re/*.py`: 현재 `data.win`과 `Norland.exe`에 돌려 §2.2의 수치
  (`SCPT` 40,813, `OBJT` 64, `CODE` 없음)가 다시 나오는지 본다.

## 9. 위험

| 위험 | 대응 |
|---|---|
| v5.0.0c 베타가 이 빌드에서 붙지 않거나 게임이 죽는다 | §3.2의 순서. `restore-game.ps1`로 되돌린다 |
| 헤더(`experimental` HEAD)와 릴리스 바이너리가 어긋난다 | 로그의 `yytk` 줄과 프로브 결과로 드러난다. 어긋나면 YYToolkit을 그 커밋에서 직접 빌드하는 안을 사용자와 다시 정한다 |
| Steam 갱신·무결성 검사가 패치를 지운다 | `game-status.ps1`이 알려 준다. `setup-aurie.ps1`을 다시 돌린다 |
| 게임 갱신으로 스크립트 이름이나 러너 내부가 바뀐다 | `check-load.ps1`이 실패로 알려 준다 |
| 백신이 패치된 exe나 주입을 오탐한다 | 발생하면 보고한다. 보안 설정은 사용자가 바꾼다 |
| Steam으로 켰을 때 exe 패치가 무시되거나 Steam이 실행을 막는다 | Phase 0에서 실측한다. 막히면 중단하고 보고한다 |
| AGPL-3.0 (Aurie, YYToolkit) | 레포 공개나 모듈 배포 전에 다시 검토한다. Phase 0은 로컬 전용이다 |

## 10. 범위 밖

- 모드 기능 일체. 내장 디버그 UI 게이트 조사(§2.4)는 Phase 1 후보다.
- ImGui 오버레이, 설정 파일, 단축키.
- 원격 저장소, CI, 릴리스 패키징.
- 데이터 오버레이(JSON·CSV 덮어쓰기) 파이프라인.
- 세이브 백업 도구. 지금은 로컬 세이브가 없다.
