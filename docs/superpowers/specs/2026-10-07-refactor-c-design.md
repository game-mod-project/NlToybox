# 리팩토링 C — 파일 분할과 책임 정리 (설계)

2026-10-07. 2026-10-07 유지보수성 리뷰(`docs/superpowers/reviews/2026-10-07-maintainability-review.md`) §6 의 묶음 C 가운데
사용자가 고른 다섯: R6 `People.cpp` 분할, R7 `World.cpp` 분할, R8 `Production.cpp` 의 책임, R10 `Game::Resolve` 제거, 건설비를 일(`Job`)로.
패널 공통 뼈대(틱·큐·Tally·Do/Queue)는 하지 않는다(사용자가 정했다: 분할하면서 드러나는 중복만 코어 조각으로).

## 1. 목적과 원칙

- 목적: 유지보수성. 한 파일이 한 책임을 말하게 한다. 기능을 더할 때 찾아 읽을 범위를 줄인다.
- **동작을 바꾸지 않는다.** 치트 표, 원격 명령의 꼴과 답, 창의 단추와 글, 로그의 줄, 세이브에 쓰는 것이 같다. 예외는 하나: 건설비 항목 옆의 글이 일의 엔진의 꼴로 통일된다(아래 4.5).
- 나누는 방식은 혼합이다: 경계가 자연스러운 조각은 제 네임스페이스·작은 인터페이스·제 상태를 가진 모듈로 빼고, 남는 `People.cpp` 는 내부 헤더로 상태를 나눠 가진 파일 둘로 나눈다.
- 코어(`src/core`)와 시험은 건드리지 않는다. 러너 쪽 파일만 옮긴다. 새 코어 조각이 필요해지면 그때 시험을 먼저 붙인다.
- 파일은 지금처럼 `src/` 에 평평하게 둔다. `CMakeLists.txt` 의 목록에 더한다.

## 2. 사람(People) 나누기 — R6

`src/People.cpp`(2,795줄) 를 여섯으로.

| 파일 | 네임스페이스 | 옮기는 것 | 인터페이스 |
|---|---|---|---|
| `TraitText.cpp/.hpp` | `NlTraitText` | 현지화·힌트 파일 읽기(`LoadTraitFiles`), 특성의 글(`TraitCaption`·`TraitHint`·`TraitLabel`·`TraitTitled`), 능력치의 화면 이름(`SkillLabel`), 게임의 속성 함수로 글 붙이기(`LoadTraitTexts`), 게임의 특성 이름 목록 읽기(`LoadTraitNames` 의 읽기 부분과 역할 프리셋의 빠진 특성 로그) | `Init(Log, GameDir)`; `LoadForGame()`(게임 스레드. 이름 목록을 읽고 글을 붙인다); `Names()`; `Known(name)`; `Caption(name)`·`Hint(name)`·`Label(name)`·`Titled(name)`; `SkillLabel(i)`; `Notes()`(창에 보일 세 줄: 이름·설명·힌트 파일) |
| `PeopleAccess.cpp/.hpp` | `NlPeopleAccess` | 사람의 자료를 읽고 쓰는 도우미. 상태는 로그 함수뿐: `Base`, `ReadSoul`, `FollowString`·`FollowNumber`, `StillThere`, `ReadTraits`·`ReadNumbers`·`ReadEquipped`, `WriteAt`, `NeedLimit`, `SetNeed`, `Attach`, `Detach`, `CallNoArgs`, `CallNumber` | 같은 이름 그대로. `Init(Log)` |
| `Shield.cpp/.hpp` | `NlShield` | 전투 가림: `BattleTick`, `ShieldOff`, `SideOff`, `g_SideHooks`, 무적·배율의 상태(`g_ShieldOn`·`g_ShieldName`·`g_BattleOn`·`g_NextShield`) | `Init(Log)`; `Tick(Now, InGame, PlayerSouls)`. `PlayerSouls` 는 `std::function<std::vector<const void*>()>`: People 이 그 틱 안에서 `g_Rows` 의 영혼 주소를 모아 준다(항목이 켜져 있고 0.5초가 지났을 때만 부른다). 주소는 틱 너머로 들지 않는다 |
| `Hold.cpp/.hpp` | `NlHold` | 인구 바퀴: `HoldTick`, `HoldNotes`·`ClearHoldNotes`, `HoldRound` 와 바퀴의 상태(`g_HoldPeople`, `g_Next*`, `g_HappyRound`, `g_HoldNoted`, 로그 셈) | `Init(Log)`; `Tick(Now, InGame, Rows)`. `Rows` 는 `std::function<bool(std::vector<PersonRow>&, std::string&)>`(People 의 `Scan` 을 쓴다). 쓰기는 `NlPeopleAccess::SetNeed` 들로 |
| `PeopleActs.cpp` | `NlPeople::Detail` | `One*` 스물한 개, `One`, `Execute`, `Run`, `WhoText`, `RunRoleSteps`·`ApplyRole`·`OneRoleUndo`, `SpawnSoldiersNow`·`SpawnHereNow` | 내부 헤더 `PeopleInternal.hpp` |
| `PeopleDraw.cpp` | `NlPeople::Detail` | `Push`, `KindText`·`NumberText`, `NotReady`, `DrawLast`, `DrawSpawnHere`, `TraitTooltip`, `DrawRole`, `DrawFamily`, `DrawDetail`, `DrawWho`, `DetailPending`, `CountPlayers` | 내부 헤더 `PeopleInternal.hpp` |

- `People.cpp` 에 남는 것: 상태의 정의(`Detail` 의 전역), `LoadTables`, `Scan`, `FindRow`, `ReadDetail`·`RefreshDetail`, `KnowledgeIndex`, `GameTick`(차례: 표 읽기 → `TraitText::LoadForGame` → 큐의 명령 → `Shield::Tick` → `Hold::Tick` → 상세 읽기), 공개 진입점(`Init`, `Draw*`, `SpawnHere`·`SpawnSoldiers`, `Do`, `List`, `Show`, `Traits`, `TraitName`, `Rows`). 700줄 안팎.
- `PeopleInternal.hpp`: `namespace NlPeople::Detail` 에 `extern` 으로 공유 상태(뮤텍스 `g_Mutex`, `g_Log`, `g_Now`, `g_Queue`, `g_Selected`, `g_Names`, 창의 입력 칸들, `g_RoleMemory`·`g_RoleLast`·`g_RoleLastFor`·`g_RolePick`, `g_FatherPick`, `g_BulkBirthArmed`·`g_BulkKnowledgeArmed`, `g_SpawnQueued`·`g_SpawnKinds`, `g_Busy`, `g_AliveLogged`)와 People.cpp 의 함수(`Log`, `Scan`, `FindRow`, `ReadDetail`, `KnowledgeIndex`, `RefreshDetail`)를 선언한다. 익명 네임스페이스는 `Detail` 로 이름을 받는다. 정의는 `People.cpp` 하나에 둔다.
- 뮤텍스: People 의 하나(`Detail::g_Mutex`)가 People·PeopleActs·PeopleDraw 를 지킨다. `Shield`·`Hold` 는 People 의 틱 안에서만 불리므로 그 잠금 아래에서 돈다(제 뮤텍스를 두지 않는다. 헤더에 그렇게 적는다). `TraitText` 는 그리기(툴팁, 특성 목록)에서도 읽으므로 제 뮤텍스를 둔다(재귀. Present 가 게임 스레드라 겹치지는 않는다).
- 공개 API(`NlPeople::Rows`·`Do`·`Show`·`List`·`Traits`·`TraitName`)와 그것을 쓰는 범죄·궁정·원격은 바뀌지 않는다. `NlPeople::TraitName` 은 `NlTraitText::Label` 로 넘긴다.

## 3. 월드(World) 나누기 — R7

`src/World.cpp`(707줄) 를 셋으로.

| 파일 | 네임스페이스 | 옮기는 것 | 인터페이스 |
|---|---|---|---|
| `Season.cpp/.hpp` | `NlSeason` | 계절 읽기(`ReadSeason`·`SeasonText`·`PlaceOf`), 미루기·끝내기(`ChangeSeason`), 붙들기(`HoldTick`, `SeasonHold`, `g_HoldWrites`, `g_HoldWasOn`), 가혹한 계절의 화면 이름(`g_SeasonCaptions`, `main.csv` 읽기), 월드 패널의 계절 블록(`DrawWorld` 의 계절 부분, `g_SeasonLast`) | `Init(Log, GameDir)`; `Tick(Now, Visible)`(1초마다 붙들기, 패널이 보이면 읽기); `Draw()`; `Do(WorldAct)`(`SeasonShow`·`SeasonDelay`·`SeasonEnd` 만. 그 밖은 빈 글) |
| `Mines.cpp/.hpp` | `NlMines` | 광산 매장량 붙들기(`MineTick`, `StockBook g_Mines`, `g_MineWrites`, `g_MineWasOn`) | `Init(Log)`; `Tick(Now)` |
| `World.cpp` | `NlWorld` | 남는 것: 이벤트 쿨다운 지우기·골라 일으키기·이름 읽기, 주교 부르기, 지금 저장, 이벤트·종교·유틸 패널, 큐와 `Do`·`ForceEvent` | 지금 그대로. `Do` 와 틱의 큐는 계절의 일 셋을 `NlSeason::Do` 에 넘긴다. `DrawWorld` 는 `NlSeason::Draw` 를 부른다(월드 패널의 글은 같다) |

- `WorldAct`, `core/WorldPlan`, `core/SeasonPlan`, 원격 `world season|season_delay|season_end` 는 그대로. 결과의 글(`g_SeasonLast`)은 `Season` 이 들고 제 `Draw` 에 보인다.
- 틱: `Menu.cpp` 가 `NlWorld::GameTick` 뒤에 `NlSeason::Tick(now, visible && page == Area::World)` 와 `NlMines::Tick(now)` 를 부른다(지금은 `NlWorld::GameTick` 이 안에서 셋을 돌린다. 차례는 같다: 큐 → 계절 붙들기 → 광산 → 계절 읽기).
- 뮤텍스: 파일마다 제 것 하나. 셋은 서로를 부르지 않는다. `g_Busy` 는 `World` 와 `Season` 에 둔다(둘 다 게임의 함수를 부른다: World 는 주교·저장·이벤트, Season 은 계절의 읽기 함수 셋 `is_extreme`·`__get_remain_time_of_current_phase`·`get_remain_time_to_extreme_season`). 광산만 읽고 쓰기만 한다. (처음에는 "World 에만"이라고 적었다가 최종 리뷰에서 고쳤다.)

## 4. 자료를 돌며 쓰는 일의 엔진과 건설비 — R8, 건설비

### 4.1 `Jobs.cpp/.hpp` (`NlJobs`)

`Production.cpp` 의 엔진을 그대로 뺀다: `Visit`(콜백 형. 담는 것, 걸음, 열쇠, 등급, 칸, 값), `JobDef`(치트 Id, 로그 이름, 0 쓰기인가, 걷는 함수, 뒤처리, 주기), `Job`(`JobDef` + 장부 `CostBook`, `Retry`, `Target`, `Misses`, `NextPass`, `Settled`, `Announced`, `Note`, `Logged`), `Pass`, `Tick`, `LogOnce`, `Place`, 그리고 게임 변수를 걷는 공용 `WalkVars(Visit, 열쇠들, Why)`.

- 인터페이스: `Init(Log)`; `Add(const JobDef&)`(영역 파일이 제 `Init` 에서 등록. 같은 치트 Id 를 두 번 등록하면 로그에 적고 무시); `GameTick(Now)`(1초마다, 등록된 차례대로 `Tick`).
- 판단은 지금처럼 코어의 `PlanValue`·`CostBook`·`Retry`. 엔진의 코드는 옮기기만 한다(이름만 `NlJobs`).

### 4.2 등록하는 쪽

| 파일 | 등록하는 일(치트 Id) | 걷는 함수 |
|---|---|---|
| `Production.cpp` | `storage_capacity`(뒤처리 캐시 비우기), `production_amount`, `production_free` | `WalkCapacity`, `WalkAmounts`·`WalkInputs`(`WalkRecipes`) — 지금 그대로 |
| `World.cpp` | `religion_free`, `piety_restore`, `preach_conversion` | `WalkReligionCosts`(설교 비용 + `WalkVars`), `WalkVars`(열쇠는 `core/WorldPlan`) |
| `People.cpp` | `pregnancy_chance`, `no_miscarriage`, `safe_childbirth` | `WalkVars`(열쇠는 `core/FamilyPlan`) |
| `Crime.cpp` | `no_bandit_turn`, `crime_minds_off`, `thug_days`, `theft_none` | `WalkVars`(열쇠는 `core/CrimePlan`) |
| `Build.cpp` | `build_free` | `WalkCosts`(아래 4.5) |

등록은 `ModuleMain` 의 `Init` 차례(지금의 `g_Jobs` 배열의 차례와 같게: 생산 → 종교 → 임신 → 범죄 → 건설비)로 하여 틱의 차례가 지금과 같게 한다. `NlProduction::GameTick` 은 없어지고 `Menu.cpp` 의 그 자리에서 `NlJobs::GameTick` 을 부른다.

### 4.3 `Buildings.cpp/.hpp` (`NlBuildings`)

건물 종류 걷기를 하나로: `ForEachType(const std::function<bool(const std::string& Name, const RValue& Generic)>&, std::string& Why)`. 이름의 배열은 `gml_Script_building_generic_get_array_of_all_buildings()`, 구조체는 `gml_Script_get_generic_building(이름)`(지금 `Build.cpp` 와 `Production.cpp` 가 각자 든 `k_AllNames`·`k_Generic` 과 걷는 코드). 둘 다 이것을 쓴다.

### 4.4 `Production.cpp` 에 남는 것

창고 용량의 걷기와 캐시 비우기, 조리법의 걷기, 그리고 세 일의 등록. `NlProduction::Init(Log)` 만 공개한다. 파일 이름이 책임(생산)을 말하게 된다.

### 4.5 건설비를 일로 (`Build.cpp`)

- `WalkCosts(Visit, Why)`: `NlBuildings::ForEachType` 으로 건물 종류마다 `__construction_cost.levels[등급]` 의 `money` 와 `resources.__array_of_resource_quantity[39]` 를 `Visit(담는 것, 걸음, 건물 이름, 등급, 칸, 값)` 으로 넘긴다(칸: 금화는 -1, 자원은 번호. 지금 `ForEachCost`·`Quantities` 가 하는 일).
- 일: `{ "build_free", "construction costs", Zero = true, &WalkCosts, nullptr, 주기 }`. 0 쓰기·장부·되돌리기·"게임이 되돌리면 다시 쓰기"·실패 때 간격 늘리기는 엔진이 한다. 지금 `Build.cpp` 의 `ZeroAll`·`RestoreAll`·`StillZero`·`TickCosts` 와 그 상태(`g_Book`, `g_Applied`, `g_Wanted`, `g_Retry`, `g_Note`)는 지운다.
- 주기: 엔진의 `Period` 는 "다 쓴 뒤 다시 훑는 간격"이다. 지금 `Build` 는 `Retry`(2초부터 두 배씩 60초까지)가 지나면 `StillZero` 로 다시 본다. 같은 뜻이 되게 주기를 2초로 둔다(엔진은 `Settled` 뒤 `Period` 마다 훑어 바뀐 것만 쓴다. 쓸 것이 없으면 쓰지 않는다).
- 바뀌는 것: 항목 옆의 글이 엔진의 꼴("N칸을 0 으로 썼습니다", "하지 못했습니다: …")이 된다. 로그의 줄도 엔진의 꼴이다. 동작(0 을 쓰는 자리, 되돌리기, 세이브에 남지 않음)은 같다.
- `Build.cpp` 에 남는 것: 즉시 업그레이드(`FinishUpgrades`, `TickUpgrades`)와 그 틱.

## 5. `Game::Resolve` 제거 — R10

- `Tweaks.cpp` 의 `NlGame::Resolve("global.__gameplay_vars", vars)` → `NlAccess::Read(NlCore::ParseAskPath("global.__gameplay_vars"), vars, why)`. 못 읽으면 지금처럼 `vars` 가 undefined 로 남아 그 항목이 "못 찾음"이 된다.
- `Dump.cpp` 의 감시 주소 읽기 → 같은 꼴. 두 파일이 `Access.hpp` 를 포함한다.
- `Game.hpp/.cpp` 에서 `Resolve` 와 그것만 쓰던 전역 멤버 열거 조각을 지운다. `Access.cpp` 의 길(빌트인으로, 모든 뿌리)이 유일한 경로 따라가기가 된다.

## 6. 틱의 차례와 등록

`Menu.cpp` 의 틱: `NlBuild::GameTick`(즉시 업그레이드만) → `NlJobs::GameTick`(모든 일) → `NlWorld::GameTick` → `NlSeason::Tick` → `NlMines::Tick` → (그 밖은 지금처럼) → `NlPeople::GameTick`(안에서 `TraitText`·`Shield`·`Hold`).
`ModuleMain` 의 `Init` 차례: `NlJobs::Init` 을 먼저, 그다음 등록하는 영역들(`Production` → `World` → `People` → `Crime` → `Build`), 그리고 `TraitText`·`PeopleAccess`·`Shield`·`Hold`·`Season`·`Mines`·`Buildings` 의 `Init`.

## 7. 확인

- 코어 시험 118 그대로 통과, 빌드 경고 0, `tools/tests/safety.tests.ps1`, 파이썬 시험.
- 게임 2번(사용자가 승인한 것): (1) `tools/check-load.ps1`, (2) 실행 묶음 하나에서 옮긴 조각마다 원격으로 본다 —
  `person show`·`person <uuid> role name=general`·`role_undo`·`traits find=`(사람·글), `cheat ally_power on` 뒤 `records`(가림), `cheat needs_all on` 의 메모(바퀴),
  `world season`·`cheat season_hold on`·`cheat mine_stock_hold on`(계절·광산), `cheat build_free on` 뒤 건설비 읽기·끄면 되돌아옴, `cheat production_free on`, `cheat religion_free on`, `cheat no_miscarriage on`,
  `cheat thug_days on` 뒤 `ask global.__gameplay_vars.dummy_criminal_days_to_thug`(2 → 20 → 끄면 2), 그리고 게임의 오류 파일의 끝.
- 보고서에 §11 "묶음 C 의 결과"를 적는다. 버전 0.29.1. 가지 `chore/refactor-c`.

## 8. 하지 않는 것

- 패널 공통 뼈대(틱·큐·Tally·Do/Queue). 범죄·궁정·외교의 `StillThere`·`Base` 중복도 이번에는 두 곳(People 의 것만 `PeopleAccess` 로) — 다른 패널이 그것을 쓰게 바꾸는 것은 뼈대와 함께 다음에.
- 코어의 새 조각. 시험의 변경.
- 글(창의 단추, 힌트, 결과 줄)의 변경 — 4.5 의 건설비 글만 예외.
