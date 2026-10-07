# 코드리뷰 — 유지보수성 (2026-10-07)

- 대상: `develop` a266691, 모듈 0.27.1. `src/` 43파일(20,350줄), `tests/native/core_tests.cpp`(3,478줄), CLAUDE.md·README 와 코드의 정합.
- 목적: 기능을 더하기 전에 구조·중복·시험 공백을 찾는다. 사용자가 고른 기준은 **유지보수성**이다(공개 배포 준비나 기능 추가 속도가 아니다).
- 방법: 영역 여섯(적재·접근·원격 / 창·치트 표·탐색기 / 경제·건설·생산·월드 / 인물·가족·역할·현지화 / 외교·궁정·범죄 / 시험·문서)으로 나눠 읽기 전용 리뷰를 돌리고,
  **모든 지적의 파일:줄을 다시 열어 확인했다.** 줄이 맞지 않거나 근거가 없는 것은 뺐다. 게임은 켜지 않았다.
- `tools/` 의 스크립트는 이번 범위가 아니다.

## 0. 요약

- **치명적인 것(게임이 끝나거나 자료가 깨지는 것)은 없다.** CLAUDE.md 의 규칙(Draw 에서 러너 안 부름, 정적 RValue 없음, 쓰기 전 uuid 재확인, 쓴 뒤 다시 읽기, g_Busy, 보호 특성, 플레이어의 영주에게만)은 지켜진다(§4).
- **코어의 공개 함수는 모두 시험이 있다.** 여섯 영역 모두 공백 0. 코어와 러너의 분리가 잘 돼 있다.
- 유지보수를 막는 것은 셋이다.
  1. **규칙 위반 둘**: 수 입력 칸의 `EnterReturnsTrue` 네 곳(R1), `Game.cpp` 를 거치지 않는 러너 호출 두 곳(R2).
  2. **큰 함수 둘과 큰 파일 둘**: `People.cpp` 의 `One` 501줄, `RemoteCommand.cpp` 의 `ParseRemoteLine` 494줄, `People.cpp` 2,699줄(책임 약 20), `World.cpp` 567줄(관심사 넷).
  3. **패널마다 되풀이한 조각**: 같은 꼴이 19가지, 2~6벌씩(§3). 가장 늦게 만든 세 패널(외교·궁정·범죄)이 서로를 베꼈다.
- 조용히 버리는 것 셋: 범죄·월드의 명령 큐가 가득 차면 알림 없이 버린다(R3), 룸 이름 읽기가 한 번 실패하면 영구히 꺼진다(R11).

## 1. 기준선

| 항목 | 값 |
|---|---|
| `tools/build.ps1` | 성공. src 의 오브젝트 46개를 지우고 다시 빌드했다 |
| 컴파일러 경고 | 1 (`src/Ui.cpp:458` C4996 `sscanf`) |
| `tools/test-native.ps1` | 110 passed |
| 작업 트리 | 깨끗 |
| 함수 수(어림) | 596. 100줄 넘는 함수 15, 200줄 넘는 3, 400줄 넘는 2 |
| 치트 표 | 77항목. 확인됨(`Verified`) 27, 확인 전 50 |
| 영역 | 17 |
| 변경 빈도(커밋 수) | `core_tests.cpp` 75, `ModuleMain.cpp` 61(버전 번호), `CheatTable.cpp` 39, `RemoteCommand.cpp` 35, `Remote.cpp` 26, `People.cpp` 26, `Menu.cpp` 19 |

긴 함수(어림. 들여쓰기로 쟀다):

| 줄 수 | 자리 |
|---|---|
| 501 | `src/People.cpp:846` `One` |
| 494 | `src/core/RemoteCommand.cpp:131` `ParseRemoteLine` |
| 315 | `src/core/CheatTable.cpp:76` `Cheats` (표 자체) |
| 180 | `src/People.cpp:2027` `DrawDetail` |
| 136 | `src/Recorder.cpp:95` `Handle` |
| 130 | `src/People.cpp:1509` `BattleTick` |
| 128 | `src/Explorer.cpp:362` `DrawBrowse` |
| 125 | `src/People.cpp:1667` `HoldTick` |
| 117 | `src/Remote.cpp:584` `Execute` |
| 112 | `src/core/Request.cpp:89` `ParseRequest` |
| 107 | `src/Dump.cpp:265` `Run`, `src/Court.cpp:164` `Scan` |
| 102 | `src/Ui.cpp:324` `Frame`, `src/Ui.cpp:221` `Capture`, `src/Diplomacy.cpp:322` `StepJob` |

배선: `ModuleMain.cpp` 는 `Init` 을 모두 끝낸 뒤(145–160) 콜백을 등록한다(162–168). 패널의 틱은 `Menu.cpp:157` `NlMenu::GameTick` 이 돌리고(163–172), 그리기는 `Menu.cpp:196` `NlMenu::Draw` 가 영역마다 부른다.
패널의 인터페이스는 네임스페이스 함수 `Init / GameTick(Now, Visible) / Draw* / Do(Command)` 로 같다.

## 2. 지적

심각도: **높음** = 규칙 위반이거나 사용자가 보는 결함. **중간** = 기능을 더할 때 실제로 걸리는 구조. **낮음** = 그 밖.

### 높음

- **R1. 수 입력 칸에 `EnterReturnsTrue`** — `src/Cheats.cpp:257`(`DrawNumber`), `:302`(`DrawScale`), `src/Explorer.cpp:329`(값 칸), `:548`(잠금 값 칸).
  CLAUDE.md: "수 입력 칸(`InputDouble`·`InputInt`)에 `ImGuiInputTextFlags_EnterReturnsTrue`를 주지 않는다 … Enter 로만 들어가고 칸을 떠나면 친 수가 버려진다." 최소값 칸은 `Economy.cpp:416` 에서 `IsItemDeactivatedAfterEdit`·`IsItemActive` + `NlCore::StepFloorEdit` 로 고쳤는데(research/18) 네 곳이 남았다.
  `InputText` 의 같은 플래그(`Explorer.cpp:337, 381, 495`)는 지원되므로 문제없다. → 네 곳을 최소값 칸과 같은 길로. 코어의 `StepFloorEdit` 를 그대로 쓸 수 있는지 본다.
- **R2. `Game.cpp` 를 거치지 않는 러너 호출** — `src/Remote.cpp:231–245`(`DoCall`: `GetNamedRoutineIndex` + `CallGameScriptEx` 직접), `src/Dump.cpp:358`(`CallGameScriptEx` 직접. 번호 범위 검사도 없다).
  CLAUDE.md: "러너에 닿는 호출은 `src/Game.cpp`를 거친다." `NlGame::CallScript`(`Game.cpp:40–48`)가 같은 일을 하고 `Remote.cpp:329` `DoTreeCall` 은 그것을 쓴다.
  우회한 까닭은 `AurieStatus` 를 돌려받아 오류 이름을 적으려는 것으로 보인다. → `CallScript` 에 상태를 돌려주는 꼴을 더하고 두 곳이 쓴다.
- **R3. 가득 찬 큐를 조용히 버린다** — `src/Crime.cpp:419`(`g_Queue.size() < 8` 이 아니면 아무 일도 없다), `src/World.cpp:459`(`< 4`), `src/Economy.cpp:401`(한도 없음).
  범죄 계획의 "미룬 것"에 적혀 있다. 외교·궁정은 `DiplomacyTally` 로 세고 버린 수를 창에 적는다. → 한도를 상수로 이름 붙이고, 넘치면 창의 결과 줄에 적는다. 패널마다 다른 정책을 하나로.

### 중간

- **R4. `One` 501줄** — `src/People.cpp:846–1346`. `PersonAct` 20 case 의 switch. `Conceive` 1047–1140(약 140줄), `Equip` 약 65줄. 판단은 이미 `core/FamilyPlan`·`PeoplePlan` 에 있다.
  → case 마다 함수로 뽑고 `One` 은 나누기만.
- **R5. `ParseRemoteLine` 494줄** — `src/core/RemoteCommand.cpp:131–624`. 동사 32개의 if 사슬. `person` 약 100줄, `diplomacy` 약 70줄, `court` 약 60줄.
  `src/Remote.cpp:584–700` `Execute` 도 같은 동사의 분기 30개(117줄). 동사를 더하면 두 곳을 찾아 끝을 읽어야 한다.
  → 동사별 파서(`ParsePersonLine` …)를 코어에 두고 `ParseRemoteLine` 은 동사를 읽어 넘기기만. 시험은 그대로 붙는다. `Execute` 는 영역별 묶음으로.
- **R6. `People.cpp` 2,699줄, 책임 약 20** — 현지화 파일 읽기(216–379), 표 읽기(431–695), `One`(846–1346), 실행·집계(1348–1437), 소환(1439–1481),
  전투 가림 `BattleTick`(1483–1638, 156줄), 인구 바퀴 `HoldTick`(1640–1791, 152줄), 그리기 열 가지(1795–2565), 공개 진입점(2217–2699).
  → 경계가 자연스러운 것부터: 전투 가림 → `Battle.cpp`, 인구 바퀴 → 제 파일, 현지화 읽기 → 제 파일(초기화 뒤 읽기 전용 맵), 역할·특성 목록 UI → 제 파일.
- **R7. `World.cpp` 567줄, 관심사 넷** — 이벤트 쿨다운(93–128), 계절(171–304), 광산(307–364), 주교(366–417). 계절만 130줄이 넘고 전역 변수가 여럿.
  → 계절과 광산을 제 파일로 빼면 `World.cpp` 는 이벤트+주교만 남는다.
- **R8. `Production.cpp` 의 일(`Job`) 13개가 생산 밖까지** — `src/Production.cpp:256–270`: 창고·조리법 셋 외에 종교 3, 임신 3, 범죄 4 의 게임 변수. 파일 이름이 책임을 말하지 않는다.
  → 이름을 바꾸거나(게임 변수를 쓰는 일의 묶음), Walk 함수를 영역의 파일로.
- **R9. 궁정이 `DiplomacyTally` 를 쓴다** — `src/Court.cpp:76`, `Court.hpp:7`. 범죄는 쓰지 않고 제 방식. 세 패널의 결과 셈이 둘이다. → `JobTally` 같은 이름으로 코어의 공통 자리에.
- **R10. 경로 따라가기가 둘** — `src/Game.cpp:101–139` `NlGame::Resolve`(열거로, `global.a.b` 만)와 `src/Access.cpp` 의 `Root`+`Step`+`Resolve`(빌트인으로, 모든 뿌리).
  `Game::Resolve` 는 `Dump.cpp:172` 와 `Tweaks.cpp:349` 만 쓴다. 둘이 같은 값을 내는지 보장하는 시험이 없다. `Game.cpp:121–128` 과 `Access.cpp:70–77` 의 열거 람다도 같다.
  → 호출자 둘을 `NlAccess::Read` 로 바꾸고 `Game::Resolve` 를 지운다(Dump 이 Access 에 기대게 된다. 설계에서 정한다).

### 낮음

- **R11.** `src/Game.cpp:86–95` `g_RoomFailed` 가 한 번 실패하면 영구히 빈 글. `src/Game.cpp:66–82` `Objects()` 가 `object_exists` 거짓인 첫 번호에서 멈춘다(빈 번호가 없다는 근거가 주석에 없다).
- **R12.** 뮤텍스가 파일마다 다르다: 있음 9(`Cheats, Court, Crime, Diplomacy, Economy, Explorer, People, Recorder, World`), 없음 4(`Build, Production, Tweaks, Menu`).
  `ModuleMain` 이 `Init` 을 끝낸 뒤 콜백을 등록하고 Present 도 게임 스레드라(CLAUDE.md 실측) 동작의 차이는 없다. 일관성과 주석의 문제다.
- **R13.** `src/Economy.cpp:311` `PlanEconomy(Command, g_Now.Gold, g_Now.Free, g_Now.Free, …)`: `Counts` 자리에 `Free` 를 넘긴다. 의도다(309줄 주석, CLAUDE.md "기준은 예약되지 않은 수"). 인자 이름이 역할을 말하지 않는다.
- **R14.** `src/World.cpp:106, 345` 의 `0x0ffffff` 매직 넘버. `src/Recorder.cpp:23` 에 `k_KindMask` 가 이미 있다.
- **R15.** `src/Ui.cpp:458` `sscanf` C4996(유일한 경고).
- **R16.** `src/Court.cpp:225` `souls[i].m_Object == king.m_Object` 로 같은 구조체인지 본다. 헤더의 공개 필드라 규칙 위반은 아니나 그 뜻은 확인 전(§5).
- **R17.** `src/Crime.cpp:373–407` `DoNow` 가 `Scan` 을 두 번(대상 찾기, 바꾼 뒤 보기).
- **R18.** `src/People.cpp:177, 188` `static const std::string none` 둘. `RValue` 가 아니라 규칙 위반은 아니다.
- **R19. 시험** — `tests/native/core_tests.cpp` 3,478줄 한 파일, 케이스 110, 논리 구역 약 28. `CMakeLists.txt:41` 이 파일 하나만 받는다.
  치트 표 시험(796–985) 190줄이 한 케이스라 깨지면 어느 영역인지 바로 모른다. 3181–3220 은 `NLTOYBOX_TEST_GAME_DIR` 이 없으면 아무 말 없이 돌아온다(SKIP 출력 없음).
  3453–3468 은 `tools/probes/*.txt` 에 기댄다(`test-native.ps1` 이 경로를 준다). `src/Tweaks.cpp:43` `g_Knobs` 와 `src/core/CheatTable.cpp:48` `KnobPlaces` 의 Id 집합이 같은지 보는 시험이 없다.
- **R20. 문서** — `README.md:84` 와 `:99` 에 "종교" 항목이 둘(99 는 이벤트 영역에 있던 주교 기능이 옮겨진 뒤 남은 것으로 보인다). 그 밖은 맞다:
  CLAUDE.md 가 이름으로 가리키는 식별자 79개 모두 코드에 있고, 원격 명령 32개가 코드와 일치하고, 역할 프리셋 13·프리셋 4·영역 17 이 시험의 수와 같다.

## 3. 반복되는 꼴

| 꼴 | 자리 | 벌 | 뽑을 조각 |
|---|---|---|---|
| `ReadText`(주소 → 글) | `Diplomacy.cpp:89`, `Court.cpp:110`, `Crime.cpp:76`, `World.cpp:130` | 4 | `NlAccess::ReadText` |
| `struct Busy` + `g_Busy` | `Diplomacy.cpp:75`, `Court.cpp:86`, `Crime.cpp:59`, `People.cpp:153`, `World.cpp:75`, `Dump.cpp:43` | 6 | `bool&` 를 받는 RAII 가드 하나 |
| `Hint`(흐린 줄바꿈 글) | `Diplomacy.cpp:478`, `Court.cpp:478`, `Crime.cpp:410`, `People.cpp:1827`, `World.cpp:444` | 5 | UI 도우미 |
| `Has`(벡터에 글이 있는가) | `core/FamilyPlan.cpp:9`, `core/RolePlan.cpp:21`, `People.cpp:691` | 3 | `core/Text` |
| uuid 검사(16자리 hex) | `core/DiplomacyPlan.cpp:320` `GoodFactionWho`, `core/CourtPlan.cpp:12` `IsUuid`, `core/FamilyPlan.cpp:95` `GoodUuid` | 3 | `NlCore::GoodUuid` 하나 |
| `StillThere`(그 자리의 uuid 재확인) | `Court.cpp:146`, `Crime.cpp:87`, `People.cpp:589` (시그니처 셋 다 다름) | 3 | `NlAccess::StillThere(base, uuid)` |
| `Base`(인스턴스 주소 글) | `Court.cpp:100`, `Crime.cpp:71`, `People.cpp:383` | 3 | `NlCore::InstancePath` |
| `GoodPath` | `Explorer.cpp:146`, `core/CheatState.cpp:25`, `core/RemoteCommand.cpp:49`(변형) | 3 | `core/AskPath` 의 public |
| ds 훑기 상수 | `Search.cpp:27–28`, `Tweaks.cpp:54–56`, `Finder.cpp:18`, `Access.cpp:18` | 4 | 헤더 하나 |
| 스크립트 이름 → 번호 → 범위(100000, 500000) | `Game.cpp:42–48`, `Recorder.cpp:249–272`, `Remote.cpp:231–239` | 3 | `Game.cpp` 함수 하나 + 상수 |
| 건물 종류 걷기(`k_AllNames`, `k_Generic`) | `Build.cpp:22–24, 49–79`; `Production.cpp:27–28, 81–133` | 2 | `ForEachBuilding` |
| `Place`(장부 자리의 글) | `Build.cpp:88`, `Production.cpp:272` | 2 | `CostBook` |
| `CostBook` 0 쓰기/되돌리기 | `Build.cpp:99–184` `ZeroAll`/`RestoreAll`, `Production.cpp:287–373` `Pass` | 2 | 건설비를 `Production` 의 `Job` 하나로. `Build.cpp` 엔 즉시 업그레이드만 남는다 |
| `DropJobs` | `Diplomacy.cpp:129`, `Court.cpp:153` | 2 | Tally 의 메서드 |
| `Do`/`Queue` 진입점의 뼈대 | `Diplomacy.cpp:563–599`, `Court.cpp:568–604` (`Crime.cpp:537–545` 는 간소) | 2~3 | 공통 뼈대(설계에서) |
| 틱 뼈대 + 명령 큐(Draw 가 Push, 틱이 집기) | `Economy.cpp:69`, `World.cpp:46`, `Crime.cpp:54`, `People.cpp:89`, 외교·궁정의 `g_Jobs` | 6 | 공통 뼈대(설계에서) |
| `Retry` 의 호출 차례 | `Build.cpp:33, 223`, `Economy.cpp:84`, `Production.cpp:247` | 4 | 지금도 코어의 한 종류를 쓴다. 꼴만 같다 |
| `ContainsNoCase` / `Contains` | `Search.cpp:43`, `core/Text.hpp:34` | 2 | `core/Text` 에 대소문자 무시 변형 |
| 인스턴스 수 세기 | `Access.cpp:632–640`, `Dump.cpp:148–155, 186–213` | 3 | `NlAccess::LiveObjects` 재사용 |
| "특성 읽기 → 있는지 → 붙이거나 떼기 → 다시 읽어 확인" | `People.cpp` `TraitAdd` 950–957, `TraitRemove` 1153–1161, `Cure` 936–946, `ApplyRole` 812–839 | 4 | `AttachIfAbsent`/`DetachIfPresent` |
| done/first_failure 집계 | `People.cpp:1388–1425` `Execute`, `Economy`, `Diplomacy` | 3 | 결과 글의 공통 조각 |

## 4. 규칙 준수(지켜진 것)

| 규칙 | 결과 |
|---|---|
| Draw* 에서 러너를 부르지 않는다 | 모든 패널 준수. Draw 는 스냅샷을 그리고 `Push` 만 한다 |
| 정적 저장 기간의 RValue 없음 | 준수(`Ui.cpp:120` 의 `static ImVector` 는 RValue 가 아니다) |
| RValue 를 틱 너머로 들지 않는다 | 준수. 함수 안의 지역 변수뿐이고 "이 함수 안에서만 든다" 주석 관례가 있다 |
| 훅 안에서 빌트인을 부르지 않는다 | 준수(`Recorder.cpp` `Handle`·`Brief`, `SelfSet`) |
| 묻는 파일을 이름을 바꿔 집는다 | 준수(`TakeRemoteRequest`: rename → read → delete, `DropStaleRemoteRequest`) |
| 쓴 뒤 다시 읽어 확인 | 준수(`Access.cpp:309–316, 366–374` 의 `Same()`과 "did not stick") |
| 쓰기 전 uuid 재확인, 죽음은 true/false 만, 사본으로 뜬 뒤 부르기, g_Busy, 보호 특성, 플레이어의 영주에게만 | 준수(`People.cpp:849, 531–548, 1373–1376, 2233, 288, 966/1014/1054`) |
| 행렬에 바로 쓰지 않는다, 붙었는지는 세어 준 수로 | 준수(`Diplomacy.cpp:403–412`, `Court.cpp:389–402`) |
| 읽지 못한 것을 "없다"로 적지 않는다, 깡패는 되돌리지 않는다 | 준수(`Crime.cpp:167–169, 189–191, 226–230, 471`) |
| 치트 표: ds 번호 없음, `global.` 뿌리의 Toggle·Number 없음, 확인 전 Hook·Custom 은 꺼진 채 시작 | 준수(`KeepKnown` 400–425, 시험 1040) |
| 창의 글에 기호 없음 | 준수 |
| 되돌릴 값을 장부가 받은 자리에만, PlaceKey 로 다른 세이브 가림, 로그의 빈도 제한 | 준수 |
| 수 입력 칸에 EnterReturnsTrue 없음 | **위반 4 (R1)** |
| 러너 호출은 `Game.cpp` 를 거친다 | **위반 2 (R2)** |

## 5. 확인하지 못한 것(게임을 켜거나 더 읽어야 안다)

- `Game::Resolve` 와 `NlAccess::Read` 가 같은 `global.a.b` 에서 같은 값을 주는지(R10 의 전제).
- `Objects()` 가 멈추는 첫 빈 번호 뒤에 오브젝트가 없는지(R11).
- `Court.cpp:225` 의 `m_Object` 비교가 "같은 게임 구조체"를 뜻하는지(YYToolkit 헤더에서 본다).
- `One` 의 `Conceive`(1109–1139)에서 아버지 uuid 를 쓴 뒤 특성 붙이기가 실패하고 지우기도 실패했을 때의 게임 쪽 결과.
- `LoadTraitTexts`(People.cpp:289–379)가 특성 282개에 속성 함수를 두 번씩 부르는 시간(게임을 불러올 때 한 번).
- `HoldTick` 의 `Scan`(People.cpp:1701)과 `GameTick` 의 `Scan` 이 같은 틱에 두 번 도는 일이 있는지.
- `Recorder.cpp` `Handle` 의 `memcpy` 가 YYToolkit 의 RValue 배치 변경에 안전한지(서브모듈이 바뀔 때 본다).

## 6. 다음 단계의 입력: 리팩토링 후보의 묶음

동작을 바꾸지 않는 것만. 코어는 시험으로, 러너 쪽은 `check-load.ps1` 과 해당 패널의 원격 명령·화면으로 확인한다.

- **묶음 A — 작고 안전(파일을 나누지 않는다)**: R1, R2, R3, R14, R15, R18, R19 의 SKIP 출력과 Knobs 집합 시험, R20, 그리고 §3 의 작은 조각 추출(`ReadText`, Busy 가드, `Hint`, `Has`, `GoodUuid`, `GoodPath`, ds 상수, 스크립트 찾기 함수, `Place`).
- **묶음 B — 파일 안에서 나눈다**: R4 `One` 분해, R5 `ParseRemoteLine`·`Execute` 분해, R9 Tally 이름, R13 인자 이름, 시험 파일을 코어 헤더 묶음마다 나누기(R19).
- **묶음 C — 파일을 나누거나 합친다(설계가 필요하다)**: R6 `People.cpp` 분할, R7 `World.cpp` 분할, R8 `Production.cpp` 의 책임, R10 `Game::Resolve` 제거, `Build` 의 건설비를 `Production` 의 `Job` 으로, 패널 공통 뼈대(틱·큐·Tally·Do/Queue).

묶음 A 는 bounded 다. B 는 파일을 나누지 않으므로 bounded 에 가깝지만 시험 파일 분할은 CMake 가 바뀐다. C 는 architectural 이다(스펙 → 계획).

## 7. 결과 — 묶음 A (0.27.2, 가지 `chore/refactor-a`, 2026-10-07)

- 한 것: R1(수 입력 칸 넷 → `core/NumberEdit` + `NlUi::InputNumber`), R2(`NlGame::FindScript`·`CallScriptStatus`. 원격 call·덤프·기록기), R3(큐 한도 상수와 넘침 알림. 경제 16 을 새로),
  R14(`NlGame::k_KindMask`·`IsRealNumber`), R15(`sscanf_s`. 경고 0), R18, R19(skip 출력, `KnobDefs` 와 `KnobPlaces` 의 집합 시험), R20, 그리고 §3 의 조각
  `Has`·`IsUuid`·`GoodPath`·`ScopedFlag`·`ReadText`·`Hint`·ds 상수·스크립트 찾기. 배율 7개의 정의는 `core/Knobs` 의 `KnobDefs` 로 옮겼다. 커밋 아홉.
- 코어 시험 110 → 116. 빌드 경고 1 → 0. 동작은 바꾸지 않았다(큐 넘침의 알림만 새 동작).
- 게임 확인(실행 1, 아덴 5일차 아침 세이브): 적재 판정 통과. 네 칸(치트의 수 `build_duration`, 배율 `production_time`, 탐색기의 값 `bet_dummy_value`, 잠금 값 `battle_dodge_shift_worse`)에
  `ui click/type/key`로 수를 쳐 넣고 Tab 과 다른 곳 누르기로 떠나자 들어갔다(상태 파일과 `ask`로 봤다). `call gml_Script_budget_money_get` → 16105, `crime list`, `world season` 그대로.
  큐의 넘침 알림은 창의 단추를 아홉 번 눌러야 해 게임에서는 재지 않았다(코어 시험만).
- 그 실행에서 게임이 켜지다 멈췄고 원인을 찾았다: Aurie 콘솔의 선택(QuickEdit) 모드(`research/06` 의 세 번째). `tools/common.ps1` 의 `Clear-NlConsoleSelect` 와 기다리는 루프 넷에 넣었다.
- 미룬 것(B 로): `Cheats.cpp:430` 의 안내 글 "수는 Enter 로 써 넣습니다"는 이제 틀린 글이다(DLL 이 바뀌므로 B 의 가지에서 고치고 B 의 실행으로 본다). `Explorer.cpp:310` 의 주석도 같다.
- 자리의 교훈: 값을 써 넣으면 결과 줄이 생겨 아래 줄들이 25px 내려간다. `ui type`은 콜론을 받지 않는다(CLAUDE.md 에 적었다).

## 8. 결과 — 묶음 B (0.27.3, 가지 `chore/refactor-b`, 2026-10-07)

- 한 것: R4(`One` 501줄 → `One<할 일>` 열일곱 + 나누기만 하는 `One`), R5(`ParseRemoteLine` 494줄 → `Parse<동사>Line` 스물다섯 + 넘기기만 하는 `ParseRemoteLine`. `Remote.cpp`의 `Execute` 사슬 30분기 → `k_Handlers` 표),
  R9(`DiplomacyTally` → `core/JobTally`. 결과 글자의 뜻 `JobFailed`도 함께), R13(`PlanEconomy`의 `Counts` → `Basis`), R19(시험 한 파일 3,500줄 → `test_<묶음>.cpp` 스물 + `common.hpp` + `main.cpp`),
  A 에서 미룬 안내 글. 커밋 다섯. 본문은 모두 글자 그대로 옮겼다(스크립트로 옮기고 단언으로 자리를 맞췄다).
- 코어 시험 116 passed 그대로(파서·Tally 의 시험이 그대로 붙었다). 빌드 경고 0.
- 게임 확인(실행 2, 같은 세이브): 적재 판정 통과. 원격 동사 `state`·`person list/show`·`crime list`·`diplomacy list`(24왕국)·`court list`(5영주)·`economy gold_add ±1`·`call`·`cheat on/off`·`world season`·`traits`·`records`·`window`·`page`·`shot`,
  `person` 의 길 넷(`skill_add` ±1: 전투 4→5→4, `trait_add`/`trait_remove` coward, `needs_fill`: 욕구 모두 100, `money_add` ±1: 244 그대로). 새 오류 없음.
  안내 글의 화면은 `shot`과 `window close`를 한 묶음에 보내 창이 닫힌 뒤에 찍혀 보지 못했다(글자만 바뀐 것이라 다시 켜지 않았다. 다음 실행에서 본다).
- 켜자마자 Aurie 콘솔이 또 선택 모드였고 `load-save.ps1`의 도우미가 풀었다(켤 때마다 나는 것으로 보인다. `research/06`).
- 남은 것: 묶음 C(§6). 리뷰의 "확인하지 못한 것"(§5)은 그대로다.

## 9. 확인 캠페인의 결과 (2026-10-07. 승인 6번 가운데 3번)

`Verified = false` 50개 가운데 1층(화면·함수)·2층(시간을 흘림)의 항목을 쟀다. 기록은 `research/27-verification.md`.

- 확인으로 올린 것 넷: `hide_gui`, `hide_names`(화면), `build_all`(사용자의 플레이), `no_tree_growth`(나무 관리자의 성장 시계가 선다. 수가 느는 것은 못 봤다 — 메모에 적었다).
- 잰 결과 "효과를 보지 못했다"로 메모를 고친 것: `thug_days`(20 으로 써도 3일째 깡패), `hire_price_factor`(함수가 변수를 안 따름), `rest_decrease`(시간당 감소가 아님), `hide_events`(이야기 창은 그대로),
  `donation_runes`(설교를 직접 걸어도 08:00 에 지워짐), `no_old_age_death`·`safe_childbirth`(대조에서도 죽음이 없어 가릴 수 없음).
- 나머지(`piety_restore`, `production_free`, 전투 스위치, 3층의 디버그 창 들)는 재지 않았다. 다음 캠페인의 첫 할 일은 **설교를 시작시키는 길**(헌금·설교 효과 셋이 그 뒤에 있다)과 **노사가 실제로 나는 조건**이다.
- 도구에 넣은 교훈: 배속은 11:00 쯤 1 로 돌아오고 08:01 에 이야기 창이 멈추므로 흘리는 스크립트는 바라는 배속과 다르면 다시 건다; `ask`의 글 답의 따옴표; 기록은 불러오자마자 건다; 대조 실행이 먼저다.

## 10. 신규 기능 검토의 첫 묶음 (2026-10-07. 0.28.0. `research/28`)

후보 30개(연구 기록의 "만들지 않았다·없다·재지 않았다")를 셋으로 갈라 사용자가 추천 묶음을 골랐다: 바로 만들 수 있는 셋 + 꼴을 기록할 둘.

- 만들어 확인한 것: **역할 프리셋 되돌리기**(`RoleMemory`·`PlanRoleUndo`. 능력치 20·20 → 5·9, 특성 7개 뗌), **왕국 평판 떼기**(`DiplomacyGoal::Clear`. 붙인 3개·2개가 떨어지고, 뗄 것이 없을 때의 호출도 탈이 없었다).
- 만들었지만 확인 전: **깡패 되돌리기**(이 실행에서는 깡패가 생기지 않았다).
- 둘째 묶음(0.29.0. 켜기 3번을 더 승인받아 7·8번째 확인, 9번째 check-load): **깡패 되돌리기 확인**(게임이 만든 깡패에게서), **지금 저장**(`save_game(0, 1)`. 파일이 생겼다), **이벤트 골라 일으키기**(이벤트의 구조체를 감독의 강제 이벤트에 쓴다. 써 둔 손님 이벤트가 그날 왔다).
  원격 `write`가 글(`s:`)도 쓴다. 남은 것: 쿨다운 중인 이벤트도 강제로 오는가, 사람의 저장(퀵세이브)의 꼴, 설교 시작의 길.


## 11. 결과 — 묶음 C (0.29.1, 가지 `chore/refactor-c`, 2026-10-07)

스펙 `docs/superpowers/specs/2026-10-07-refactor-c-design.md`. 동작 불변. 새 파일: `Jobs`, `Buildings`, `Season`, `Mines`, `PeopleAccess`, `TraitText`, `Shield`, `Hold`, `PeopleInternal.hpp`, `PeopleActs`, `PeopleDraw`.
- R6: `People.cpp` 2,795줄 → `People.cpp` 494줄 + 여덟 조각. R7: `World.cpp` 707줄 → `World`·`Season`·`Mines`. R8: `Production.cpp`는 생산의 셋만. R10: `Game::Resolve` 제거. 건설비는 `Jobs`의 일 하나.
- 확인: 코어 시험 118, 안전 29, 파이썬 16+99, `check-load` PASS, 실행 묶음에서 옮긴 조각마다 원격으로 본 것(Task 11 의 표).
