# 00 — 게임 구조와 Phase 0 실측

조사일 2026-10-04. 대상: Norland `0.5588.9777.0` (Steam buildid `25211575`).
다시 재려면 `tools/game-status.ps1`과 `tools/re/*.py`를 쓴다.

## 설치

| 항목 | 값 |
|---|---|
| 설치 경로 | `E:\SteamLibrary\steamapps\common\Norland Story Generating Strategy` |
| `Norland.exe` | 129,225,728 B, SHA256 `609C17CFD812E90C7D3E84D3EB730EC559EA3976F30A239395922F604516863E` |
| 빌드 날짜 / 구성 | 2026-09-09 / `SteamRelease` |
| 세이브·설정 | `%LOCALAPPDATA%\Strategy` (GEN8 프로젝트 이름이 `Strategy`) |
| 게임이 실행 때 쓰는 파일 | 게임 폴더의 `imgui.ini`, `Norland_DLL_log.txt` |

## 엔진

- GameMaker **YYC**. `data.win`에 `CODE`·`VARI`·`FUNC` 청크가 없다. GML 디컴파일은 불가능하다.
- `data.win`: `SCPT` 40,813개, `OBJT` 64개, `STRG` 49,102개. 280MB 가운데 270MB가 `TXTR`다.
- 스크립트 이름(`gml_Script_*`)은 `data.win`의 문자열 표와 exe 양쪽에 남아 있다.
- exe에 `2023.4.0.113` 문자열이 있다. 런타임 버전으로 보이나 확인하지 않았다.
- 렌더러는 Direct3D 11이다(러너 로그 `DirectX11: Using hardware device`, `d3d11.dll`·`dxgi.dll` 적재).
- 게임 창의 클래스는 `YYGameMakerYY`, 제목은 `Norland`다.
- 동봉 확장: `Imguigml_x64.dll`, `Steamworks_x64.dll`, `hardware_info.dll`, `YYRunnerExperiments.dll`.

## 느슨한 데이터 파일

exe가 파일 이름을 직접 참조한다. JSON 약 4,700개와 CSV 9개.

| 위치 | 내용 |
|---|---|
| `building_constructor/` | JSON 4,099개 (건물·에셋·그래프, `NewWorld.json`) |
| `actor_constructor/` | JSON 269개 (배너, 색 모음) |
| `knowledge/technology/` | JSON 121개 |
| `sounds/` | JSON 201개 |
| `localization/` | CSV 8개 (18.9MB), `locale_definition.json` |
| 최상위 | `gameplay_variables.json`, `battle_params.json`, `director_params.json`, `debug_params.json`, `NewWorldParams.csv` |

## 모드 지원

공식 지원이 없다. `mods` 폴더, 워크숍 구독, exe 안의 모드 로더 문자열이 없다.
`data.win` 문자열 표의 `steam_ugc_*`는 Steamworks 확장이 내보내는 함수 이름일 뿐이다.

## 내장 디버그 도구 (Phase 1 후보)

릴리스 빌드에 디버그 스크립트가 들어 있다. 켜는 방법은 확인하지 않았다.

- 창: `imgui_debug_actor`, `imgui_debug_building`, `imgui_debug_province`, `imgui_debug_managers`,
  `imgui_debug_system_window`, `imgui_debug_main_menu_window`, `imgui_debug_prices_setup`,
  `imgui_debug_traits`, `imgui_debug_animal`, `imgui_debug_console_log`
- 동작: `debug_spawn_army`
- 게이트로 보이는 이름: `is_debug_forced`, `current_debug_mode`, `current_debug_mode_on_init`,
  `command_line_parameters_init`, `CommandLineParameter`, `CommandLineParametersOperator`
- 빌드 구성 이름: `SteamRelease`, `SteamReleaseForTesters`, `SteamReleaseForOpenBeta`

## Phase 0 실측

| 항목 | 값 |
|---|---|
| 붙은 구성 | Aurie v2.0.2 + YYToolkit v5.0.0c (헤더: `experimental` `d5cc0078`) |
| Steam으로 켰을 때 패치된 exe가 실행되는가 | 실행된다 (`steam://rungameid/1857090`, 2회) |
| YYToolkit이 보고한 버전 | `5.0.0` (헤더의 `YYTK_MAJOR 5`와 맞는다) |
| 모듈에서 빌트인 호출 | `code_is_compiled` → `true` |
| 모듈에서 이름으로 스크립트 찾기 | `gml_Script_command_line_parameters_init` → 찾음 |
| 먼저 온 게임 스레드 콜백 | `EVENT_OBJECT_CALL` |
| 켜서 `probe done`까지 | 약 33초 |
| 실행 중 Aurie·YYToolkit이 만든 파일 | 게임 폴더의 `aurie.log` 하나 |
| 패치된 exe | 129,508,352 B (+282,624). SHA256은 패치할 때마다 달라진다 |
| `AuriePatcher remove`가 원본을 바이트 단위로 되살리는가 | 되살린다 (사본으로 확인, SHA256 일치) |
| 복원 뒤 exe 해시 | `609C17CF…863E`와 일치 |

통과한 실행의 `NlToyBox.log`:

    NlToyBox 0.0.1 loaded
    yytk 5.0.0
    trigger object_call
    builtin code_is_compiled = true
    script gml_Script_command_line_parameters_init = found
    probe done

## Aurie·YYToolkit이 이 게임에서 하는 일 (`aurie.log`)

순서대로:

1. `Aurie Core v2.0.2 loaded`. `mods\aurie\YYToolkit.dll`을 적재하고 `ModuleEntrypoint`를 부른다.
2. YYToolkit 1단계: 러너 인터페이스 훅 지점과 코드 실행 훅 지점을 찾는다. 모두 `AURIE_SUCCESS`.
   경고가 하나 나온다: `Unknown LEA address at … (lea rcx, [rbp-0x70])`.
3. YYToolkit 2단계(러너 인터페이스가 만들어진 직후): 함수 배열, `code_is_compiled`,
   `m_IsYYCRunner => true`, 스크립트 데이터, 룸 데이터, `window_handle`, 2단계 훅.
   모두 `AURIE_SUCCESS`. 여기서 `EVENT_RUNNER_INIT`이 한 번 나간다.
4. Aurie가 게임 창을 기다린다(약 18초). 창이 뜨면 "나머지 모드"를 적재한다.
   `ModuleInitialize`만 있는 모듈(`NlToyBox.dll`)은 **이 단계에 Aurie의 스레드로** 초기화된다.
   그때 러너는 이미 메인 루프에 들어가 있다.
5. YYToolkit 3단계: 스왑체인을 얻는다. `YYToolkit has been loaded successfully.`

## 알아 둘 것

- **`EVENT_FRAME`은 불리지 않는다.** YYToolkit v5.0.0c는 Present 훅을 걸지 않는다.
  소스(`d5cc0078`)의 `Hooks::InitializeStage3Hooks`는 선언과 정의만 있고 호출부가 없으며,
  릴리스 DLL에도 그 훅의 이름 문자열 `ResizeBuffers`가 없다. `EVENT_RESIZE`도 같은 이유로
  오지 않을 것이다(확인하지 않았다). 프레임마다 그려야 하는 기능(오버레이)은 이 훅을 직접 걸거나
  YYToolkit을 고쳐 빌드해야 한다.
- 게임 스레드로 들어가는 길은 `EVENT_OBJECT_CALL`이다(실측). `EVENT_WNDPROC`은 2단계에서
  훅이 걸리지만 이 콜백이 실제로 오는지는 확인하지 않았다(`EVENT_OBJECT_CALL`이 먼저 왔다).
- `EVENT_RUNNER_INIT`은 `ModuleInitialize`만 있는 모듈이 적재되기 전에 이미 지나간다.
- 헤더의 `FWCodeEvent` 별칭은 인자 순서가 `(…, int, RValue*)`인데 실제 훅 함수 `HkExecuteIt`은
  `(…, RValue* Arguments, INT Flags)`다. 래퍼의 인자를 읽는 콜백은 소스를 보고 맞춘다.
- v5 인터페이스에는 `Print` 계열이 없다. 출력은 Aurie의 `DbgPrintEx`로 한다.
- YYToolkit의 git 태그 `v5.0.0c`(와 `v5.0.0b`)는 `stable` 브랜치의 v4 시절 커밋 `86c133dc`에 붙어 있다.
  v5 소스는 `experimental` 브랜치다. 헤더를 태그로 받으면 v4 헤더가 온다.
- `ExamplePlugin/include`의 헤더 사본은 `experimental`에서도 정본(`YYToolkit/source/YYTK/Shared`,
  `YYToolkit/include`)과 내용이 다르다. 정본을 쓴다.
- `aurie.log`는 실행 중에는 0바이트이고 게임이 **정상 종료**될 때 채워진다. 게임 창에 `WM_CLOSE`를
  보내면 정상 종료된다. 실행 중에는 Aurie 콘솔 창(제목 `Aurie Framework Log | Press Ctrl+C to close`)에
  같은 내용이 나오고, 게임 러너의 디버그 출력도 그 콘솔에 섞여 나온다.
- AuriePatcher는 넘겨준 `AurieCore.dll`의 경로를 exe 안에 적어 둔다. 게임 폴더를 옮기면 다시 패치한다.
- 실패한 실행과 통과한 실행의 로그 사본은 `refs/phase0/`에 있다(추적 안 함).
