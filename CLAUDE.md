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
- 원격은 `origin` = `https://github.com/game-mod-project/NlToybox`(공개)다. 작업 브랜치를 푸시하고 `develop`으로 PR 을 올려 merge commit 으로 머지한다
  (`gh pr create --base develop`, `gh pr merge --merge --delete-branch`). `main`에는 릴리스 때 `develop` → `main` PR 로만 들어간다.
  `main`·`develop`에 직접 푸시하지 않는다(저장소를 처음 채울 때 한 번만 그대로 올렸다. 2026-10-06).
- 커밋의 작성자 이메일은 GitHub 의 noreply 주소다(이 레포의 `user.email` 설정). 공개 이력에 개인 이메일을 남기지 않는다.
  2026-10-06 에 이력 전체의 이메일을 다시 썼다: 그 전에 적어 둔 해시는 문서에서 새 해시로 바꿨고, 옛 이력의 사본은 `backups\git\`에 있다(추적 안 함).
- CI가 없다. 코드가 바뀌면 머지 전에 `tools/build.ps1` 성공, `tools/test-native.ps1`·
  `tools/tests/safety.tests.ps1` 통과, `py -3.14 -m unittest discover -s tools/re/tests`와 `py -3.14 -m unittest discover -s tools/overlay/tests` 통과,
  `tools/check-load.ps1`(또는 같은 판정을 함께 내는 `tools/ui-check.ps1`) 종료 코드 0을 확인한다. 문서만 바뀌면 생략해도 된다.
  `check-load.ps1`과 `ui-check.ps1`을 뺀 나머지는 게임을 켜지 않는다(`safety.tests.ps1`은 임시 폴더의 가짜 게임으로 돈다).
- 머지한 브랜치는 지운다(원격은 `--delete-branch`, 로컬은 `git branch -d`).
- **푸시 전에 본다**: 아래 "저작물 경계"의 확인, 그리고 추적되는 파일에 비밀 값·개인 정보·게임 파일의 글이 없는가. 공개 저장소다.

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
    pwsh -File tools/ui-check.ps1      # 게임을 켜서 모드창이 그려진 프레임을 refs\ui\ 로 받아 온다, 끝나면 끈다. 적재 판정도 함께 낸다
                                       # -Page <영역> -Path <주소> -Ask '<주소>,…' -Poke '<주소>=<수>,…' 로 영역, 주소, 읽기, 쓰기를 시험한다
    pwsh -File tools/session.ps1 -Action start   # 게임을 켜 둔다(사용자의 배율·치트 설정은 *.kept 로 치운다). 기다리지 않는다
    & tools\ask.ps1 -Lines 'state', 'list inst:o_debug max=20'   # 켜 둔 게임에 묻는다. 몇 번이든
    pwsh -File tools/load-save.ps1 -Name <이름의 일부>          # 켜 둔 게임의 메인 메뉴에서 세이브를 불러온다(-List 로 이름을 본다). 사람이 누르지 않아도 된다
    pwsh -File tools/session.ps1 -Action stop -Name <이름>       # 끄고, 설정을 되돌리고, 답을 refs\runtime\ 으로 옮긴다

- 스크립트는 `pwsh`(PowerShell 7)로 실행한다. Windows PowerShell 5.1은 BOM 없는 UTF-8의
  한글을 깨뜨린다.
- 모듈 변경은 게임을 다시 켜야 적용된다. 배포는 게임을 끈 상태에서 한다.
- `check-load.ps1`이 찾는 줄 형식은 `src/ModuleMain.cpp`가 쓴다. 한쪽을 바꾸면 다른 쪽도 바꾼다.
- 게임을 켜는 확인은 한 번에 몰아서 하고 끝나면 바로 끈다. 켜기 전에 사용자에게
  게임 창을 누르지 말라고 알린다(사용자가 새 게임을 시작해야 하는 실행이면 할 일을 차례대로 알린다).
  켜는 횟수는 사용자에게 승인받은 만큼만 쓴다.
- **세이브는 직접 불러온다**(`tools/load-save.ps1`. 게임의 `GameLoadOperator.load_save`를 부른다. `research/11`). 사용자에게 넘기는 것은 새 게임을 만드는 일뿐이다.
  게임 창을 눌러야 하면(이야기 창의 "계속하기", 인물의 초상) `shot`으로 자리를 보고, 게임 창이 앞에 있을 때 `SetCursorPos` + `mouse_event`로 누른다.
  시간을 흘리다 게임이 멈추면(`time_warp` 0) 화면부터 뜬다: 이야기 창이 떠 있을 수 있다(아덴 세이브는 4일차 08:00).
  **`load-save.ps1`은 실행 묶음이면 불러온 뒤 게임의 저장을 끈다**(`게임의 저장: 끔 (no_autosave)`. 치트 표의 `no_autosave` = `inst:o_debug.is_save_disabled`. `research/25`:
  켠 채 자동 저장의 시각을 세 번 넘겨도 파일이 생기지 않았다). 그 줄이 나왔는지 본다. 경고가 나오면 아래의 시각 규칙을 따른다. 사용자의 게임(실행 묶음이 아닌 것)에서는 켜지 않는다.
  **`__set_warp(0)`으로 멈춰 둔 게임이 다시 흐를 수 있다**(배속 1 로. 세 번 봤고 누가 풀었는지 가리지 못했다). 멈춰 둔 것에 기대어 시험 값을 남겨 두지 않는다.
- 게임은 하루에 두 번(아침, 저녁) 자동 저장을 한다. 파일 이름에 실제 시각이 들어가 **새 파일이 하나 더 생긴다**(앞의 자동 저장은 그대로 남는다. `research/16`).
  **저녁의 것은 18시보다 이르다: 게임 시각 16:30 과 17:37 사이에 난다**(17:37 까지 돌린 실행이 시험 상태가 든 자동 저장을 만들었다. `research/17`).
  **게임의 저장을 끄지 못한 실행에서는, 시험 값이 든 채 게임 시각 16:30(`game_time` 을 하루로 나눈 나머지 59400)을 넘기지 않는다.** 시간을 흘리는 스크립트는 폴링의 넘침까지 넣어 한도를 잡는다.
  아침의 것은 06:00 에 난다(실행 1 에서 06:00:06 에 생겼다. `research/25`).
  **시험 값이 든 채 그 시각을 넘기지 않는다**
  (넘겨야 하면 먼저 사용자에게 알린다. 켜기 전의 사본은 `backups\saves\`에 있다).
- 게임이 켜지다 멈추는 일이 있다(지금까지 두 번). 게임 창이 뜨지 않고, `NlToyBox.log`가 없고, `aurie.log`가
  `Using LEA pattern for RI ending`에서 끝난다. 모듈이 적재되기 전이라 모듈과 무관하다. 승인받은 횟수 안에서 다시 켠다.
  강제로 끄기 전에 Aurie 콘솔 창의 글을 받아 둔다(`research/06-cheat-menu.md`).
- `check-load.ps1`과 `probe.ps1`은 게임 창에 `WM_CLOSE`를 보내 정상 종료시키고(그래야 `aurie.log`가
  채워진다), 15초 안에 끝나지 않을 때만 강제 종료한다(`Stop-NlGame`).

## 모듈을 쓸 때

- 러너를 건드리는 호출은 게임 스레드의 콜백 안에서 한다. `ModuleInitialize`는 Aurie의 스레드에서 돈다.
- 게임 스레드 진입점은 `EVENT_OBJECT_CALL`이다. **`EVENT_FRAME`은 불리지 않는다**
  (YYToolkit v5.0.0c가 Present 훅을 걸지 않는다. `research/00-game-structure.md` 참고).
- **`EVENT_WNDPROC` 콜백도 오지 않는다**(한 실행에 0번. `research/05-mod-window.md`). 입력이 필요하면 창 프로시저를 직접 바꿔 건다.
- 화면에 그리는 것은 `src/Ui.cpp`가 한다: `os_get_info`의 `video_d3d11_swapchain`으로 스왑체인을 얻어 Present(가상 함수표 8번)에
  Aurie의 `MmCreateHook`으로 훅을 건다. Present는 게임 스레드에서 불린다(실측). 모드창은 Dear ImGui로 그리고 F8로 여닫는다.
  ImGui의 `IniFilename`은 비워 둔다(게임 폴더에 게임의 `imgui.ini`가 있다).
- 배율은 `src/Tweaks.cpp`가 실행 중인 게임의 값에 써 넣는다(데이터 파일을 읽은 ds_map, `global.__gameplay_vars`, 지식의 구조체).
  대상은 번호가 아니라 키 이름으로 찾고, 처음 본 값을 바탕으로 기억해 배율을 곱한다. **써 넣은 값이 남는 것까지만 확인했다.**
  게임이 그 값을 따르는지는 항목마다 플레이해 봐야 안다. 배율은 `mods\Aurie\NlToyBox.settings.txt`에 저장된다(사용자의 설정이다. 도구가 지우지 않는다).
  이 배율 7개는 제 영역의 패널 아래에 그린다(`core/CheatTable`의 `KnobPlaces`. 왼쪽 목록의 "배율" 영역은 없앴다).
- 치트 메뉴(`src/Menu.cpp`)는 왼쪽 목록의 영역마다 패널을 그린다. 스펙은 `docs/superpowers/specs/2026-10-05-cheat-menu-design.md`.
  - 게임의 값은 주소(`src/core/AskPath`) 하나로 가리킨다: `global.a.b[3]`, `inst:o_debug.is_x`, `inst:o_building:1.generic`, `map:150@key#0`.
    읽고 쓰는 것은 `src/Access.cpp`가 한다. 없는 것을 만들지 않고, 쓴 뒤에는 다시 읽어 확인한다.
  - **그리는 쪽(`Draw*`)에서 러너를 부르지 않는다.** 창은 바라는 상태나 명령만 남기고, 러너는 `GameTick`이 건드린다.
    `RValue`를 틱 너머로 들지 않는다(스냅샷은 글이다).
  - 치트 항목을 더하는 일은 `src/core/CheatTable.cpp`의 표에 한 줄을 더하는 일이다. 주소에 ds 번호를 적지 않는다.
    `Verified`는 플레이에서 효과를 본 뒤에만 참으로 바꾸고 `research/`에 적는다. **이름에서 읽은 뜻은 추정이다.**
  - 켠 치트와 즐겨찾기·잠금은 `mods\Aurie\NlToyBox.cheats.txt`에 저장된다(사용자의 설정이다. 도구가 지우지 않는다).
    파일에서 불러온 잠금은 꺼진 채로 시작한다(`inst:<오브젝트>:<n>`의 n 이 실행마다 다른 인스턴스일 수 있다).
  - 게임 속도는 게임의 함수로 건다: `inst:o_time_controller.__set_warp(배속)`(`research/08`. 24 로 불러 흐름이 약 24배가 되는 것을 쟀다).
    `set_time_speed(번호)`는 게임의 단추가 쓰는 길이고 일시정지를 푼다. 사용자가 게임의 속도 단추를 누르면 건 배속은 바로 풀린다.
    흐름은 `src/Cheats.cpp`가 `core/Rate`로 재서 보여 준다(후보 값을 써 보던 속도 시험은 3나-3 에서 지웠다).
  - 표의 항목이 없어도 제 패널이 있는 영역은 `AreaInfo::Panel`을 참으로 둔다. 아니면 왼쪽 목록에서 꺼진다(경제가 그랬다. `research/08`).
- 경제 패널(`src/Economy.cpp`)은 금화와 영지 창고의 자원을 게임의 함수로 바꾼다(`research/07`, `08`. 화면까지 확인했다).
  - 금화: `gml_Script_budget_money_change(변화량)`. 자원: `…__province.__warehouse.change(자원 번호, 변화량)`(정적 메서드. `NlAccess::CallMethod`가 창고에 묶어 부른다).
  - 넘기는 변화량은 언제나 유한한 정수다(`NlCore::PlanEconomy`). 부르기 전에 호출마다 로그를 남긴다.
  - **신성 반지는 자원 0번(`rune`)이고 영지 창고의 그 칸에 있다**(`research/18`. 창고의 `change(0, 변화량)`으로 화면의 반지 수가 따라온다). 어느 갈래에도 들지 않으므로
    하나씩 하는 명령(더하기, 맞추기, 최소값)만 반지를 받고 "모든 자원 +N"은 받지 않는다(`NlCore::EconomyTargets`). 번호는 0 이라 적지 않고 열쇠 `rune`의 자리로 찾는다(`RingResource`).
    영주의 반지는 소지품 0번 칸이다(`person <uuid> item_add index=0`. 게임의 `character_runes_get_count`가 그 수를 돌려준다).
  - **최소값 유지**(치트 표의 `resource_floor`): 금화·신성 반지·자원마다의 바닥은 상태 파일의 `floor <열쇠>=<수>` 줄에 있다(열쇠는 자원의 열쇠나 `gold`).
    틱이 1초마다 화면의 수를 보고 모자란 만큼 단추와 같은 함수로 채운다(`NlCore::PlanFloors`. 줄이지 않는다). 같은 것을 채우는 호출의 로그는 30초에 한 번만 적는다.
    자원의 수는 정수가 아닐 수 있다(당근 192.5). 채울 양은 올림한다.
  - 게임의 화면이 보이는 자원의 수는 예약되지 않은 수(`__no_reserve__` = `__total__` − 예약)다. 패널도 그 수를 보이고 그 수를 기준으로 맞춘다.
    청한 만큼 바뀌었는지는 `__total__`의 앞뒤로 본다(함수의 반환값에 기대지 않는다). `change`는 용량을 보지 않는다.
  - 원격 명령 `economy`와 `page`가 같은 길을 창 없이 태운다. 패널을 고치면 실행 묶음에서 `economy …`와 화면으로 확인한다.
  - **수 입력 칸(`InputDouble`·`InputInt`)에 `ImGuiInputTextFlags_EnterReturnsTrue`를 주지 않는다.** Dear ImGui 가 지원하지 않는다(`InputScalar`의 단언. 릴리스 빌드에서는 조용히 지나간다):
    Enter 로만 들어가고 칸을 떠나면 친 수가 버려진다(최소값 칸이 그랬다. `research/18`의 끝). 치는 동안의 수는 들고 있다가 `IsItemDeactivatedAfterEdit()`에 넣는다(`NlCore::StepFloorEdit`).
  - **창의 입력 칸은 글자를 쳐 넣어 확인한다**: 원격 `ui click x= y=`, `ui type text=`, `ui key name=<enter|tab|escape|backspace>`가 Dear ImGui 의 입력 큐에만 넣는다
    (게임 창과 진짜 마우스·키보드는 건드리지 않는다. 자리는 `shot`으로 본다. Tab 은 다음 입력 칸으로 옮기며 그 칸을 보이는 데로 끌어온다). 원격 명령만으로 확인한 칸은 확인한 것이 아니다.
- 게임의 판정을 바꿀 때는 그 함수가 돌려주는 값을 훅으로 바꾼다(`NlRecorder::Override`. 스펙 §3 의 수단 D. `research/09`).
  - 바꾸기 전에 `statics <주소>`로 이름을 찾고 `record`로 그 함수가 무엇을 받고 무엇을 돌려주는지 본다. 불리언·수를 돌려주는 함수만 바꾼다
    (`is_allow_to_build`처럼 구조체를 돌려주는 함수는 바꾸지 않는다). 켜 둔 게임에서 `override`로 먼저 풀어 본 뒤 치트 표의 `Hook` 항목으로 굳힌다.
  - 바꾼 값은 결과 자리(`Result`)에 두고 그것을 돌려준다. 원래 함수를 건너뛰는 것(`skip`)은 들어올 때의 `Result`가 `undefined`인 것을 표본에서 본 함수에만.
  - 자주 불리는 함수(`is_under_upgrade`: 초당 수천 번)에는 기록을 걸어 두지 않는다.
  - **수를 돌려주는 함수에는 배율을 곱할 수 있다**(`override <대상> x:<배율> [whole]`, 치트 표의 `HookScale`. `research/10`). 게임의 자료를 쓰지 않으므로
    세이브에 남는 것이 없고 되돌릴 값도 필요 없다. 0 이하의 값과 수가 아닌 값은 그대로 지나간다(`NlCore::ScaleResult`). 가격처럼 정수여야 하는 값에는 `whole`.
  - **함수 하나를 바꾼 뒤에는 그 값을 쓰는 다른 길도 따르는지 본다.** 창고 용량을 돌려주는 함수만 바꾸자 HUD 는 바뀌었지만 자정의 부패 처리는 원래 용량으로 깎았다.
    그럴 때는 그 함수가 읽는 자료(창고 종류의 용량)를 고친다.
  - 치트 표의 `Hook` 항목은 켠 것과 실제로 걸린 것을 틱마다 견준다(`NlCore::ChooseHookStep`). 원격 `unoverride`로 꺼도 체크가 켜져 있으면 다시 걸린다.
    끌 때는 `cheat <Id> off`. 걸다 실패한 함수에는 그 실행에서 다시 걸지 않는다(`NlCore::PickHookSlot`. 실패한 자리는 다시 쓰지 않는다).
  - **확인 전(`Verified = false`)의 `Hook`·`Custom` 항목은 켠 채 저장돼 있어도 꺼진 채로 시작한다**(`KeepKnown`). 창을 열지도 않았는데 판정이 바뀌거나 값이 고쳐 쓰이지 않게.
  - 건설 창이 건물을 다루는 길은 `ConstructionManager.get_building_gui_struct(이름)`이다. 사용자가 창을 다루지 않아도 이것을 직접 불러 어떤 판정이 불리는지 볼 수 있다.
- 건물 종류(171개)는 `gml_Script_get_generic_building(이름)`으로 얻는다(이름은 `gml_Script_building_generic_get_array_of_all_buildings()`). ds_map 에 들어 있어
  전역 탐색으로는 보이지 않는다(`find … in=ds`). 건설비는 `…__construction_cost.levels[등급]`(`money`, `resources.__array_of_resource_quantity[39]`)이고
  업그레이드의 비용은 같은 종류의 다음 등급이다. `src/Build.cpp`가 0 으로 쓰고 `core/CostBook`의 값으로 되돌린다.
  함수가 돌려준 구조체 안은 `NlAccess::Follow`로 보고 `NlAccess::SetNumber`로 쓴다(주소의 뿌리가 없다. 쓴 뒤 다시 읽어 확인한다).
  - 자료를 돌며 배율을 쓰는 항목(창고 용량, 조리법의 수와 재료)은 `src/Production.cpp`에 일(`Job`) 하나를 더한다: 대상을 넘기는 함수 하나와 치트 표의 `Custom`·`CustomScale` 항목.
    한 자리에 쓸 값은 `NlCore::PlanValue`가 정한다(처음 본 값이 바탕. 써 둔 값을 다시 봐도 두 번 곱하지 않는다). 열쇠는 번호가 아니라 이름으로 삼는다.
  - 창고 종류의 용량은 `inst:o_data.__building_warehouse_data.__generic_warehouses.<종류>.__capacity_in_categories.<갈래>.capacity`, 조리법은 건물 종류의
    `__production.__map_of_production`(ds_map: 만드는 자원 → 재료의 배열과 만들어지는 수)이다(`research/10`).
  - **세이브 파일(`%LOCALAPPDATA%\Strategy\saves\*.norland`)은 머리(버전, 이름) 뒤가 평문 JSON 이다.** 고쳐 쓰는 자료가 세이브에 남는지는 그 열쇠가 파일에 있는지로 본다(읽기만 한다).
    기본 가격·창고 종류의 용량·조리법·생산 비용·건설비의 열쇠는 없었다(시장 깊이와 포화도는 있다).
    **세이브의 열쇠에는 런타임 이름의 앞 `__`가 없다**(`__events_cooldowns` → `"events_cooldowns"`). 밑줄을 떼고 찾는다(`research/14`. 위의 "없었다"는 두 이름 모두로 다시 봤다).
  - 되돌릴 값을 장부가 받은 자리에만 0 을 쓴다(`CostBook::Remember`의 반환값). 되돌린 자리만 장부에서 지운다(`Forget`). 실패하면 간격을 늘려 다시 한다(`core/Retry`).
  - 0 으로 쓴 비용은 세이브에 남지 않는다(켠 채 저장한 세이브를 치트 없이 불러와 쟀다. `research/09`). 게임의 값을 고쳐 쓰는 항목을 새로 만들면 이것부터 잰다.
- 건물의 건설 구성요소는 `inst:o_building:<n>.c_construction`이다(`__construction_status`: 평소 0, 업그레이드 중 3. 등급은 `inst:o_building:<n>.__level`).
  업그레이드 중인 건물에 `c_construction.build_instantly()`(인자 없음)를 부르면 바로 끝난다(`src/Build.cpp`의 `instant_upgrade`. `research/09`). 3 이 아닌 상태에는 부르지 않는다.
- 인물·영주·인구 패널(`src/People.cpp`, `core/PeoplePlan`. `research/11`). 영주·손님은 `o_character`, 주민·노예는 `o_dummy`이고 값은 `__soul` 아래에 있다.
  - 사람은 `__soul.__uuid`로 가리킨다. `inst:o_character:<n>`의 n 은 인물이 드나들면 바뀐다: 쓰기 전에 그 자리의 uuid 를 다시 본다(`StillThere`).
    플레이어의 사람은 `__soul.__faction.__system_name == "player"`다. 여럿에게 하는 명령과 표의 항목은 플레이어의 산 사람에게만 간다(손님은 `unique_guests`였다).
  - **죽음은 `c_status.__is_dead`의 `true`·`false`만 믿는다.** 그것은 게임의 캐시라 특성이 바뀌면 `-4`로 비워진다. 그때는 `__soul.is_alive()`로 묻는다
    (`-4`를 죽음으로 읽어 특성을 붙인 바로 뒤의 명령이 그 사람을 놓친 적이 있다). 게임의 `c_*` 구성요소에서 `-4`인 수는 "아직 셈하지 않음"이다.
  - 능력치는 `__soul.__skills.__level.<이름>`에 바로 쓴다(0~20. 게임의 인물 창이 그 수를 보인다. 게임의 `set_level`이 받는 기술 구조체는 찾지 못했다).
    욕구는 `__soul.__motive.__motive[0..5]`(수면, 음식, 휴식, 신앙심, 성관계, 돌봄)에 쓴다. 나이·특성·생각·치료는 게임의 메서드로 한다:
    `set_age(나이)`, `Traits.trait_attach("이름")`·`trait_detach("이름")`, `Minds.attach_generic_mind(생각 구조체)`, `cure_all_disease()`·`cure_bleeding()`.
  - 기분은 바로 써도 남지 않는다(게임이 10분쯤마다 생각의 합으로 다시 셈한다). 올리려면 생각을 붙인다(`inst:o_data.mind_debug_totally_happy`: +100, 하루).
  - 종과 죽음의 특성(`human`, `dead`, `lost_head` …)은 붙이지도 떼지도 않는다(`IsProtectedTrait`). 여럿에게는 능력치 최대·욕구·행복·치료만 한다.
  - 표의 인구 항목은 사람을 40명씩 0.25초마다 돌며 쓴다. 바퀴의 판단(노화 깃발을 끈 뒤 처음부터 온전한 한 바퀴로 되돌리기)은 `core`의 `HoldRound`에 있고 시험한다.
    **오브젝트 이벤트마다 불리는 틱에서는 시각부터 본다**(치트 표를 읽는 것도 그 뒤에).
  - 여기서 부른 게임의 함수가 오브젝트 이벤트를 일으켜 틱이 다시 들어올 수 있다. 다시 들어온 틱은 아무것도 하지 않는다(`g_Busy`). 대상의 목록은 부르기 전에 사본으로 뜬다.
  - 인구를 늘릴 때는 소환 함수가 아니라 이주 관리자의 `__next_day_migrants_bonus`에 쓴다(다음 이주 때 그만큼 더 온다. 세이브에 남는 열쇠다).
  - 지식과 아이템 패널도 이 파일에 있다(`research/12`). **지식은 영주가 가진다**: `__soul.__character_soul.__knowledge`의 `add_all_knowledge()`(인자 없음),
    `add_knowledge(지식 구조체, true, true)`. 지식 구조체는 `inst:o_data.__knowledge_data.__knowledge_list[n]`(121개. `__name`, `__caption_replaced`)에서 얻고 그 자리의 이름을 다시 본다.
    소지금은 `__inventory.change_money(변화량)`, 소지품은 `__inventory.change(자원 번호, 변화량)`으로 바꾼다(더할 정수는 `NlCore::GiftDelta`: 가진 것보다 많이 빼지 않는다).
  - 사람에게 하는 일을 새로 더할 때는 `core/PeoplePlan`의 `PersonAct`에 한 줄, `CheckPersonCommand`·`BulkAllowed`의 검사와 시험, `src/People.cpp`의 `One()`에 case 하나를 더한다
    (창의 단추와 원격 `person`이 같은 길을 탄다).
  - **인자가 없는 함수는 기계어로 가린다**: 본문이 `argc`(r9d)를 레지스터에도 스택에도 옮기지 않으면 인자를 읽지 않는다(`research/11`. 게임이 부르지 않는 디버그 함수에 쓴다).
  - **들머리의 인자 맞춤(`0x14018A9B0`, N)은 "생략해도 된다"가 아니다.** 모자란 인자를 `undefined`로 채울 뿐이고, 그 값을 수로 쓰는 함수는 GML 오류로 게임을 끝낸다
    (`ArmyLoyaltyManager.get_summary_army_loyalty()`를 인자 없이 불러 "I32 argument is undefined"로 죽었다. `research/13`). 인자 맞춤이 있는 함수는 인자가 N 개인 함수로 보고,
    게임이 부르는 꼴을 `record`로 본 뒤에만 부른다(`rebellion_debug_spawn_player_soldier`는 인자 없이 되는 것을 불러서 본 예외다).
  - **인자가 없는 디버그 함수도 게임을 끝낼 수 있다.** `AmbushManager.debug_start_ambush_archers()`는 본문이 `argc`를 옮기지 않는데도 안에서 빈 값을 써서
    "REAL argument incorrect type undefined"로 죽었다(`research/14`). 게임이 스스로 부르지 않는 함수를 처음 부를 때는 **실행의 맨 마지막에, 그 뒤에 할 일을 남기지 않고** 하나만 부르고,
    사용자에게 오류 창이 뜰 수 있다고 먼저 알린다. 불러서 된 것: `debug_force_send_bishop()`, 디버그 소환기의 `__spawn_*()`, `rebellion_debug_spawn_player_soldier()`,
    `debug_pregnancy_next_stage()`. **불러서 게임이 끝난 것**(다시 부르지 않는다): `get_summary_army_loyalty()`, `debug_start_ambush_archers()`, `start_ambush_wolves()`, `begin_pregnant()`.
- 군대(`research/13`). 병사는 `o_dummy`이고 갈래(`__soul.__social_strata`)가 2 다. 병영의 목록은 `…__soldiers_barracks_manager.get_array_of_soldiers()`.
  - 병사 추가: `gml_Script_rebellion_debug_spawn_player_soldier()`(인자 없음). 지도 가장자리에 생겨 걸어온다. 한 틱에 20번까지 불러 봤다(`NlCore::SoldierBatch`).
  - 고용 값: `SoulBasic.get_soldier_cost()`가 돌려주는 수(치트 표의 `hire_cost`. 고용 창과 실제로 빠진 금화로 봤다).
  - **게임의 디버그 소환기 `inst:o_debug.debug_spawner`**: `__spawn_soldier`·`__spawn_knight`·`__spawn_peasant`·`__spawn_slave`·`__spawn_lord`·`__spawn_bandit`·`__spawn_wolf`()(인자 없음)가
    **마우스가 가리키는 지도의 자리**에 하나를 만든다. 앞의 다섯은 플레이어의 사람이다(`core/PeoplePlan`의 `SpawnKind`. 틱마다 하나씩 부른다).
    **싸움을 붙이는 길은 아직 없다**: 도적·늑대·깡패를 풀어도 싸우지 않았다(도적은 지도 밖으로 나간다). 매복 관리자의 `debug_start_ambush_archers()`와 `start_ambush_wolves()`는
    인자 없이 부르면 게임이 끝난다(`research/14`, `16`). 게임이 스스로 거는 싸움(아덴 세이브는 4일차 14:20 쯤과 20:00 쯤)은 플레이어의 사람이 아닌 것들끼리다(전투 기술 3 과 5).
  - 상처는 `SoulBasic.take_damage("상처의 이름", 구조체, 불리언) -> true`가 입힌다. 건너뛰면(`b:0 skip`) 상처가 생기지 않는다. 들어올 때 `Result`는 `undefined`다.
  - **훅을 불린 대상(self)으로 가릴 수 있다**(`NlRecorder::Forced::Who`: `'p'` 플레이어의 영혼일 때만, `'o'` 아닐 때만). 훅 안에서는 진영을 읽을 수 없으므로
    틱이 플레이어의 사람들의 `__soul` 구조체 주소를 모아 `NlRecorder::SetPlayerSelves`에 넣는다(`src/People.cpp`의 `ShieldTick`. 0.5초마다).
    가려졌는지는 `records <이름>`의 "applied to N call(s), let M pass"와 표본의 `[self in, other out, …]`으로 본다.
    **게임이 영혼의 메서드를 부를 때 self 는 그 영혼이다**(통증 한도 함수의 표본 `[self in]`. `research/16`).
    한 함수에 아군과 적의 배율을 따로 걸 수 있다(`Forced{Kind 'x', Number, Other, Who 'p', Cap}`. `core/Hooks`의 `HookFactor`·`ScaleCapped`, `core/BattlePlan`의 `PlanSides`).
    **묶음이 빈 동안에는 가리는 배율을 곱하지 않는다**(메뉴에서 막 들어온 때. 비면 모두가 "그 밖"으로 읽힌다). 게임 화면이 아니면 배율의 바꾸기를 끄고, 주소부터 넣은 뒤에 건다.
    "적"은 플레이어의 사람이 아닌 모두다(손님, 상인, 다른 세력, 짐승). 새로 온 사람은 0.5초까지 "그 밖"이다(모듈이 만든 사람은 다음 틱에 넣는다).
    싸움이 없을 때는 그 함수를 `method`로 직접 불러 잰다: `take_damage`는 `s:<상처의 이름> p:<영혼의 주소> b:0`으로 플레이어의 병사와 다른 진영의 사람에게 한 번씩(앞은 건너뛰어지고 뒤는 멍이 생겼다).
  - 장비(`research/17`): 소지품에 넣은 장비는 바로 착용되지만, 게임은 영혼마다의 **선호 장비**(`__soul.__preferred_equipment`)에 없는 것을 몇 시간 안에 무기고(영지 창고)로 돌려보낸다.
    소환한 병사의 선호 장비는 비어 있다(`__empty__`). 영주와 기사는 `__any__`(아무거나)라 남는다. 그래서 장비를 줄 때는 게임의 세터로 선호 장비부터 정한다:
    `SoulBasic.set_preferred_equipment(inst:o_data.__preferred_equipment_data.<묶음>)`(구조체 하나. 게임이 그 꼴로 부른다. `PersonAct::Equip`, `core/PeoplePlan`의 `Loadouts`·`EquipGifts`).
    **병과(`__soul.__soldier_class`)의 구조체에는 쓰지 않는다**: 영혼들이 함께 쓰는 자료다(한 병사의 것에 쓰자 다른 병사와 영주의 것이 함께 바뀌었다).
  - **사람을 여러 시간 따라갈 때는 `inst:o_dummy:<n>`의 번호가 아니라 uuid 로 다시 찾는다**(누가 떠나면 번호가 밀린다. 그래서 한 실험의 결과를 읽지 못했다).
  - **실제 싸움에서 본 것**(`research/23`. 사용자의 게임에 온 침입: 진영 `raid`의 병사 일곱). 전투의 목록은 `inst:o_game_map_controller.battle_debugger.__array_of_battles`
    (**훈련도 전투다**: `__cached_is_training`. 그래서 싸움이 없어도 전투 함수가 불린다), 분대는 `…battle_squads_manager.__array_of_squads[n]`(`__cached_average_moral`, `__is_enemy_to_player`),
    사람의 것은 `c_battle`(`__battle`: 전투 중이면 구조체. `__is_retreat`, `__is_surrender`)이다. 분대의 사기(`BattleSquad.get_average_moral() -> 수`)는 분대원의 기분(`__soul.__moral`)으로 보인다.
    게임이 부르는 꼴: `battle_hit(구조체, 배열, undefined, 정수, 수) -> 구조체`, `ComponentBattle.attack(ref, undefined, undefined, 불리언)`, `set_current_battle(구조체, 구조체)`, `set_surrender(불리언)`,
    `get_close_combat_support_power() -> 수`, `SoulBasic.get_bravery_threshold() -> 수`, `__get_dodge_chance(구조체) -> 수`, `get_battle_lottery_tickets_factor() -> 수`, `is_pain_shock() -> 불리언`.
    아군의 전투 기술이 실제 싸움에서 두 배(상한 20)로 넘어가는 것을 봤다(12·13 → 20). **배율의 효과는 아직 말할 수 없다**(대조가 없고 26명이 일곱을 상대했다).
    전투 기술의 둘(`ally_power`, `enemy_power`)은 사용자가 확인으로 올렸다(0.24.2. 신 묶음에도 든다). 맷집의 둘은 다음 싸움으로 미뤘다.
    둘째 싸움(도적 기지. 딴 지도이고 거기 있는 동안 `game_time`이 멈춰 있다)에서 적의 표본도 받았다: 4 → 2, 5 → 3, 3 → 2. **추첨의 배율(`get_battle_lottery_tickets_factor`)이 배율을 곱한 뒤의
    전투 기술과 맞아떨어진다**(값 넷에 맞춘 식 `0.1 + 0.09 x 기술`: 20 → 1.9, 2 → 0.28, 3 → 0.37, 4 → 0.46. 추정). `take_damage`의 셋째 인자는 훈련에서 `true`, 실전에서 `false`였다.
    지도를 떠나면 분대의 주소가 없어진다: 그 주소로 건 기록은 스크립트 이름으로 멈춘다.
  - **사용자가 하는 게임을 지켜볼 때는 읽기와 `record`만 쓴다**: 값을 쓰지 않고, 게임의 함수를 부르지 않고, `session.ps1`로 끄지 않는다. 끝나면 내가 건 기록만 멈춘다
    (`unrecord <주소>`. `unrecord all`은 치트가 건 것의 기록까지 멈춘다). **기록의 표본은 건 뒤의 처음 여섯뿐이다**: 적의 표본을 받으려면 싸움이 붙은 뒤에 `record`를 다시 보낸다.
    `list <그릇> max=5`는 찾는 칸을 자를 수 있다(칸 하나는 `ask`로 바로 묻는다).
- 프리셋(`src/core/Presets`, `src/Cheats.cpp`의 `ApplyPresetLocked`): 치트 표의 **확인된 항목의 묶음**이다. 항목을 묶음에 넣을 때는 `Verified`인지, 값을 써 넣는 종류(`Number`)가 아닌지 본다
  (`CheckPreset`과 시험이 막는다). 묶음에 없는 표의 항목은 끄고 묶음의 항목은 켠다(이미 켜져 있고 배율이 같으면 건드리지 않는다). 탐색기의 잠금과 배율 7개는 건드리지 않는다.
  새 항목이 확인되면 맞는 묶음에 한 줄을 더한다. 원격 `preset <normal|easy|sandbox|god>`, `time pause|resume`.
- 종교·이벤트 패널(`src/World.cpp`, `core/WorldPlan`. `research/14`): 이벤트 쿨다운 지우기(`gm.__game_director.__events_cooldowns`·`__events_groups_cooldowns`의 0 보다 큰 수에 0),
  주교 부르기(`…__religiosity_manager.debug_force_send_bishop()`. `is_has_bishop()`이 거짓일 때만). 원격 `world cooldowns_clear|bishop`이 같은 길을 탄다.
  세력의 자료는 `gm.__factions_manager`(`__array_of_factions[57]`, `__player_faction`)에 있고, 게임은 `Faction.get_relation_with(세력) -> 수`, `is_enemy_with(세력) -> 불리언`을 부른다.
- 외교 패널(`src/Diplomacy.cpp`, `core/DiplomacyPlan`. `research/19`). 왕국 사이의 관계(종류)는 `…__factions_manager.__allies_matrix.__matrix.<A 의 uuid>.<B 의 uuid>`에 있다
  (A 가 B 를 보는 관계. **방향이 있다**). 수의 뜻: 0 allies, 1 enemies, 2 deadly enemies, 3 neutrals, 4 friends, 5 vassal, 6 sir, 7 opponent.
  - **행렬에 바로 쓰지 않는다.** 게임이 `Faction.__update_relations(세력)`로 왕끼리의 평판(`OpinionMinds.get_opinion`)에서 다시 셈해 덮어쓴다(`set_relationship`으로 쓴 값이 되돌아갔다).
    관계를 움직이려면 평판을 갖는 쪽의 `attach_opinion_about_faction(대상 세력, inst:o_data.opinion_mind_debug_positive|negative)`(±5)를 부르고 `__update_relations(대상 세력)`을 부른 뒤 칸을 다시 읽는다.
    떼는 함수는 `detach_opinion_about_faction(대상 세력, 평판의 자료)`(하나를 뗀다. 붙은 것이 없을 때는 불러 보지 않았다).
  - 왕국은 `__system_name`이 `faction.new.name.<수>`인 세력이다(`NlCore::IsKingdom`. 57개 가운데 24개. 나머지는 도적·상인·교단·플레이어의 꾸러미다). 이름은 `get_caption()`.
    세력을 `__array_of_factions[n]`으로 가리킬 때는 쓰기 전에 그 자리의 uuid 를 다시 본다.
  - `Faction.get_relation_with(세력)`은 수치가 아니라 관계의 종류를 준다. `is_enemy_with`는 관계가 2(deadly enemies)일 때 참이었고 행렬의 읽기 함수를 거치지 않는다.
  - 문턱(한 쌍에서 잰 것): 평판 28 에서 friends, 음수면 opponent, −20 쯤 enemies, −45 쯤 deadly enemies. 그래서 수가 아니라 관계를 다시 읽으며 한 걸음씩 붙인다(`NlCore::StepToward`).
  - **같은 디버그 평판은 한 대상에게 50개까지만 센다**(`__stack_limit`). 하나가 5 씩이고 붙일 때마다 그 왕의 `__opinion_minds`에 원소가 하나 는다.
    51번째부터는 함수가 구조체를 돌려주지만 평판도 원소도 늘지 않는다(40개에 +200, 55개에 +250. 나쁜 평판과 상대 왕도 같다). **붙었는지를 반환값으로 판정하지 않는다**:
    붙이기 바로 앞뒤로 `get_king_character_soul()`이 돌려준 구조체의 `__opinions.__opinion_minds`의 길이를 견준다(`NlCore::AttachCheck`·`AfterAttach`).
    "그대로다"를 한도로 읽는 것은 그 일에서 느는 것을 한 번이라도 본 뒤에만이다. 확인하지 못하고 센 것은 결과 줄에 그렇게 적는다(`UnsureNote`).
  - 세력을 건드리기 전에 `is_destroyed()`와 양쪽의 `get_king_character_soul()`을 묻는다(게임이 그 꼴로 부른다). 묻지 못한 것은 "망했다"가 아니라 실패다(`NlCore::AliveOutcome`).
  - 협정은 `…__factions_manager.__agreement_matrix.__matrix.<A>.<B>`의 비트다: 평화 4, 교역 협정 8, 방어 동맹 192(게임의 판정 `is_declared_*_with`가 `is_has_agreement`에 넘기는 수).
    협정이 없으면 칸도 없다. `set_agreement(세력, 세력, 비트)`가 양쪽 칸에 쓴다. 이미 든 협정을 지우지 않게 지금의 비트에 더한 수를 넘기고(`NlCore::PactCell`) 쓴 뒤 칸을 다시 읽는다.
    푸는 함수(`reset_agreement`, 인자 5)는 부르지 않았다.
  - 원격 `diplomacy list`, `diplomacy <uuid|all> <friends|neutral|hostile> [side=them|us|both]`, `diplomacy <uuid> opinion amount=<수>`, `diplomacy <uuid> pact name=<peace|trade|defence>`.
    `queue=1`이면 창의 단추와 같은 길(쌓기)을 탄다.
- 영주의 호감·충성(`src/Court.cpp`, `core/CourtPlan`. `research/20`). 영주 영역의 아래에 그린다.
  - 영주가 다른 영주를 보는 평판은 `inst:o_character:<n>.__soul.__character_soul.__opinions`(OpinionMinds)에 든 평판들의 합이다. **대상을 가리키는 인자는 상대의 `__soul.__character_soul`이다**
    (구조체의 주소로 맞춰 봤다). 수를 바로 쓰지 않고 게임의 디버그 평판(`inst:o_data.opinion_mind_debug_positive|negative`: ±5, 겹침 50)을
    `opinion_attach(대상, 자료, 1, undefined)`로 붙이고 `detach_generic_opinion_mind(대상, 자료)`로 하나씩 뗀다.
  - **붙었는지·떼어졌는지는 게임이 세어 주는 수로 본다**: `get_number_of_attached_opinion_minds_for_character(대상, 자료)`가 걸음의 앞뒤로 하나만 바뀌었는가(`NlCore::CourtStepDone`).
    좋은 것과 나쁜 것을 함께 두지 않는다: 올릴 때 나쁜 것이 있으면 그것부터 뗀다(`PlanCourtStep`). 그래서 겹침 한도에 막혀 반대로 못 가는 일이 없다.
  - 왕에 대한 충성은 왕을 보는 평판이다: `SoulBasic.get_loyalty_to_king()`(수), `get_loyalty_state()`(0·1·2. 55 에서 2, -19 에서 0 을 봤다. 창의 "낮음·보통·높음"은 모드가 붙인 이름이다),
    `is_has_loyalty()`(왕과 아이는 거짓). 충성 올리기(`loyal`)는 `is_has_loyalty`가 참인 영주에게만 간다. 왕은 `Faction.get_king_character_soul()`이 돌려준 구조체와 주소가 같은 영주다.
    왕을 찾지 못하면 충성의 함수를 부르지 않는다(왕이 있는 세이브에서만 쟀다).
  - 붙인 평판은 두 시간 뒤에도 남았다(세이브에 남는지는 재지 않았다). 좋은 평판 하나의 크기는 사람에 따라 다르다(+5, +6). 그래서 수를 셈하지 않고 평판을 다시 읽으며 한 걸음씩 한다.
  - 주민·병사의 충성 대상은 `__soul.__fealty.__loyaled_to_uuid`(영주의 uuid, 없으면 빈 글)이고 `__fealty.reset_loyaled_to()`(인자 없음)가 지운다. `o_dummy`에게만 부른다(영주에게는 불러 보지 않았다).
  - `NlPeople::Rows`가 `Busy`를 돌려주면(인물 쪽이 게임의 함수를 부르는 중에 다시 들어온 틱) 그 틱만 건너뛴다. 쌓인 일을 버리지 않는다.
  - `__is_king_whose_opinion_defines_loyalty`는 부르지 않는다(그 호출이 든 요청 때 게임의 경고가 났고 원인을 가리지 못했다).
  - 원격 `court list`, `court <uuid|lords> loyal [goal=]`, `like about=<uuid|lords|king> [goal=]`, `court <uuid> opinion about=<uuid|king> amount=<수>`(한 짝씩만), `clear about=…`, `release`. `queue=1`은 외교와 같다.
- 종교(`research/21`). 패널은 표의 항목, 배율, `src/World.cpp`의 "주교 부르기", `src/Court.cpp`의 "주교와의 평판"으로 이루어진다.
  - **주교의 평판(`ReligiosityManager.get_bishop_opinion()`)은 주교가 우리 왕을 보는 평판이다.** 주교는 `o_character`(진영 `holy_synod`. uuid 는 `…__religiosity_manager.__bishop_uuid`)이고
    그 평판 구조체에 영주의 호감과 같은 함수(`opinion_attach`, `detach_generic_opinion_mind`)가 듣는다. 그래서 Court 에 주교를 평판의 주체로 넣었다(`court bishop like|opinion|clear about=…`.
    `CourtLord::Bishop`). 주교는 "lords"에 들지 않고, 대상으로 삼지 않고, 걸음마다 아직 주교인지 다시 본다. 충성 올리기와 충성 대상 지우기는 주교에게 없다.
  - 신앙심은 욕구 3번이다. `inst:o_debug.debug_piety_decrease_per_hour`를 0 으로 쓰면 줄지 않는다(`piety_decrease`. 14명에게서 봤다). 채워 두는 것은 욕구 항목의 바퀴가 한다(`piety_full`. `NeedsToHold`의 넷째 인자).
  - 성스러운 보호는 `inst:o_game_map_controller.__onboard_manager.__is_under_holy_defence`(수 1)이고 `is_under_holy_defence()`가 그 수를 돌려준다. **수를 돌려주는 판정은 `CheatKind::HookNumber`로 건다**
    (돌려줄 형은 `NlCore::HookForcedKind`가 정한다: 불리언 `'b'`, 수 `'n'`, 배율 `'x'`). 게임이 돌려주던 형 그대로 바꾼다.
  - 종교의 게임 변수(`global.__gameplay_vars`의 `religiosity_*_cost`, `*_piety_restore`, `church_preach_conversion_factor`)와 설교의 비용(`inst:o_data.__preach_data.__preach_list[n].__cost`)은
    `src/Production.cpp`의 일이 쓴다(`WalkVars`, `WalkReligionCosts`. 열쇠의 목록은 `core/WorldPlan`). 세이브에는 그 열쇠가 없다. **게임이 그 값을 따르는지는 보지 못했다**(설교를 정하는 것은 게임의 창이다).
  - **표에 `global.` 뿌리로 값을 써 넣는 항목(Toggle, Number)을 두지 않는다**(시험이 막는다): 메인 메뉴에서도 써지고, 창이 보이는 동안 전역 4,700여 개를 훑는다. 게임 변수는 모듈의 일(Custom·CustomScale)로 쓴다.
  - 설교의 효과(설교 강도, 전환 확률, 헌금)는 재지 못했다: 설교가 정해져 있지 않은 세이브에서는 그 스크립트들이 한 번도 불리지 않는다. 돌려주는 형을 보지 못한 함수에는 배율을 걸지 않는다.
  - 건물의 종류는 `inst:o_building:<n>.raw_caption`(`"building.stone_church"`)으로 가린다. 교회의 구성요소는 `c_church`(교회가 아니면 -4).
- **시간을 흘리는 스크립트는 배속이 0 으로 떨어지면 다시 건다**(아덴 세이브는 4일차 08:00 에 이야기 창이 떠 멈춘다. 다시 걸지 않으면 거기서 선다. `research/21`).
- 특성의 글(`core/Localization`, `src/People.cpp`. `research/20`). **게임 파일의 글을 레포에 싣지 않는다**: 모듈이 시작할 때 게임 폴더의 `localization\main.csv`(화면 이름)와
  힌트 파일 셋(`hints_tutorial.csv`, `hints_with_icons.csv`, `hints.csv`)을 읽는다(Korean 칸, 비면 English 칸. `ReadLocalization`). 시험은 지어낸 글로 한다.
  - 화면 이름의 열쇠와 설명의 열쇠는 게임에 묻는다: `gml_Script_trait_property_get(이름, 번호)`(게임이 (글, 정수)로 부른다). 0번 이름, 1번 화면 이름의 열쇠(대개 `trait.<이름>`.
    `aging`은 `trait.oldman`, 안쪽 특성 `__…__`은 빈 글), **21번 힌트(설명)의 열쇠**. **열쇠를 이름으로 어림하지 않는다**(`hint_<이름>`·`hint_talent_<이름>`·`hint_trait_<이름>`이 섞여 있고 44개는 어느 꼴도 아니다).
  - 번호는 이 빌드의 것이다. 쓰기 전에 배치를 본다: 이름의 줄이 있는 특성 셋에서 0번이 그 이름이고 1번이 `trait.`로 시작하는가(`TraitLayoutProbes`·`TraitLayoutOk`. 이름순의 앞쪽인 `__…__`으로 확인하지 않는다),
    21번이 준 열쇠의 절반은 힌트 파일에 있는가(`HintKeysPlausible`). 어긋나면 설명을 붙이지 않고 까닭을 창과 로그에 적는다. 게임이 갱신되면 `research/20`의 표를 다시 잰다.
  - 힌트의 첫 줄은 제목이고 그 아래가 설명이다(`SplitHint`). 꺾쇠 표식은 지우고 `{…}` 자리는 "(값)"으로 보인다(`PlainHint`). 이름의 줄이 없는 특성은 힌트의 제목을 흐린 글씨의 명칭으로 보인다(`GoodHintTitle`).
  - 진짜 파일을 코어의 코드로 읽어 보는 선택 시험: `$env:NLTOYBOX_TEST_GAME_DIR = <게임 폴더>`를 주고 `build\nlcore_tests.exe tools\probes`(글은 내지 않고 수만 낸다. 평소 시험은 게임 파일에 기대지 않는다).
  - **긴 글을 표의 좁은 칸에 두지 않는다**(설명 칸이 한 글자 너비가 돼 세로로 흘렀다). 명칭 아래의 줄로 그린다. 창을 고치면 `shot`으로 받아 눈으로 본다.
- 역할 프리셋(`src/core/RolePlan`, `src/People.cpp`의 `ApplyRole`. `research/22`): 한 사람에게 능력치를 올리고(내리지 않는다), 그 역할에 해로운 특성을 떼고, 재능을 붙인다.
  인물 탭의 "역할 프리셋"과 원격 `person <uuid> role name=<Id>`가 같은 길을 탄다(한 사람을 짚어서만. `lords`·`people`에게는 가지 않는다).
  - **표(`RolePresets`)는 사용자가 정한 것이다**: 핵심 능력치 20, 보조 15, 올리기만. 고칠 때는 시험의 크기 표(`Size`)와 넣지 않기로 한 이름들도 함께 본다.
    특성의 이름이 게임에 있는지는 게임을 불러올 때의 로그로 본다(`role presets name N trait(s) this game does not have`. 이 빌드에서는 0).
  - 차례는 능력치, 떼기, 붙이기다(`RoleSteps`). 특성의 걸음마다 목록을 다시 읽어 이미 없는 것을 떼거나 이미 있는 것을 붙이려 부르지 않는다(`RoleStepNeeded`).
    특성을 읽지 못한 것과 없는 것을 가른다(`ReadTraits`의 반환값). 결과의 글은 한 것과 하지 못한 것을 수로 말한다(`RoleReport`).
  - 미리 보기: 요약(`RolePreview`)은 단추 옆이 아니라 제 줄에 둔다(옆에 두자 기본 너비의 창에서 잘렸다). 뗄 특성을 붙일 특성보다 먼저 그린다.
    떼기 전에 알릴 것은 **잰 것만** `RemoveNote`에 적는다(지금은 `stupidity` 하나).
  - 잰 것: 프리셋 13개의 특성 74개가 모두 붙고, 해로운 특성 아홉 종이 떼어지고, 두 시간 뒤에도 그대로다. `coward`를 가진 사람에게 `fearless`·`brave`가 함께 붙는다(게임이 막지 않는다).
  - **재능마다의 효과는 재지 않았다.** 본 것은 둘이다: `stupidity`를 떼면 생각의 합이 25 내려간다(다시 붙이면 돌아온다),
    붙인 재능의 행동을 게임이 돌리려 한다(`redeemer`·`musician`의 "Trait action …" 경고가 게임의 오류 파일에 남았다).
  - 능력치의 화면 이름은 `main.csv`의 `actor.skill.<열쇠>`에서 읽는다. **전투만 `actor.skill.fight`다**(`SkillCaptionKey`. 여덟이 같은 꼴이라고 어림했다가 틀렸다).
  - **`shot`에는 이름을 준다**(`shot role-before`. 이름이 없으면 찍히지 않는다). 받은 화면은 `ask.ps1`이 `refs\ui\<이름>.png`로 옮긴다. 명령 뒤의 화면은 2초쯤 두고 찍는다.
- 임신·출생·성장(`src/core/FamilyPlan`, `src/People.cpp`. `research/24`). **플레이어의 영주에게만 한다**(`NlCore::IsPlayersLord`. 주민·손님·다른 진영에게는 불러 본 적이 없다).
  - 임신의 단계는 특성이다: `pregnant_st1`·`st2`·`st3`. 출산 뒤 어머니에게 `pregnant_forbid`가 붙는다. 구성요소는 `__soul.__pregnancy`(`__father_soul_uuid`: 아버지의 uuid, 평소에는 빈 글).
    성별은 `SoulBasic.get_gender() -> 1(여성) | 0(남성)`.
  - 다음 단계: `…__pregnancy.debug_pregnancy_next_stage()`(인자 없음). 임신 중인 사람에게만 부르고 부를 때마다 특성을 다시 읽는다(본 바뀜 1→2, 2→3, 3→0 만 된 것으로 친다. `AfterStageCall`).
    한 틱에 잇달아 불러도 된다. **게임의 확률을 그대로 탄다**: 3/3기의 다음이 출산이 아니라 유산일 수 있다.
    **아이가 생겼는지는 사람의 수로 본다**(`StageReport`의 `Children`). 단계가 0 이 된 것만 보고 "출산했습니다"라고 적었다가 틀렸다.
  - 임신 시작: 게임의 판정 `is_can_pregant()`를 묻고(수·불리언일 때만 읽는다), 아버지의 uuid 를 `__father_soul_uuid`에 적고(`NlAccess::WriteString`), `trait_attach("pregnant_st1")`.
    그렇게 시작한 임신 49번이 모두 출산이나 유산까지 갔다. **`begin_pregnant()`는 부르지 않는다**: 아버지를 적고 불렀는데 게임이 끝났다("I32 argument is undefined").
  - 아이를 어른으로: `set_age(18)`. 게임이 `kid`를 떼고 `untitled_lord`를 붙이고 진영을 `player_untitled`로 바꾼다(15, 16 에서는 아이 그대로였다. 자란 사람은 플레이어의 영주 목록에서 빠진다).
  - 표의 셋(게임 변수. `src/Production.cpp`의 일): `no_miscarriage`는 확인됐다(켠 채 임신 25번에 유산 0, 끈 채 27번에 5). `safe_childbirth`(끈 채 22번의 출산에도 죽음이 없어 가리지 못했다)와
    `pregnancy_chance`(모듈의 임신 시작은 그 확률을 타지 않는다)는 확인 전이다.
  - **확률로 정해지는 것은 되풀이해 센다**: 멈춘 채로 한 실행에서 임신 시작과 바로 출산을 수십 번 돌렸다. 모듈의 로그의 `people: birth on …` 줄로 센다.
    원격의 답은 첫 줄이 `running …`이다. 결과는 그다음 줄이다.
- 월드(`src/World.cpp`, `core/SeasonPlan`, `core/WorldPlan`, `core/PlaceKey`. `research/25`). 표의 항목들 아래에 계절 패널을 그린다.
  - **계절**: `inst:o_game_map_controller.__current_local_map.__season_manager`(지도마다 하나). 단계 0..3, 가혹한 계절은 단계 2(게임이 `__get_remain_time_to_phase(2)`를 부른다).
    남은 시간은 시작 시각(`__start_phase_time`)에서 셈한다. 읽을 때는 게임의 함수로(`is_extreme()`, `__get_remain_time_of_current_phase()`, `get_remain_time_to_extreme_season()`: 인자 없음),
    바꿀 때는 시작 시각에 쓰고 **게임의 함수가 돌려주는 남은 시간이 따라 움직였는지** 본다(`RemainMoved`). `__set_phase`는 부르지 않는다: 게임이 매시 정각에 스스로 넘긴다(수 하나로 부른다).
  - 미루기는 지금보다 뒤로 밀지 않고 실제로 밀린 만큼을 말한다(`DelayReport`). 끝내기는 남은 시간을 60초로 줄인다(다음 정각에 넘어간다. 새 시작은 지난 08:00).
    가혹한 계절 중의 미루기, 붙들기를 켠 동안의 끝내기, 0 보다 앞의 시작 시각을 써야 하는 끝내기는 거절한다.
  - **붙들기 둘(`season_hold`, `mine_stock_hold`)이 기억한 값은 그 자리의 것이다**(`PlaceKey`: 관리자 구조체의 주소 + 지도 관리 인스턴스). 자리나 단계가 달라지거나 시각이 거꾸로 가면
    쓰지 않고 다시 기억한다. 값을 기억해 두었다가 되돌려 쓰는 항목을 새로 만들면 같은 가림을 넣는다(다른 세이브를 불러온 뒤 앞의 게임의 값으로 쓰지 않게).
    붙들기는 실제 시간 1초마다 쓴다. 끈 틱에 한 번 정리한다(`g_HoldWasOn`).
  - **세계 지도**: `inst:o_global_map.__m_global_map`(GlobalMapManager. `…__province`가 아니다). 안개는 `is_initial_area_visible(지역 구조체) -> 불리언`이 참이면 걷힌다(표의 `reveal_map`).
    물체의 `is_visible`만 바꾸면 지역이 어두운 채이고 `is_in_fog_of_war`만 바꾸면 화면이 그대로다. `open_map()`·`close_map()`(인자 없음)은 직접 불러도 된다.
    지도의 화면을 넓게 받으려면 열린 동안 `global.__current_camera.__zoom_ratio`를 0.7 로 쓴다(시험에서만. 끝나면 1.8 로 되돌린다).
  - 늑대: `…__wolf_manager.get_max_number_of_wolves()`(밤에 한 번, 수)에 0 을 돌려준다. 광산: `…__mines_manager.__mines_stock.<자리>`(게임이 캘 때마다 1 준다)를 되돌려 쓴다.
  - **그 실행에서만 가는 항목은 `Cheat::ThisRunOnly`**(표의 줄 끝에 `true`. 지금은 `no_autosave` 하나): 켠 것을 잊으면 잃는 것이 큰 항목에 쓴다.
  - 날씨를 일으키는 것은 만들지 않았다: `__start_rain(구조체, 수)`의 구조체가 무엇인지 모른다. `fast_map_moving`·`fast_global_tasks`·`no_tree_growth`는 재지 않았다.
  - 원격 `world season|season_delay|season_end`. 새 항목은 프리셋에 넣지 않았다(세이브에 남는 값을 쓰거나 난이도의 것이 아니다. 넣을지는 사용자가 정한다).
- 범죄(`src/Crime.cpp`, `core/CrimePlan`. `research/26`). 왼쪽 목록의 "범죄" 영역: 표의 항목 아래에 부랑자와 영주의 죄를 그린다.
  - **범죄자("부랑자")는 주민의 `c_criminal.__is_dummy_criminal`이다**(처음에는 수 0, 지정된 뒤로는 불리언. 0 보다 클 때만 범죄자로 읽는다: `NlCore::IsVagabondFlag`). 그 구성요소는 영주에게도 있다.
    관리자는 `inst:o_game_map_controller.__province.__criminal_manager`(그 안의 수들은 재지 않았다. 창에 적지 않는다). 화면 왼쪽 위의 넷째 수가 범죄자의 수다.
  - 게임은 **저녁 18:00** 에 주민을 범죄자로 만든다: 그 사람의 `is_criminal_immunity()`(인자 없음, 불리언)를 묻고 `set_criminal_scum(true, true)`나 `set_criminal_scum(true)`를 부른다.
    그 답을 참으로 바꾼 시도 셋은 모두 막혔다(표의 `no_new_criminals`. 훅의 주소는 `inst:o_character:0.c_criminal.is_criminal_immunity`: 스크립트는 모두가 함께 쓰고 게임 화면에는 `o_character`가 언제나 있다.
    그래서 누구의 호출이든 바꾼다). 지정은 `set_criminal_scum(false, true)`로 푼다.
  - **읽지 못한 것을 "없다"나 "됐다"로 적지 않는다**: 깃발을 읽지 못한 주민과 특성을 읽지 못한 영주는 따로 센다(`CrimeSummary`·`LordsLine`·`TraitClearReport`의 `Unread`).
    지정 풀기는 부른 뒤 그 자리의 uuid 와 깃발을 다시 읽어 판정하고(`NlCore::AfterClear`), 확인하지 못한 사람은 "부른 뒤 확인하지 못함"으로 센다(`ClearReport`의 `Unsure`).
  - **깡패(`__is_dummy_thug`. 게임의 `is_thug()`가 읽는 깃발)는 되돌리지 않는다**(`NlCore::CanClear`): 게임이 만든 깡패를 본 적이 없어 풀면 어떻게 되는지 재지 못했다.
  - 훔친 것 되돌리기(`return_back_stolen_to_player_warehouse()`. 인자 없음)는 사람을 짚어 부르고 훔친 금화(`__stolen_gold`)의 앞뒤를 적는다(`StolenReport`). 창의 단추는 훔친 금화가 있는 줄에만 나온다.
    훔친 것이 있는 사람에게서는 보지 못했다(확인 전).
  - **죄는 특성이다**(이름이 `sin_`으로 시작한다. 열두 가지). 영주의 범죄 혐의도 특성이다(`character_crime`, `…_blamed_by_bishop`, `…_blamed_by_fanatics`).
    지울 때는 인물 모듈의 특성 떼기를 그 특성마다 부르고(`NlPeople::Do`의 `TraitRemove`) 특성을 다시 읽어 없어졌는지 본다. 플레이어의 영주에게만 한다.
    **게임이 건 혐의는 보지 못했다**(직접 붙인 `character_crime`을 떼어 본 것뿐이다). 혐의 지우기의 단추에는 "(확인 전)"이 붙어 있다.
  - 플레이어의 산 주민만 본다(`NlCore::IsPlayers`). 사람의 번호는 누가 떠나면 밀린다: 부르기 바로 전에 그 자리의 uuid 와 깃발을 다시 본다.
  - **게임이 범죄자를 만드는 저녁은 실행마다 다르다**(같은 세이브의 7일차 18:00 에 시도 5번, 1번, 0번). 그래서 면책 훅의 근거는 저녁이 아니라 시도마다 센다(그대로 둔 여섯은 모두 생겼고 바꾼 셋은 모두 막혔다).
    **시험용 범죄자는 게임이 부르는 꼴로 직접 만든다**: 플레이어의 주민(갈래 1)에게 `method inst:o_dummy:<n>.c_criminal.set_criminal_scum b:1 b:1`(그 자리의 uuid 를 먼저 본다. 시간을 흘리지 않아도 된다).
    영주의 죄와 혐의도 `person <uuid> trait_add name=sin_fight|character_crime`으로 붙여 시험한다. 주민을 슬프게 하는 것만으로는 범죄자가 되지 않았다.
    범죄자들이 이틀 동안 범죄를 저지르지 않아 도둑질·깡패·도적의 것(게임 변수 넷, 훔친 것 되돌리기)은 확인 전이다.
  - 범죄의 깃발은 세이브에 남는 자료다(열쇠 `is_dummy_criminal` 들. 게임 변수 일곱의 열쇠는 세이브에 없다).
  - 원격 `crime list | clear <uuid|all> | return_stolen <uuid|all> | absolve <uuid|lords> | acquit <uuid|lords>`.
- 게임은 잡은 오류와 불러오기·저장의 시각을 `%LOCALAPPDATA%\Strategy\catched_errors_<버전>.txt`에 적는다. 실행 묶음 뒤에 그 파일의 끝을 본다(읽기만 한다).
  **매복이 진행 중이면 게임이 스스로 오류를 쏟아 낸다**(이벤트 `ambush_squad`, `map_get_building_id`. 게임 시각 1분에 15건쯤. 아덴의 아침 자동 저장에서는 7일차 오후부터).
  모듈의 기능·배속·저장 끄기와 무관하다(대조 실행. `research/25`). 그 파일이 하루에 수십 MB 씩 커지므로 시간을 흘리는 실행은 그 전에 끝내거나, 넘겨야 하면 사용자에게 알린다.
  오류 파일에 새 오류가 있으면 **대조 실행**(같은 세이브, 같은 배속, 아무것도 하지 않음)으로 내 변경 때문인지부터 가린다.
  - `variable_instance_exists`·`variable_instance_set`·`array_set`·`variable_global_set`·`is_method`는 이 러너에서 된다(`research/06-cheat-menu.md`).
  - 모드창의 글꼴에는 한글과 라틴-1 만 있다. 창의 글에 화살표나 별 같은 기호를 쓰지 않는다.
- 켜져 있는 게임에 도구가 파일로 묻는다(`src/Remote.cpp`, 스펙 §14). 줄의 꼴은 `src/core/RemoteCommand.hpp`에 있다:
  `ask`·`about`, `list`, `tree`, `find`·`refine`, `write`·`poke`, `state`, `shot`, `window`·`page`, `record`·`records`, `call`·`method`,
  `statics`·`treecall`, `override`(`n:`·`b:`·`u`·`x:<배율>`)·`unoverride`, `economy`(`floor`·`gold_floor` 포함), `cheat <Id> on|off|<수>`, `ui click|type|key`,
  `world cooldowns_clear|bishop|season|season_delay|season_end`, `crime list|clear|return_stolen|absolve|acquit`,
  `person list|show <uuid>|<uuid·lords·people> <할 일>`(능력치, 욕구, 나이, 특성, 행복, 치료, 지식, 소지금, 소지품, 역할 프리셋 `role name=<Id>`, 임신 `pregnancy_next`·`birth`·`conceive name=<아버지의 uuid>`, `grow_up`), `diplomacy`, `court`(영주와 `bishop`), `traits [find=] [max=]`.
  - **기록의 표본과 `ask`·`method`의 답은 구조체의 주소를 적는다**(`struct@1369ca73600`). 인자로 온 구조체가 어느 것인지(영혼인가, 인물 영혼인가, 어느 자료인가)를 추측하지 않고 주소로 맞춰 본다.
  - 게임이 스스로 부르는 것을 보려면 그 일을 일으킨다: 특성을 붙이자(`person … trait_add`) 게임이 이름 함수와 속성 함수를 불러 꼴이 기록에 남았다. 위험한 호출을 쓰지 않아도 됐다.
  - **실행 중에 사용자가 모드창의 단추를 누를 수 있다.** 보내지 않은 일이 로그에 있으면 그것이다. 시간이 지나도 남는지를 잴 때는 창을 닫고 잰다(`window close`).
  잰 것은 `research/07-remote.md`.
  - 게임 화면의 값을 찾는 일은 사용자에게 넘기지 않는다. 실행 묶음을 켜고 세이브를 불러와(`load-save.ps1`) `ask.ps1`로 찾는다.
  - **게임 스크립트는 `gml_Script_` 이름으로만 부르고 훅을 건다.** 접두 없는 이름은 러너에서 다른 루틴을 가리킨다(`NlCore::ScriptRoutineName`).
  - `call`과 `method`는 인자의 수와 형을 본 함수에만 쓴다. 수는 `record`의 기록이나 본문의 기계어(`dumpbin /disasm`. `research/07`)로,
    형은 기록으로 본다. 본 적 없는 꼴로 부르지 않는다. 순서를 모르면 뒤바뀌어도 탈이 없는 값으로 처음 부른다.
  - **훅을 걸기 전에 `py -3.14 tools/re/script_calls.py <exe> <이름>`으로 직접 호출을 센다.** 0곳이면 참조로만 불리거나 부르는 곳마다
    본문이 들어가 있는 것이다. 뒤의 경우 게임의 호출이 훅에 오지 않는다(`pub_sub_event_perform`, `time_hour`). 그때 기록 0번은 "안 불렸다"가 아니다.
    메서드는 참조로 불려서 0곳이어도 온다.
  - **값을 바로 쓰는 것과 게임의 함수로 바꾸는 것은 다르다.** 금화와 영지 창고는 바로 쓰면 게임의 읽기 함수는 그 값을 주지만
    HUD 는 이벤트(`main_budget_change`, `main_resource_change`)가 와야 고쳐진다. 게임의 함수가 있으면 그것으로 바꾼다
    (금화: `gml_Script_budget_money_change(변화량)`. 화면에서 확인했다).
  - 생성자의 정적 메서드(`…__province.__warehouse.change`)는 `list`에 나오지 않지만 이름으로 읽힌다. 묶인 곳이 없으면
    `NlAccess::CallMethod`가 그것을 가진 구조체나 인스턴스(주소의 부모)에 묶어 부른다(`method`, `method_call`). 묶을 곳이 없으면 부르지 않는다.
    부르기 전에 `about <주소>`로 무엇이 어떻게 불릴지 본다.
  - 한 번 건 훅은 떼지 않는다(`src/Recorder.cpp`. 한 실행에 64개까지). 훅 안에서는 빌트인을 부르지 않는다.
    후보 40개에 한꺼번에 기록을 걸면 한 실행에 자리가 바닥난다(`no free hook slot`). 고른 것에만 건다. 이미 건 함수에 `record`를 다시 보내면 표본만 새로 받는다(자리를 더 쓰지 않는다).
  - 게임이 인자 없이 부르는 것을 기록한 함수는 `method`로 직접 불러 그 자리에서 결과를 볼 수 있다(부패 처리 `spoilage_process`를 불러 용량이 먹는지 봤다).
  - 묻는 파일은 이름을 바꿔 집은 것만 실행한다(`NlCore::TakeRemoteRequest`). 같은 요청을 두 번 실행하지 않는다.
  - 실행 묶음이 죽으면 `*.kept`와 `NlToyBox.session.txt`(켤 때 없던 설정 파일의 표식)가 남는다. `session.ps1 -Action stop`이 되돌린다.
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
- `VERIFIED`도 "값이 메모리에 올라온다"까지다. 게임이 그 값을 쓰는지는 `effect`이고, 이 빌드에서 본 것은 `initial_budget` 하나다.
  `gameplay_variables.json`의 키 가운데 `global.__gameplay_vars`에 이름이 없는 56개는 파일을 읽은 ds_map에만 올라온다(`research/04-overlay-verify.md`).
- 카탈로그와 프리셋에는 게임 파일의 값을 옮겨 적지 않는다(키 이름, 등급, 해시만). 값이 든 표는
  `overlay.ps1 keys -Out refs\registry\<버전>.tsv`로 뽑는다(추적 안 함).
- 수정기(`tools/overlay/jsonedit.py`)는 값의 글자만 바꾼다. 게임 파일에 한 번도 없던 문법(주석, 작은따옴표, `NaN`, 같은 키의 중복)은
  추측해서 읽지 않고 오류를 낸다. 문법의 근거는 `research/03-data-files.md`.
- 게임이 갱신되면 그 버전의 카탈로그가 없어 `overlay.ps1`이 거부한다. **Steam 무결성 검사 직후에** `catalog/<새 버전>/keys.json`을 놓고
  `overlay.ps1 pin`으로 `files.json`을 만든 뒤(프리셋을 입혀 둔 채 갱신됐으면 고친 파일이 남아 있을 수 있다. `pin`은 상태 파일이 남아 있으면 거부한다) `overlay.ps1 scan`과 `check`로 키가 그대로인지 본다. 등급은 새 빌드에서 다시 잰다.

## 의존

- Aurie v2.0.2, YYToolkit v5.0.0c 릴리스 바이너리. 출처·크기·SHA256은 `tools/pins.json`.
- Dear ImGui v1.91.9 (MIT). 서브모듈 `external/imgui`. 모듈에 함께 빌드한다.
- 헤더는 서브모듈 `external/YYToolkit` (`experimental` 브랜치 `d5cc0078`).
  **git 태그 `v5.0.0c`를 체크아웃하지 않는다.** 그 태그는 v4 헤더를 가리킨다.
  새로 클론했으면 `git submodule update --init` 먼저.
- 서브모듈 안의 파일은 고치지 않는다.
- Aurie와 YYToolkit은 AGPL-3.0이다. 레포는 소스만 공개했다(2026-10-06. 빌드한 DLL 과 `refs/`·`backups/`·`downloads/`는 추적하지 않는다.
  공개 전에 추적되는 파일 205개에 바이너리·비밀 값·개인 정보가 없는 것을 봤다). **라이선스 파일은 아직 없다**: 무엇으로 할지는 사용자가 정한다.
  모듈(DLL)을 배포하려면 그 전에 AGPL-3.0 과 맞는 라이선스를 정한다.

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
