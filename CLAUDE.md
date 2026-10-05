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
  `tools/check-load.ps1`(또는 같은 판정을 함께 내는 `tools/ui-check.ps1`) 종료 코드 0을 확인한다. 문서만 바뀌면 생략해도 된다.
  `check-load.ps1`과 `ui-check.ps1`을 뺀 나머지는 게임을 켜지 않는다(`safety.tests.ps1`은 임시 폴더의 가짜 게임으로 돈다).
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
- 게임은 게임 시각 06시와 18시에 자동 저장을 하고 같은 종류의 앞 자동 저장을 갈아 끼운다. **시험 값이 든 채 그 시각을 넘기지 않는다**
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
  - 넘기는 변화량은 언제나 유한한 정수다(`NlCore::PlanEconomy`). 갈래에 없는 자원(0번)은 건드리지 않는다. 부르기 전에 호출마다 로그를 남긴다.
  - 게임의 화면이 보이는 자원의 수는 예약되지 않은 수(`__no_reserve__` = `__total__` − 예약)다. 패널도 그 수를 보이고 그 수를 기준으로 맞춘다.
    청한 만큼 바뀌었는지는 `__total__`의 앞뒤로 본다(함수의 반환값에 기대지 않는다). `change`는 용량을 보지 않는다.
  - 원격 명령 `economy`와 `page`가 같은 길을 창 없이 태운다. 패널을 고치면 실행 묶음에서 `economy …`와 화면으로 확인한다.
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
- 게임은 잡은 오류와 불러오기·저장의 시각을 `%LOCALAPPDATA%\Strategy\catched_errors_<버전>.txt`에 적는다. 실행 묶음 뒤에 그 파일의 끝을 본다(읽기만 한다).
  - `variable_instance_exists`·`variable_instance_set`·`array_set`·`variable_global_set`·`is_method`는 이 러너에서 된다(`research/06-cheat-menu.md`).
  - 모드창의 글꼴에는 한글과 라틴-1 만 있다. 창의 글에 화살표나 별 같은 기호를 쓰지 않는다.
- 켜져 있는 게임에 도구가 파일로 묻는다(`src/Remote.cpp`, 스펙 §14). 줄의 꼴은 `src/core/RemoteCommand.hpp`에 있다:
  `ask`·`about`, `list`, `tree`, `find`·`refine`, `write`·`poke`, `state`, `shot`, `window`·`page`, `record`·`records`, `call`·`method`,
  `statics`·`treecall`, `override`(`n:`·`b:`·`u`·`x:<배율>`)·`unoverride`, `economy`, `cheat <Id> on|off|<수>`,
  `person list|show <uuid>|<uuid·lords·people> <할 일>`(능력치, 욕구, 나이, 특성, 행복, 치료, 지식, 소지금, 소지품).
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
