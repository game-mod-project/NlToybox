# 리팩토링 C 구현 계획 (파일 분할과 책임 정리)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `People.cpp`·`World.cpp`·`Production.cpp`·`Build.cpp` 를 책임 하나씩의 파일로 나누고 `Game::Resolve` 를 지운다. 동작은 바꾸지 않는다.

**Architecture:** 경계가 자연스러운 조각(자료를 돌며 쓰는 일의 엔진 `Jobs`, 건물 종류 걷기 `Buildings`, 계절 `Season`, 광산 `Mines`, 특성의 글 `TraitText`, 사람의 자료 읽기·쓰기 `PeopleAccess`, 전투 가림 `Shield`, 인구 바퀴 `Hold`)은 제 네임스페이스·작은 인터페이스·제 상태를 가진 모듈로 뺀다. 남는 `People.cpp` 는 내부 헤더 `PeopleInternal.hpp` 로 상태를 나눠 가진 `PeopleActs.cpp`·`PeopleDraw.cpp` 와 셋이 된다. 코어(`src/core`)와 시험은 건드리지 않는다.

**Tech Stack:** C++20, Aurie + YYToolkit(러너 호출은 `src/Game.cpp`·`src/Access.cpp` 를 거친다), Dear ImGui, CMake(`tools/build.ps1`), 코어 시험 `tools/test-native.ps1`.

**Spec:** `docs/superpowers/specs/2026-10-07-refactor-c-design.md`

## Global Constraints

- 동작 불변: 치트 표, 원격 명령의 꼴과 답, 창의 단추·글, 로그의 줄, 세이브에 쓰는 것이 같다. 예외는 건설비 항목 옆의 글(엔진의 꼴로 통일. 스펙 4.5).
- 코어(`src/core/*`)와 `tests/native/*` 는 건드리지 않는다. 새 코어 조각을 만들지 않는다.
- 파일은 `src/` 에 평평하게. 새 파일마다 `CMakeLists.txt` 의 `add_library(nltoybox SHARED …)` 목록에 더한다.
- 러너를 건드리는 호출은 `Game.cpp`·`Access.cpp` 를 거친다. 정적 저장 기간의 `RValue` 를 두지 않는다(함수 안에서만). 그리는 쪽(`Draw*`)에서 러너를 부르지 않는다.
- 빌드 경고 0(`warning C` 줄이 없어야 한다). 코어 시험 118 그대로 통과.
- 이 계획에는 새 시험이 없다(동작 불변의 이동). 각 Task 의 문은 **빌드 성공 + 경고 0 + 코어 시험 118 통과**이고, 마지막 Task 에서 게임을 켜(승인 2번) 옮긴 조각마다 본다.
- 커밋 메시지는 한국어, 작성자 이메일은 이 레포의 noreply 설정 그대로. `refs/`·`backups/`·`downloads/`·`build/` 를 커밋하지 않는다.
- 각 Task 는 `E:\NlToyBox` 에서 절대경로로 작업한다(`git -C E:\NlToyBox …`, `pwsh -File E:\NlToyBox\tools\build.ps1`).

## Review Focus

- 틱의 차례가 바뀌면 안 된다: 건설비·생산·게임 변수의 일은 지금처럼 `Build`(즉시 업그레이드) 뒤, `World` 앞에 돌아야 한다(Task 1·3·4 의 `Menu.cpp`·`ModuleMain.cpp` 차례).
- 공유 상태가 두 정의가 되면 안 된다: `PeopleInternal.hpp` 의 `extern` 과 `People.cpp` 의 정의가 하나씩 짝이어야 한다(Task 8 의 링크 오류가 문). 
- 뮤텍스가 둘로 갈리면 안 된다: `Shield`·`Hold` 는 `People` 의 틱 안에서만 불려 People 의 잠금 아래에서 돈다(Task 7). `TraitText` 만 제 뮤텍스.
- 사람이 드나들어 번호가 밀린 뒤에도 `StillThere` 로 다시 보는 길이 그대로여야 한다(`PeopleAccess::StillThere` 로 옮긴 뒤 모든 호출자가 그것을 쓴다. Task 6).
- 끌 때 되돌리는 길이 그대로여야 한다: 건설비는 엔진의 장부로 되돌리고(Task 3), 계절·광산 붙들기는 끈 틱에 한 번 정리한다(Task 5 의 `g_HoldWasOn`·`g_MineWasOn` 이 각 파일 안에 남는다).

---

## 파일 지도

| 파일 | 책임 | 만든다/고친다 |
|---|---|---|
| `src/Jobs.hpp/.cpp` | 자료를 돌며 값을 쓰는 일의 엔진(`Visit`, `JobDef`, `Add`, `GameTick`, `WalkVars`) | 만든다 (Task 1) |
| `src/Production.cpp/.hpp` | 창고 용량·조리법 걷기와 세 일의 등록 | 고친다 (Task 1, 2, 4) |
| `src/Buildings.hpp/.cpp` | 건물 종류 걷기(`ForEachType`) | 만든다 (Task 2) |
| `src/Build.cpp/.hpp` | 건설비의 걷기(`WalkCosts`)와 일의 등록, 즉시 업그레이드 | 고친다 (Task 2, 3) |
| `src/World.cpp/.hpp` | 이벤트·주교·지금 저장·종교 일 셋의 등록 | 고친다 (Task 4, 5) |
| `src/Crime.cpp` | 범죄 일 넷의 등록 | 고친다 (Task 4) |
| `src/Season.hpp/.cpp` | 계절 읽기·미루기·끝내기·붙들기·그리기 | 만든다 (Task 5) |
| `src/Mines.hpp/.cpp` | 광산 매장량 붙들기 | 만든다 (Task 5) |
| `src/PeopleAccess.hpp/.cpp` | 사람의 자료 읽기·쓰기 도우미 | 만든다 (Task 6) |
| `src/TraitText.hpp/.cpp` | 특성의 글, 능력치 이름, 게임의 특성 이름 목록 | 만든다 (Task 6) |
| `src/Shield.hpp/.cpp` | 전투 가림 | 만든다 (Task 7) |
| `src/Hold.hpp/.cpp` | 인구 바퀴 | 만든다 (Task 7) |
| `src/PeopleInternal.hpp` | People 셋이 나눠 갖는 상태와 함수의 선언 | 만든다 (Task 8) |
| `src/PeopleActs.cpp` | 한 사람에게 하는 일(`One*`), 실행·집계, 역할, 소환 | 만든다 (Task 8) |
| `src/PeopleDraw.cpp` | 그리기 | 만든다 (Task 8) |
| `src/People.cpp/.hpp` | 상태의 정의, 표 읽기, 훑기, 틱, 공개 진입점, 임신 일 셋의 등록 | 고친다 (Task 4, 6, 7, 8) |
| `src/Game.cpp/.hpp`, `src/Tweaks.cpp`, `src/Dump.cpp` | `Resolve` 제거 | 고친다 (Task 9) |
| `src/Menu.cpp`, `src/ModuleMain.cpp`, `CMakeLists.txt` | 틱의 차례, `Init` 의 차례, 파일 목록 | 고친다 (Task 1, 3, 4, 5, 6, 7, 8) |

줄 번호는 가지 `chore/refactor-c` 의 커밋 `a259348` 기준이다. 앞의 Task 가 파일을 고치면 밀리므로 **함수의 이름으로 찾는다**(번호는 길잡이).

빌드와 시험(모든 Task 의 문):

```bash
pwsh -File E:\NlToyBox\tools\build.ps1          # 기대: 마지막 줄 "build ok -> E:\NlToyBox\build\NlToyBox.dll", "warning C" 줄 없음
pwsh -File E:\NlToyBox\tools\test-native.ps1    # 기대: "core tests: 118 passed"
```

---

### Task 1: 일의 엔진 `Jobs` 를 `Production.cpp` 에서 뺀다

**Files:**
- Create: `src/Jobs.hpp`, `src/Jobs.cpp`
- Modify: `src/Production.cpp`(엔진 부분을 지우고 등록만), `src/Production.hpp`, `src/Menu.cpp:167`, `src/ModuleMain.cpp:157-158`, `CMakeLists.txt:80`

**Interfaces:**
- Produces: `NlJobs::Visit`, `NlJobs::WalkFn`, `NlJobs::JobDef{Cheat, What, Zero, Walk, After, Period}`, `NlJobs::Init(LogFn)`, `NlJobs::Add(const JobDef&)`, `NlJobs::GameTick(double Now)`, `NlJobs::WalkVars(const Visit&, const std::vector<const char*>& Keys, std::string& Why)`, `NlJobs::Log(const std::string&)`.

- [ ] **Step 1: `src/Jobs.hpp` 를 만든다**

```cpp
#pragma once
// 게임의 자료를 돌며 값을 쓰는 일(Job)의 엔진. 치트 표의 Custom(0 쓰기)·CustomScale(배율) 항목이 등록한다(영역 파일이 제 Init 에서 Add).
// 처음 본 값을 장부(core/CostBook)에 적고 끄면 그 값으로 되돌린다. 쓴 뒤에는 다시 읽어 남았는지 본다. 한 자리에 쓸 값은 core/PlanValue 가 정한다.
// 잰 것은 research/10(창고 용량, 조리법), 21(종교), 24(임신), 26(범죄), 09(건설비). 2026-10-07 리팩토링 C 에서 src/Production.cpp 의 엔진을 옮겼다.

#include "core/AskPath.hpp"

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>
#include <vector>

namespace NlJobs
{
	using LogFn = std::function<void(const std::string&)>;

	// 대상 하나: In 안의 Step 자리에 수가 있다. Key·Level·Slot 은 장부에서 그 자리를 가리는 이름이다.
	using Visit = std::function<void(const YYTK::RValue& In, const NlCore::PathStep& Step, const std::string& Key, int Level, int Slot, const YYTK::RValue& Value)>;
	// 걷는 함수: 대상마다 Visit 을 부른다. 자료를 열지 못하면 거짓이고 Why 에 까닭.
	using WalkFn = bool (*)(const Visit& V, std::string& Why);

	struct JobDef
	{
		const char* Cheat;			// 치트 표의 Id
		const char* What;			// 로그에 쓸 이름
		bool Zero;					// 켜면 0 을 쓴다(배율이 없다). 아니면 창에서 정한 배율을 곱한다
		WalkFn Walk;
		void (*After)();			// 값을 쓴 뒤에 부른다(없으면 nullptr)
		double Period;				// 다 쓴 뒤 다시 훑는 간격(초). 게임이 자료를 다시 만들면 그때 다시 쓴다
	};

	void Init(LogFn Log);
	// 일을 등록한다(ModuleInitialize 의 Init 들에서. 틱의 차례는 등록한 차례다). 같은 Cheat 를 두 번 등록하면 로그에 적고 무시한다.
	void Add(const JobDef& Def);
	// 게임 스레드의 틱. Now: 모듈이 뜬 뒤의 초(1초에 한 번 모든 일을 본다).
	void GameTick(double Now);

	// 게임 변수(global.__gameplay_vars)의 열쇠들을 넘긴다. Key 는 그 열쇠다. 없는 열쇠는 건너뛴다(한 번 로그). 영역의 걷는 함수가 쓴다.
	bool WalkVars(const Visit& V, const std::vector<const char*>& Keys, std::string& Why);
	// 영역의 걷는 함수가 로그를 남길 때.
	void Log(const std::string& Line);
}
```

- [ ] **Step 2: `src/Jobs.cpp` 를 만든다** — `src/Production.cpp` 에서 아래를 **잘라** 옮긴다(복사가 아니다. 옮긴 뒤 Production.cpp 에는 남지 않는다).

옮기는 것(Production.cpp 의 이름으로): `k_GameplayVars`, `g_Log`·`Log`, `g_Next`, `g_VarsMissingLogged`, `Visit` 의 using(헤더로 갔으니 지운다), `WalkVars`(136–169), `struct Job`(237–255), `Place`(272–276), `LogOnce`(278–285), `Pass`(287–373), `Tick`(375–400), `NlProduction::GameTick` 의 몸(411–418)은 `NlJobs::GameTick` 이 된다.

파일의 뼈대:

```cpp
#include "Jobs.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "core/CostBook.hpp"
#include "core/Knobs.hpp"
#include "core/Retry.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <vector>

using namespace YYTK;
using NlCore::PathStep;
using NlCore::Shortest;
using NlJobs::Visit;

namespace
{
	constexpr const char* k_GameplayVars = "global.__gameplay_vars";

	NlJobs::LogFn g_Log;
	double g_Next = 0;
	bool g_VarsMissingLogged = false;		// 같은 줄을 주기마다 적지 않는다

	struct Job
	{
		NlJobs::JobDef Def;
		NlCore::CostBook Book;		// 처음 본 값. 비어 있지 않으면 되돌릴 것이 남아 있다
		NlCore::Retry Retry{ 2, 60 };
		double Target = 1;			// 바라는 배율. 1 은 원래 값, 0 은 "0 으로 쓴다"
		int Misses = 0;				// 되돌릴 자리를 잇달아 찾지 못한 횟수
		double NextPass = 0;
		bool Settled = false;		// 바라는 배율이 다 써졌다
		bool Announced = false;
		std::string Note, Logged;
	};
	std::vector<Job> g_Jobs;		// 등록한 차례대로(Init 에서만 더한다. 틱은 보기만 한다)

	// … Place, LogOnce, Pass, Tick 을 Production.cpp 에서 그대로. 안에서 J.Cheat → J.Def.Cheat, J.What → J.Def.What, J.Zero → J.Def.Zero,
	//     J.Walk → J.Def.Walk, J.After → J.Def.After, J.Period → J.Def.Period 로 바꾼다. Log(…) 는 이 파일의 Log 다.
}

void NlJobs::Log(const std::string& Line)
{
	if (g_Log)
		g_Log(Line);
}

bool NlJobs::WalkVars(const Visit& V, const std::vector<const char*>& Keys, std::string& Why)
{
	// Production.cpp 의 WalkVars 몸 그대로(k_GameplayVars, g_VarsMissingLogged 를 쓴다)
}

void NlJobs::Init(LogFn Log_)
{
	g_Log = std::move(Log_);
}

void NlJobs::Add(const JobDef& Def)
{
	for (const Job& job : g_Jobs)
		if (std::string(job.Def.Cheat) == Def.Cheat)
		{
			Log(std::string("jobs: ") + Def.Cheat + " is already registered; ignoring the second");
			return;
		}
	Job job;
	job.Def = Def;
	g_Jobs.push_back(std::move(job));
}

void NlJobs::GameTick(double Now)
{
	if (Now < g_Next)
		return;
	g_Next = Now + 1.0;
	for (Job& job : g_Jobs)
		Tick(job, Now);
}
```

익명 네임스페이스 안의 `Log` 는 `NlJobs::Log` 를 부르는 한 줄로 두거나, 익명의 것을 지우고 `NlJobs::Log` 를 쓴다(둘 가운데 하나. 이름이 겹치지 않게).

- [ ] **Step 3: `src/Production.cpp` 를 등록만 하게 고친다**

남기는 것: `k_AllNames`, `k_Generic`, `k_Warehouses`, `k_Preaches`, `k_CleanCache`, `g_Log`·`Log`, `g_PreachSkippedLogged`, `WalkCapacity`, `WalkRecipes`, `WalkReligionCosts`(`WalkVars` 호출은 `NlJobs::WalkVars` 로), `WalkPreachFactor` … `WalkTheftAmount`(한 줄짜리 열 개. `WalkVars` → `NlJobs::WalkVars`), `WalkAmounts`, `WalkInputs`, `CleanCapacityCache`. 지우는 것: Step 2 에서 옮긴 것 전부와 `g_Next`, `NlProduction::GameTick`.
`Visit` 는 `using NlJobs::Visit;` 로 받는다. 포함에 `#include "Jobs.hpp"` 를 더한다(`core/CostBook.hpp`·`core/Retry.hpp` 는 더 쓰지 않으면 뺀다).

`Init` 은 열셋을 **지금의 배열 차례 그대로** 등록한다:

```cpp
void NlProduction::Init(LogFn Log_)
{
	g_Log = std::move(Log_);
	// 틱의 차례는 등록한 차례다(앞의 g_Jobs 배열과 같다). 종교·임신·범죄·건설비의 일은 Task 4 에서 제 영역의 파일로 간다.
	NlJobs::Add({ "storage_capacity", "capacity", false, &WalkCapacity, &CleanCapacityCache, 3 });
	NlJobs::Add({ "production_amount", "production amount", false, &WalkAmounts, nullptr, 15 });
	NlJobs::Add({ "production_free", "production inputs", true, &WalkInputs, nullptr, 15 });
	NlJobs::Add({ "religion_free", "religion costs", true, &WalkReligionCosts, nullptr, 15 });
	NlJobs::Add({ "piety_restore", "piety restore", false, &WalkPietyRestore, nullptr, 15 });
	NlJobs::Add({ "preach_conversion", "preach conversion", false, &WalkPreachFactor, nullptr, 15 });
	NlJobs::Add({ "pregnancy_chance", "pregnancy chance", false, &WalkPregnancyChance, nullptr, 15 });
	NlJobs::Add({ "no_miscarriage", "miscarriage chance", true, &WalkMiscarriage, nullptr, 15 });
	NlJobs::Add({ "safe_childbirth", "childbirth death chance", true, &WalkChildbirthDeath, nullptr, 15 });
	NlJobs::Add({ "no_bandit_turn", "bandit turn chance", true, &WalkBanditTurn, nullptr, 15 });
	NlJobs::Add({ "crime_minds_off", "crime minds", true, &WalkCrimeMinds, nullptr, 15 });
	NlJobs::Add({ "thug_days", "days to thug", false, &WalkThugDays, nullptr, 15 });
	NlJobs::Add({ "theft_none", "storage theft amount", true, &WalkTheftAmount, nullptr, 15 });
}
```

`src/Production.hpp`: `GameTick` 선언을 지우고 머리의 설명을 "엔진은 src/Jobs 에 있다. 여기는 걷는 함수와 등록" 으로 고친다.

- [ ] **Step 4: `Menu.cpp`·`ModuleMain.cpp`·`CMakeLists.txt`**

`src/Menu.cpp:167` 의 `NlProduction::GameTick(now);` → `NlJobs::GameTick(now);` (`#include "Jobs.hpp"` 를 더한다. 자리는 그대로: `NlBuild::GameTick` 뒤, `NlWorld::GameTick` 앞).
`src/ModuleMain.cpp`: `NlProduction::Init(…)` 줄(158) **앞**에 `NlJobs::Init([](const std::string& Line) { LogLine(Line); });` 를 더한다(`#include "Jobs.hpp"`).
`CMakeLists.txt`: `src/Production.cpp` 줄 뒤에 `  src/Jobs.cpp` 를 더한다.

- [ ] **Step 5: 빌드와 시험**

Run: 위의 빌드·시험 명령. 기대: `build ok`, `warning C` 없음, `core tests: 118 passed`.

- [ ] **Step 6: 커밋**

```bash
git -C E:\NlToyBox add src/Jobs.hpp src/Jobs.cpp src/Production.cpp src/Production.hpp src/Menu.cpp src/ModuleMain.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "refactor(jobs): 자료를 돌며 쓰는 일의 엔진을 src/Jobs 로 뺀다. Production 은 걷는 함수와 등록만 (리팩토링 C, Task 1)"
```

---

### Task 2: 건물 종류 걷기 `Buildings` 를 하나로

**Files:**
- Create: `src/Buildings.hpp`, `src/Buildings.cpp`
- Modify: `src/Production.cpp`(`WalkRecipes`), `src/Build.cpp`(`ForEachCost`), `CMakeLists.txt`

**Interfaces:**
- Produces: `NlBuildings::ForEachType(const std::function<bool(const std::string& Name, const YYTK::RValue& Generic)>& Visit, std::string& Why) -> bool`.

- [ ] **Step 1: `src/Buildings.hpp`**

```cpp
#pragma once
// 건물 종류(171개)를 걷는다(research/09): 이름은 gml_Script_building_generic_get_array_of_all_buildings()(인자 없음), 종류의 구조체는
// gml_Script_get_generic_building(이름)(게임이 (string)으로 부른다). ds_map 에 들어 있어 전역 탐색으로는 보이지 않는다.
// src/Build.cpp 와 src/Production.cpp 가 각자 들던 것을 2026-10-07 리팩토링 C 에서 하나로.

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>

namespace NlBuildings
{
	// 종류마다 Visit(이름, 구조체) 를 부른다(이름이 글이 아니거나 구조체를 얻지 못한 종류는 건너뛴다). Visit 이 거짓을 돌려주면 그만 돈다.
	// 이름의 목록을 얻지 못하면 거짓이고 Why 에 까닭.
	bool ForEachType(const std::function<bool(const std::string& Name, const YYTK::RValue& Generic)>& Visit, std::string& Why);
}
```

- [ ] **Step 2: `src/Buildings.cpp`**

```cpp
#include "Buildings.hpp"

#include "Access.hpp"
#include "Game.hpp"

using namespace YYTK;
using NlAccess::Holder;

namespace
{
	constexpr const char* k_AllNames = "gml_Script_building_generic_get_array_of_all_buildings";
	constexpr const char* k_Generic = "gml_Script_get_generic_building";
}

bool NlBuildings::ForEachType(const std::function<bool(const std::string&, const RValue&)>& Visit, std::string& Why)
{
	RValue names;		// 이 함수 안에서만 든다
	if (!NlGame::CallScript(k_AllNames, {}, names) || !names.IsArray())
	{
		Why = "the list of buildings is not available";
		return false;
	}
	NlAccess::ForEachChild(names, Holder::Array, [&](const NlCore::PathStep&, const RValue& name) {
		if (!name.IsString())
			return true;
		RValue generic;
		if (!NlGame::CallScript(k_Generic, { name }, generic) || !generic.IsStruct())
			return true;
		return Visit(name.ToString(), generic);
	});
	return true;
}
```

- [ ] **Step 3: 두 호출자가 이것을 쓴다**

`src/Production.cpp` `WalkRecipes`: `names` 를 얻고 `ForEachChild` 로 돌며 `k_Generic` 을 부르는 부분(지금의 `RValue names; if (!NlGame::CallScript(k_AllNames…` 부터 `NlAccess::ForEachChild(names, …` 의 바깥 람다 머리까지)을 다음으로 바꾼다. 안쪽(`__production.__map_of_production` 을 따라가 `seen`·ds_map 을 도는 부분)은 그대로:

```cpp
	bool WalkRecipes(const Visit& V, bool Inputs, std::string& Why)
	{
		std::vector<double> seen;
		return NlBuildings::ForEachType([&](const std::string& key, const RValue& generic) {
			RValue map;
			std::string why;
			if (!NlAccess::Follow(generic, { { '.', "__production", 0 }, { '.', "__map_of_production", 0 } }, map, why) || !NlGame::IsNumber(map))
				return true;		// 만드는 것이 없는 건물 종류
			const double id = map.ToDouble();
			if (std::find(seen.begin(), seen.end(), id) != seen.end())
				return true;
			seen.push_back(id);
			// (여기부터 지금의 안쪽 람다 그대로: NlAccess::ForEachChild(map, Holder::Map, …))
			return true;
		}, Why);
	}
```

`k_AllNames`·`k_Generic` 상수는 Production.cpp 에서 지운다. `#include "Buildings.hpp"`.

`src/Build.cpp` `ForEachCost`: 같은 꼴로. `Seen.Buildings`·`Seen.Skipped` 는 그대로 센다(구조체를 얻은 종류가 비용의 자리가 없으면 `Skipped++`):

```cpp
	bool ForEachCost(const std::function<void(const std::string& Building, int Level, const RValue& Cost)>& Visit, Walk& Seen, std::string& Why)
	{
		return NlBuildings::ForEachType([&](const std::string& building, const RValue& generic) {
			RValue levels;
			std::string why;
			if (!NlAccess::Follow(generic, { { '.', "__construction_cost", 0 }, { '.', "levels", 0 } }, levels, why) || !levels.IsArray())
			{
				Seen.Skipped++;
				return true;
			}
			Seen.Buildings++;
			NlAccess::ForEachChild(levels, Holder::Array, [&](const PathStep& step, const RValue& cost) {
				if (cost.IsStruct())
					Visit(building, static_cast<int>(step.Index), cost);
				return true;
			});
			return true;
		}, Why);
	}
```

(지금은 구조체를 얻지 못한 종류도 `Skipped` 로 셌다. `ForEachType` 은 그런 종류를 부르지 않으므로 그 수가 로그에서 조금 줄 수 있다. 로그의 수만 다르고 쓰는 자리는 같다.) `k_AllNames`·`k_Generic` 을 Build.cpp 에서 지운다. `#include "Buildings.hpp"`.

`CMakeLists.txt`: `src/Build.cpp` 뒤에 `  src/Buildings.cpp`.

- [ ] **Step 4: 빌드와 시험** — 기대: `build ok`, 경고 0, 118 통과.

- [ ] **Step 5: 커밋**

```bash
git -C E:\NlToyBox add src/Buildings.hpp src/Buildings.cpp src/Production.cpp src/Build.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "refactor(buildings): 건물 종류 걷기를 src/Buildings 하나로 (리팩토링 C, Task 2)"
```

---

### Task 3: 건설비를 일(`Job`)로

**Files:**
- Modify: `src/Build.cpp`(비용 부분을 `WalkCosts` + 등록으로), `src/Build.hpp`, `src/ModuleMain.cpp`(Init 의 차례)

**Interfaces:**
- Consumes: `NlJobs::Add`, `NlJobs::Visit`, `NlBuildings::ForEachType`.

- [ ] **Step 1: `WalkCosts` 를 쓰고 비용의 상태·함수를 지운다**

`src/Build.cpp` 에서 지우는 것: `g_Book`, `g_Applied`, `g_Wanted`, `g_CheckLogged`, `g_Retry`, `g_Note`, `k_Cheat`, `struct Walk`, `ForEachCost`, `Place`, `struct Outcome`, `ZeroAll`, `RestoreAll`, `StillZero`, `TickCosts`, 그리고 `GameTick` 의 `TickCosts(Now);` 줄. `Quantities` 는 남긴다. `#include "core/CostBook.hpp"`·`"core/Retry.hpp"` 는 업그레이드 쪽(`g_UpgradeRetry`)이 `Retry` 를 쓰므로 `Retry` 만 남긴다.

더하는 것(익명 네임스페이스 안, `Quantities` 아래):

```cpp
	// 건설비(research/09): 건물 종류마다 __construction_cost.levels[등급] 의 money 와 resources.__array_of_resource_quantity[39].
	// 엔진(src/Jobs)이 0 으로 쓰고 장부로 되돌린다. Key 는 건물 종류의 이름, Level 은 등급, Slot 은 금화 -1·자원 번호.
	bool WalkCosts(const NlJobs::Visit& V, std::string& Why)
	{
		return NlBuildings::ForEachType([&](const std::string& building, const RValue& generic) {
			RValue levels;
			std::string why;
			if (!NlAccess::Follow(generic, { { '.', "__construction_cost", 0 }, { '.', "levels", 0 } }, levels, why) || !levels.IsArray())
				return true;
			NlAccess::ForEachChild(levels, Holder::Array, [&](const PathStep& level, const RValue& cost) {
				if (!cost.IsStruct())
					return true;
				const PathStep money_step{ '.', "money", 0 };
				RValue money, quantities;
				std::string ignored;
				if (NlAccess::Follow(cost, { money_step }, money, ignored))
					V(cost, money_step, building, static_cast<int>(level.Index), -1, money);
				if (!Quantities(cost, quantities))
					return true;
				NlAccess::ForEachChild(quantities, Holder::Array, [&](const PathStep& step, const RValue& value) {
					V(quantities, step, building, static_cast<int>(level.Index), static_cast<int>(step.Index), value);
					return true;
				});
				return true;
			});
			return true;
		}, Why);
	}
```

`Init` 에서 등록한다(주기 2초: 지금의 Retry 가 2초부터 다시 보던 것과 같은 뜻. 엔진은 다 쓴 뒤 주기마다 훑어 바뀐 것만 쓴다):

```cpp
void NlBuild::Init(LogFn Log_)
{
	g_Log = std::move(Log_);
	NlJobs::Add({ "build_free", "construction costs", true, &WalkCosts, nullptr, 2 });
}
```

`GameTick` 은 `TickUpgrades(Now)` 만 남긴다. `#include "Jobs.hpp"`, `#include "Buildings.hpp"`. `src/Build.hpp` 의 머리 설명을 "건설비는 src/Jobs 의 일로 등록한다. 여기는 걷기와 즉시 업그레이드" 로.

- [ ] **Step 2: `ModuleMain.cpp` 의 `Init` 차례** — `NlBuild::Init` 줄을 `NlProduction::Init` 줄 **뒤**로 옮긴다(건설비가 등록의 맨 끝: 지금의 열셋 뒤).

- [ ] **Step 3: 빌드와 시험** — 기대: `build ok`, 경고 0, 118 통과.

- [ ] **Step 4: 커밋**

```bash
git -C E:\NlToyBox add src/Build.cpp src/Build.hpp src/ModuleMain.cpp
git -C E:\NlToyBox commit -m "refactor(build): 건설비를 Jobs 의 일 하나로. Build 는 걷기와 즉시 업그레이드만 (리팩토링 C, Task 3)"
```

---

### Task 4: 종교·임신·범죄의 일을 제 영역의 파일로

**Files:**
- Modify: `src/Production.cpp`(열 개의 걷는 함수와 등록을 지운다), `src/World.cpp`·`src/World.hpp`(종교 셋), `src/People.cpp`(임신 셋), `src/Crime.cpp`(범죄 넷), `src/ModuleMain.cpp`(Init 의 차례)

**Interfaces:**
- Consumes: `NlJobs::Add`, `NlJobs::WalkVars`, `NlJobs::Visit`, `NlJobs::Log`; 열쇠 목록 `NlCore::ReligionCostVars`·`PreachFactorVars`·`PietyRestoreVars`(`core/WorldPlan.hpp`), `NlCore::PregnancyChanceVars`·`MiscarriageVars`·`ChildbirthDeathVars`(`core/FamilyPlan.hpp`), `NlCore::BanditTurnVars`·`CrimeMindVars`·`ThugDaysVars`·`TheftAmountVars`(`core/CrimePlan.hpp`).

- [ ] **Step 1: `Production.cpp` 에서 열을 뺀다**

`WalkReligionCosts`(설교 비용 걷기 + `k_Preaches`, `g_PreachSkippedLogged`), `WalkPreachFactor`, `WalkPietyRestore`, `WalkPregnancyChance`, `WalkMiscarriage`, `WalkChildbirthDeath`, `WalkBanditTurn`, `WalkCrimeMinds`, `WalkThugDays`, `WalkTheftAmount` 와 그 등록 열 줄을 지운다. 포함에서 `core/CrimePlan.hpp`·`core/FamilyPlan.hpp`·`core/WorldPlan.hpp` 를 뺀다. 남는 등록은 셋(창고 용량, 생산량, 생산 재료).

- [ ] **Step 2: `World.cpp` 에 종교 셋**

익명 네임스페이스에 `WalkReligionCosts` 를 **그대로** 옮긴다(`k_Preaches`, `g_PreachSkippedLogged` 와 함께. `WalkVars` 는 `NlJobs::WalkVars`, 로그는 이 파일의 `Log`). 한 줄짜리 둘은 람다 대신 함수로:

```cpp
	bool WalkPreachFactor(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::PreachFactorVars(), Why); }
	bool WalkPietyRestore(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::PietyRestoreVars(), Why); }
```

`NlWorld::Init` 의 끝에:

```cpp
	NlJobs::Add({ "religion_free", "religion costs", true, &WalkReligionCosts, nullptr, 15 });
	NlJobs::Add({ "piety_restore", "piety restore", false, &WalkPietyRestore, nullptr, 15 });
	NlJobs::Add({ "preach_conversion", "preach conversion", false, &WalkPreachFactor, nullptr, 15 });
```

`#include "Jobs.hpp"`, `#include "core/WorldPlan.hpp"`(이미 있으면 그대로).

- [ ] **Step 3: `People.cpp` 에 임신 셋**

익명 네임스페이스에:

```cpp
	// 임신의 게임 변수 셋(research/24. 열쇠는 core/FamilyPlan). 엔진은 src/Jobs.
	bool WalkPregnancyChance(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::PregnancyChanceVars(), Why); }
	bool WalkMiscarriage(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::MiscarriageVars(), Why); }
	bool WalkChildbirthDeath(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::ChildbirthDeathVars(), Why); }
```

`NlPeople::Init` 의 끝에:

```cpp
	NlJobs::Add({ "pregnancy_chance", "pregnancy chance", false, &WalkPregnancyChance, nullptr, 15 });
	NlJobs::Add({ "no_miscarriage", "miscarriage chance", true, &WalkMiscarriage, nullptr, 15 });
	NlJobs::Add({ "safe_childbirth", "childbirth death chance", true, &WalkChildbirthDeath, nullptr, 15 });
```

`#include "Jobs.hpp"`(`core/FamilyPlan.hpp` 는 이미 있다).

- [ ] **Step 4: `Crime.cpp` 에 범죄 넷**

```cpp
	// 범죄의 게임 변수 넷(research/26. 열쇠는 core/CrimePlan). 엔진은 src/Jobs.
	bool WalkBanditTurn(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::BanditTurnVars(), Why); }
	bool WalkCrimeMinds(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::CrimeMindVars(), Why); }
	bool WalkThugDays(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::ThugDaysVars(), Why); }
	bool WalkTheftAmount(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::TheftAmountVars(), Why); }
```

`NlCrime::Init` 의 끝에:

```cpp
	NlJobs::Add({ "no_bandit_turn", "bandit turn chance", true, &WalkBanditTurn, nullptr, 15 });
	NlJobs::Add({ "crime_minds_off", "crime minds", true, &WalkCrimeMinds, nullptr, 15 });
	NlJobs::Add({ "thug_days", "days to thug", false, &WalkThugDays, nullptr, 15 });
	NlJobs::Add({ "theft_none", "storage theft amount", true, &WalkTheftAmount, nullptr, 15 });
```

`#include "Jobs.hpp"`.

- [ ] **Step 5: `ModuleMain.cpp` 의 `Init` 차례** — 등록의 차례가 지금의 배열(생산 → 종교 → 임신 → 범죄 → 건설비)과 같게 줄의 차례를 바꾼다:

```cpp
	NlJobs::Init([](const std::string& Line) { LogLine(Line); });
	NlProduction::Init([](const std::string& Line) { LogLine(Line); });
	NlWorld::Init([](const std::string& Line) { LogLine(Line); }, module_dir.parent_path().parent_path());
	NlPeople::Init([](const std::string& Line) { LogLine(Line); }, module_dir.parent_path().parent_path());
	NlCourt::Init([](const std::string& Line) { LogLine(Line); });
	NlCrime::Init([](const std::string& Line) { LogLine(Line); });
	NlDiplomacy::Init([](const std::string& Line) { LogLine(Line); });
	NlBuild::Init([](const std::string& Line) { LogLine(Line); });
	NlMenu::Init(module_dir, k_Version, [](const std::string& Line) { LogLine(Line); });
```

(이 `Init` 들은 로그 함수를 받고 파일을 읽을 뿐 서로에게 기대지 않는다. `NlRecorder`·`NlRemote`·`NlUi` 들은 그 앞에 지금 그대로.)

- [ ] **Step 6: 빌드와 시험** — 기대: `build ok`, 경고 0, 118 통과. 모듈 로그에 `jobs: … already registered` 가 나면 안 된다(등록이 겹치지 않는지는 Task 11 의 로그에서 본다).

- [ ] **Step 7: 커밋**

```bash
git -C E:\NlToyBox add src/Production.cpp src/World.cpp src/People.cpp src/Crime.cpp src/ModuleMain.cpp
git -C E:\NlToyBox commit -m "refactor(jobs): 종교·임신·범죄의 일을 제 영역의 파일이 등록한다. Production 은 생산의 셋만 (리팩토링 C, Task 4)"
```

---

### Task 5: 월드를 셋으로 — `Season`, `Mines`, `World`

**Files:**
- Create: `src/Season.hpp`, `src/Season.cpp`, `src/Mines.hpp`, `src/Mines.cpp`
- Modify: `src/World.cpp`, `src/World.hpp`, `src/Menu.cpp:168,270-273`, `src/ModuleMain.cpp`, `CMakeLists.txt`

**Interfaces:**
- Produces: `NlSeason::Init(LogFn, const std::filesystem::path& GameDir)`, `NlSeason::Tick(double Now, bool Visible)`, `NlSeason::Draw()`, `NlSeason::Do(NlCore::WorldAct) -> std::string`; `NlMines::Init(LogFn)`, `NlMines::Tick(double Now)`.
- Changes: `NlWorld::Init(LogFn)`(GameDir 를 더 받지 않는다), `NlWorld::GameTick(double Now)`(Visible 을 더 받지 않는다).

- [ ] **Step 1: `src/Season.hpp`**

```cpp
#pragma once
// 계절(research/25): 지금 지도의 ExtremeSeasonManager 에서 남은 시간을 게임의 함수로 읽고, 시작 시각(__start_phase_time)에 써서 미루거나 끝내고,
// 치트 표의 season_hold 가 켜진 동안 시작 시각을 흐른 만큼 따라 민다. 월드 패널의 "계절" 블록을 그린다.
// 2026-10-07 리팩토링 C 에서 src/World.cpp 에서 옮겼다. 그리는 쪽은 청만 쌓고 틱이 한다.

#include "core/WorldPlan.hpp"

#include <filesystem>
#include <functional>
#include <string>

namespace NlSeason
{
	using LogFn = std::function<void(const std::string&)>;

	// GameDir: 게임 폴더(가혹한 계절의 화면 이름을 localization\main.csv 에서 읽는다. 러너를 부르지 않는다).
	void Init(LogFn Log, const std::filesystem::path& GameDir);
	// 게임 스레드의 틱. 1초에 한 번: 쌓인 청, 붙들기, 패널이 보이면 읽기. Visible: 월드 패널이 보이는가.
	void Tick(double Now, bool Visible);
	// 월드 패널의 계절 블록(제목, 글, 단추 둘, 마지막 결과).
	void Draw();
	// 계절의 일(SeasonShow·SeasonDelay·SeasonEnd)을 지금 한다(원격). 그 밖의 일에는 빈 글. 돌려주는 것: 한 일.
	std::string Do(NlCore::WorldAct Act);
}
```

- [ ] **Step 2: `src/Season.cpp`** — `World.cpp` 에서 **잘라** 옮긴다: `k_Season`, `k_GameTime`, `k_HoldCheat`, `k_SeasonCaptionPrefix`, `k_DelayStep`, `k_EndLead`, `struct Season`, `g_Season`, `g_SeasonLast`, `g_NextSeason`, `g_Hold`, `g_HoldWrites`, `g_SeasonCallsLogged`, `g_SeasonCaptions`, `g_HoldWasOn`, `CallNumber`, `PlaceOf`, `ReadSeason`, `SeasonText`, `ChangeSeason`, `HoldTick`, `NlWorld::Init` 의 main.csv 읽기 블록(→ `NlSeason::Init`), `NlWorld::DrawWorld` 의 몸 전체(→ `NlSeason::Draw`. `ImGui::SeparatorText("계절")` 부터 `g_SeasonLast` 의 힌트까지 그대로), `NlWorld::GameTick` 의 계절 부분.

제 상태: `std::recursive_mutex g_Mutex; LogFn g_Log; std::deque<WorldAct> g_Queue;`(계절의 단추가 쌓는 청), `Log`, `Push`:

```cpp
	constexpr size_t k_MaxQueue = 4;

	void Push(WorldAct Act)
	{
		if (g_Queue.size() >= k_MaxQueue)
		{
			g_SeasonLast = NlCore::QueueFullText(k_MaxQueue);
			return;
		}
		g_Queue.push_back(Act);
	}

	std::string DoNow(WorldAct Act)
	{
		if (!NlAccess::InGame())
			return "게임 화면이 아닙니다";
		switch (Act)
		{
		case WorldAct::SeasonShow:
			g_Season = ReadSeason();
			return SeasonText(g_Season);
		case WorldAct::SeasonDelay:
		case WorldAct::SeasonEnd:
			return ChangeSeason(Act);
		default:
			return std::string();
		}
	}

	std::string Remember(WorldAct Act, std::string Text)
	{
		if (NlCore::WorldActChanges(Act))
			g_SeasonLast = Text;
		return Text;
	}
```

`Draw` 의 단추 둘은 `Push(WorldAct::SeasonDelay)`·`Push(WorldAct::SeasonEnd)`(지금과 같다).

```cpp
void NlSeason::Tick(double Now, bool Visible)
{
	std::lock_guard lock(g_Mutex);
	if (!g_Queue.empty())
	{
		const WorldAct act = g_Queue.front();
		g_Queue.pop_front();
		Remember(act, DoNow(act));
	}
	if (Now < g_NextSeason)
		return;
	g_NextSeason = Now + 1;
	const bool hold = NlCheats::IsOn(k_HoldCheat);
	if (hold || g_HoldWasOn)
		HoldTick(hold);
	g_HoldWasOn = hold;
	if (Visible)
		g_Season = ReadSeason();
	else
		g_Season = Season{};		// 패널을 다시 열면 새로 읽은 것을 보인다
}

std::string NlSeason::Do(NlCore::WorldAct Act)
{
	std::lock_guard lock(g_Mutex);
	return Remember(Act, DoNow(Act));
}
```

(`World` 의 틱에서는 청 → 붙들기 → 광산 → 읽기의 차례였다. 광산이 `NlMines::Tick` 으로 빠지므로 여기는 청 → 붙들기 → 읽기이고, `Menu.cpp` 가 `NlSeason::Tick` 바로 뒤에 `NlMines::Tick` 을 부른다.)

- [ ] **Step 3: `src/Mines.hpp`·`src/Mines.cpp`**

```cpp
#pragma once
// 광산 매장량 붙들기(research/25. 치트 표의 mine_stock_hold): 켠 동안 광산(자리)마다 본 가장 큰 매장량을 기억하고, 줄었으면 되돌려 쓴다.
// 기억한 값은 그 자리(관리자 구조체의 주소 + 지도 관리 인스턴스)의 것이다(core/WorldPlan 의 StockBook·PlaceKey). 2026-10-07 리팩토링 C 에서 src/World.cpp 에서 옮겼다.

#include <functional>
#include <string>

namespace NlMines
{
	using LogFn = std::function<void(const std::string&)>;
	void Init(LogFn Log);
	// 게임 스레드의 틱. 1초에 한 번 치트 표를 보고 붙든다. 끈 틱에 한 번 정리한다.
	void Tick(double Now);
}
```

`Mines.cpp`: `World.cpp` 에서 `k_MineStock`, `k_MineCheat`, `g_Mines`, `g_MineWrites`, `g_MineWasOn`, `MineTick`, 그리고 `PlaceOf`(계절과 광산이 함께 쓰던 것. 둘 다 제 사본을 갖는다. 15줄이다) 를 옮긴다. `k_GameTime` 도 쓰면 사본.

```cpp
void NlMines::Tick(double Now)
{
	std::lock_guard lock(g_Mutex);
	if (Now < g_Next)
		return;
	g_Next = Now + 1;
	const bool mines = NlCheats::IsOn(k_MineCheat);
	if (mines || g_MineWasOn)
		MineTick(mines);
	g_MineWasOn = mines;
}
```

- [ ] **Step 4: `World.cpp`·`World.hpp` 를 줄인다**

지우는 것: Step 2·3 에서 옮긴 것 전부, `NlWorld::DrawWorld` 의 몸(→ `NlSeason::Draw()` 한 줄), `GameTick` 의 `g_NextSeason` 가림과 계절·광산 부분, `IsSeasonAct`, `LastOf` 의 계절 갈래(`g_SeasonLast` 가 없어진다: `LastOf` 는 `Act == WorldAct::SaveNow ? g_UtilLast : g_Last`). `DoNow` 의 `SeasonShow`·`SeasonDelay`·`SeasonEnd` 갈래는 `return NlSeason::Do(Act);`.

```cpp
void NlWorld::GameTick(double Now)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return;
	const bool names = g_EventNames.empty() && Now >= g_NextNames;
	if (g_Queue.empty() && !names)		// 시각부터 본다(이 틱은 오브젝트 이벤트마다 불린다)
		return;
	const NlCore::ScopedFlag busy(g_Busy);
	if (names)
	{
		g_NextNames = Now + 5;
		if (NlAccess::InGame())
			ReadEventNames();
	}
	if (!g_Queue.empty())
	{
		const WorldAct act = g_Queue.front();
		g_Queue.pop_front();
		Remember(act, DoNow(act));
	}
}

void NlWorld::DrawWorld()
{
	NlSeason::Draw();
}
```

`NlWorld::Init(LogFn Log_)` 는 로그 함수와 Task 4 의 등록만. `World.hpp` 의 `Init`·`GameTick` 선언을 맞춘다(`<filesystem>` 포함은 뺀다). `#include "Season.hpp"`.

- [ ] **Step 5: `Menu.cpp`·`ModuleMain.cpp`·`CMakeLists.txt`**

`src/Menu.cpp:168`:

```cpp
	NlWorld::GameTick(now);
	NlSeason::Tick(now, visible && page == Area::World);
	NlMines::Tick(now);
```

(`#include "Season.hpp"`, `#include "Mines.hpp"`.) `case Area::World:` 의 `NlWorld::DrawWorld();` 는 그대로(안에서 `NlSeason::Draw` 를 부른다).
`src/ModuleMain.cpp`: `NlWorld::Init(…, module_dir.parent_path().parent_path())` → `NlWorld::Init([](const std::string& Line) { LogLine(Line); });` 로 바꾸고 그 바로 뒤에
`NlSeason::Init([](const std::string& Line) { LogLine(Line); }, module_dir.parent_path().parent_path());` 와 `NlMines::Init([](const std::string& Line) { LogLine(Line); });` 를 더한다.
`CMakeLists.txt`: `src/World.cpp` 뒤에 `  src/Season.cpp`, `  src/Mines.cpp`.

- [ ] **Step 6: 빌드와 시험** — 기대: `build ok`, 경고 0, 118 통과.

- [ ] **Step 7: 커밋**

```bash
git -C E:\NlToyBox add src/Season.hpp src/Season.cpp src/Mines.hpp src/Mines.cpp src/World.cpp src/World.hpp src/Menu.cpp src/ModuleMain.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "refactor(world): 계절을 src/Season, 광산 붙들기를 src/Mines 로. World 는 이벤트·주교·저장 (리팩토링 C, Task 5)"
```

---

### Task 6: 사람 1 — `PeopleAccess` 와 `TraitText`

**Files:**
- Create: `src/PeopleAccess.hpp`, `src/PeopleAccess.cpp`, `src/TraitText.hpp`, `src/TraitText.cpp`
- Modify: `src/People.cpp`, `src/ModuleMain.cpp`, `CMakeLists.txt`

**Interfaces:**
- Produces(`NlPeopleAccess`): `Init(LogFn)`, `k_Unknown = -1e9`, `std::string Base(const NlCore::PersonRow&)`, `bool FollowString(const YYTK::RValue& From, const std::vector<NlCore::PathStep>& Steps, std::string& Out)`, `bool FollowNumber(const YYTK::RValue&, const std::vector<NlCore::PathStep>&, double& Out)`, `bool ReadSoul(const NlCore::PersonRow&, YYTK::RValue& Soul)`, `bool CallNoArgs(const std::string& Path, YYTK::RValue& Result, std::string& Why)`, `bool CallNumber(const std::string& Path, double& Out)`, `bool StillThere(const NlCore::PersonRow&, YYTK::RValue& Soul)`, `void ReadNumbers(const YYTK::RValue& Soul, const std::vector<NlCore::PathStep>&, std::vector<double>& Out)`, `bool ReadTraits(const YYTK::RValue& Soul, std::vector<std::string>& Out)`, `void ReadEquipped(const YYTK::RValue& Soul, std::vector<double>& Out)`, `bool WriteAt(const std::string& Path, double Value, std::string& Note)`, `double NeedLimit(const std::string& Soul, int Index)`, `bool SetNeed(const std::string& Soul, int Index, double Asked, std::string& Note)`, `bool Detach(const NlCore::PersonRow&, const std::string& Name, std::string& Note)`, `bool Attach(const NlCore::PersonRow&, const std::string& Name, YYTK::RValue& Soul, std::string& Note)`.
- Produces(`NlTraitText`): `Init(LogFn, GameDir)`, `LoadForGame()`, `const std::vector<std::string>& Names()`, `const std::vector<std::string>& Shown()`, `bool Known(const std::string&)`, `const std::string& Caption(const std::string&)`, `const std::string& Hint(const std::string&)`, `bool Titled(const std::string&)`, `std::string Label(const std::string&)`, `const char* SkillLabel(size_t)`, `struct Notes { std::string Names, Hints, Files; }`, `Notes GetNotes()`.

- [ ] **Step 1: `src/PeopleAccess.hpp`**

```cpp
#pragma once
// 사람(o_character 영주·손님, o_dummy 주민·노예)의 자료를 읽고 쓰는 도우미(research/11). 상태는 로그 함수뿐이다.
// 사람은 __soul.__uuid 로 가리킨다. 자리의 번호는 사람이 드나들면 밀린다: 쓰기 전에 StillThere 로 그 자리의 uuid 를 다시 본다.
// 2026-10-07 리팩토링 C 에서 src/People.cpp 에서 옮겼다. src/People*.cpp, src/Shield.cpp, src/Hold.cpp 가 쓴다.

#include "core/AskPath.hpp"
#include "core/PeoplePlan.hpp"

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>
#include <vector>

namespace NlPeopleAccess
{
	using LogFn = std::function<void(const std::string&)>;
	constexpr double k_Unknown = -1e9;		// 읽지 못한 수

	void Init(LogFn Log);

	std::string Base(const NlCore::PersonRow& Row);					// "inst:o_character:3" 꼴
	bool FollowString(const YYTK::RValue& From, const std::vector<NlCore::PathStep>& Steps, std::string& Out);
	bool FollowNumber(const YYTK::RValue& From, const std::vector<NlCore::PathStep>& Steps, double& Out);
	bool ReadSoul(const NlCore::PersonRow& Row, YYTK::RValue& Soul);	// 그 자리의 __soul
	bool CallNoArgs(const std::string& Path, YYTK::RValue& Result, std::string& Why);
	bool CallNumber(const std::string& Path, double& Out);
	// 그 자리에 아직 그 사람이 있는가(uuid 가 같다). 있으면 Soul 을 채운다.
	bool StillThere(const NlCore::PersonRow& Row, YYTK::RValue& Soul);
	void ReadNumbers(const YYTK::RValue& Soul, const std::vector<NlCore::PathStep>& Steps, std::vector<double>& Out);
	bool ReadTraits(const YYTK::RValue& Soul, std::vector<std::string>& Out);
	void ReadEquipped(const YYTK::RValue& Soul, std::vector<double>& Out);
	// 수를 쓰고 다시 읽어 확인한다. 실패하면 Note 에 까닭.
	bool WriteAt(const std::string& Path, double Value, std::string& Note);
	double NeedLimit(const std::string& Soul, int Index);
	bool SetNeed(const std::string& Soul, int Index, double Asked, std::string& Note);
	// 특성을 뗀다·붙인다(게임의 Traits.trait_detach·trait_attach). 붙인 뒤에는 Soul 을 다시 읽는다.
	bool Detach(const NlCore::PersonRow& Row, const std::string& Name, std::string& Note);
	bool Attach(const NlCore::PersonRow& Row, const std::string& Name, YYTK::RValue& Soul, std::string& Note);
}
```

- [ ] **Step 2: `src/PeopleAccess.cpp`** — `People.cpp` 에서 `Base`(380), `FollowString`(385), `FollowNumber`(395), `ReadSoul`(406), `CallNoArgs`(413), `CallNumber`(418), `StillThere`(586), `ReadNumbers`(592), `ReadTraits`(606), `ReadEquipped`(622), `WriteAt`(689), `NeedLimit`(698), `SetNeed`(705), `Detach`(717), `Attach`(725), `k_Unknown` 을 **잘라** 옮긴다. 각 함수는 `NlPeopleAccess::` 의 정의가 된다(몸은 그대로. `Log` 는 이 파일의 `g_Log` 를 쓰는 익명 `Log`). 포함: `"Access.hpp"`, `"Game.hpp"`, `"core/Text.hpp"`.

`People.cpp` 에는 `using namespace NlPeopleAccess;` 를 `namespace { … }` 앞에 두어 호출하는 곳을 고치지 않아도 되게 한다(이름이 같다). `k_Unknown` 도 그 using 으로 보인다.

- [ ] **Step 3: `src/TraitText.hpp`**

```cpp
#pragma once
// 특성의 글(research/20): 화면 이름은 게임 폴더의 localization\main.csv(Korean 칸, 비면 English 칸)에서, 설명은 힌트 파일 셋에서 읽는다(게임의 글을 레포에 싣지 않는다).
// 화면 이름의 열쇠와 설명의 열쇠는 게임의 gml_Script_trait_property_get(이름, 번호)에 묻는다(0 이름, 1 화면 이름의 열쇠, 21 힌트의 열쇠. 배치는 쓰기 전에 본다).
// 게임의 특성 이름 목록(inst:o_data.game_trait_list)도 여기서 읽는다. 2026-10-07 리팩토링 C 에서 src/People.cpp 에서 옮겼다.
// 그리기(툴팁, 특성 목록)에서도 읽으므로 제 뮤텍스를 둔다.

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace NlTraitText
{
	using LogFn = std::function<void(const std::string&)>;

	void Init(LogFn Log, const std::filesystem::path& GameDir);		// ModuleInitialize 에서. 파일만 읽는다
	// 게임 스레드에서, 게임을 불러올 때마다: 게임의 특성 이름 목록을 읽고 글을 붙인다(People 의 Scan 이 처음 성공할 때 부른다).
	void LoadForGame();
	const std::vector<std::string>& Names();		// 게임에 있는 특성의 이름(이름순. 이분 탐색에 쓴다)
	const std::vector<std::string>& Shown();		// 같은 이름들을 창에 보일 차례로(core 의 TraitByCaption)
	bool Known(const std::string& Name);			// 게임에 그 이름의 특성이 있는가
	const std::string& Caption(const std::string& Name);	// 화면 이름. 없으면 빈 글
	const std::string& Hint(const std::string& Name);		// 설명. 없으면 빈 글
	bool Titled(const std::string& Name);					// 화면 이름이 힌트의 제목에서 온 것인가(흐린 글씨)
	std::string Label(const std::string& Name);				// "화면 이름 (게임의 이름)" 또는 게임의 이름
	const char* SkillLabel(size_t Index);					// 능력치의 화면 이름(없으면 모듈의 이름)
	struct Notes
	{
		std::string Names, Hints, Files;		// 이름을 어디서 몇 개 읽었는가, 설명이 몇 개 붙었는가, 힌트 파일을 읽은 결과
	};
	Notes GetNotes();
}
```

- [ ] **Step 4: `src/TraitText.cpp`** — `People.cpp` 에서 `k_TraitList`, `k_TraitProperty`, `struct TraitText`, `g_TraitCaptions`, `g_TraitTextNote`, `g_Hints`, `g_TraitTexts`, `g_TraitHintNote`, `g_HintFilesNote`, `g_SkillCaptions`, `TraitCaption`(174), `TraitHint`(184), `TraitTitled`(191), `TraitLabel`(198), `SkillLabel`(205), `LoadTraitFiles`(213), `TraitProperty`(276), `LoadTraitTexts`(286), `LoadTraitNames`(464. `g_Now.TraitNames`·`g_Now.TraitShown` 은 이 파일의 `g_Names`·`g_Shown` 이 된다. 역할 프리셋의 빠진 특성 로그까지 그대로) 를 **잘라** 옮긴다. `NlPeople::Init` 의 `LoadTraitFiles(GameDir)` 호출은 `NlTraitText::Init` 이 한다.

```cpp
namespace
{
	std::recursive_mutex g_Mutex;
	NlTraitText::LogFn g_Log;
	std::vector<std::string> g_Names, g_Shown;
	// … 옮긴 전역들
}

void NlTraitText::Init(LogFn Log_, const std::filesystem::path& GameDir)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	LoadTraitFiles(GameDir);
}

void NlTraitText::LoadForGame()
{
	std::lock_guard lock(g_Mutex);
	LoadTraitNames();		// 안에서 LoadTraitTexts(g_Names) 를 부른다
}

const std::vector<std::string>& NlTraitText::Names() { std::lock_guard lock(g_Mutex); return g_Names; }
const std::vector<std::string>& NlTraitText::Shown() { std::lock_guard lock(g_Mutex); return g_Shown; }
bool NlTraitText::Known(const std::string& Name)
{
	std::lock_guard lock(g_Mutex);
	return std::binary_search(g_Names.begin(), g_Names.end(), Name);		// 지금의 KnownTrait(683) 의 몸
}
// Caption/Hint/Titled/Label/SkillLabel/GetNotes 도 같은 잠금 아래에서 옮긴 몸 그대로
```

- [ ] **Step 5: `People.cpp` 가 둘을 쓴다**

- `Snapshot` 에서 `TraitNames`·`TraitShown` 을 지운다. 그것을 읽던 곳: `KnownTrait`(683) 는 지우고 호출(`RunRoleSteps` 등)을 `NlTraitText::Known` 으로; `Scan` 의 `LoadTraitNames()` 호출은 `NlTraitText::LoadForGame()` 으로(같은 자리, 같은 조건); `Traits()`(2723)·`DrawDetail` 의 특성 목록은 `NlTraitText::Names()`·`Shown()` 으로; `TraitCaption/Hint/Label/Titled`·`SkillLabel` 호출은 `NlTraitText::Caption/Hint/Label/Titled/SkillLabel` 로; 창의 메모 셋(`g_TraitTextNote`·`g_TraitHintNote`·`g_HintFilesNote`)은 `NlTraitText::GetNotes()` 의 `Names`·`Hints`·`Files` 로.
- `NlPeople::TraitName` 은 `return NlTraitText::Label(Name);`.
- `NlPeople::Init(LogFn, GameDir)` 는 `GameDir` 를 더 쓰지 않는다(시그니처는 두고 설명만 고친다. `ModuleMain` 의 호출은 그대로).
- 포함: `"PeopleAccess.hpp"`, `"TraitText.hpp"`. `core/Localization.hpp` 는 더 쓰지 않으면 뺀다.

`src/ModuleMain.cpp`: `NlPeople::Init` 줄 **앞**에 `NlPeopleAccess::Init([](const std::string& Line) { LogLine(Line); });` 와 `NlTraitText::Init([](const std::string& Line) { LogLine(Line); }, module_dir.parent_path().parent_path());`.
`CMakeLists.txt`: `src/People.cpp` 뒤에 `  src/PeopleAccess.cpp`, `  src/TraitText.cpp`.

- [ ] **Step 6: 빌드와 시험** — 기대: `build ok`, 경고 0, 118 통과.

- [ ] **Step 7: 커밋**

```bash
git -C E:\NlToyBox add src/PeopleAccess.hpp src/PeopleAccess.cpp src/TraitText.hpp src/TraitText.cpp src/People.cpp src/ModuleMain.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "refactor(people): 사람의 자료 도우미를 src/PeopleAccess, 특성의 글을 src/TraitText 로 (리팩토링 C, Task 6)"
```

---

### Task 7: 사람 2 — `Shield` 와 `Hold`

**Files:**
- Create: `src/Shield.hpp`, `src/Shield.cpp`, `src/Hold.hpp`, `src/Hold.cpp`
- Modify: `src/People.cpp`, `src/ModuleMain.cpp`, `CMakeLists.txt`

**Interfaces:**
- Produces: `NlShield::Init(LogFn)`, `NlShield::Tick(double Now)`; `NlHold::RowsFn = std::function<bool(std::vector<NlCore::PersonRow>&)>`, `NlHold::HappyFn = std::function<bool(const NlCore::PersonRow&, std::string& Note)>`, `NlHold::Init(LogFn, RowsFn, HappyFn)`, `NlHold::Tick(double Now)`.
- Consumes: `NlPeopleAccess::ReadSoul`·`FollowString`·`FollowNumber`·`StillThere`.

- [ ] **Step 1: `src/Shield.hpp`**

```cpp
#pragma once
// 전투의 항목들(research/13·16·23): 아군 무적(take_damage 를 self 가 플레이어의 영혼일 때 건너뛴다), 아군·적의 전투력과 맷집(한 함수에 두 배율).
// 훅은 self 가 플레이어의 영혼인지로 가린다: 0.5초마다 플레이어의 사람들의 __soul 구조체 주소를 모아 NlRecorder::SetPlayerSelves 에 넣는다(주소부터 넣고 건다).
// 2026-10-07 리팩토링 C 에서 src/People.cpp 에서 옮겼다. People 의 틱 안에서만 불린다(People 의 잠금 아래. 제 뮤텍스는 없다).

#include <functional>
#include <string>

namespace NlShield
{
	using LogFn = std::function<void(const std::string&)>;
	void Init(LogFn Log);
	// 게임 스레드의 틱(People::GameTick 이 부른다). 0.5초에 한 번.
	void Tick(double Now);
}
```

- [ ] **Step 2: `src/Shield.cpp`** — `People.cpp` 에서 `k_TakeDamage`, `g_NextShield`, `g_BattleOn`, `struct SideHook`, `g_SideHooks`, `g_ShieldOn`, `g_ShieldName`, `ShieldOff`(1577), `SideOff`(1588), `BattleTick`(1602 → `NlShield::Tick` 의 몸) 을 **잘라** 옮긴다. 안의 `ReadSoul`·`FollowString` 은 `NlPeopleAccess::` 의 것(`using namespace NlPeopleAccess;`). 로그의 글("people: ally_invincible on, …" 들)은 **그대로** 둔다(동작 불변). 포함: `"Access.hpp"`, `"Cheats.hpp"`, `"PeopleAccess.hpp"`, `"Recorder.hpp"`, `"core/BattlePlan.hpp"`, `"core/PeoplePlan.hpp"`, `"core/Text.hpp"`.

- [ ] **Step 3: `src/Hold.hpp`**

```cpp
#pragma once
// 표의 인구 항목(배고픔 없음, 피로 없음, 모든 욕구, 신앙심 채우기, 언제나 행복, 노화로 죽지 않음. research/11): 플레이어의 사람을 40명씩 0.25초마다 돌며 쓴다.
// 바퀴의 판단(노화 깃발을 끈 뒤 처음부터 온전한 한 바퀴로 되돌리기)은 core/PeoplePlan 의 HoldRound 에 있다.
// 2026-10-07 리팩토링 C 에서 src/People.cpp 에서 옮겼다. People 의 틱 안에서만 불린다(People 의 잠금 아래. 제 뮤텍스는 없다).

#include "core/PeoplePlan.hpp"

#include <functional>
#include <string>
#include <vector>

namespace NlHold
{
	using LogFn = std::function<void(const std::string&)>;
	// 사람들을 다시 읽는다(People 의 Scan 뒤 목록의 사본). 못 읽으면 거짓.
	using RowsFn = std::function<bool(std::vector<NlCore::PersonRow>& Out)>;
	// 한 사람에게 행복 생각을 붙인다(People 의 One(Happy, Bulk)). 됐고 Note 가 비면 참.
	using HappyFn = std::function<bool(const NlCore::PersonRow& Row, std::string& Note)>;

	void Init(LogFn Log, RowsFn Rows, HappyFn Happy);
	// 게임 스레드의 틱(People::GameTick 이 부른다). 0.25초에 한 번.
	void Tick(double Now);
}
```

- [ ] **Step 4: `src/Hold.cpp`** — `People.cpp` 에서 `k_HoldIds`, `g_HoldPeople`, `g_Hold`, `g_NextHold`, `g_NextHoldScan`, `g_NextHappy`, `g_NextHoldLog`, `g_HappyRound`, `g_HoldNoted`, `g_RoundPeople`, `g_LogNeeds`·`g_LogHappy`·`g_LogAge`, `HoldNotes`, `ClearHoldNotes`, `HoldTick`(→ `NlHold::Tick` 의 몸) 을 **잘라** 옮긴다. 두 곳만 바꾼다:

```cpp
	// 바퀴의 처음: 사람들을 다시 읽는다(지금의 "if (!Scan()) return; … PickTargets(g_Now.People, "people")" 자리)
	std::vector<NlCore::PersonRow> people;
	if (!g_Rows || !g_Rows(people))
		return;
	g_HoldPeople.clear();
	for (const size_t at : NlCore::PickTargets(people, "people"))
		g_HoldPeople.push_back(people[at]);
```

```cpp
	// 행복 생각(지금의 "PersonCommand command; command.Act = PersonAct::Happy; … One(command, row, note, true)" 자리)
	std::string note;
	if (g_Happy && g_Happy(row, note) && note.empty())
		g_LogHappy++;
```

`StillThere`·`FollowNumber` 는 `NlPeopleAccess::` 의 것. 로그의 글은 그대로.

- [ ] **Step 5: `People.cpp` 가 둘을 부른다**

`GameTick` 의 `HoldTick(Now); BattleTick(Now);` → `NlHold::Tick(Now); NlShield::Tick(Now);`(차례 그대로). `NlPeople::Init` 의 끝에:

```cpp
	NlHold::Init(g_Log,
		[](std::vector<PersonRow>& Out) {		// People 의 잠금 아래에서 불린다(Hold 는 People 의 틱 안에서만 돈다)
			if (!Scan())
				return false;
			Out = g_Now.People;
			return true;
		},
		[](const PersonRow& Row, std::string& Note) {
			PersonCommand command;
			command.Act = PersonAct::Happy;
			command.Who = Row.Uuid;
			return One(command, Row, Note, true);
		});
```

`src/ModuleMain.cpp`: `NlPeople::Init` 줄 앞에 `NlShield::Init([](const std::string& Line) { LogLine(Line); });`(`NlHold::Init` 은 People 이 부른다). `CMakeLists.txt`: `src/TraitText.cpp` 뒤에 `  src/Shield.cpp`, `  src/Hold.cpp`. `People.cpp` 의 포함에서 `"Recorder.hpp"`·`"core/BattlePlan.hpp"` 를 더 쓰지 않으면 뺀다.

- [ ] **Step 6: 빌드와 시험** — 기대: `build ok`, 경고 0, 118 통과.

- [ ] **Step 7: 커밋**

```bash
git -C E:\NlToyBox add src/Shield.hpp src/Shield.cpp src/Hold.hpp src/Hold.cpp src/People.cpp src/ModuleMain.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "refactor(people): 전투 가림을 src/Shield, 인구 바퀴를 src/Hold 로 (리팩토링 C, Task 7)"
```

---

### Task 8: 사람 3 — `PeopleInternal.hpp`, `PeopleActs.cpp`, `PeopleDraw.cpp`

**Files:**
- Create: `src/PeopleInternal.hpp`, `src/PeopleActs.cpp`, `src/PeopleDraw.cpp`
- Modify: `src/People.cpp`, `CMakeLists.txt`

**Interfaces:**
- Produces(`namespace NlPeople::Detail`, 선언은 헤더·정의는 아래의 파일): 상태 `extern` 들, `People.cpp` 의 `Log`, `Scan`, `FindRow`, `ReadDetail`, `RefreshDetail`, `KnowledgeIndex`; `PeopleActs.cpp` 의 `One`, `Execute`, `Run`, `WhoText`, `SpawnSoldiersNow`, `SpawnHereNow`; `PeopleDraw.cpp` 의 `KindText`, `NumberText`, `Push`.

- [ ] **Step 1: `src/PeopleInternal.hpp`**

```cpp
#pragma once
// src/People.cpp·PeopleActs.cpp·PeopleDraw.cpp 가 나눠 갖는 상태와 함수(2026-10-07 리팩토링 C). 이 헤더는 그 셋만 포함한다.
// 상태의 정의는 People.cpp 에 하나씩 있다. 모두 Detail::g_Mutex 아래에서 쓴다(그리기는 러너를 부르지 않는다).

#include "core/PeoplePlan.hpp"
#include "core/RolePlan.hpp"

#include <YYTK_Shared.hpp>

#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace NlPeople::Detail
{
	using LogFn = std::function<void(const std::string&)>;

	constexpr double k_Unknown = -1e9;		// 읽지 못한 수(PeopleAccess 의 것과 같은 값)
	// 자리와 함수는 research/11·12·13·17·24 에서 잰 것이다.
	constexpr const char* k_HappyMind = "inst:o_data.mind_debug_totally_happy";
	constexpr const char* k_KnowledgeList = "inst:o_data.__knowledge_data.__knowledge_list";
	constexpr const char* k_ResourceCaptions = "global.__resource_caption";
	constexpr const char* k_SpawnSoldier = "gml_Script_rebellion_debug_spawn_player_soldier";
	constexpr const char* k_PreferredData = "inst:o_data.__preferred_equipment_data";
	constexpr const char* k_Spawner = "inst:o_debug.debug_spawner";

	struct Detail			// 고른 사람의 값. RValue 를 담지 않는다
	{
		bool Ready = false;
		std::string Uuid;
		double Age = k_Unknown, Moral = k_Unknown, Pain = k_Unknown, MindSum = k_Unknown;
		double Gender = k_Unknown;
		std::vector<double> Skills, Points;
		std::vector<double> Needs, Limits;
		std::vector<std::string> Traits;
		double Money = k_Unknown, KnowledgeCount = k_Unknown;
		std::vector<double> Items;
		std::vector<double> Equipped;
	};
	struct KnowledgeName
	{
		std::string Name, Caption, Category;
	};
	struct Snapshot			// 틱이 채우고 Draw 가 읽는다
	{
		bool Ready = false;
		std::string Why;
		std::vector<NlCore::PersonRow> People;
		std::vector<KnowledgeName> Knowledge;
		std::vector<std::string> Resources;
		Detail One;
		std::string Last;
	};

	extern std::recursive_mutex g_Mutex;
	extern LogFn g_Log;
	extern Snapshot g_Now;
	extern std::deque<NlCore::PersonCommand> g_Queue;
	extern std::string g_Selected;
	extern std::map<std::string, std::string> g_Names;
	extern double g_NextScan, g_NextDetail;
	extern std::string g_DetailLogged;
	extern bool g_ShowAll;
	extern int g_AgeInput;
	extern std::string g_AgeInputFor;
	extern char g_TraitFilter[48];
	extern bool g_TraitListOpen, g_TraitScroll;
	extern int g_RolePick;
	extern std::unordered_map<std::string, NlCore::RoleMemory> g_RoleMemory;
	extern std::string g_RoleLast, g_RoleLastFor;
	extern std::string g_FatherPick;
	extern bool g_BulkBirthArmed, g_BulkKnowledgeArmed;
	extern char g_KnowledgeFilter[48];
	extern int g_SpawnQueued;
	extern std::deque<NlCore::SpawnKind> g_SpawnKinds;
	extern bool g_AliveLogged, g_Busy;

	// People.cpp
	void Log(const std::string& Line);
	bool Scan();
	const NlCore::PersonRow* FindRow(const std::string& Uuid);
	bool ReadDetail(const NlCore::PersonRow& Row, Detail& Out);
	void RefreshDetail();
	int KnowledgeIndex(const std::string& Name);
	// PeopleActs.cpp
	bool One(const NlCore::PersonCommand& C, const NlCore::PersonRow& Row, std::string& Note, bool Bulk = false);
	std::string Execute(const NlCore::PersonCommand& C);
	void Run(const NlCore::PersonCommand& C);
	std::string WhoText(const std::string& Who);
	std::string SpawnSoldiersNow(double Asked);
	std::string SpawnHereNow(NlCore::SpawnKind Kind);
	// PeopleDraw.cpp
	std::string KindText(const NlCore::PersonRow& Row);
	std::string NumberText(double Value, int Digits);
	void Push(NlCore::PersonAct Act, const std::string& Who, int Index = -1, double Amount = 0, const std::string& Text = std::string());
}
```

(`Detail`·`KnowledgeName`·`Snapshot` 의 필드 주석은 People.cpp 의 것을 그대로 옮긴다. Task 6 에서 `TraitNames`·`TraitShown` 은 이미 빠졌다.)

- [ ] **Step 2: `src/PeopleActs.cpp`** — `People.cpp` 에서 `RunRoleSteps`·`ApplyRole`·`OneRoleUndo`(757–891), `One*` 스물한 개와 `One`(892–1440), `WhoText`·`Execute`·`Run`(1441–1532), `SpawnSoldiersNow`·`SpawnHereNow`(1533–1576) 를 **잘라** 옮긴다. 파일의 머리:

```cpp
#include "PeopleInternal.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "PeopleAccess.hpp"
#include "TraitText.hpp"
#include "core/AskPath.hpp"
#include "core/EconomyPlan.hpp"
#include "core/FamilyPlan.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <cmath>

using namespace YYTK;
using namespace NlPeopleAccess;
using namespace NlPeople::Detail;
using NlAccess::Holder;
using NlCore::PathStep;
using NlCore::PersonAct;
using NlCore::PersonCommand;
using NlCore::PersonRow;
using NlCore::Shortest;

namespace NlPeople::Detail
{
	// (옮긴 함수들. 헤더에 선언된 One·Execute·Run·WhoText·SpawnSoldiersNow·SpawnHereNow 는 여기서 정의한다. 나머지(One*, RunRoleSteps …)는
	//  이 네임스페이스 안의 익명 네임스페이스에 둔다: 다른 파일이 쓰지 않는다.)
}
```

`k_Unknown` 이 `NlPeopleAccess` 와 `Detail` 양쪽에 있어 `using namespace` 둘로 이름이 겹치면(`k_Unknown` 모호) `Detail::k_Unknown` 을 지우고 `NlPeopleAccess::k_Unknown` 만 쓴다(헤더에서 `using NlPeopleAccess::k_Unknown;` 로 받는다).

- [ ] **Step 3: `src/PeopleDraw.cpp`** — `People.cpp` 에서 `Push`(1888), `KindText`, `NumberText`, `NotReady`, `DrawLast`, `DrawSpawnHere`, `TraitTooltip`, `DrawRole`, `DrawFamily`, `DrawDetail`, `DrawWho`, `DetailPending`, `CountPlayers`(1886–2312, 2383–2412) 와 공개 진입점 `NlPeople::DrawPerson`·`DrawKnowledge`·`DrawArmy`·`DrawItems`·`DrawLords`·`DrawPeople` 을 **잘라** 옮긴다. 머리는 Step 2 와 같고 `#include "Ui.hpp"`, `#include <imgui.h>` 가 더 든다. `Draw*` 는 지금처럼 `std::lock_guard lock(g_Mutex)` 로 시작한다. 러너를 부르지 않는다(지금도 그렇다. `Push` 만 한다).

- [ ] **Step 4: `People.cpp` 를 정리한다**

남는 것: `#include "PeopleInternal.hpp"` 와 필요한 포함, `namespace NlPeople::Detail { … }` 안에 **상태의 정의**(헤더의 `extern` 과 하나씩 짝: `std::recursive_mutex g_Mutex; LogFn g_Log; Snapshot g_Now; …`), `Log`, `LoadTables`, `Scan`, `FindRow`, `ReadDetail`, `RefreshDetail`, `KnowledgeIndex`, 임신 일 셋의 걷기(Task 4), 그리고 공개 진입점 `Init`, `GameTick`, `SpawnHere`, `SpawnSoldiers`, `Do`, `List`, `Show`, `Traits`, `TraitName`, `Rows`. 익명 네임스페이스는 `NlPeople::Detail` 로 바뀐다(이름이 헤더와 맞아야 링크된다).

- [ ] **Step 5: `CMakeLists.txt`** — `src/Hold.cpp` 뒤에 `  src/PeopleActs.cpp`, `  src/PeopleDraw.cpp`.

- [ ] **Step 6: 빌드와 시험** — 기대: `build ok`, 경고 0, 118 통과. 링크 오류 `unresolved external symbol NlPeople::Detail::g_…` 가 나면 그 전역의 정의가 `People.cpp` 에 없는 것이다(헤더의 `extern` 과 짝을 맞춘다).

- [ ] **Step 7: 줄 수를 본다** — `People.cpp` 가 900줄 아래인지(`(Get-Content E:\NlToyBox\src\People.cpp).Count`). 넘으면 `LoadTables`·`ReadDetail` 가 남아 있는지 보고, 그래도 넘으면 그대로 두고 커밋 메시지에 수를 적는다(스펙의 700줄은 목표이지 문이 아니다).

- [ ] **Step 8: 커밋**

```bash
git -C E:\NlToyBox add src/PeopleInternal.hpp src/PeopleActs.cpp src/PeopleDraw.cpp src/People.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "refactor(people): People 을 상태·훑기·진입점(People), 명령(PeopleActs), 그리기(PeopleDraw)로 (리팩토링 C, Task 8)"
```

---

### Task 9: `Game::Resolve` 제거

**Files:**
- Modify: `src/Tweaks.cpp:329-331`, `src/Dump.cpp:172-174`, `src/Game.hpp:68-69`, `src/Game.cpp:131-175`

- [ ] **Step 1: 두 호출자**

`src/Tweaks.cpp`(`#include "Access.hpp"` 를 더한다):

```cpp
	RValue vars;		// 못 찾으면 undefined 로 남고, 그 항목은 "못 찾음"이 된다
	std::string why;
	NlAccess::Read(NlCore::ParseAskPath("global.__gameplay_vars"), vars, why);
```

(`#include "core/AskPath.hpp"` 도.) `src/Dump.cpp`(`#include "Access.hpp"`, `"core/AskPath.hpp"`):

```cpp
			RValue value;
			std::string why;
			const std::string text = NlAccess::Read(NlCore::ParseAskPath(g_Request.Watches[i]), value, why) ? NlGame::Describe(value) : "\"kind\":\"missing\"";
```

- [ ] **Step 2: `Game.hpp`·`Game.cpp`** — `Resolve` 의 선언(헤더의 "// "global.a.b" 를 따라간다. 없으면 거짓." 둘)과 정의(131–175)를 지운다. `EnumInstanceMembers` 를 쓰는 곳이 `Game.cpp` 에 더 없으면 그 포함·주석도 정리한다(다른 곳에서 쓰면 그대로).

- [ ] **Step 3: 빌드와 시험** — 기대: `build ok`, 경고 0, 118 통과. `Resolve` 를 더 쓰는 곳이 없는지: `grep -rn "NlGame::Resolve" E:\NlToyBox\src` 가 비어야 한다.

- [ ] **Step 4: 커밋**

```bash
git -C E:\NlToyBox add src/Tweaks.cpp src/Dump.cpp src/Game.hpp src/Game.cpp
git -C E:\NlToyBox commit -m "refactor(game): Game::Resolve 를 지우고 두 호출자가 NlAccess::Read 를 쓴다 (리팩토링 C, Task 9)"
```

---

### Task 10: 문서, 버전, 모든 시험

**Files:**
- Modify: `CLAUDE.md`, `docs/superpowers/reviews/2026-10-07-maintainability-review.md`, `src/ModuleMain.cpp:31`

- [ ] **Step 1: 버전** — `src/ModuleMain.cpp` 의 `k_Version = "0.29.0"` → `"0.29.1"`.

- [ ] **Step 2: `CLAUDE.md`** — "모듈을 쓸 때" 절의 첫 항목들 앞에 파일 지도 한 항목을 더한다(기존 문장들의 파일 이름은 그대로 두되, 옮긴 것은 괄호로 새 자리를 적는다):

```markdown
- **파일 지도(2026-10-07 리팩토링 C)**: 사람은 `src/People.cpp`(상태·훑기·틱·진입점) + `PeopleActs.cpp`(한 사람에게 하는 일) + `PeopleDraw.cpp`(그리기)이고 셋이 `PeopleInternal.hpp`의 상태를 나눠 갖는다.
  사람의 자료 도우미는 `PeopleAccess`, 특성의 글은 `TraitText`, 전투 가림은 `Shield`, 인구 바퀴는 `Hold`. 월드는 `World.cpp`(이벤트·주교·지금 저장) + `Season.cpp`(계절) + `Mines.cpp`(광산).
  게임의 자료를 돌며 값을 쓰는 일은 `Jobs.cpp`(엔진)에 영역 파일이 등록한다(생산 `Production.cpp`, 종교 `World.cpp`, 임신 `People.cpp`, 범죄 `Crime.cpp`, 건설비 `Build.cpp`). 건물 종류 걷기는 `Buildings.cpp`.
```

그리고 본문에서 어긋나는 문장 둘을 고친다: "자료를 돌며 배율을 쓰는 항목(창고 용량, 조리법의 수와 재료)은 `src/Production.cpp`에 일(`Job`) 하나를 더한다" → "… `src/Jobs`의 엔진에 그 영역의 파일이 `NlJobs::Add`로 등록한다(걷는 함수 하나와 치트 표의 항목)". "`src/Build.cpp`가 0 으로 쓰고 `core/CostBook`의 값으로 되돌린다" → "`src/Build.cpp`의 `WalkCosts`가 자리를 넘기고 `src/Jobs`의 엔진이 0 으로 쓰고 되돌린다".

- [ ] **Step 3: 보고서 §11** — 파일 끝에 더한다:

```markdown
## 11. 결과 — 묶음 C (0.29.1, 가지 `chore/refactor-c`, 2026-10-07)

스펙 `docs/superpowers/specs/2026-10-07-refactor-c-design.md`. 동작 불변. 새 파일: `Jobs`, `Buildings`, `Season`, `Mines`, `PeopleAccess`, `TraitText`, `Shield`, `Hold`, `PeopleInternal.hpp`, `PeopleActs`, `PeopleDraw`.
- R6: `People.cpp` 2,795줄 → `People.cpp` N줄 + 여덟 조각. R7: `World.cpp` 707줄 → `World`·`Season`·`Mines`. R8: `Production.cpp`는 생산의 셋만. R10: `Game::Resolve` 제거. 건설비는 `Jobs`의 일 하나.
- 확인: 코어 시험 118, 안전 29, 파이썬 16+99, `check-load` PASS, 실행 묶음에서 옮긴 조각마다 원격으로 본 것(Task 11 의 표).
```

(N 과 Task 11 의 표는 Task 11 에서 채운다.)

- [ ] **Step 4: 모든 시험**

```bash
pwsh -File E:\NlToyBox\tools\build.ps1
pwsh -File E:\NlToyBox\tools\test-native.ps1
pwsh -File E:\NlToyBox\tools\tests\safety.tests.ps1      # 기대: "safety tests: 29 passed"(게임이 꺼져 있어야 돈다)
py -3.14 -m unittest discover -s E:\NlToyBox\tools\re\tests        # 기대: OK
py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests   # 기대: OK
```

- [ ] **Step 5: 커밋**

```bash
git -C E:\NlToyBox add CLAUDE.md docs/superpowers/reviews/2026-10-07-maintainability-review.md src/ModuleMain.cpp
git -C E:\NlToyBox commit -m "docs: 리팩토링 C 의 파일 지도와 결과(보고서 §11), 0.29.1 (리팩토링 C, Task 10)"
```

---

### Task 11: 게임에서 확인(승인 2번), PR

**Files:**
- Modify: `docs/superpowers/reviews/2026-10-07-maintainability-review.md`(§11 의 표)

**사용자에게 먼저 알린다**: 게임을 켠다(두 번). 게임 창을 누르지 말 것.

- [ ] **Step 1: 배포와 적재 판정(1번째)**

```bash
pwsh -File E:\NlToyBox\tools\deploy.ps1        # 게임이 꺼져 있을 때
pwsh -File E:\NlToyBox\tools\check-load.ps1    # 기대: PASS, 종료 코드 0
```

- [ ] **Step 2: 실행 묶음(2번째)에서 옮긴 조각마다 본다**

```bash
pwsh -File E:\NlToyBox\tools\session.ps1 -Action start
pwsh -File E:\NlToyBox\tools\load-save.ps1 -Name 'time_8_23'      # 아덴 4일차 저녁. "게임의 저장: 끔 (no_autosave)" 과 PASS 를 본다
```

그리고 `tools/ask.ps1` 로 차례로(PowerShell 에서 `& E:\NlToyBox\tools\ask.ps1 -Lines '<줄>','<줄>'`). 기대를 함께 적는다:

| 조각 | 명령 | 기대 |
|---|---|---|
| People·PeopleAccess·TraitText | `person list`, `person show 25556c3312bce178`, `traits find=brave max=3` | 영주 다섯 줄; Barra 의 능력치·욕구·특성 줄; 특성 줄에 화면 이름("대담" 같은 한국어)이 붙는다 |
| PeopleActs(역할) | `person 25556c3312bce178 role name=general`, `person 25556c3312bce178 role_undo`, `person show 25556c3312bce178` | "능력치 2개를 올리고, 특성 7개를 붙였습니다" → "되돌리고 … 뗐습니다" → 능력치·특성이 처음 그대로 |
| Shield | `cheat ally_power 2`, 2초 뒤 `records`, `cheat ally_power off` | 기록에 `get_combat_level_in_battle` 의 훅이 보인다(모듈 로그 `people: ally_power x2 …`) |
| Hold | `cheat needs_full on`, 3초 뒤 `person show 25556c3312bce178`, `cheat needs_full off` | 욕구 여섯이 상한(100)으로 간다. 항목의 메모가 "적용 중" 꼴 |
| Season·Mines | `world season`, `cheat season_hold on`, `cheat mine_stock_hold on`, 3초 뒤 `cheat season_hold off`, `cheat mine_stock_hold off` | 계절의 글(남은 시간) 한 줄; 모듈 로그에 붙들기의 줄, 오류 없음 |
| Jobs·Build(건설비) | `cheat build_free on`, 4초 뒤 `method gml_Script_get_generic_building s:hut_8x8` 뒤 `ask` 로 `__construction_cost.levels[0].money` 읽기 … 또는 간단히 모듈 로그의 `construction costs: wrote N value(s) to 0` 줄; `cheat build_free off` 뒤 `… wrote N value(s) to the first values` | 0 을 쓴 줄과 되돌린 줄이 모두 있다 |
| Jobs·Production | `cheat production_free on`, 4초, `cheat production_free off` | 로그 `production inputs: wrote … to 0` → `… to the first values` |
| Jobs·World(종교) | `cheat religion_free on`, 4초, `cheat religion_free off` | 로그 `religion costs: wrote …` 둘 |
| Jobs·People(임신) | `cheat no_miscarriage on`, 4초, `ask global.__gameplay_vars.pregnancy_miscarriage_chance`, `cheat no_miscarriage off`, 4초, 같은 `ask` | 0 → 0.2 |
| Jobs·Crime(범죄) | `cheat thug_days on`, 4초, `ask global.__gameplay_vars.dummy_criminal_days_to_thug`, `cheat thug_days off`, 4초, 같은 `ask` | 20 → 2 |
| World(이벤트·저장) | `world event name=u_guest_bard`, `world save`(저장이 꺼져 있어 거절) | "강제 이벤트로 써 두었습니다", "게임의 저장이 꺼져 있어 저장하지 않았습니다" |
| Resolve 제거(Tweaks) | 배율 하나를 모드창 없이: `cheat` 가 아니라 `tweak` 는 원격이 없으므로 모듈 로그에서 `tweak … target not found` 가 메인 메뉴에서만 나고 게임 안에서는 안 나는지 본다 | 게임 안에서 `tweak` 의 "not found" 줄이 없다 |

끝나면:

```bash
pwsh -File E:\NlToyBox\tools\session.ps1 -Action stop -Name refactor-c-run1     # 적재 판정 통과, 설정 되돌림
```

게임의 오류 파일 끝(`%LOCALAPPDATA%\Strategy\catched_errors_0.5588.9777.0.txt`)에 새 `ERROR` 가 없는지 본다(읽기만). 모듈 로그(`refs\runtime\refactor-c-run1.log`)에 `jobs: … already registered` 가 없는지 본다.

- [ ] **Step 3: 보고서 §11 의 표를 채운다** — 위의 표에 본 것을 적고(`People.cpp` 의 줄 수도), 커밋한다.

```bash
git -C E:\NlToyBox add docs/superpowers/reviews/2026-10-07-maintainability-review.md
git -C E:\NlToyBox commit -m "docs: 리팩토링 C 의 확인 결과 (Task 11)"
```

- [ ] **Step 4: 푸시와 PR**

```bash
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"    # 비어야 한다
git -C E:\NlToyBox push -u origin chore/refactor-c
gh pr create --repo game-mod-project/NlToybox --base develop --title "refactor: 리팩토링 C — People·World 분할, Jobs 엔진과 건설비, Game::Resolve 제거 (0.29.1)" --body-file E:\NlToyBox\build\pr-body.md
```

`build\pr-body.md`(추적되지 않는 폴더에 잠깐 쓴다)의 내용:

```markdown
리팩토링 C(동작 불변). 스펙 `docs/superpowers/specs/2026-10-07-refactor-c-design.md`, 결과 보고서 §11.

- People.cpp → People(상태·훑기·진입점) + PeopleActs + PeopleDraw, 그리고 PeopleAccess·TraitText·Shield·Hold.
- World.cpp → World(이벤트·주교·저장) + Season + Mines.
- 자료를 돌며 쓰는 일의 엔진 Jobs(영역 파일이 등록), 건물 종류 걷기 Buildings, 건설비는 Jobs 의 일 하나.
- Game::Resolve 제거(두 호출자가 NlAccess::Read).
- 코어 시험 118, 안전 29, 파이썬 16+99, check-load PASS, 실행 묶음에서 옮긴 조각마다 원격으로 확인(보고서 §11 의 표).

🤖 Generated with [Claude Code](https://claude.com/claude-code)
```

머지는 `gh pr merge <번호> --merge --delete-branch`, 그 뒤 `git checkout develop && git pull`.
