# 치트 메뉴 1·2단계 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 모드창(F8)에 왼쪽 목록이 있는 치트 메뉴를 세우고, 게임의 아무 값이나 보고 고치는 탐색기와 `o_debug` 스위치 31개, 스스로 손잡이를 고르는 게임 속도를 넣는다.

**Architecture:** 주소(`AskPath`) 하나로 게임의 값을 가리킨다. 러너에 닿는 호출은 게임 스레드의 틱(`EVENT_OBJECT_CALL`)에서만 하고, 그리는 쪽은 글로 된 스냅샷을 읽고 명령을 큐에 넣는다. 러너에 기대지 않는 것(주소, 치트 표, 상태 파일, 흐름 재기, 속도 시험)은 `src/core`에 두고 네이티브 시험으로 먼저 고정한다.

**Tech Stack:** C++ (`/std:c++latest`, MSVC), Aurie v2.0.2 + YYToolkit v5.0.0c (`external/YYToolkit`), Dear ImGui v1.91.9 (`external/imgui`), PowerShell 7 도구, CMake + Ninja (`tools/build.ps1`).

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` (§4~§7, §9, §10의 1·2단계, §12)

## Global Constraints

- 작업 브랜치는 `feat/cheat-menu`다. `main`·`develop`에 직접 커밋하지 않는다. 끝나면 `git -C E:\NlToyBox merge --no-ff`로 `develop`에 넣는다.
- git 명령은 `git -C E:\NlToyBox`로 쓴다. `refs/`, `backups/`, `downloads/`, `build/`를 커밋하지 않는다.
- 스크립트는 `pwsh`로 돌린다. Python 은 `py -3.14`.
- 러너를 건드리는 호출(`NlGame::Call`, `NlAccess::*`, `NlSearch::*`)은 `GameTick` 안에서만 한다. `Draw` 안에서는 부르지 않는다.
- `RValue`를 정적 저장 기간이나 틱 너머로 두지 않는다. 함수 안에서만 든다.
- 빌트인에 넘기는 수는 `double`로 만든다(`RValue(1.0)`). 정수 리터럴은 `VALUE_INT64`가 된다.
- ds 함수는 `ds_exists`가 참인 번호에만 부른다. 치트 표의 주소에 ds 번호를 적지 않는다.
- 없는 것을 만들지 않는다: 쓰기는 읽기가 성공한 자리에만 한다.
- 인자의 형을 모르는 게임 스크립트를 부르지 않는다(이 계획은 게임 스크립트를 하나도 부르지 않는다).
- 서브모듈(`external/`) 안의 파일을 고치지 않는다.
- 모드창의 글꼴에는 한글과 라틴-1 만 있다. 창의 글에 `★ … ← — ◀ ▶`를 쓰지 않는다(`×`와 `·`는 된다).
- 도구와 시험은 소리를 내지 않는다.
- 게임을 켜는 것은 이 계획 전체에서 **한 번**이다(Task 12). 켜기 전에 사용자에게 게임 창을 누르지 말라고 알린다.
- 모듈 버전은 `0.4.0`.

## Review Focus

스펙이 암시하지만 놓치기 쉬운 입력들. 줄마다 그것을 잡는 시험이 어느 Task 에 있는지 적었다.

1. 지금 속도가 시험 배율과 같으면 "달라지지 않았다"와 "먹었다"가 가려지지 않는다 → 시험 배율은 요청한 배율이 아니라 지금 `time_warp`의 2배다. Task 5 "지금 속도가 1 이 아니어도 고른다".
2. 속도 시험 중에 사용자가 일시정지하면 흐름이 0 이 되어 모든 후보가 "효과 없음"으로 보인다 → 시험을 그만두고 원래 값을 되돌린다. Task 5 "시험 중에 멈추면 그만둔다".
3. 새 게임이나 불러오기로 게임 시간이 줄면 흐름이 음수가 된다 → 처음부터 다시 잰다. Task 4 "값이 줄면 처음부터 다시 잰다".
4. 손으로 고친 상태 파일: 범위 밖의 수, 종류가 다른 `Id`, 주소 안의 `=` → Task 2 "읽을 수 없는 줄은 버린다", Task 3 "모르는 Id 와 종류가 다른 Id 를 버리고".
5. 읽을 수 없는 후보(변수가 없다)를 건드리면 없는 변수를 만들게 된다 → 읽지 못한 후보는 쓰지 않고 넘어간다. Task 5 "읽을 수 없는 후보는 건드리지 않는다".

러너에 닿는 쪽(없는 인스턴스, 지워진 ds 번호, 써지지 않는 쓰기)은 네이티브 시험으로 잡을 수 없다. Task 12 의 실행이 `ask`·`poke`로 본다.

## File Structure

| 파일 | 하는 일 | Task |
|---|---|---|
| `src/core/AskPath.{hpp,cpp}` | 주소를 읽고 쓴다(앞 세션의 것에 전체 쓰기·부모·자식을 더한다) | 1 |
| `src/core/Text.{hpp,cpp}` | `Shortest`(다시 읽히는 가장 짧은 수)를 더한다 | 2 |
| `src/core/CheatState.{hpp,cpp}` | 상태 파일 `NlToyBox.cheats.txt`를 읽고 쓴다 | 2 |
| `src/core/CheatTable.{hpp,cpp}` | 영역 17개와 치트 항목 31개 | 3 |
| `src/core/Rate.{hpp,cpp}` | 흐름의 빠르기 | 4 |
| `src/core/SpeedTrial.{hpp,cpp}` | 게임 속도의 손잡이를 시험으로 고르는 상태 기계 | 5 |
| `tests/native/core_tests.cpp` | 위의 시험 | 1~5 |
| `src/Access.{hpp,cpp}` | 주소를 따라가 읽고, 쓰고, 자식을 늘어놓는다 | 6 |
| `src/Search.{hpp,cpp}` | 이름·값으로 찾기, 다시 거르기 | 7 |
| `src/Explorer.{hpp,cpp}` | 탐색기: 훑기, 즐겨찾기·잠금, 찾기 | 8 |
| `src/Cheats.{hpp,cpp}` | 치트 표의 적용과 영역 패널, 게임 속도 | 9 |
| `src/Menu.{hpp,cpp}` | 왼쪽 목록, 상태 줄, 상태 파일, 시험 설정(`ask`, `poke`) | 10 |
| `src/Ui.{hpp,cpp}`, `src/ModuleMain.cpp`, `CMakeLists.txt` | 시험 설정의 나머지 줄을 내어 주고, 새 창을 붙인다 | 10 |
| `tools/common.ps1`, `tools/check-load.ps1`, `tools/ui-check.ps1`, `tools/tests/safety.tests.ps1` | 적재 판정을 한 곳에 두고 `ui-check`가 함께 낸다 | 11 |
| `research/06-cheat-menu.md`, `CLAUDE.md`, `README.md`, 스펙 | 잰 것과 쓰는 법 | 12 |

빌드와 네이티브 시험(모든 Task 가 쓴다):

    pwsh -File tools/build.ps1          # 끝에 "build ok -> ...\build\NlToyBox.dll"
    pwsh -File tools/test-native.ps1    # 끝에 "core tests: N passed", 종료 코드 0

출력의 `'vswhere.exe' is not recognized` 한 줄은 무해하다(`CLAUDE.md`).

---

### Task 1: 주소 — 전체를 글로 쓰기, 부모, 자식

앞 세션이 `src/core/AskPath.{hpp,cpp}`와 그 시험을 만들어 두고 커밋하지 않았다. 그것에 셋을 더해 함께 커밋한다.

**Files:**
- Modify: `src/core/AskPath.hpp`, `src/core/AskPath.cpp` (추적되지 않은 채 있다)
- Modify: `CMakeLists.txt:13-19` (들여쓰기가 흐트러져 있다)
- Test: `tests/native/core_tests.cpp`

**Interfaces:**
- Consumes: `NlCore::Number`, `NlCore::ParseNumber` (`src/core/Text.hpp`)
- Produces:
  - `struct PathStep { char Kind; std::string Name; double Index; }` — `'.'` 멤버, `'['` 배열 원소, `'@'` ds_map 의 키, `'#'` ds_list 의 자리
  - `struct AskPath { std::string Root; std::string Object; double Number; std::vector<PathStep> Steps; std::string Error; }` — `Root`는 `"global"`, `"inst"`, `"map"`, `"list"`
  - `AskPath ParseAskPath(const std::string& Text)`, `std::string FormatStep(const PathStep& Step)`
  - `std::string FormatAskPath(const AskPath& Path)`, `AskPath ParentPath(AskPath Path)`, `AskPath ChildPath(AskPath Path, PathStep Step)`

- [ ] **Step 1: 실패하는 시험을 쓴다**

`tests/native/core_tests.cpp`의 `Test("묻는 경로: 단계를 글로 쓰면 다시 읽힌다", …)` 바로 뒤에 넣는다.

```cpp
	Test("묻는 경로: 전체를 글로 쓰면 다시 읽힌다", [] {
		for (const char* text : { "global", "global.a.b[3].c", "inst:o_debug.is_x", "inst:o_building:1.generic",
			"map:150@building_resources@woodcutter_lvl_1#0#1", "map:44@{building.menu.x}.y", "list:281#0" })
			CHECK_STR(FormatAskPath(ParseAskPath(text)), text);
		CHECK_STR(FormatAskPath(ParseAskPath("inst:o_debug:0.x")), "inst:o_debug.x");		// 첫 인스턴스는 번호를 적지 않는다
		CHECK_STR(FormatAskPath(ParseAskPath("o_debug.x")), "");							// 읽지 못한 경로
	});

	Test("묻는 경로: 부모와 자식", [] {
		const AskPath path = ParseAskPath("inst:o_debug.a[2]");
		CHECK_STR(FormatAskPath(ParentPath(path)), "inst:o_debug.a");
		CHECK_STR(FormatAskPath(ParentPath(ParentPath(ParentPath(path)))), "inst:o_debug");	// 단계가 없으면 그대로다
		CHECK_STR(FormatAskPath(ChildPath(ParentPath(path), { '@', "k.x", 0 })), "inst:o_debug.a@{k.x}");
		CHECK_STR(FormatAskPath(ChildPath(ParseAskPath("global"), { '.', "v", 0 })), "global.v");
	});
```

- [ ] **Step 2: 빌드해서 실패를 본다**

Run: `pwsh -File tools/build.ps1`
Expected: 컴파일 오류 `'FormatAskPath': identifier not found` (그리고 `ParentPath`, `ChildPath`)

- [ ] **Step 3: 구현한다**

`src/core/AskPath.hpp`의 `std::string FormatStep(const PathStep& Step);` 뒤에 더한다.

```cpp

	// 경로 전체를 글로. ParseAskPath 가 다시 읽을 수 있다. 읽지 못한 경로(Error 가 있다)는 빈 글이다.
	std::string FormatAskPath(const AskPath& Path);

	// 마지막 단계를 뗀 경로. 단계가 없으면 그대로다.
	AskPath ParentPath(AskPath Path);

	// 단계를 하나 붙인 경로.
	AskPath ChildPath(AskPath Path, PathStep Step);
```

`src/core/AskPath.cpp`의 `#include <cmath>` 뒤에 `#include <utility>`를 더하고, 파일 끝의 `FormatStep` 뒤(네임스페이스를 닫기 전)에 더한다.

```cpp

	std::string FormatAskPath(const AskPath& Path)
	{
		if (!Path.Error.empty() || Path.Root.empty())
			return "";

		std::string text = Path.Root;
		if (Path.Root == "inst")
			text += ":" + Path.Object + (Path.Number > 0 ? ":" + Number(Path.Number) : "");
		else if (Path.Root != "global")
			text += ":" + Number(Path.Number);
		for (const PathStep& step : Path.Steps)
			text += FormatStep(step);
		return text;
	}

	AskPath ParentPath(AskPath Path)
	{
		if (!Path.Steps.empty())
			Path.Steps.pop_back();
		return Path;
	}

	AskPath ChildPath(AskPath Path, PathStep Step)
	{
		Path.Steps.push_back(std::move(Step));
		return Path;
	}
```

`CMakeLists.txt`의 `nlcore` 소스 목록에서 흐트러진 두 줄을 바로잡는다.

```cmake
add_library(nlcore STATIC
  src/core/AskPath.cpp
  src/core/Knobs.cpp
  src/core/Text.cpp
  src/core/Request.cpp
  src/core/Schedule.cpp
)
```

- [ ] **Step 4: 시험이 통과하는지 본다**

Run: `pwsh -File tools/build.ps1; pwsh -File tools/test-native.ps1`
Expected: `ok - 묻는 경로: 전체를 글로 쓰면 다시 읽힌다`, `ok - 묻는 경로: 부모와 자식`, 끝에 `core tests: N passed`, 종료 코드 0

- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add src/core/AskPath.hpp src/core/AskPath.cpp CMakeLists.txt tests/native/core_tests.cpp
git -C E:\NlToyBox commit -m "feat(core): 게임의 값을 가리키는 주소(AskPath) — 읽기, 쓰기, 부모와 자식"
```

---

### Task 2: 상태 파일

**Files:**
- Modify: `src/core/Text.hpp`, `src/core/Text.cpp`
- Create: `src/core/CheatState.hpp`, `src/core/CheatState.cpp`
- Modify: `CMakeLists.txt` (`nlcore`에 `src/core/CheatState.cpp`)
- Test: `tests/native/core_tests.cpp`

**Interfaces:**
- Consumes: `ParseAskPath` (Task 1), `Trim`, `ParseNumber` (`Text.hpp`)
- Produces:
  - `std::string Shortest(double Value)` — `"0.83"`, `"3"`, `"-4"`. 유한하지 않으면 `"nan"`/`"inf"`/`"-inf"`
  - `struct LockLine { std::string Path; double Value; }`
  - `struct CheatState { std::set<std::string> On; std::map<std::string, double> Numbers; std::vector<std::string> Pins; std::vector<LockLine> Locks; }`
  - `CheatState ParseCheatState(std::istream& In)`, `std::string FormatCheatState(const CheatState& State)`

- [ ] **Step 1: 실패하는 시험을 쓴다**

`tests/native/core_tests.cpp`의 맨 위 include 에 더한다(사전순).

```cpp
#include "core/CheatState.hpp"
```

```cpp
#include <set>
#include <vector>
```

Task 1 의 시험 뒤에 넣는다.

```cpp
	Test("Shortest 는 다시 읽으면 같은 수가 되는 가장 짧은 글을 쓴다", [] {
		CHECK_STR(Shortest(0.83), "0.83");
		CHECK_STR(Shortest(3), "3");
		CHECK_STR(Shortest(-4), "-4");
		CHECK_STR(Shortest(0.1 + 0.2), "0.30000000000000004");
		CHECK_STR(Shortest(std::numeric_limits<double>::quiet_NaN()), "nan");
		CHECK_STR(Shortest(-std::numeric_limits<double>::infinity()), "-inf");
		for (const double value : { 36550.20449999981, 1e20, -0.000123, 57.4 })
		{
			double again = 0;
			CHECK(ParseNumber(Shortest(value), again) && again == value);
		}
	});

	Test("치트 상태: 읽고 쓰면 같다", [] {
		CheatState state;
		state.On = { "instant_build", "no_dodge" };
		state.Numbers = { { "rest_decrease", 0 }, { "piety_decrease", 0.83 } };
		state.Pins = { "inst:o_time_controller.time_warp", "map:128@{messenger_cost }" };
		state.Locks = { { "inst:o_character:2.starving_hours", 0 }, { "global.a.b[3]", -4.5 } };

		const std::string text = FormatCheatState(state);
		std::istringstream in(text);
		const CheatState again = ParseCheatState(in);
		CHECK(again.On == state.On && again.Numbers == state.Numbers && again.Pins == state.Pins);
		CHECK(again.Locks.size() == 2 && again.Locks[0].Path == "inst:o_character:2.starving_hours" && again.Locks[0].Value == 0
			&& again.Locks[1].Path == "global.a.b[3]" && again.Locks[1].Value == -4.5);
		CHECK_STR(FormatCheatState(again), text);
	});

	Test("치트 상태: 읽을 수 없는 줄은 버린다", [] {
		std::istringstream in(
			"# 주석\n"
			"\n"
			"on\n"								// 이름이 없다
			"on a b\n"							// 이름에 공백
			"num x=abc\n"						// 수가 아니다
			"num x=nan\n"						// 유한하지 않다
			"num =3\n"							// 이름이 없다
			"pin o_debug.x\n"					// 뿌리를 모른다
			"pin global\n"						// 단계가 없다
			"lock inst:o_debug.x\n"				// 값이 없다
			"lock inst:o_debug.x=1e999\n"		// 유한하지 않다
			"what inst:o_debug.x=1\n"			// 모르는 낱말
			"  on  good  \n"
			"num n = 2.5\n"
			"pin inst:o_debug.x\n"
			"pin inst:o_debug.x\n"				// 같은 주소는 한 번만
			"lock map:1@{a=b}=7\n"				// 마지막 '=' 에서 가른다
			"lock map:1@{a=b}=8\n");			// 같은 주소는 뒤의 것이 이긴다
		const CheatState state = ParseCheatState(in);
		CHECK(state.On == std::set<std::string>{ "good" });
		CHECK(state.Numbers.size() == 1 && state.Numbers.at("n") == 2.5);
		CHECK(state.Pins == std::vector<std::string>{ "inst:o_debug.x" });
		CHECK(state.Locks.size() == 1 && state.Locks[0].Path == "map:1@{a=b}" && state.Locks[0].Value == 8);
	});
```

- [ ] **Step 2: 빌드해서 실패를 본다**

Run: `pwsh -File tools/build.ps1`
Expected: `Cannot open include file: 'core/CheatState.hpp'`

- [ ] **Step 3: 구현한다**

`src/core/Text.hpp`의 `Fixed` 선언 뒤에 더한다.

```cpp

	// 다시 읽으면 같은 수가 되는 가장 짧은 글("0.83", "3", "-4"). 유한하지 않으면 "nan" / "inf" / "-inf".
	std::string Shortest(double Value);
```

`src/core/Text.cpp`의 `Fixed` 뒤에 더한다(`<charconv>`와 `<cmath>`는 이미 include 돼 있다).

```cpp

	std::string Shortest(double Value)
	{
		if (!std::isfinite(Value))
			return Value != Value ? "nan" : (Value > 0 ? "inf" : "-inf");
		char buf[40];
		const std::to_chars_result result = std::to_chars(buf, buf + sizeof(buf), Value);
		return std::string(buf, result.ptr);
	}
```

`src/core/CheatState.hpp`:

```cpp
#pragma once
// 치트의 상태 파일(NlToyBox.cheats.txt). 러너에 기대지 않는다. 스펙: 치트 메뉴 §6.3.
//   on <id>              켠 스위치
//   num <id>=<수>        값을 정한 항목
//   pin <주소>           즐겨찾기
//   lock <주소>=<수>     잠금 값. 불러올 때는 꺼진 채다(인스턴스의 차례가 실행마다 다를 수 있다)

#include <istream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace NlCore
{
	struct LockLine
	{
		std::string Path;
		double Value = 0;
	};

	struct CheatState
	{
		std::set<std::string> On;
		std::map<std::string, double> Numbers;
		std::vector<std::string> Pins;		// 적힌 차례대로. 같은 주소는 한 번만
		std::vector<LockLine> Locks;		// 적힌 차례대로. 같은 주소는 뒤의 것이 이긴다
	};

	// 읽을 수 없는 줄(모르는 낱말, 수가 아닌 값, 읽히지 않는 주소)은 버린다.
	CheatState ParseCheatState(std::istream& In);

	// ParseCheatState 가 읽는 꼴로 쓴다.
	std::string FormatCheatState(const CheatState& State);
}
```

`src/core/CheatState.cpp`:

```cpp
#include "CheatState.hpp"

#include "AskPath.hpp"
#include "Text.hpp"

#include <algorithm>
#include <cmath>

namespace NlCore
{
	namespace
	{
		// "이름=수" 를 마지막 '=' 에서 가른다(주소의 중괄호 안에 '=' 가 있을 수 있다).
		bool SplitNumber(const std::string& Text, std::string& Name, double& Value)
		{
			const size_t eq = Text.rfind('=');
			if (eq == std::string::npos)
				return false;
			Name = Trim(Text.substr(0, eq));
			return !Name.empty() && ParseNumber(Trim(Text.substr(eq + 1)), Value) && std::isfinite(Value);
		}

		// 값 하나를 가리키는 주소인가(뿌리만 있는 주소는 값이 아니다).
		bool GoodPath(const std::string& Path)
		{
			const AskPath parsed = ParseAskPath(Path);
			return parsed.Error.empty() && !parsed.Steps.empty();
		}
	}

	CheatState ParseCheatState(std::istream& In)
	{
		CheatState state;
		std::string line;
		while (std::getline(In, line))
		{
			line = Trim(line);
			const size_t space = line.find(' ');
			if (line.empty() || line[0] == '#' || space == std::string::npos)
				continue;

			const std::string word = line.substr(0, space);
			const std::string rest = Trim(line.substr(space + 1));
			std::string name;
			double value = 0;
			if (word == "on")
			{
				if (!rest.empty() && rest.find_first_of(" =") == std::string::npos)
					state.On.insert(rest);
			}
			else if (word == "num")
			{
				if (SplitNumber(rest, name, value) && name.find(' ') == std::string::npos)
					state.Numbers[name] = value;
			}
			else if (word == "pin")
			{
				if (GoodPath(rest) && std::find(state.Pins.begin(), state.Pins.end(), rest) == state.Pins.end())
					state.Pins.push_back(rest);
			}
			else if (word == "lock" && SplitNumber(rest, name, value) && GoodPath(name))
			{
				const auto it = std::find_if(state.Locks.begin(), state.Locks.end(), [&](const LockLine& lock) { return lock.Path == name; });
				if (it == state.Locks.end())
					state.Locks.push_back({ name, value });
				else
					it->Value = value;
			}
		}
		return state;
	}

	std::string FormatCheatState(const CheatState& State)
	{
		std::string text = "# NlToyBox 의 치트 상태. 모드창(F8)에서 바꾸면 여기에 저장된다.\n";
		for (const std::string& id : State.On)
			text += "on " + id + "\n";
		for (const auto& [id, value] : State.Numbers)
			text += "num " + id + "=" + Shortest(value) + "\n";
		for (const std::string& path : State.Pins)
			text += "pin " + path + "\n";
		for (const LockLine& lock : State.Locks)
			text += "lock " + lock.Path + "=" + Shortest(lock.Value) + "\n";
		return text;
	}
}
```

`CMakeLists.txt`의 `nlcore` 목록에 `src/core/AskPath.cpp` 다음 줄로 `  src/core/CheatState.cpp`를 더한다.

- [ ] **Step 4: 시험이 통과하는지 본다**

Run: `pwsh -File tools/build.ps1; pwsh -File tools/test-native.ps1`
Expected: `ok - Shortest 는 …`, `ok - 치트 상태: 읽고 쓰면 같다`, `ok - 치트 상태: 읽을 수 없는 줄은 버린다`, 종료 코드 0

- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add src/core/Text.hpp src/core/Text.cpp src/core/CheatState.hpp src/core/CheatState.cpp CMakeLists.txt tests/native/core_tests.cpp
git -C E:\NlToyBox commit -m "feat(core): 치트 상태 파일(NlToyBox.cheats.txt)을 읽고 쓴다"
```

---

### Task 3: 치트 표

항목의 이름과 값은 새 게임 덤프 `refs/runtime/stage0b-run2.late1.json`의 `instances.o_debug.members`에서 봤다(스펙 §2.2). `Off`는 그 덤프의 값이다.
`battle_dodge_base`만 그 실행이 23 으로 바꿔 둔 것이라 바닐라 20 을 따로 확인했다(`research/02-new-game-state.md`).

**Files:**
- Create: `src/core/CheatTable.hpp`, `src/core/CheatTable.cpp`
- Modify: `CMakeLists.txt` (`nlcore`에 `src/core/CheatTable.cpp`)
- Test: `tests/native/core_tests.cpp`

**Interfaces:**
- Consumes: `CheatState` (Task 2), `ParseAskPath` (Task 1)
- Produces:
  - `enum class Area { Explorer, Economy, Build, Person, Lord, People, Knowledge, Items, Army, Diplomacy, Religion, Time, World, Events, Util, Presets, Tweaks }`
  - `struct AreaInfo { Area Id; const char* Key; const char* Label; int Stage; }`
  - `const std::vector<AreaInfo>& Areas()`, `const AreaInfo& GetArea(Area Id)`, `const AreaInfo* FindArea(const std::string& Key)`
  - `enum class CheatKind { Toggle, Number }`
  - `struct Cheat { const char* Id; Area Where; const char* Label; const char* Path; CheatKind Kind; double On, Off; double Min, Max; bool Verified; const char* Help; }`
  - `const std::vector<Cheat>& Cheats()`, `const Cheat* FindCheat(const std::string& Id)`, `CheatState KeepKnown(CheatState State)`

- [ ] **Step 1: 실패하는 시험을 쓴다**

include 에 `#include "core/CheatTable.hpp"`를 더하고(`CheatState.hpp` 다음), Task 2 의 시험 뒤에 넣는다.

```cpp
	Test("치트 표: 주소가 모두 읽히고 Id 가 겹치지 않는다", [] {
		std::set<std::string> ids;
		for (const Cheat& cheat : Cheats())
		{
			const AskPath path = ParseAskPath(cheat.Path);
			if (!path.Error.empty() || path.Steps.empty())
			{
				std::printf("  FAIL %s: %s (%s)\n", cheat.Id, cheat.Path, path.Error.c_str());
				g_Failed++;
			}
			CHECK(path.Root == "global" || path.Root == "inst");		// 표에는 ds 번호를 적지 않는다
			CHECK(ids.insert(cheat.Id).second);
			CHECK(std::string(cheat.Id).find_first_of(" =") == std::string::npos);
			CHECK(cheat.Label[0] != 0 && cheat.Help[0] != 0);
			if (cheat.Kind == CheatKind::Toggle)
				CHECK(cheat.On != cheat.Off);
			else
				CHECK(cheat.Min < cheat.Max);
		}
		CHECK(Cheats().size() == 31);
	});

	Test("치트 표: 영역은 Key 로 찾고 목록의 차례가 열거형과 같다", [] {
		std::set<std::string> keys;
		for (const AreaInfo& area : Areas())
		{
			CHECK(keys.insert(area.Key).second);
			CHECK(FindArea(area.Key) == &area);
			CHECK(&GetArea(area.Id) == &area);
			CHECK(area.Stage >= 2 && area.Stage <= 7);
		}
		CHECK(Areas().size() == 17);
		CHECK(FindArea("nope") == nullptr);
		CHECK_STR(GetArea(Area::Time).Key, "time");
		for (const Cheat& cheat : Cheats())
			CHECK(FindArea(GetArea(cheat.Where).Key) != nullptr);
	});

	Test("치트 표: 모르는 Id 와 종류가 다른 Id 를 버리고 수를 범위 안으로 당긴다", [] {
		CheatState state;
		state.On = { "instant_build", "rest_decrease", "nope" };			// rest_decrease 는 Number 다
		state.Numbers = { { "rest_decrease", 999 }, { "instant_build", 1 }, { "nope", 1 } };
		state.Pins = { "inst:o_debug.x" };
		state.Locks = { { "inst:o_debug.y", 3 } };
		const CheatState kept = KeepKnown(state);
		CHECK(kept.On == std::set<std::string>{ "instant_build" });
		CHECK(kept.Numbers.size() == 1 && kept.Numbers.at("rest_decrease") == FindCheat("rest_decrease")->Max);
		CHECK(kept.Pins == state.Pins && kept.Locks.size() == 1);
		CHECK(FindCheat("nope") == nullptr && FindCheat("instant_build")->Kind == CheatKind::Toggle);
	});
```

- [ ] **Step 2: 빌드해서 실패를 본다**

Run: `pwsh -File tools/build.ps1`
Expected: `Cannot open include file: 'core/CheatTable.hpp'`

- [ ] **Step 3: 구현한다**

`src/core/CheatTable.hpp`:

```cpp
#pragma once
// 치트 표. 영역(왼쪽 목록)과 항목(이름, 주소, 종류). 러너에 기대지 않는다. 스펙: 치트 메뉴 §6, §8.
// 항목을 더하는 일은 CheatTable.cpp 의 표에 한 줄을 더하는 일이다.

#include "CheatState.hpp"

#include <string>
#include <vector>

namespace NlCore
{
	enum class Area
	{
		Explorer, Economy, Build, Person, Lord, People, Knowledge, Items, Army,
		Diplomacy, Religion, Time, World, Events, Util, Presets, Tweaks,
	};

	struct AreaInfo
	{
		Area Id;
		const char* Key;		// 시험 설정의 page= 에 쓰는 이름
		const char* Label;		// 왼쪽 목록에 보이는 이름
		int Stage;				// 스펙 §10 의 몇 단계에서 채우는가
	};

	// 왼쪽 목록의 차례대로. 차례는 Area 의 열거 차례와 같다.
	const std::vector<AreaInfo>& Areas();
	const AreaInfo& GetArea(Area Id);
	// Key 로 찾는다. 없으면 nullptr.
	const AreaInfo* FindArea(const std::string& Key);

	enum class CheatKind { Toggle, Number };

	struct Cheat
	{
		const char* Id;			// 상태 파일의 이름
		Area Where;
		const char* Label;		// 창에 보이는 이름
		const char* Path;		// AskPath 의 주소
		CheatKind Kind;
		double On, Off;			// Toggle: 켤 때와 끌 때 써 넣는 값
		double Min, Max;		// Number: 범위
		bool Verified;			// 플레이에서 효과를 봤는가
		const char* Help;		// 변수 이름에서 읽은 뜻. Verified 가 아니면 추정이다
	};

	const std::vector<Cheat>& Cheats();
	const Cheat* FindCheat(const std::string& Id);

	// 표에 없는 Id 와 종류가 다른 Id 를 버리고, 수를 범위 안으로 당긴다. 즐겨찾기와 잠금은 그대로 둔다.
	CheatState KeepKnown(CheatState State);
}
```

`src/core/CheatTable.cpp`:

```cpp
#include "CheatTable.hpp"

#include <algorithm>

namespace NlCore
{
	const std::vector<AreaInfo>& Areas()
	{
		static const std::vector<AreaInfo> areas = {
			{ Area::Explorer, "explorer", "탐색기", 2 },
			{ Area::Economy, "economy", "경제", 3 },
			{ Area::Build, "build", "건설·생산", 3 },
			{ Area::Person, "person", "인물", 4 },
			{ Area::Lord, "lord", "영주", 4 },
			{ Area::People, "people", "인구·욕구", 4 },
			{ Area::Knowledge, "knowledge", "지식", 5 },
			{ Area::Items, "items", "아이템", 5 },
			{ Area::Army, "army", "군대·전투", 5 },
			{ Area::Diplomacy, "diplomacy", "외교", 6 },
			{ Area::Religion, "religion", "종교", 6 },
			{ Area::Time, "time", "시간", 2 },
			{ Area::World, "world", "월드", 6 },
			{ Area::Events, "events", "이벤트", 6 },
			{ Area::Util, "util", "유틸", 7 },
			{ Area::Presets, "presets", "프리셋", 7 },
			{ Area::Tweaks, "tweaks", "배율", 2 },
		};
		return areas;
	}

	const AreaInfo& GetArea(Area Id)
	{
		return Areas()[static_cast<size_t>(Id)];
	}

	const AreaInfo* FindArea(const std::string& Key)
	{
		for (const AreaInfo& area : Areas())
			if (Key == area.Key)
				return &area;
		return nullptr;
	}

	const std::vector<Cheat>& Cheats()
	{
		constexpr CheatKind T = CheatKind::Toggle, N = CheatKind::Number;

		// 이름과 값은 새 게임 덤프(refs/runtime/stage0b-run2.late1.json 의 instances.o_debug.members)에서 봤다.
		// Off 는 그 덤프의 값이다. 뜻은 변수 이름에서 읽은 것이고 효과는 아직 재지 않았다(Verified = false).
		// 차례: Id, 영역, 이름, 주소, 종류, On, Off, Min, Max, Verified, 설명
		static const std::vector<Cheat> cheats = {
			{ "resources_edit_mode", Area::Economy, "자원 수량 편집 모드", "inst:o_debug.is_resources_edit_mode", T, 1, 0, 0, 0, false,
				"게임의 창고 화면에서 자원의 수량을 고치게 하는 개발자 스위치로 보인다" },

			{ "instant_build", Area::Build, "건물 즉시 건설", "inst:o_debug.is_instant_build_buildings", T, 1, 0, 0, 0, false,
				"건물을 놓으면 바로 다 지어지게 하는 개발자 스위치로 보인다" },
			{ "build_all", Area::Build, "모든 건물 건설 가능", "inst:o_debug.is_can_build_all_buildings", T, 1, 0, 0, 0, false,
				"지식이나 조건 없이 모든 건물을 지을 수 있게 하는 개발자 스위치로 보인다" },
			{ "build_duration", Area::Build, "건설 시간 계수", "inst:o_debug.debug_building_duration_factor", N, 0, 0, 0, 5, false,
				"debug_params.json 의 building_duration_factor 가 옮겨진 값이다(원래 0.5). 작을수록 빨리 지어질 것으로 보인다" },

			{ "rest_decrease", Area::People, "휴식 감소(시간당)", "inst:o_debug.debug_rest_decrease_per_hour", N, 0, 0, 0, 20, false,
				"한 시간에 휴식이 줄어드는 양으로 보인다(원래 3). 0 이면 피로가 쌓이지 않을 것으로 보인다" },
			{ "no_occupational_disease", Area::People, "직업병 끔", "inst:o_debug.debug_is_occupational_disease_enabled", T, 0, 1, 0, 0, false,
				"직업병이 생기는지를 정하는 값으로 보인다(원래 켜져 있다)" },

			{ "combat_no_injuries", Area::Army, "부상 없는 전투", "inst:o_debug.is_combat_without_injuries", T, 1, 0, 0, 0, false,
				"전투에서 부상이 생기지 않게 하는 개발자 스위치로 보인다" },
			{ "no_dodge", Area::Army, "회피 끔", "inst:o_debug.is_disable_dodge", T, 1, 0, 0, 0, false,
				"전투에서 회피를 끄는 개발자 스위치로 보인다(양쪽 모두일 수 있다)" },
			{ "no_firefight", Area::Army, "사격전 끔", "inst:o_debug.is_disable_firefight", T, 1, 0, 0, 0, false,
				"사격전을 끄는 개발자 스위치로 보인다" },
			{ "no_equipment_destroy", Area::Army, "장비 파손 끔", "inst:o_debug.is_disable_equipment_destroy", T, 1, 0, 0, 0, false,
				"장비가 부서지지 않게 하는 개발자 스위치로 보인다" },
			{ "no_surrender", Area::Army, "항복 끔", "inst:o_debug.is_surrender_disable", T, 1, 0, 0, 0, false,
				"항복이 일어나지 않게 하는 개발자 스위치로 보인다" },
			{ "no_ambush_attack", Area::Army, "적 매복이 인물을 공격하지 않음", "inst:o_debug.is_enemy_ambush_attack_actors", T, 0, 1, 0, 0, false,
				"적의 매복이 인물을 공격하는지를 정하는 값으로 보인다(원래 켜져 있다)" },
			{ "dodge_base", Area::Army, "회피 기본값", "inst:o_debug.battle_dodge_base", N, 0, 0, 0, 100, false,
				"battle_params.json 의 battle_dodge_base 가 옮겨진 값이다(원래 20)" },
			{ "hire_price_factor", Area::Army, "병사 고용가 계수", "inst:o_debug.soldier_hiring_price_skill_factor", N, 0, 0, 0, 20, false,
				"battle_params.json 의 soldier_hiring_price_skill_factor 가 옮겨진 값이다(원래 5). 전투 기술에 따른 고용가로 보인다" },

			{ "no_rebellions", Area::Diplomacy, "반란이 일어나지 않음", "inst:o_debug.is_rebellions_can_started", T, 0, 1, 0, 0, false,
				"반란이 시작될 수 있는지를 정하는 값으로 보인다(원래 켜져 있다)" },

			{ "piety_decrease", Area::Religion, "신앙 감소(시간당)", "inst:o_debug.debug_piety_decrease_per_hour", N, 0, 0, 0, 10, false,
				"한 시간에 신앙이 줄어드는 양으로 보인다(원래 0.83)" },
			{ "donation_runes", Area::Religion, "헌금 룬", "inst:o_debug.church_donation_runes", N, 0, 0, 0, 100, false,
				"교회 헌금으로 내는 룬의 수로 보인다(원래 1)" },
			{ "donation_runes_fanatic", Area::Religion, "헌금 룬(광신도)", "inst:o_debug.church_donation_runes_fanatic", N, 0, 0, 0, 100, false,
				"광신도가 헌금으로 내는 룬의 수로 보인다(원래 2)" },

			{ "fast_global_tasks", Area::World, "전역 지도의 행동을 빠르게", "inst:o_debug.is_fast_action_task_on_global_map", T, 1, 0, 0, 0, false,
				"전역 지도에서 하는 행동을 빨리 끝내는 개발자 스위치로 보인다" },
			{ "no_tree_growth", Area::World, "나무가 자라지 않음", "inst:o_debug.is_disable_trees_grow", T, 1, 0, 0, 0, false,
				"나무의 성장을 끄는 개발자 스위치로 보인다" },

			{ "hide_events", Area::Events, "이벤트 표시 끔", "inst:o_debug.is_display_event_disabled", T, 1, 0, 0, 0, false,
				"이벤트 알림을 띄우지 않는 개발자 스위치로 보인다(이벤트 자체를 막는지는 모른다)" },

			{ "game_debug", Area::Util, "게임의 디버그 모드", "inst:o_debug.is_debug_enabled", T, 1, 0, 0, 0, false,
				"게임에 들어 있는 디버그 기능의 큰 스위치로 보인다. 켜면 게임의 디버그 창이 뜰 수 있다" },
			{ "debug_managers", Area::Util, "게임의 디버그 창: 매니저", "inst:o_debug.is_show_debug_managers", T, 1, 0, 0, 0, false,
				"게임의 'Debug managers' 창을 여는 스위치로 보인다" },
			{ "traits_windows", Area::Util, "게임의 디버그 창: 특성", "inst:o_debug.is_show_traits_windows", T, 1, 0, 0, 0, false,
				"게임의 특성 디버그 창을 여는 스위치로 보인다" },
			{ "production_window", Area::Util, "게임의 디버그 창: 생산", "inst:o_debug.is_visible_production_window", T, 1, 0, 0, 0, false,
				"게임의 생산 디버그 창을 여는 스위치로 보인다" },
			{ "debug_log", Area::Util, "게임의 디버그 로그", "inst:o_debug.debug_log_is_enabled", T, 1, 0, 0, 0, false,
				"게임의 'Debug Log' 창을 여는 스위치로 보인다" },
			{ "show_grid", Area::Util, "격자 보기", "inst:o_debug.is_show_grid", T, 1, 0, 0, 0, false,
				"지도의 격자를 그리는 개발자 스위치로 보인다" },
			{ "hide_gui", Area::Util, "게임 UI 숨기기", "inst:o_debug.is_gw_gui_draw_disabled", T, 1, 0, 0, 0, false,
				"게임의 UI 를 그리지 않는 개발자 스위치로 보인다(스크린샷용)" },
			{ "hide_popups", Area::Util, "알림 팝업 숨기기", "inst:o_debug.is_hide_popup_messages", T, 1, 0, 0, 0, false,
				"인물 위의 알림 글을 숨기는 개발자 스위치로 보인다" },
			{ "hide_bubbles", Area::Util, "말풍선 숨기기", "inst:o_debug.is_hide_speech_bubbles", T, 1, 0, 0, 0, false,
				"말풍선을 숨기는 개발자 스위치로 보인다" },
			{ "hide_names", Area::Util, "인물 이름 숨기기", "inst:o_debug.is_hide_character_names", T, 1, 0, 0, 0, false,
				"인물의 이름표를 숨기는 개발자 스위치로 보인다" },
		};
		return cheats;
	}

	const Cheat* FindCheat(const std::string& Id)
	{
		for (const Cheat& cheat : Cheats())
			if (Id == cheat.Id)
				return &cheat;
		return nullptr;
	}

	CheatState KeepKnown(CheatState State)
	{
		std::erase_if(State.On, [](const std::string& id) {
			const Cheat* cheat = FindCheat(id);
			return !cheat || cheat->Kind != CheatKind::Toggle;
		});
		for (auto it = State.Numbers.begin(); it != State.Numbers.end();)
		{
			const Cheat* cheat = FindCheat(it->first);
			if (!cheat || cheat->Kind != CheatKind::Number)
			{
				it = State.Numbers.erase(it);
				continue;
			}
			it->second = std::clamp(it->second, cheat->Min, cheat->Max);
			++it;
		}
		return State;
	}
}
```

`CMakeLists.txt`의 `nlcore` 목록에 `src/core/CheatState.cpp` 다음 줄로 `  src/core/CheatTable.cpp`를 더한다.

- [ ] **Step 4: 시험이 통과하는지 본다**

Run: `pwsh -File tools/build.ps1; pwsh -File tools/test-native.ps1`
Expected: `ok - 치트 표: …` 셋, 종료 코드 0

- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add src/core/CheatTable.hpp src/core/CheatTable.cpp CMakeLists.txt tests/native/core_tests.cpp
git -C E:\NlToyBox commit -m "feat(core): 치트 표 — 영역 17개와 o_debug 의 스위치·수 31개"
```

---

### Task 4: 흐름 재기

**Files:**
- Create: `src/core/Rate.hpp`, `src/core/Rate.cpp`
- Modify: `CMakeLists.txt` (`nlcore`에 `src/core/Rate.cpp`)
- Test: `tests/native/core_tests.cpp`

**Interfaces:**
- Consumes: 없음
- Produces:
  - `class Rate { explicit Rate(double WindowSeconds = 1.0); void Add(double Seconds, double Value); void Reset(); bool Ready() const; double PerSecond() const; }`
  - `bool RateMatches(double Before, double After, double Factor, double Tolerance = 0.25)`

- [ ] **Step 1: 실패하는 시험을 쓴다**

include 에 `#include "core/Rate.hpp"`를 더하고(`PathTable.hpp` 다음), Task 3 의 시험 뒤에 넣는다.

```cpp
	Test("흐름: 일정하게 느는 값의 빠르기를 잰다", [] {
		Rate rate(1.0);
		CHECK(!rate.Ready() && rate.PerSecond() == 0);
		for (int i = 0; i <= 4; i++)
			rate.Add(i * 0.1, 100 + i * 5.74);			// 실제 1초에 57.4
		CHECK(!rate.Ready());								// 0.4초: 창의 절반이 안 된다
		for (int i = 5; i <= 30; i++)
			rate.Add(i * 0.1, 100 + i * 5.74);
		CHECK(rate.Ready() && std::abs(rate.PerSecond() - 57.4) < 1e-6);
	});

	Test("흐름: 창보다 오래된 표본은 잊는다", [] {
		Rate rate(1.0);
		double value = 0;
		for (int i = 0; i <= 20; i++)						// 2초 동안 1초에 10
			rate.Add(i * 0.1, value = i * 1.0);
		for (int i = 21; i <= 35; i++)						// 그 뒤 1.5초 동안 1초에 50
			rate.Add(i * 0.1, value += 5.0);
		CHECK(std::abs(rate.PerSecond() - 50) < 1e-6);
	});

	Test("흐름: 값이 줄면 처음부터 다시 잰다", [] {
		Rate rate(1.0);
		for (int i = 0; i <= 20; i++)
			rate.Add(i * 0.1, 1000 + i);
		CHECK(rate.Ready());
		rate.Add(2.1, 5);									// 새 게임: 게임 시간이 처음으로 돌아갔다
		CHECK(!rate.Ready() && rate.PerSecond() == 0);
		rate.Reset();
		CHECK(!rate.Ready());
	});

	Test("흐름: 기대한 배율인지 가린다", [] {
		CHECK(RateMatches(57.4, 114.8, 2));
		CHECK(RateMatches(57.4, 100, 2));					// 13% 모자라다. 허용 안이다
		CHECK(!RateMatches(57.4, 57.4, 2));				// 그대로다
		CHECK(!RateMatches(57.4, 0, 2));
		CHECK(!RateMatches(0, 100, 2));					// 누르기 전에 멈춰 있었다
		CHECK(RateMatches(57.4, 14.35, 0.25));
	});
```

- [ ] **Step 2: 빌드해서 실패를 본다**

Run: `pwsh -File tools/build.ps1`
Expected: `Cannot open include file: 'core/Rate.hpp'`

- [ ] **Step 3: 구현한다**

`src/core/Rate.hpp`:

```cpp
#pragma once
// 흐르는 값(게임 시간)이 실제 1초에 얼마나 느는지 잰다. 러너에 기대지 않는다. 스펙: 치트 메뉴 §9.

#include <deque>
#include <utility>

namespace NlCore
{
	class Rate
	{
	public:
		explicit Rate(double WindowSeconds = 1.0) : m_Window(WindowSeconds) {}

		// Seconds: 실제 시각(늘기만 한다). Value 가 줄면(새 게임, 불러오기) 처음부터 다시 잰다.
		void Add(double Seconds, double Value);
		void Reset() { m_Samples.clear(); }

		// 창의 절반 이상을 덮는 표본이 있는가.
		bool Ready() const;
		// 실제 1초에 느는 양. Ready 가 아니면 0.
		double PerSecond() const;

	private:
		double m_Window;
		std::deque<std::pair<double, double>> m_Samples;	// (시각, 값)
	};

	// 배율을 건 뒤의 흐름이 기대(Before × Factor)에서 Tolerance 비율 안인가. Before 나 Factor 가 0 이하이면 거짓.
	bool RateMatches(double Before, double After, double Factor, double Tolerance = 0.25);
}
```

`src/core/Rate.cpp`:

```cpp
#include "Rate.hpp"

#include <cmath>

namespace NlCore
{
	void Rate::Add(double Seconds, double Value)
	{
		if (!m_Samples.empty() && (Value < m_Samples.back().second || Seconds <= m_Samples.back().first))
			m_Samples.clear();
		m_Samples.push_back({ Seconds, Value });

		// 창보다 오래된 표본을 버린다. 창을 덮는 가장 오래된 표본 하나는 남긴다.
		while (m_Samples.size() > 2 && Seconds - m_Samples[1].first >= m_Window)
			m_Samples.pop_front();
	}

	bool Rate::Ready() const
	{
		return m_Samples.size() >= 2 && m_Samples.back().first - m_Samples.front().first >= m_Window * 0.5;
	}

	double Rate::PerSecond() const
	{
		if (!Ready())
			return 0;
		return (m_Samples.back().second - m_Samples.front().second) / (m_Samples.back().first - m_Samples.front().first);
	}

	bool RateMatches(double Before, double After, double Factor, double Tolerance)
	{
		if (Before <= 0 || Factor <= 0)
			return false;
		const double expected = Before * Factor;
		return std::abs(After - expected) <= expected * Tolerance;
	}
}
```

`CMakeLists.txt`의 `nlcore` 목록에 `src/core/Knobs.cpp` 다음 줄로 `  src/core/Rate.cpp`를 더한다.

- [ ] **Step 4: 시험이 통과하는지 본다**

Run: `pwsh -File tools/build.ps1; pwsh -File tools/test-native.ps1`
Expected: `ok - 흐름: …` 넷, 종료 코드 0

- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add src/core/Rate.hpp src/core/Rate.cpp CMakeLists.txt tests/native/core_tests.cpp
git -C E:\NlToyBox commit -m "feat(core): 흐름의 빠르기를 잰다(게임 시간이 실제 1초에 얼마나 느는가)"
```

---

### Task 5: 게임 속도의 손잡이를 시험으로 고른다

메뉴에서는 시간이 흐르지 않아 속도의 효과를 미리 잴 수 없다(스펙 §2.3). 그래서 고르는 논리를 러너와 떼어 가짜 게임으로 시험한다.

**Files:**
- Create: `src/core/SpeedTrial.hpp`, `src/core/SpeedTrial.cpp`
- Modify: `CMakeLists.txt` (`nlcore`에 `src/core/SpeedTrial.cpp`)
- Test: `tests/native/core_tests.cpp`

**Interfaces:**
- Consumes: `RateMatches` (Task 4)
- Produces:
  - `struct SpeedStep { enum class Kind { None, Write, Restore }; Kind What; int Candidate; double Value; }`
  - `struct SpeedAttempt { int Candidate; bool Readable; double Old; double Rate; bool Matched; }`
  - `class SpeedTrial`:
    - `enum class Phase { Idle, Holding, Settling, Done, Failed, Aborted }`
    - `explicit SpeedTrial(int Candidates, double HoldSeconds = 2.0, double SettleSeconds = 1.5)`
    - `bool Start(double BaseRate, double BaseWarp)` — 둘 중 하나가 0 이하이거나 이미 시험 중이면 거짓
    - `SpeedStep Tick(double Now, bool Readable, double Current, double Rate)` — 0.1초마다. `Current`는 `Candidate()`의 지금 값
    - `Phase State() const`, `bool Running() const`, `int Candidate() const`, `bool Sticky() const`, `double SavedOld() const`, `double ProbeWarp() const`, `double UnitRate() const`, `const std::vector<SpeedAttempt>& Attempts() const`

- [ ] **Step 1: 실패하는 시험을 쓴다**

include 에 `#include "core/SpeedTrial.hpp"`를 더한다(`Schedule.hpp` 다음). 파일 위쪽 이름 없는 네임스페이스의 `Parse` 함수 뒤에 가짜 게임을 넣는다.

```cpp

	// 가짜 게임: 손잡이 넷 가운데 Live 번이 속도를 정한다(값이 0 보다 클 때. 아니면 배율 1). 흐름 = 57.4 × 배율.
	struct FakeGame
	{
		double Values[4] = { 1, 1, -4, 1 };
		int Live = 0;					// -1 이면 아무 손잡이도 듣지 않는다
		bool Overwrites = false;		// 쓰지 않는 틱마다 게임이 Live 손잡이를 Reset 으로 되돌린다
		double Reset = 1;
		bool Unreadable[4] = {};
		int PauseAtTick = -1;			// 이 틱부터 흐름이 0 이다

		double Flow(int Tick) const
		{
			if (PauseAtTick >= 0 && Tick >= PauseAtTick)
				return 0;
			return 57.4 * (Live >= 0 && Values[Live] > 0 ? Values[Live] : 1);
		}
	};

	// 시험을 끝까지 돌린다(0.1초 간격). 바깥이 할 일을 가짜 게임에 그대로 한다.
	void RunTrial(SpeedTrial& Trial, FakeGame& Game, double Warp = 1)
	{
		if (!Trial.Start(Game.Flow(0), Warp))
			return;
		double now = 10;
		for (int tick = 1; tick <= 400 && Trial.Running(); tick++)
		{
			now += 0.1;
			const int candidate = Trial.Candidate();
			const SpeedStep step = Trial.Tick(now, !Game.Unreadable[candidate], Game.Values[candidate], Game.Flow(tick));
			if (step.What != SpeedStep::Kind::None)
				Game.Values[step.Candidate] = step.Value;
			else if (Game.Overwrites && Game.Live >= 0)
				Game.Values[Game.Live] = Game.Reset;
		}
	}
```

Task 4 의 시험 뒤에 넣는다.

```cpp
	Test("속도 시험: 첫 후보가 들으면 고르고, 쓰기를 멈춰도 남으면 한 번 쓰기다", [] {
		FakeGame game;
		SpeedTrial trial(4);
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Done && trial.Candidate() == 0 && trial.Sticky());
		CHECK(trial.SavedOld() == 1 && trial.ProbeWarp() == 2 && std::abs(trial.UnitRate() - 57.4) < 1e-9);
		CHECK(trial.Attempts().size() == 1 && trial.Attempts()[0].Matched);
	});

	Test("속도 시험: 듣지 않는 후보는 원래 값으로 되돌리고 다음으로 간다", [] {
		FakeGame game;
		game.Live = 2;					// __debug_custom_wrap 의 자리. 원래 값은 -4 다
		SpeedTrial trial(4);
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Done && trial.Candidate() == 2 && trial.SavedOld() == -4);
		CHECK(game.Values[0] == 1 && game.Values[1] == 1 && game.Values[3] == 1);
		CHECK(trial.Attempts().size() == 3 && !trial.Attempts()[0].Matched && !trial.Attempts()[1].Matched && trial.Attempts()[2].Matched);
		CHECK(trial.Attempts()[0].Readable && trial.Attempts()[0].Old == 1);
	});

	Test("속도 시험: 게임이 값을 되돌리면 계속 쓰기다", [] {
		FakeGame game;
		game.Live = 1;
		game.Overwrites = true;
		SpeedTrial trial(4);
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Done && trial.Candidate() == 1 && !trial.Sticky());
	});

	Test("속도 시험: 아무 후보도 듣지 않으면 실패하고 모두 원래 값이다", [] {
		FakeGame game;
		game.Live = -1;
		SpeedTrial trial(4);
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Failed && trial.Candidate() == -1 && !trial.Running());
		CHECK(game.Values[0] == 1 && game.Values[1] == 1 && game.Values[2] == -4 && game.Values[3] == 1);
		CHECK(trial.Attempts().size() == 4);
	});

	Test("속도 시험: 읽을 수 없는 후보는 건드리지 않는다", [] {
		FakeGame game;
		game.Live = 1;
		game.Unreadable[0] = true;
		game.Values[0] = 123;			// 건드렸다면 바뀐다
		SpeedTrial trial(4);
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Done && trial.Candidate() == 1);
		CHECK(game.Values[0] == 123 && !trial.Attempts()[0].Readable && !trial.Attempts()[0].Matched);
	});

	Test("속도 시험: 지금 속도가 1 이 아니어도 고른다", [] {
		FakeGame game;
		game.Values[0] = 2;				// 게임이 2배속이다
		game.Values[1] = 2;
		SpeedTrial trial(4);
		RunTrial(trial, game, 2);
		CHECK(trial.State() == SpeedTrial::Phase::Done && trial.Candidate() == 0 && trial.SavedOld() == 2);
		CHECK(trial.ProbeWarp() == 4 && std::abs(trial.UnitRate() - 57.4) < 1e-9);
	});

	Test("속도 시험: 시험 중에 멈추면 그만두고 원래 값을 되돌린다", [] {
		FakeGame game;
		game.PauseAtTick = 10;			// 첫 후보를 쓰는 도중에 일시정지
		SpeedTrial trial(4);
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Aborted && !trial.Running());
		CHECK(game.Values[0] == 1 && trial.Attempts().empty());

		game.PauseAtTick = -1;			// 풀고 다시 누르면 된다
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Done && trial.Candidate() == 0);
	});

	Test("속도 시험: 멈춰 있으면 시작하지 않는다", [] {
		SpeedTrial trial(4);
		CHECK(!trial.Start(0, 1) && !trial.Start(57.4, 0) && trial.State() == SpeedTrial::Phase::Idle);
		CHECK(trial.Start(57.4, 1) && trial.Running() && !trial.Start(57.4, 1));		// 시험 중에는 다시 시작하지 않는다
	});
```

- [ ] **Step 2: 빌드해서 실패를 본다**

Run: `pwsh -File tools/build.ps1`
Expected: `Cannot open include file: 'core/SpeedTrial.hpp'`

- [ ] **Step 3: 구현한다**

`src/core/SpeedTrial.hpp`:

```cpp
#pragma once
// 게임 속도의 손잡이를 시험으로 고른다. 러너에 기대지 않는다: 값을 읽고 쓰는 일은 바깥이 한다. 스펙: 치트 메뉴 §9.
// 후보 하나마다: 원래 값을 적어 두고, HoldSeconds 동안 시험 값을 계속 쓰게 하고, 흐름이 기대만큼 달라졌으면 고른다.
// 고른 뒤에는 쓰기를 멈추게 하고 SettleSeconds 뒤에도 흐름이 남는지 본다(남으면 한 번 쓰기, 아니면 계속 쓰기).
// 시험 값은 요청한 배율이 아니라 지금 time_warp 의 2배다. 요청한 배율이 지금 속도와 같으면 "달라지지 않았다"와
// "먹었다"를 가릴 수 없기 때문이다.

#include <vector>

namespace NlCore
{
	struct SpeedStep			// Tick 이 바깥에 시키는 일
	{
		enum class Kind { None, Write, Restore };
		Kind What = Kind::None;
		int Candidate = -1;
		double Value = 0;
	};

	struct SpeedAttempt			// 후보 하나를 시험한 기록(로그에 적는다)
	{
		int Candidate = -1;
		bool Readable = false;	// 값을 읽을 수 있었는가
		double Old = 0;			// 시험 전의 값
		double Rate = 0;		// 시험 끝의 흐름
		bool Matched = false;
	};

	class SpeedTrial
	{
	public:
		enum class Phase { Idle, Holding, Settling, Done, Failed, Aborted };

		explicit SpeedTrial(int Candidates, double HoldSeconds = 2.0, double SettleSeconds = 1.5);

		// 시험을 시작한다. BaseRate: 지금의 흐름. BaseWarp: 지금의 time_warp.
		// 둘 중 하나라도 0 이하이거나(멈춰 있다) 이미 시험 중이면 거짓.
		bool Start(double BaseRate, double BaseWarp);

		// 0.1초마다 부른다. Current: 지금 후보(Candidate())의 지금 값. 읽지 못했으면 Readable 이 거짓.
		// 돌려주는 값은 바깥이 할 일이다: Write 는 그 값을 계속 써 넣기 시작하라, Restore 는 쓰기를 멈추고 그 값으로 되돌려라,
		// None 은 쓰기를 멈춰라.
		SpeedStep Tick(double Now, bool Readable, double Current, double Rate);

		Phase State() const { return m_Phase; }
		bool Running() const { return m_Phase == Phase::Holding || m_Phase == Phase::Settling; }
		int Candidate() const { return m_Candidate; }		// 지금 시험하는(또는 고른) 후보. 없으면 -1
		bool Sticky() const { return m_Sticky; }			// Done: 한 번 쓰면 남는가
		double SavedOld() const { return m_Old; }			// 고른 후보의 원래 값
		double ProbeWarp() const { return m_Probe; }		// 시험에 쓰는 값
		double UnitRate() const { return m_BaseWarp > 0 ? m_BaseRate / m_BaseWarp : 0; }	// 배율 1 의 흐름
		const std::vector<SpeedAttempt>& Attempts() const { return m_Attempts; }

	private:
		void Next();

		int m_Count;
		double m_Hold, m_Settle;
		Phase m_Phase = Phase::Idle;
		int m_Candidate = -1;
		bool m_Began = false;		// 지금 후보에 쓰기 시작했는가
		bool m_Sticky = false;
		double m_Old = 0, m_Until = 0;
		double m_BaseRate = 0, m_BaseWarp = 0, m_Probe = 0;
		std::vector<SpeedAttempt> m_Attempts;
	};
}
```

`src/core/SpeedTrial.cpp`:

```cpp
#include "SpeedTrial.hpp"

#include "Rate.hpp"

namespace NlCore
{
	SpeedTrial::SpeedTrial(int Candidates, double HoldSeconds, double SettleSeconds)
		: m_Count(Candidates), m_Hold(HoldSeconds), m_Settle(SettleSeconds)
	{
	}

	bool SpeedTrial::Start(double BaseRate, double BaseWarp)
	{
		if (Running() || BaseRate <= 0 || BaseWarp <= 0 || m_Count <= 0)
			return false;

		m_Phase = Phase::Holding;
		m_Candidate = 0;
		m_Began = false;
		m_Sticky = false;
		m_BaseRate = BaseRate;
		m_BaseWarp = BaseWarp;
		m_Probe = BaseWarp * 2;
		m_Attempts.clear();
		return true;
	}

	void SpeedTrial::Next()
	{
		m_Began = false;
		if (++m_Candidate >= m_Count)
		{
			m_Candidate = -1;
			m_Phase = Phase::Failed;
		}
	}

	SpeedStep SpeedTrial::Tick(double Now, bool Readable, double Current, double Rate)
	{
		if (m_Phase == Phase::Holding)
		{
			if (!m_Began)
			{
				if (!Readable)		// 읽을 수 없는 후보는 쓰지 않는다(없는 변수를 만들지 않는다)
				{
					m_Attempts.push_back({ m_Candidate, false, 0, Rate, false });
					Next();
					return {};
				}
				m_Began = true;
				m_Old = Current;
				m_Until = Now + m_Hold;
				return { SpeedStep::Kind::Write, m_Candidate, m_Probe };
			}
			if (Now < m_Until)
				return { SpeedStep::Kind::Write, m_Candidate, m_Probe };

			const SpeedStep restore{ SpeedStep::Kind::Restore, m_Candidate, m_Old };
			if (Rate <= 0)			// 시험 중에 멈췄다(일시정지). 판정할 수 없다
			{
				m_Phase = Phase::Aborted;
				return restore;
			}

			const bool matched = RateMatches(m_BaseRate, Rate, m_Probe / m_BaseWarp);
			m_Attempts.push_back({ m_Candidate, true, m_Old, Rate, matched });
			if (!matched)
			{
				Next();
				return restore;
			}
			m_Phase = Phase::Settling;
			m_Until = Now + m_Settle;
			return {};
		}

		if (m_Phase == Phase::Settling && Now >= m_Until)
		{
			m_Sticky = RateMatches(m_BaseRate, Rate, m_Probe / m_BaseWarp);
			m_Phase = Phase::Done;
		}
		return {};
	}
}
```

`CMakeLists.txt`의 `nlcore` 목록에 `src/core/Schedule.cpp` 다음 줄로 `  src/core/SpeedTrial.cpp`를 더한다. 이 Task 가 끝난 `nlcore` 목록:

```cmake
add_library(nlcore STATIC
  src/core/AskPath.cpp
  src/core/CheatState.cpp
  src/core/CheatTable.cpp
  src/core/Knobs.cpp
  src/core/Rate.cpp
  src/core/Text.cpp
  src/core/Request.cpp
  src/core/Schedule.cpp
  src/core/SpeedTrial.cpp
)
```

- [ ] **Step 4: 시험이 통과하는지 본다**

Run: `pwsh -File tools/build.ps1; pwsh -File tools/test-native.ps1`
Expected: `ok - 속도 시험: …` 여덟, 종료 코드 0

- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add src/core/SpeedTrial.hpp src/core/SpeedTrial.cpp CMakeLists.txt tests/native/core_tests.cpp
git -C E:\NlToyBox commit -m "feat(core): 게임 속도의 손잡이를 흐름으로 판정해 고르는 시험"
```

---

### Task 6: 접근 층 — 주소를 따라가 읽고, 쓰고, 늘어놓는다

러너에 닿는 코드다. 네이티브 시험이 없다. 이 Task 의 검증은 빌드이고, 동작은 Task 12 의 실행이 `ask`·`poke`로 본다.

쓰는 빌트인과 근거:

| 빌트인 | 근거 |
|---|---|
| `instance_number`, `instance_find`, `variable_instance_get_names`, `variable_instance_get`, `array_length`, `array_get`, `ds_exists`, `ds_map_exists`, `ds_map_find_value`, `ds_map_keys_to_array`, `ds_map_replace`, `ds_list_size`, `ds_list_find_value`, `ds_list_replace`, `variable_struct_exists`, `variable_struct_get`, `variable_struct_set` | 이 러너에서 써 봤다(`src/Dump.cpp`, `src/Finder.cpp`, `src/Tweaks.cpp`, `research/02`, `research/05`) |
| `variable_instance_exists(instance_id, name)`, `variable_instance_set(instance_id, name, val)`, `array_set(variable, index, value)`, `variable_global_set(name, val)`, `instance_exists(obj)`, `is_method(val)` | exe 문자열에 이름이 있고 매뉴얼(Context7 `/yoyogames/gamemaker-manual`)에서 인자를 확인했다. **이 러너에서는 처음 쓴다** |

**Files:**
- Create: `src/Access.hpp`, `src/Access.cpp`
- Modify: `CMakeLists.txt` (`nltoybox`에 `src/Access.cpp`)

**Interfaces:**
- Consumes: `NlCore::AskPath`, `PathStep`, `ParseAskPath`, `FormatStep` (Task 1), `NlCore::Shortest` (Task 2), `NlCore::Quote`, `NlGame::Call`, `NlGame::CallNumber`, `NlGame::IsNumber`, `NlGame::Objects`, `NlGame::Global`, `NlGame::Yytk`, `NlGame::ArrayLength` (`src/Game.hpp`)
- Produces (모두 `namespace NlAccess`):
  - `enum class Holder { None, Global, Instance, Struct, Array, Map, List }`
  - `struct Row { NlCore::PathStep Step; std::string Name, Type, Text, Raw; double Number; bool IsNumber, IsBool, IsString, IsContainer; }`
  - `struct RootObject { std::string Name; int Count; }`
  - `bool Read(const NlCore::AskPath&, YYTK::RValue& Out, std::string& Why)`
  - `bool Open(const NlCore::AskPath&, YYTK::RValue& Out, Holder& Kind, std::string& Why)`
  - `bool Write(const NlCore::AskPath&, const YYTK::RValue& Value, std::string& Why)`
  - `bool ReadNumber(const std::string& Path, double& Out)`
  - `bool WriteNumber(const std::string& Path, double Number, std::string& Why)`
  - `bool WriteString(const std::string& Path, const std::string& Text, std::string& Why)`
  - `Row Describe(const NlCore::PathStep&, const YYTK::RValue&)`
  - `Holder Classify(const YYTK::RValue&)`
  - `double ForEachChild(const YYTK::RValue&, Holder, const std::function<bool(const NlCore::PathStep&, const YYTK::RValue&)>& Visit)`
  - `bool List(const NlCore::AskPath&, Holder As, size_t Limit, std::vector<Row>& Rows, size_t& Total, std::string& Why)`
  - `std::vector<RootObject> LiveObjects()`, `int InstanceCount(const std::string& Object)`, `bool InGame()`

- [ ] **Step 1: 머리 파일을 쓴다**

`src/Access.hpp`:

```cpp
#pragma once
// 주소(core/AskPath)를 따라가 게임의 값을 읽고, 쓰고, 자식을 늘어놓는다. 스펙: 치트 메뉴 §5.
// 모든 함수는 게임 스레드의 틱에서만 부른다. RValue 는 부른 쪽의 함수 안에서만 든다.

#include "core/AskPath.hpp"

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>
#include <vector>

namespace NlAccess
{
	// 값을 어떤 그릇으로 여는가. 수는 그릇이 아니지만 ds 번호로 열 수 있다(Map, List).
	enum class Holder { None, Global, Instance, Struct, Array, Map, List };

	// 값 하나를 글로 적은 것. RValue 를 담지 않으므로 틱 너머로 들고 있어도 된다.
	struct Row
	{
		NlCore::PathStep Step;		// 부모에서 이 값으로 가는 단계
		std::string Name;			// 표에 보일 이름
		std::string Type;			// "number", "bool", "string", "struct", "method", "array", "ref", "undefined", …
		std::string Text;			// 값의 글. 그릇이면 크기
		std::string Raw;			// 문자열이면 그 글 그대로
		double Number = 0;			// 수·불리언이면 그 값
		bool IsNumber = false, IsBool = false, IsString = false, IsContainer = false;
	};

	struct RootObject
	{
		std::string Name;
		int Count = 0;				// instance_number. 자식 오브젝트의 인스턴스도 센다
	};

	// 주소가 가리키는 값. 없으면 거짓이고 Why 에 까닭.
	bool Read(const NlCore::AskPath& Path, YYTK::RValue& Out, std::string& Why);
	// 값과 그것을 여는 방식.
	bool Open(const NlCore::AskPath& Path, YYTK::RValue& Out, Holder& Kind, std::string& Why);

	// 있는 자리에만 쓴다. 쓴 뒤 뿌리부터 다시 읽어 그 값인지 본다. 아니면 거짓이고 Why 는 "did not stick".
	bool Write(const NlCore::AskPath& Path, const YYTK::RValue& Value, std::string& Why);

	// 수(불리언 포함)로 읽고 쓴다. 수가 아닌 값은 수로 덮어쓰지 않는다. 형은 원래 값의 것을 따른다.
	bool ReadNumber(const std::string& Path, double& Out);
	bool WriteNumber(const std::string& Path, double Number, std::string& Why);
	// 문자열인 자리에만 쓴다.
	bool WriteString(const std::string& Path, const std::string& Text, std::string& Why);

	// 값을 글로 적는다.
	Row Describe(const NlCore::PathStep& Step, const YYTK::RValue& Value);

	// 값을 보고 여는 방식을 정한다(구조체, 배열, 있는 인스턴스를 가리키는 ref). 그 밖은 None.
	Holder Classify(const YYTK::RValue& Value);

	// 그릇의 자식을 차례로 넘긴다. Visit 이 거짓을 돌려주면 그만둔다.
	// 돌려주는 값: 자식의 수(구조체와 전역은 도중에 그만두면 그때까지 본 수). 그릇이 아니거나 없는 ds 면 -1.
	double ForEachChild(const YYTK::RValue& Value, Holder Kind,
		const std::function<bool(const NlCore::PathStep&, const YYTK::RValue&)>& Visit);

	// 주소가 가리키는 그릇의 자식들. As: 그것이 수일 때 Map 이나 List 로 연다. Limit 개까지만 적고 Total 은 전체 수.
	bool List(const NlCore::AskPath& Path, Holder As, size_t Limit, std::vector<Row>& Rows, size_t& Total, std::string& Why);

	// 인스턴스가 하나라도 있는 오브젝트들.
	std::vector<RootObject> LiveObjects();
	int InstanceCount(const std::string& Object);

	// 게임 화면인가: o_main_menu 가 없고 o_character 가 있다(research/02. 새 게임에서 잰 것이다).
	bool InGame();
}
```

- [ ] **Step 2: 구현한다**

`src/Access.cpp`:

```cpp
#include "Access.hpp"

#include "Game.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <cmath>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::AskPath;
using NlCore::PathStep;
using NlCore::Shortest;

namespace
{
	// ds 형 상수. 이 러너에서 맞는 것을 만들고 지워서 확인했다(research/02).
	constexpr double k_DsMap = 1, k_DsList = 2;

	struct Cursor			// 따라가는 동안 든 값
	{
		Holder Kind = Holder::None;
		RValue Value;
	};

	bool Truthy(const char* Name, const std::vector<RValue>& Args)
	{
		return NlGame::CallNumber(Name, Args, 0) > 0;
	}

	RValue Str(const std::string& Value)
	{
		return RValue(std::string_view(Value));
	}

	// 0 이상의 정수인 수인가(ds 번호).
	bool WholeNumber(const RValue& Value, double& Out)
	{
		if (!NlGame::IsNumber(Value))
			return false;
		const double number = Value.ToDouble();
		if (!(number >= 0) || number != std::floor(number))
			return false;
		Out = number;
		return true;
	}

	// 오브젝트의 번호. 없으면 -1.
	double ObjectIndex(const std::string& Name)
	{
		for (const NlGame::Object& object : NlGame::Objects())
			if (object.Name == Name)
				return object.Index;
		return -1;
	}

	bool DsExists(double Id, double Type)
	{
		return Truthy("ds_exists", { RValue(Id), RValue(Type) });
	}

	// 전역의 멤버를 이름으로 찾는다. Game::Resolve 가 쓰던 길이다(열거. 없는 이름으로 GetInstanceMember 를 부르지 않는다).
	bool MemberOfGlobal(const std::string& Name, RValue& Out)
	{
		CInstance* global = NlGame::Global();
		if (!global)
			return false;

		bool found = false;
		NlGame::Yytk()->EnumInstanceMembers(RValue(global), [&](const char* MemberName, RValue* Value) -> bool
		{
			if (!MemberName || !Value || Name != MemberName)
				return false;
			Out = *Value;
			found = true;
			return true;	// 찾았으니 그만 돈다
		});
		return found;
	}

	// ds_map 의 키. 글로 된 키가 없고 그 글이 수이면 수로 된 키도 본다(battle_params 의 "0", "1").
	bool MapKey(double Map, const std::string& Name, RValue& Key)
	{
		Key = Str(Name);
		if (Truthy("ds_map_exists", { RValue(Map), Key }))
			return true;

		double number = 0;
		if (!NlCore::ParseNumber(Name, number))
			return false;
		Key = RValue(number);
		return Truthy("ds_map_exists", { RValue(Map), Key });
	}

	bool Root(const AskPath& Path, Cursor& Out, std::string& Why)
	{
		if (!Path.Error.empty())
		{
			Why = Path.Error;
			return false;
		}
		if (Path.Root == "global")
		{
			CInstance* global = NlGame::Global();
			if (!global)
			{
				Why = "no global instance";
				return false;
			}
			Out = { Holder::Global, RValue(global) };
			return true;
		}
		if (Path.Root == "inst")
		{
			const double object = ObjectIndex(Path.Object);
			if (object < 0)
			{
				Why = "no such object: " + Path.Object;
				return false;
			}
			if (NlGame::CallNumber("instance_number", { RValue(object) }, 0) <= Path.Number)
			{
				Why = "no instance of " + Path.Object + " at " + Shortest(Path.Number);
				return false;
			}
			RValue id;
			if (!NlGame::Call("instance_find", { RValue(object), RValue(Path.Number) }, id)
				|| (NlGame::IsNumber(id) && id.ToDouble() < 0))		// noone (-4)
			{
				Why = "instance_find failed for " + Path.Object;
				return false;
			}
			Out = { Holder::Instance, id };
			return true;
		}

		// map:<번호>, list:<번호>: 번호를 든다. 그 번호의 ds 가 있는지는 단계(@, #)나 List 가 본다.
		Out = { Holder::None, RValue(Path.Number) };
		return true;
	}

	// 한 단계를 읽는다.
	bool Step(const Cursor& From, const PathStep& S, Cursor& To, std::string& Why)
	{
		RValue next;
		double id = 0;
		switch (S.Kind)
		{
		case '.':
			if (From.Kind == Holder::Global)
			{
				if (!MemberOfGlobal(S.Name, next))
				{
					Why = "no global named " + S.Name;
					return false;
				}
			}
			else if (From.Kind == Holder::Instance)
			{
				const RValue name = Str(S.Name);
				if (!Truthy("variable_instance_exists", { From.Value, name })
					|| !NlGame::Call("variable_instance_get", { From.Value, name }, next))
				{
					Why = "no instance variable " + S.Name;
					return false;
				}
			}
			else if (From.Value.IsStruct())
			{
				const RValue name = Str(S.Name);
				if (!Truthy("variable_struct_exists", { From.Value, name })
					|| !NlGame::Call("variable_struct_get", { From.Value, name }, next))
				{
					Why = "no member " + S.Name;
					return false;
				}
			}
			else
			{
				Why = "." + S.Name + ": not a struct or an instance";
				return false;
			}
			break;

		case '[':
			if (!From.Value.IsArray() || S.Index >= NlGame::ArrayLength(From.Value)
				|| !NlGame::Call("array_get", { From.Value, RValue(S.Index) }, next))
			{
				Why = "[" + Shortest(S.Index) + "]: not an array or out of range";
				return false;
			}
			break;

		case '@':
		{
			RValue key;
			if (!WholeNumber(From.Value, id) || !DsExists(id, k_DsMap) || !MapKey(id, S.Name, key)
				|| !NlGame::Call("ds_map_find_value", { RValue(id), key }, next))
			{
				Why = "@" + S.Name + ": no such ds_map or key";
				return false;
			}
			break;
		}

		case '#':
			if (!WholeNumber(From.Value, id) || !DsExists(id, k_DsList)
				|| S.Index >= NlGame::CallNumber("ds_list_size", { RValue(id) }, 0)
				|| !NlGame::Call("ds_list_find_value", { RValue(id), RValue(S.Index) }, next))
			{
				Why = "#" + Shortest(S.Index) + ": no such ds_list or out of range";
				return false;
			}
			break;

		default:
			Why = "unknown step";
			return false;
		}

		To = { NlAccess::Classify(next), next };
		return true;
	}

	// 뿌리에서 앞의 Steps 단계까지 따라간다.
	bool Resolve(const AskPath& Path, size_t Steps, Cursor& Out, std::string& Why)
	{
		Cursor at;
		if (!Root(Path, at, Why))
			return false;
		for (size_t i = 0; i < Steps; i++)
		{
			Cursor next;
			if (!Step(at, Path.Steps[i], next, Why))
				return false;
			at = next;
		}
		Out = at;
		return true;
	}

	// 같은 값인가(쓴 뒤에 다시 읽어 견준다).
	bool Same(const RValue& A, const RValue& B)
	{
		if (A.IsString() || B.IsString())
			return A.IsString() && B.IsString() && A.ToString() == B.ToString();
		return NlGame::IsNumber(A) && NlGame::IsNumber(B) && A.ToDouble() == B.ToDouble();
	}

	// 수를 원래 값의 형에 맞춰 만든다(불리언은 불리언으로, 정수형은 정수로).
	RValue NumberLike(const RValue& Old, double Number)
	{
		if (Old.m_Kind == VALUE_BOOL)
			return RValue(Number != 0);
		if ((Old.m_Kind == VALUE_INT32 || Old.m_Kind == VALUE_INT64) && Number == std::floor(Number))
			return RValue(static_cast<int64_t>(Number));
		return RValue(Number);
	}
}

bool NlAccess::Open(const AskPath& Path, RValue& Out, Holder& Kind, std::string& Why)
{
	Cursor at;
	if (!Resolve(Path, Path.Steps.size(), at, Why))
		return false;
	Out = at.Value;
	Kind = at.Kind;
	return true;
}

bool NlAccess::Read(const AskPath& Path, RValue& Out, std::string& Why)
{
	Holder ignored = Holder::None;
	return Open(Path, Out, ignored, Why);
}

bool NlAccess::Write(const AskPath& Path, const RValue& Value, std::string& Why)
{
	if (!Path.Error.empty() || Path.Steps.empty())
	{
		Why = Path.Error.empty() ? "nothing to write: the path has no steps" : Path.Error;
		return false;
	}

	// 없는 것을 만들지 않는다: 지금 읽히는 자리에만 쓴다.
	const size_t last = Path.Steps.size() - 1;
	const PathStep& step = Path.Steps[last];
	Cursor parent, old;
	if (!Resolve(Path, last, parent, Why) || !Step(parent, step, old, Why))
		return false;

	RValue ignored, key;
	double id = 0;
	switch (step.Kind)
	{
	case '.':
		if (parent.Kind == Holder::Global)
			NlGame::Call("variable_global_set", { Str(step.Name), Value }, ignored);
		else if (parent.Kind == Holder::Instance)
			NlGame::Call("variable_instance_set", { parent.Value, Str(step.Name), Value }, ignored);
		else
			NlGame::Call("variable_struct_set", { parent.Value, Str(step.Name), Value }, ignored);
		break;
	case '[':
		NlGame::Call("array_set", { parent.Value, RValue(step.Index), Value }, ignored);
		break;
	case '@':
		if (WholeNumber(parent.Value, id) && MapKey(id, step.Name, key))
			NlGame::Call("ds_map_replace", { RValue(id), key, Value }, ignored);
		break;
	case '#':
		if (WholeNumber(parent.Value, id))
			NlGame::Call("ds_list_replace", { RValue(id), RValue(step.Index), Value }, ignored);
		break;
	}

	// 반환값이 없는 빌트인들이다. 뿌리부터 다시 읽어 판정한다.
	RValue again;
	if (!Read(Path, again, Why))
		return false;
	if (!Same(again, Value))
	{
		Why = "did not stick";
		return false;
	}
	return true;
}

bool NlAccess::ReadNumber(const std::string& Path, double& Out)
{
	RValue value;
	std::string why;
	if (!Read(NlCore::ParseAskPath(Path), value, why) || !NlGame::IsNumber(value))
		return false;
	Out = value.ToDouble();
	return true;
}

bool NlAccess::WriteNumber(const std::string& Path, double Number, std::string& Why)
{
	const AskPath path = NlCore::ParseAskPath(Path);
	RValue old;
	if (!Read(path, old, Why))
		return false;
	if (!NlGame::IsNumber(old))
	{
		Why = "not a number (" + old.GetKindName() + ")";
		return false;
	}
	return Write(path, NumberLike(old, Number), Why);
}

bool NlAccess::WriteString(const std::string& Path, const std::string& Text, std::string& Why)
{
	const AskPath path = NlCore::ParseAskPath(Path);
	RValue old;
	if (!Read(path, old, Why))
		return false;
	if (!old.IsString())
	{
		Why = "not a string (" + old.GetKindName() + ")";
		return false;
	}
	return Write(path, Str(Text), Why);
}

Holder NlAccess::Classify(const RValue& Value)
{
	if (Value.IsArray())
		return Holder::Array;
	if (Value.IsStruct())
		return Holder::Struct;
	if (Value.m_Kind == VALUE_REF && Truthy("instance_exists", { Value }))
		return Holder::Instance;
	return Holder::None;
}

NlAccess::Row NlAccess::Describe(const PathStep& Step, const RValue& Value)
{
	Row row;
	row.Step = Step;
	row.Name = Step.Kind == '.' || Step.Kind == '@' ? Step.Name : NlCore::FormatStep(Step);

	if (Value.IsString())
	{
		row.Type = "string";
		row.IsString = true;
		row.Raw = Value.ToString();
		row.Text = NlCore::Quote(row.Raw, 80);
	}
	else if (Value.IsArray())
	{
		row.Type = "array";
		row.IsContainer = true;
		row.Text = "[" + Shortest(NlGame::ArrayLength(Value)) + "]";
	}
	else if (Value.IsStruct())
	{
		// 메서드도 YYToolkit 에서는 구조체로 보인다(VALUE_OBJECT). 빌트인으로 가린다.
		if (Truthy("is_method", { Value }))
		{
			row.Type = "method";
			row.Text = "함수";
		}
		else
		{
			row.Type = "struct";
			row.IsContainer = true;
			row.Text = "{...}";
		}
	}
	else if (Value.m_Kind == VALUE_REF)
	{
		row.Type = "ref";
		row.IsContainer = Truthy("instance_exists", { Value });
		row.Text = row.IsContainer ? "인스턴스" : "ref";
	}
	else if (Value.m_Kind == VALUE_UNDEFINED || Value.m_Kind == VALUE_UNSET)
		row.Type = "undefined";
	else if (NlGame::IsNumber(Value))
	{
		row.Type = Value.GetKindName();
		row.IsNumber = true;
		row.IsBool = Value.m_Kind == VALUE_BOOL;
		row.Number = Value.ToDouble();
		row.Text = row.IsBool ? (row.Number != 0 ? "true" : "false") : Shortest(row.Number);
	}
	else
		row.Type = Value.GetKindName();
	return row;
}

double NlAccess::ForEachChild(const RValue& Value, Holder Kind, const std::function<bool(const PathStep&, const RValue&)>& Visit)
{
	double id = 0;
	switch (Kind)
	{
	case Holder::Global:
	case Holder::Struct:
	{
		if (!Value.IsStruct())
			return -1;
		double count = 0;
		// 콜백이 참을 돌려주면 열거가 멈춘다(YYToolkit MI_Public.cpp 의 EnumInstanceMembers).
		NlGame::Yytk()->EnumInstanceMembers(Value, [&](const char* MemberName, RValue* Member) -> bool
		{
			count++;
			if (!MemberName || !Member)
				return false;
			return !Visit({ '.', MemberName, 0 }, *Member);
		});
		return count;
	}

	case Holder::Instance:
	{
		RValue names;
		if (!NlGame::Call("variable_instance_get_names", { Value }, names) || !names.IsArray())
			return -1;
		const std::vector<RValue> list = names.ToVector();
		for (const RValue& name : list)
		{
			RValue member;
			if (!name.IsString() || !NlGame::Call("variable_instance_get", { Value, name }, member))
				continue;
			if (!Visit({ '.', name.ToString(), 0 }, member))
				break;
		}
		return static_cast<double>(list.size());
	}

	case Holder::Array:
	{
		if (!Value.IsArray())
			return -1;
		const double length = NlGame::ArrayLength(Value);
		for (double i = 0; i < length; i++)
		{
			RValue item;
			if (!NlGame::Call("array_get", { Value, RValue(i) }, item))
				continue;
			if (!Visit({ '[', "", i }, item))
				break;
		}
		return length < 0 ? -1 : length;
	}

	case Holder::Map:
	{
		if (!WholeNumber(Value, id) || !DsExists(id, k_DsMap))
			return -1;
		RValue keys;
		if (!NlGame::Call("ds_map_keys_to_array", { RValue(id) }, keys) || !keys.IsArray())
			return 0;		// 있는 ds_map 인데 키의 배열을 얻지 못했다: 빈 것으로 본다
		const std::vector<RValue> list = keys.ToVector();
		for (const RValue& key : list)
		{
			RValue item;
			if (!NlGame::Call("ds_map_find_value", { RValue(id), key }, item))
				continue;
			const std::string name = key.IsString() ? key.ToString()
				: NlGame::IsNumber(key) ? Shortest(key.ToDouble()) : key.GetKindName();
			if (!Visit({ '@', name, 0 }, item))
				break;
		}
		return static_cast<double>(list.size());
	}

	case Holder::List:
	{
		if (!WholeNumber(Value, id) || !DsExists(id, k_DsList))
			return -1;
		const double size = NlGame::CallNumber("ds_list_size", { RValue(id) }, 0);
		for (double i = 0; i < size; i++)
		{
			RValue item;
			if (!NlGame::Call("ds_list_find_value", { RValue(id), RValue(i) }, item))
				continue;
			if (!Visit({ '#', "", i }, item))
				break;
		}
		return size;
	}

	default:
		return -1;
	}
}

bool NlAccess::List(const AskPath& Path, Holder As, size_t Limit, std::vector<Row>& Rows, size_t& Total, std::string& Why)
{
	Rows.clear();
	Total = 0;

	RValue value;
	Holder kind = Holder::None;
	if (!Open(Path, value, kind, Why))
		return false;
	if (kind == Holder::None)
	{
		// 수는 ds 번호로만 열 수 있다. map:<번호> 와 list:<번호> 는 뿌리가 방식을 말한다.
		if (Path.Steps.empty() && Path.Root == "map")
			As = Holder::Map;
		else if (Path.Steps.empty() && Path.Root == "list")
			As = Holder::List;
		if (!NlGame::IsNumber(value) || (As != Holder::Map && As != Holder::List))
		{
			Why = "not a container";
			return false;
		}
		kind = As;
	}

	const double count = ForEachChild(value, kind, [&](const PathStep& step, const RValue& child) {
		if (Rows.size() < Limit)
			Rows.push_back(Describe(step, child));
		return true;		// 끝까지 센다
	});
	if (count < 0)
	{
		Why = kind == Holder::Map ? "no such ds_map" : kind == Holder::List ? "no such ds_list" : "not a container";
		return false;
	}
	Total = static_cast<size_t>(count);

	// 이름이 있는 것은 이름순으로(열거의 차례는 실행마다 다를 수 있다). 배열과 리스트는 자리순 그대로.
	if (kind != Holder::Array && kind != Holder::List)
		std::stable_sort(Rows.begin(), Rows.end(), [](const Row& a, const Row& b) { return a.Name < b.Name; });
	return true;
}

std::vector<NlAccess::RootObject> NlAccess::LiveObjects()
{
	std::vector<RootObject> live;
	for (const NlGame::Object& object : NlGame::Objects())
	{
		const int count = static_cast<int>(NlGame::CallNumber("instance_number", { RValue(object.Index) }, 0));
		if (count > 0)
			live.push_back({ object.Name, count });
	}
	return live;
}

int NlAccess::InstanceCount(const std::string& Object)
{
	const double index = ObjectIndex(Object);
	return index < 0 ? 0 : static_cast<int>(NlGame::CallNumber("instance_number", { RValue(index) }, 0));
}

bool NlAccess::InGame()
{
	return InstanceCount("o_character") > 0 && InstanceCount("o_main_menu") == 0;
}
```

`CMakeLists.txt`의 `add_library(nltoybox SHARED` 목록에서 `src/ModuleMain.cpp` 다음 줄에 `  src/Access.cpp`를 더한다.

- [ ] **Step 3: 빌드한다**

Run: `pwsh -File tools/build.ps1`
Expected: `build ok -> E:\NlToyBox\build\NlToyBox.dll`. 경고 `C4244`(double → size_t 등)가 나오면 그 줄에 `static_cast`를 넣어 없앤다.

- [ ] **Step 4: 네이티브 시험이 그대로인지 본다**

Run: `pwsh -File tools/test-native.ps1`
Expected: 종료 코드 0

- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add src/Access.hpp src/Access.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "feat(module): 주소를 따라가 게임의 값을 읽고 쓰고 늘어놓는 접근 층"
```

---

### Task 7: 찾기

**Files:**
- Create: `src/Search.hpp`, `src/Search.cpp`
- Modify: `CMakeLists.txt` (`nltoybox`에 `src/Search.cpp`)

**Interfaces:**
- Consumes: `NlAccess::ForEachChild`, `NlAccess::Describe`, `NlAccess::ReadNumber`, `NlAccess::Holder` (Task 6), `NlCore::PathTable` (`src/core/PathTable.hpp`), `NlCore::FormatStep` (Task 1), `NlCore::Shortest` (Task 2)
- Produces (모두 `namespace NlSearch`):
  - `struct Spec { std::string Name; bool HasValue; double Value; bool Globals, Instances, Ds; }`
  - `struct Hit { std::string Path, Type, Text; }`
  - `struct Result { std::vector<Hit> Hits; size_t Visited; bool Truncated; double Seconds; }`
  - `Result Run(const Spec& Spec)`, `void Refine(std::vector<Hit>& Hits, double Value)`

- [ ] **Step 1: 머리 파일을 쓴다**

`src/Search.hpp`:

```cpp
#pragma once
// 이름이나 값으로 게임의 값을 찾는다. 스펙: 치트 메뉴 §7. 게임 스레드의 틱에서만 부른다.
// 한 번의 호출 안에서 끝낸다(RValue 를 틱 너머로 들지 않으려고). 그동안 게임이 멈춘다.

#include <string>
#include <vector>

namespace NlSearch
{
	struct Spec
	{
		std::string Name;			// 이름의 일부(대소문자를 가리지 않는다). 비면 이름을 보지 않는다
		bool HasValue = false;
		double Value = 0;			// HasValue 면 이 수와 같은 값만
		bool Globals = true, Instances = true, Ds = false;
	};

	struct Hit
	{
		std::string Path;			// AskPath 의 주소
		std::string Type, Text;
	};

	struct Result
	{
		std::vector<Hit> Hits;
		size_t Visited = 0;
		bool Truncated = false;		// 한도(방문 수, 시간, 결과 수)에 걸려 일부만 봤다
		double Seconds = 0;
	};

	// 이름과 값 가운데 하나는 있어야 한다(둘 다 없으면 빈 결과). 둘 다 있으면 둘 다 맞는 것만.
	Result Run(const Spec& Spec);

	// 지금 값이 Value 인 것만 남긴다.
	void Refine(std::vector<Hit>& Hits, double Value);
}
```

- [ ] **Step 2: 구현한다**

`src/Search.cpp`:

```cpp
#include "Search.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "core/PathTable.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <chrono>
#include <deque>
#include <unordered_set>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::PathStep;
using Clock = std::chrono::steady_clock;

namespace
{
	// 한도. 스펙 §7.
	constexpr int k_MaxDepth = 10;
	constexpr size_t k_MaxVisited = 3'000'000;
	constexpr size_t k_MaxHits = 500;
	constexpr double k_MaxArray = 4096;
	constexpr double k_MaxSeconds = 3.0;
	constexpr int k_MaxPerObject = 64;			// 오브젝트마다 들어가는 인스턴스 수
	constexpr int k_MaxDsId = 20000;			// ds 번호를 훑는 범위. 게임 안에서 본 가장 큰 번호는 1,467 이다(research/02)
	constexpr int k_MaxMissing = 500;			// 이만큼 잇달아 비어 있으면 그만 훑는다(번호는 0 부터 빈틈없이 배정돼 있었다)

	char LowerChar(char C)
	{
		return C >= 'A' && C <= 'Z' ? static_cast<char>(C - 'A' + 'a') : C;
	}

	std::string Lower(std::string Text)
	{
		for (char& c : Text)
			c = LowerChar(c);
		return Text;
	}

	// Text 안에 LowerPart 가 있는가(영문 대소문자를 가리지 않는다). LowerPart 는 이미 소문자다.
	bool ContainsNoCase(const std::string& Text, const std::string& LowerPart)
	{
		if (LowerPart.size() > Text.size())
			return false;
		for (size_t at = 0; at + LowerPart.size() <= Text.size(); at++)
		{
			size_t k = 0;
			while (k < LowerPart.size() && LowerChar(Text[at + k]) == LowerPart[k])
				k++;
			if (k == LowerPart.size())
				return true;
		}
		return false;
	}

	struct Pending
	{
		RValue Value;
		Holder Kind;
		int Node;
		int Depth;
	};

	// 너비 우선으로 내려간다. 모든 RValue 는 Run 이 끝나기 전에 사라진다.
	class Walker
	{
	public:
		explicit Walker(const NlSearch::Spec& Spec) : m_Spec(Spec), m_Name(Lower(Spec.Name)), m_Start(Clock::now()) {}

		// 뿌리 하나와 그 아래를 다 본다. 뿌리 자체는 맞는 것으로 치지 않는다.
		void Root(const RValue& Value, Holder Kind, const std::string& Address)
		{
			if (m_Result.Truncated)
				return;
			m_Queue.push_back({ Value, Kind, m_Paths.Add(-1, Address), 0 });
			while (!m_Queue.empty() && !m_Result.Truncated)
			{
				const Pending item = m_Queue.front();		// Offer 가 큐에 더 넣으므로 사본으로 든다
				m_Queue.pop_front();
				NlAccess::ForEachChild(item.Value, item.Kind, [&](const PathStep& step, const RValue& child) {
					Offer(child, item.Node, step, item.Depth + 1);
					return !m_Result.Truncated;
				});
			}
			m_Queue.clear();
		}

		NlSearch::Result Finish()
		{
			m_Result.Seconds = Elapsed();
			return std::move(m_Result);
		}

	private:
		double Elapsed() const
		{
			return std::chrono::duration<double>(Clock::now() - m_Start).count();
		}

		void Offer(const RValue& Value, int Parent, const PathStep& Step, int Depth)
		{
			m_Result.Visited++;
			if (m_Result.Visited > k_MaxVisited || ((m_Result.Visited & 0xFFF) == 0 && Elapsed() > k_MaxSeconds))
			{
				m_Result.Truncated = true;
				return;
			}

			// 이름은 멤버와 ds_map 의 키에만 있다. 배열과 리스트의 원소는 값으로만 맞는다.
			const bool name_ok = m_Name.empty() || ((Step.Kind == '.' || Step.Kind == '@') && ContainsNoCase(Step.Name, m_Name));
			const bool value_ok = !m_Spec.HasValue || (NlGame::IsNumber(Value) && Value.ToDouble() == m_Spec.Value);
			if (name_ok && value_ok)
			{
				if (m_Result.Hits.size() >= k_MaxHits)
				{
					m_Result.Truncated = true;
					return;
				}
				const NlAccess::Row row = NlAccess::Describe(Step, Value);
				m_Result.Hits.push_back({ m_Paths.Path(Parent) + NlCore::FormatStep(Step), row.Type, row.Text });
			}

			// 구조체와 배열만 내려간다. ref(인스턴스)는 인스턴스 뿌리에서 따로 본다.
			const Holder kind = Value.IsArray() ? Holder::Array : Value.IsStruct() ? Holder::Struct : Holder::None;
			if (kind == Holder::None || Depth >= k_MaxDepth)
				return;
			if (kind == Holder::Array && NlGame::ArrayLength(Value) > k_MaxArray)
				return;
			if (Value.m_Pointer && !m_Seen.insert(Value.m_Pointer).second)
				return;			// 너비 우선이라 처음 닿은 길이 가장 얕다
			m_Queue.push_back({ Value, kind, m_Paths.Add(Parent, NlCore::FormatStep(Step)), Depth });
		}

		const NlSearch::Spec& m_Spec;
		std::string m_Name;
		Clock::time_point m_Start;
		NlCore::PathTable m_Paths;
		std::deque<Pending> m_Queue;
		std::unordered_set<const void*> m_Seen;
		NlSearch::Result m_Result;
	};
}

NlSearch::Result NlSearch::Run(const Spec& Spec)
{
	Walker walker(Spec);
	if (Spec.Name.empty() && !Spec.HasValue)
		return walker.Finish();

	if (Spec.Globals)
		if (CInstance* global = NlGame::Global())
			walker.Root(RValue(global), Holder::Global, "global");

	if (Spec.Instances)
	{
		// 인스턴스 수가 적은 오브젝트부터 본다. instance_number 와 instance_find 는 자식 오브젝트의 인스턴스도
		// 포함하므로(매뉴얼), 그래야 인스턴스가 부모가 아니라 자기 오브젝트의 이름으로 적힌다(Dump.cpp 의 CollectInstances 와 같다).
		std::vector<std::pair<int, const NlGame::Object*>> order;
		for (const NlGame::Object& object : NlGame::Objects())
		{
			const int count = static_cast<int>(NlGame::CallNumber("instance_number", { RValue(object.Index) }, 0));
			if (count > 0)
				order.push_back({ count, &object });
		}
		std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

		std::unordered_set<int64_t> seen;
		for (const auto& [count, object] : order)
			for (int n = 0; n < count && n < k_MaxPerObject; n++)
			{
				RValue id;
				if (!NlGame::Call("instance_find", { RValue(object->Index), RValue(static_cast<double>(n)) }, id))
					continue;
				if ((NlGame::IsNumber(id) && id.ToDouble() < 0) || !seen.insert(id.m_i64).second)		// noone, 이미 본 것
					continue;
				walker.Root(id, Holder::Instance, "inst:" + object->Name + (n > 0 ? ":" + std::to_string(n) : ""));
			}
	}

	if (Spec.Ds)
	{
		// ds 형 상수 1(map)과 2(list)는 이 러너에서 맞다(research/02).
		int missing = 0;
		for (int id = 0; id < k_MaxDsId && missing < k_MaxMissing; id++)
		{
			const RValue number(static_cast<double>(id));
			const bool map = NlGame::CallNumber("ds_exists", { number, RValue(1.0) }, 0) > 0;
			const bool list = NlGame::CallNumber("ds_exists", { number, RValue(2.0) }, 0) > 0;
			missing = map || list ? 0 : missing + 1;
			if (map)
				walker.Root(number, Holder::Map, "map:" + std::to_string(id));
			if (list)
				walker.Root(number, Holder::List, "list:" + std::to_string(id));
		}
	}
	return walker.Finish();
}

void NlSearch::Refine(std::vector<Hit>& Hits, double Value)
{
	std::erase_if(Hits, [&](Hit& hit) {
		double now = 0;
		if (!NlAccess::ReadNumber(hit.Path, now) || now != Value)
			return true;
		hit.Text = NlCore::Shortest(now);
		return false;
	});
}
```

`CMakeLists.txt`의 `nltoybox` 목록에서 `src/Game.cpp` 다음 줄에 `  src/Search.cpp`를 더한다.

- [ ] **Step 3: 빌드한다**

Run: `pwsh -File tools/build.ps1`
Expected: `build ok -> …\NlToyBox.dll`

- [ ] **Step 4: 커밋**

```powershell
git -C E:\NlToyBox add src/Search.hpp src/Search.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "feat(module): 이름과 값으로 게임의 값을 찾고 다시 거른다"
```

---

### Task 8: 탐색기

그리는 쪽은 러너를 부르지 않는다. 창에서 한 일은 `Command`가 되어 큐에 들어가고 다음 틱에 실행된다.

**Files:**
- Create: `src/Explorer.hpp`, `src/Explorer.cpp`
- Modify: `CMakeLists.txt` (`nltoybox`에 `src/Explorer.cpp`)

**Interfaces:**
- Consumes: `NlAccess::*` (Task 6), `NlSearch::*` (Task 7), `NlCore::LockLine` (Task 2), `NlCore::ParseAskPath`·`FormatAskPath`·`ParentPath`·`FormatStep` (Task 1), `NlCore::Shortest`·`Trim`·`ParseNumber`·`Fixed`
- Produces (모두 `namespace NlExplorer`):
  - `using LogFn = std::function<void(const std::string&)>`
  - `void Init(LogFn Log, const std::vector<std::string>& Pins, const std::vector<NlCore::LockLine>& Locks)`
  - `void GameTick(double Now, bool Shown)`
  - `void Draw()`
  - `void Navigate(const std::string& Address)`
  - `bool TakeChanges(std::vector<std::string>& Pins, std::vector<NlCore::LockLine>& Locks)`
  - `int ActiveLocks()`, `void ReleaseAll()`

- [ ] **Step 1: 머리 파일을 쓴다**

`src/Explorer.hpp`:

```cpp
#pragma once
// 탐색기. 게임의 아무 값이나 보고 고친다: 훑기, 즐겨찾기·잠금, 찾기. 스펙: 치트 메뉴 §7.
// 그리는 쪽(Draw)은 글로 된 스냅샷만 읽고 명령을 큐에 넣는다. 러너는 GameTick 만 건드린다.

#include "core/CheatState.hpp"

#include <functional>
#include <string>
#include <vector>

namespace NlExplorer
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번. 상태 파일의 즐겨찾기와 잠금을 받는다. 잠금은 꺼진 채로 둔다.
	void Init(LogFn Log, const std::vector<std::string>& Pins, const std::vector<NlCore::LockLine>& Locks);

	// 게임 스레드의 틱. Now: 모듈이 뜬 뒤의 초. Shown: 탐색기가 화면에 보이는가(안 보이면 잠금만 건다).
	void GameTick(double Now, bool Shown);

	// 패널 안을 그린다.
	void Draw();

	// 그 주소의 그릇으로 간다(시험 설정의 path= 가 쓴다). 다음 틱에 읽는다.
	void Navigate(const std::string& Address);

	// 즐겨찾기나 잠금이 바뀌었으면 상태 파일에 적을 것을 채우고 참을 돌려준다.
	bool TakeChanges(std::vector<std::string>& Pins, std::vector<NlCore::LockLine>& Locks);

	// 지금 걸려 있는 잠금의 수.
	int ActiveLocks();
	// 잠금을 모두 푼다(값은 남긴다).
	void ReleaseAll();
}
```

- [ ] **Step 2: 구현한다**

`src/Explorer.cpp`:

```cpp
#include "Explorer.hpp"

#include "Access.hpp"
#include "Search.hpp"
#include "core/AskPath.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <mutex>

using NlAccess::Holder;
using NlCore::AskPath;
using NlCore::Shortest;

namespace
{
	constexpr size_t k_MaxRows = 8000;		// 한 그릇에서 적는 자식의 수. 전역이 4,700여 개다(research/02)
	constexpr size_t k_LiveRows = 2000;		// 이보다 많으면 "새로 고침"을 누를 때만 다시 읽는다

	struct Watch			// 즐겨찾기 하나. 잠금 값이 있을 수 있다
	{
		std::string Path;
		bool HasLock = false;
		bool Locked = false;			// 지금 걸려 있는가
		double LockValue = 0;
		// 아래는 GameTick 이 채우는 스냅샷
		bool Found = false;
		NlAccess::Row Row;
		std::string Note;
	};

	struct Command			// 창에서 한 일. 다음 틱에 실행한다
	{
		enum class Kind { Navigate, Refresh, WriteNumber, WriteString, Pin, Unpin, SetLock, Search, Refine };
		Kind What;
		std::string Path, Text;
		double Number = 0;
		bool Flag = false;
		Holder As = Holder::None;
		NlSearch::Spec Spec;
	};

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다(지금은 그리는 스레드와 게임 스레드가 같다. research/05)
	NlExplorer::LogFn g_Log;
	std::vector<Command> g_Queue;

	// 훑기
	std::string g_Address;				// 지금 보는 그릇. 비어 있으면 뿌리 화면
	Holder g_As = Holder::None;			// 수를 ds 로 여는 방식
	std::vector<NlAccess::Row> g_Rows;
	size_t g_Total = 0;
	int g_Instances = 0;				// 주소가 inst: 일 때 그 오브젝트의 인스턴스 수
	std::string g_Error, g_Note;
	std::vector<NlAccess::RootObject> g_Roots;
	bool g_Stale = true;				// 다음 틱에 다시 읽는다
	bool g_FocusBrowse = false;			// 다음에 그릴 때 "훑기" 탭을 앞에 낸다
	double g_NextView = 0, g_NextWatch = 0, g_NextLock = 0;

	// 즐겨찾기·잠금
	std::vector<Watch> g_Watches;
	bool g_Changed = false;

	// 찾기
	NlSearch::Result g_Found;
	bool g_Searched = false;
	std::string g_SearchNote;

	// 입력 칸
	char g_AddressBox[512] = "", g_Filter[64] = "", g_AddPin[512] = "", g_SearchName[64] = "", g_SearchValue[32] = "";
	bool g_ScopeGlobals = true, g_ScopeInstances = true, g_ScopeDs = false;

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	void Push(Command Cmd)
	{
		g_Queue.push_back(std::move(Cmd));
	}

	void PushPath(Command::Kind What, const std::string& Path, double Number = 0, bool Flag = false)
	{
		Command command{ What };
		command.Path = Path;
		command.Number = Number;
		command.Flag = Flag;
		Push(std::move(command));
	}

	void Go(const std::string& Address, Holder As = Holder::None)
	{
		Command command{ Command::Kind::Navigate };
		command.Path = Address;
		command.As = As;
		Push(std::move(command));
		g_FocusBrowse = true;
	}

	// 단계를 뗄 때, 뗀 단계가 ds 의 것이면 부모(수)를 그 ds 로 연다.
	Holder AsFor(char StepKind)
	{
		return StepKind == '@' ? Holder::Map : StepKind == '#' ? Holder::List : Holder::None;
	}

	// 그 값이 든 그릇으로 간다.
	void GoToParent(const std::string& Path)
	{
		const AskPath path = NlCore::ParseAskPath(Path);
		if (!path.Error.empty() || path.Steps.empty())
			Go(Path);
		else
			Go(NlCore::FormatAskPath(NlCore::ParentPath(path)), AsFor(path.Steps.back().Kind));
	}

	Watch* FindWatch(const std::string& Path)
	{
		for (Watch& watch : g_Watches)
			if (watch.Path == Path)
				return &watch;
		return nullptr;
	}

	Watch& NeedWatch(const std::string& Path)
	{
		if (Watch* watch = FindWatch(Path))
			return *watch;
		Watch watch;
		watch.Path = Path;
		g_Watches.push_back(std::move(watch));
		g_Changed = true;
		return g_Watches.back();
	}

	// 값 하나를 가리키는 주소인가.
	bool GoodPath(const std::string& Path)
	{
		const AskPath path = NlCore::ParseAskPath(Path);
		return path.Error.empty() && !path.Steps.empty();
	}

	// ---- 게임 스레드 ----

	void LoadView()
	{
		g_Rows.clear();
		g_Total = 0;
		g_Instances = 0;
		g_Error.clear();
		if (g_Address.empty())
		{
			g_Roots = NlAccess::LiveObjects();
			return;
		}

		const AskPath path = NlCore::ParseAskPath(g_Address);
		if (!path.Error.empty())
		{
			g_Error = path.Error;
			return;
		}
		if (path.Root == "inst")
			g_Instances = NlAccess::InstanceCount(path.Object);
		std::string why;
		if (!NlAccess::List(path, g_As, k_MaxRows, g_Rows, g_Total, why))
			g_Error = why;
	}

	void Snapshot(Watch& W)
	{
		YYTK::RValue value;		// 이 함수 안에서만 든다
		std::string why;
		W.Found = NlAccess::Read(NlCore::ParseAskPath(W.Path), value, why);
		W.Row = W.Found ? NlAccess::Describe({}, value) : NlAccess::Row{};
	}

	void Hold(Watch& W)
	{
		double now = 0;
		if (!NlAccess::ReadNumber(W.Path, now))
		{
			W.Note = "대상 없음";
			return;
		}
		std::string why;
		if (now == W.LockValue || NlAccess::WriteNumber(W.Path, W.LockValue, why))
			W.Note = "잠금";
		else
			W.Note = "써지지 않음: " + why;
	}

	void Run(const Command& C)
	{
		std::string why;
		switch (C.What)
		{
		case Command::Kind::Navigate:
			g_Address = NlCore::Trim(C.Path);
			g_As = C.As;
			g_Rows.clear();
			g_Total = 0;
			g_Error.clear();
			g_Note.clear();
			g_Filter[0] = 0;
			std::snprintf(g_AddressBox, sizeof(g_AddressBox), "%s", g_Address.c_str());
			g_Stale = true;
			break;

		case Command::Kind::Refresh:
			g_Stale = true;
			break;

		case Command::Kind::WriteNumber:
		{
			const bool ok = NlAccess::WriteNumber(C.Path, C.Number, why);
			g_Note = C.Path + " = " + Shortest(C.Number) + (ok ? "  써 넣음" : "  써지지 않음: " + why);
			Log("explorer write " + C.Path + " = " + Shortest(C.Number) + (ok ? ": ok" : ": " + why));
			g_Stale = true;
			break;
		}

		case Command::Kind::WriteString:
		{
			const bool ok = NlAccess::WriteString(C.Path, C.Text, why);
			g_Note = C.Path + (ok ? "  써 넣음" : "  써지지 않음: " + why);
			Log("explorer write " + C.Path + " = " + NlCore::Quote(C.Text, 80) + (ok ? ": ok" : ": " + why));
			g_Stale = true;
			break;
		}

		case Command::Kind::Pin:
			if (GoodPath(C.Path))
				NeedWatch(C.Path);
			else
				g_Note = "즐겨찾기에 넣을 수 없는 주소: " + C.Path;
			break;

		case Command::Kind::Unpin:
			if (std::erase_if(g_Watches, [&](const Watch& watch) { return watch.Path == C.Path; }) > 0)
				g_Changed = true;
			break;

		case Command::Kind::SetLock:
		{
			if (!GoodPath(C.Path))
				break;
			Watch& watch = NeedWatch(C.Path);
			watch.HasLock = true;
			watch.LockValue = C.Number;
			watch.Locked = C.Flag;
			watch.Note = C.Flag ? "" : "풀림";
			g_Changed = true;
			Log("explorer lock " + C.Path + " = " + Shortest(C.Number) + (C.Flag ? " on" : " off"));
			break;
		}

		case Command::Kind::Search:
			g_Found = NlSearch::Run(C.Spec);
			g_Searched = true;
			Log("explorer search '" + C.Spec.Name + "'" + (C.Spec.HasValue ? " value " + Shortest(C.Spec.Value) : "")
				+ ": " + std::to_string(g_Found.Hits.size()) + " hits, " + std::to_string(g_Found.Visited) + " visited, "
				+ NlCore::Fixed(g_Found.Seconds, 2) + "s" + (g_Found.Truncated ? ", truncated" : ""));
			break;

		case Command::Kind::Refine:
			NlSearch::Refine(g_Found.Hits, C.Number);
			break;
		}
	}

	// ---- 그리는 쪽 (러너를 부르지 않는다) ----

	// 값 칸. 수와 불리언과 짧은 글은 고칠 수 있다(Enter 로 써 넣는다).
	void DrawValue(const NlAccess::Row& Row, const std::string& Path)
	{
		if (Row.IsBool)
		{
			bool value = Row.Number != 0;
			if (ImGui::Checkbox("##b", &value))
				PushPath(Command::Kind::WriteNumber, Path, value ? 1 : 0);
		}
		else if (Row.IsNumber)
		{
			double value = Row.Number;
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::InputDouble("##n", &value, 0, 0, "%.10g", ImGuiInputTextFlags_EnterReturnsTrue))
				PushPath(Command::Kind::WriteNumber, Path, value);
		}
		else if (Row.IsString && Row.Raw.size() < 255)
		{
			char buffer[256];
			std::snprintf(buffer, sizeof(buffer), "%s", Row.Raw.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::InputText("##s", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				Command command{ Command::Kind::WriteString };
				command.Path = Path;
				command.Text = buffer;
				Push(std::move(command));
			}
		}
		else
			ImGui::TextUnformatted(Row.Text.c_str());
	}

	void DrawRoots()
	{
		ImGui::TextDisabled("뿌리를 고르거나 주소를 넣으세요. map:<번호> 와 list:<번호> 도 됩니다.");
		if (ImGui::Selectable("global  (전역)"))
			Go("global");
		for (const NlAccess::RootObject& root : g_Roots)
		{
			const std::string label = root.Name + "  ×" + std::to_string(root.Count);
			if (ImGui::Selectable(label.c_str()))
				Go("inst:" + root.Name);
		}
	}

	void DrawBrowse()
	{
		if (ImGui::Button("뿌리"))
			Go("");
		ImGui::SameLine();
		if (ImGui::Button("위로") && !g_Address.empty())
		{
			const AskPath path = NlCore::ParseAskPath(g_Address);
			if (!path.Error.empty() || path.Steps.empty())
				Go("");
			else
				Go(NlCore::FormatAskPath(NlCore::ParentPath(path)), AsFor(path.Steps.back().Kind));
		}
		ImGui::SameLine();
		if (ImGui::Button("새로 고침"))
			Push({ Command::Kind::Refresh });
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputTextWithHint("##address", "주소 (예: inst:o_debug   global.__gameplay_vars   map:150)", g_AddressBox,
			sizeof(g_AddressBox), ImGuiInputTextFlags_EnterReturnsTrue))
			Go(g_AddressBox);

		if (!g_Error.empty())
			ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.45f, 1.0f), "열 수 없음: %s", g_Error.c_str());
		if (!g_Note.empty())
			ImGui::TextDisabled("%s", g_Note.c_str());

		if (g_Address.empty())
		{
			DrawRoots();
			return;
		}

		// 인스턴스가 여럿이면 옆의 것으로 넘긴다(같은 단계를 그대로 따라간다).
		const AskPath here = NlCore::ParseAskPath(g_Address);
		if (here.Error.empty() && here.Root == "inst" && g_Instances > 1)
		{
			AskPath other = here;
			if (ImGui::SmallButton("<") && here.Number > 0)
			{
				other.Number = here.Number - 1;
				Go(NlCore::FormatAskPath(other), g_As);
			}
			ImGui::SameLine();
			ImGui::Text("인스턴스 %d / %d", static_cast<int>(here.Number) + 1, g_Instances);
			ImGui::SameLine();
			if (ImGui::SmallButton(">") && here.Number + 1 < g_Instances)
			{
				other.Number = here.Number + 1;
				Go(NlCore::FormatAskPath(other), g_As);
			}
			ImGui::SameLine();
		}

		ImGui::SetNextItemWidth(220);
		ImGui::InputTextWithHint("##filter", "이름 거르기", g_Filter, sizeof(g_Filter));
		ImGui::SameLine();
		ImGui::TextDisabled("%zu개%s%s", g_Total, g_Total > g_Rows.size() ? " (일부만 적음)" : "",
			g_Total > k_LiveRows ? " - 많아서 자동으로 새로 읽지 않습니다" : "");

		// 거른 줄들. 번호는 g_Rows 의 것이다(다시 읽어도 같은 줄이 같은 ID 를 갖는다).
		static std::vector<int> shown;
		shown.clear();
		const std::string filter = g_Filter;
		for (int i = 0; i < static_cast<int>(g_Rows.size()); i++)
			if (filter.empty() || g_Rows[i].Name.find(filter) != std::string::npos)
				shown.push_back(i);

		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY
			| ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
		if (!ImGui::BeginTable("rows", 4, flags))
			return;
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("이름", ImGuiTableColumnFlags_WidthStretch, 3.0f);
		ImGui::TableSetupColumn("종류", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("값", ImGuiTableColumnFlags_WidthStretch, 2.5f);
		ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthStretch, 2.0f);
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(shown.size()));
		while (clipper.Step())
			for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; line++)
			{
				const int index = shown[line];
				const NlAccess::Row& row = g_Rows[index];
				const std::string path = g_Address + NlCore::FormatStep(row.Step);
				ImGui::PushID(index);
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				if (row.IsContainer)
				{
					if (ImGui::Selectable(row.Name.c_str()))
						Go(path);
				}
				else
					ImGui::TextUnformatted(row.Name.c_str());

				ImGui::TableSetColumnIndex(1);
				ImGui::TextDisabled("%s", row.Type.c_str());

				ImGui::TableSetColumnIndex(2);
				DrawValue(row, path);

				ImGui::TableSetColumnIndex(3);
				if (row.IsNumber)
				{
					if (ImGui::SmallButton("핀"))
						PushPath(Command::Kind::Pin, path);
					ImGui::SameLine();
					if (ImGui::SmallButton("잠금"))
						PushPath(Command::Kind::SetLock, path, row.Number, true);
					// 0 이상의 정수는 ds 번호일 수 있다. 눌렀을 때 그 번호의 ds 가 없으면 "열 수 없음"이 뜬다.
					if (!row.IsBool && row.Number >= 0 && row.Number == std::floor(row.Number))
					{
						ImGui::SameLine();
						if (ImGui::SmallButton("map"))
							Go(path, Holder::Map);
						ImGui::SameLine();
						if (ImGui::SmallButton("list"))
							Go(path, Holder::List);
					}
				}
				ImGui::PopID();
			}
		ImGui::EndTable();
	}

	void DrawWatches()
	{
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputTextWithHint("##addpin", "주소를 넣고 Enter (예: inst:o_time_controller.time_warp)", g_AddPin, sizeof(g_AddPin),
			ImGuiInputTextFlags_EnterReturnsTrue))
		{
			PushPath(Command::Kind::Pin, NlCore::Trim(g_AddPin));
			g_AddPin[0] = 0;
		}
		if (!g_Note.empty())
			ImGui::TextDisabled("%s", g_Note.c_str());
		if (g_Watches.empty())
		{
			ImGui::TextWrapped("훑기나 찾기에서 '핀'을 누르면 여기에 모입니다. 잠그면 0.1초마다 그 값으로 다시 써 넣습니다. "
				"저장된 잠금은 다음 실행에 꺼진 채로 돌아옵니다.");
			return;
		}

		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY
			| ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
		if (!ImGui::BeginTable("watches", 4, flags))
			return;
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("주소", ImGuiTableColumnFlags_WidthStretch, 4.0f);
		ImGui::TableSetupColumn("값", ImGuiTableColumnFlags_WidthStretch, 2.0f);
		ImGui::TableSetupColumn("잠금", ImGuiTableColumnFlags_WidthStretch, 3.0f);
		ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthStretch, 0.8f);
		ImGui::TableHeadersRow();

		for (int i = 0; i < static_cast<int>(g_Watches.size()); i++)
		{
			const Watch& watch = g_Watches[i];
			ImGui::PushID(i);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			if (ImGui::Selectable(watch.Path.c_str()))
				GoToParent(watch.Path);

			ImGui::TableSetColumnIndex(1);
			if (watch.Found)
				DrawValue(watch.Row, watch.Path);
			else
				ImGui::TextDisabled("대상 없음");

			ImGui::TableSetColumnIndex(2);
			const bool can_lock = watch.HasLock || (watch.Found && watch.Row.IsNumber);
			ImGui::BeginDisabled(!can_lock);
			bool locked = watch.Locked;
			if (ImGui::Checkbox("##lock", &locked))
				PushPath(Command::Kind::SetLock, watch.Path, watch.HasLock ? watch.LockValue : watch.Row.Number, locked);
			ImGui::EndDisabled();
			if (watch.HasLock)
			{
				ImGui::SameLine();
				double value = watch.LockValue;
				ImGui::SetNextItemWidth(110);
				if (ImGui::InputDouble("##lv", &value, 0, 0, "%.10g", ImGuiInputTextFlags_EnterReturnsTrue))
					PushPath(Command::Kind::SetLock, watch.Path, value, watch.Locked);
				if (!watch.Note.empty())
				{
					ImGui::SameLine();
					ImGui::TextDisabled("%s", watch.Note.c_str());
				}
			}

			ImGui::TableSetColumnIndex(3);
			if (ImGui::SmallButton("삭제"))
				PushPath(Command::Kind::Unpin, watch.Path);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	void DrawSearch()
	{
		ImGui::SetNextItemWidth(240);
		ImGui::InputTextWithHint("##name", "이름의 일부 (예: gold)", g_SearchName, sizeof(g_SearchName));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(160);
		ImGui::InputTextWithHint("##value", "값 (예: 3000)", g_SearchValue, sizeof(g_SearchValue));
		ImGui::SameLine();
		ImGui::Checkbox("전역", &g_ScopeGlobals);
		ImGui::SameLine();
		ImGui::Checkbox("인스턴스", &g_ScopeInstances);
		ImGui::SameLine();
		ImGui::Checkbox("ds", &g_ScopeDs);

		double value = 0;
		const std::string value_text = NlCore::Trim(g_SearchValue);
		const bool has_value = NlCore::ParseNumber(value_text, value);
		if (ImGui::Button("찾기"))
		{
			Command command{ Command::Kind::Search };
			command.Spec.Name = NlCore::Trim(g_SearchName);
			command.Spec.HasValue = has_value;
			command.Spec.Value = value;
			command.Spec.Globals = g_ScopeGlobals;
			command.Spec.Instances = g_ScopeInstances;
			command.Spec.Ds = g_ScopeDs;
			if (!value_text.empty() && !has_value)
				g_SearchNote = "값이 수가 아닙니다.";
			else if (command.Spec.Name.empty() && !has_value)
				g_SearchNote = "이름이나 값을 넣으세요.";
			else
			{
				g_SearchNote.clear();
				Push(std::move(command));
			}
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!g_Searched || !has_value);
		if (ImGui::Button("다시 거르기"))
			PushPath(Command::Kind::Refine, "", value);
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("찾는 동안 게임이 잠깐 멈춥니다(길어도 3초).");
		if (!g_SearchNote.empty())
			ImGui::TextUnformatted(g_SearchNote.c_str());

		if (!g_Searched)
		{
			ImGui::TextWrapped("값으로 자리를 찾는 법: 금화가 3000 이면 값에 3000 을 넣어 찾습니다. 돈을 써서 2950 이 되면 "
				"값에 2950 을 넣고 '다시 거르기'를 누릅니다. 남은 것이 금화의 자리입니다.");
			return;
		}
		ImGui::Text("%zu개 찾음 (%zu개를 %.1f초에 봄)%s", g_Found.Hits.size(), g_Found.Visited, g_Found.Seconds,
			g_Found.Truncated ? " - 한도에 걸려 일부만 봤습니다" : "");

		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY
			| ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
		if (!ImGui::BeginTable("hits", 4, flags))
			return;
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("주소", ImGuiTableColumnFlags_WidthStretch, 5.0f);
		ImGui::TableSetupColumn("종류", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("값", ImGuiTableColumnFlags_WidthStretch, 2.0f);
		ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthStretch, 0.6f);
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(g_Found.Hits.size()));
		while (clipper.Step())
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
			{
				const NlSearch::Hit& hit = g_Found.Hits[i];
				ImGui::PushID(i);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				if (ImGui::Selectable(hit.Path.c_str()))
					GoToParent(hit.Path);
				ImGui::TableSetColumnIndex(1);
				ImGui::TextDisabled("%s", hit.Type.c_str());
				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(hit.Text.c_str());
				ImGui::TableSetColumnIndex(3);
				if (ImGui::SmallButton("핀"))
					PushPath(Command::Kind::Pin, hit.Path);
				ImGui::PopID();
			}
		ImGui::EndTable();
	}
}

void NlExplorer::Init(LogFn Log_, const std::vector<std::string>& Pins, const std::vector<NlCore::LockLine>& Locks)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	for (const std::string& pin : Pins)
		NeedWatch(pin);
	for (const NlCore::LockLine& line : Locks)
	{
		Watch& watch = NeedWatch(line.Path);
		watch.HasLock = true;
		watch.LockValue = line.Value;
		watch.Locked = false;		// 인스턴스의 차례가 실행마다 다를 수 있다. 사용자가 다시 건다(스펙 §5)
	}
	g_Changed = false;
}

void NlExplorer::GameTick(double Now, bool Shown)
{
	std::lock_guard lock(g_Mutex);

	std::vector<Command> queue;
	queue.swap(g_Queue);
	for (const Command& command : queue)
		Run(command);

	if (Now >= g_NextLock)
	{
		g_NextLock = Now + 0.1;
		for (Watch& watch : g_Watches)
			if (watch.Locked)
				Hold(watch);
	}
	if (!Shown)
		return;

	// 큰 그릇(전역)은 "새로 고침"을 누를 때만 다시 읽는다. 열지 못한 그릇은 0.5초마다 다시 해 본다(대상이 나중에 생긴다).
	if (g_Stale || (Now >= g_NextView && g_Total <= k_LiveRows))
	{
		g_Stale = false;
		g_NextView = Now + 0.5;
		LoadView();
	}
	if (Now >= g_NextWatch)
	{
		g_NextWatch = Now + 0.25;
		for (Watch& watch : g_Watches)
			Snapshot(watch);
	}
}

void NlExplorer::Draw()
{
	std::lock_guard lock(g_Mutex);
	if (!ImGui::BeginTabBar("explorer"))
		return;

	const ImGuiTabItemFlags focus = g_FocusBrowse ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
	g_FocusBrowse = false;
	if (ImGui::BeginTabItem("훑기", nullptr, focus))
	{
		DrawBrowse();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("즐겨찾기·잠금"))
	{
		DrawWatches();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("찾기"))
	{
		DrawSearch();
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
}

void NlExplorer::Navigate(const std::string& Address)
{
	std::lock_guard lock(g_Mutex);
	Go(Address);
}

bool NlExplorer::TakeChanges(std::vector<std::string>& Pins, std::vector<NlCore::LockLine>& Locks)
{
	std::lock_guard lock(g_Mutex);
	if (!g_Changed)
		return false;
	g_Changed = false;

	Pins.clear();
	Locks.clear();
	for (const Watch& watch : g_Watches)
	{
		Pins.push_back(watch.Path);
		if (watch.HasLock)
			Locks.push_back({ watch.Path, watch.LockValue });
	}
	return true;
}

int NlExplorer::ActiveLocks()
{
	std::lock_guard lock(g_Mutex);
	return static_cast<int>(std::count_if(g_Watches.begin(), g_Watches.end(), [](const Watch& watch) { return watch.Locked; }));
}

void NlExplorer::ReleaseAll()
{
	std::lock_guard lock(g_Mutex);
	for (Watch& watch : g_Watches)
		if (watch.Locked)
		{
			watch.Locked = false;
			watch.Note = "풀림";
		}
}
```

`CMakeLists.txt`의 `nltoybox` 목록에서 `src/Dump.cpp` 다음 줄에 `  src/Explorer.cpp`를 더한다.

- [ ] **Step 3: 빌드한다**

Run: `pwsh -File tools/build.ps1`
Expected: `build ok -> …\NlToyBox.dll`

- [ ] **Step 4: 커밋**

```powershell
git -C E:\NlToyBox add src/Explorer.hpp src/Explorer.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "feat(module): 탐색기 — 게임의 값을 훑고, 고치고, 잠그고, 이름과 값으로 찾는다"
```

---

### Task 9: 치트 표의 적용, 영역 패널, 게임 속도

창은 `Item`의 바라는 상태만 바꾼다(러너를 부르지 않는다). 값을 써 넣는 것은 틱이다.

**Files:**
- Create: `src/Cheats.hpp`, `src/Cheats.cpp`
- Modify: `CMakeLists.txt` (`nltoybox`에 `src/Cheats.cpp`)

**Interfaces:**
- Consumes: `NlCore::Cheats`, `Cheat`, `CheatKind`, `Area`, `GetArea` (Task 3), `NlCore::CheatState` (Task 2), `NlCore::Rate` (Task 4), `NlCore::SpeedTrial`, `SpeedStep` (Task 5), `NlAccess::ReadNumber`, `WriteNumber` (Task 6), `NlCore::Shortest`, `Fixed`
- Produces (모두 `namespace NlCheats`):
  - `using LogFn = std::function<void(const std::string&)>`
  - `void Init(LogFn Log, const NlCore::CheatState& State)` — `KeepKnown`을 거친 상태
  - `void GameTick(double Now, bool Visible)`
  - `void DrawArea(NlCore::Area Where)`, `void DrawTime()`
  - `bool HasItems(NlCore::Area Where)`
  - `bool TakeChanges(std::set<std::string>& On, std::map<std::string, double>& Numbers)`
  - `int ActiveCount()`, `void ReleaseAll()`

- [ ] **Step 1: 머리 파일을 쓴다**

`src/Cheats.hpp`:

```cpp
#pragma once
// 치트 표(core/CheatTable)의 항목을 게임에 적용하고 영역의 패널을 그린다. 게임 속도도 여기서 건다.
// 스펙: 치트 메뉴 §6.2, §9. 창(Draw*)은 바라는 상태만 바꾸고, 값을 써 넣는 것은 GameTick 이다.

#include "core/CheatState.hpp"
#include "core/CheatTable.hpp"

#include <functional>
#include <map>
#include <set>
#include <string>

namespace NlCheats
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번. State 는 KeepKnown 을 거친 것이다. 켜져 있던 항목은 대상이 생기면 다시 적용된다.
	void Init(LogFn Log, const NlCore::CheatState& State);

	// 게임 스레드의 틱. Now: 모듈이 뜬 뒤의 초. Visible: 모드창이 열려 있는가(닫혀 있으면 켠 것만 건드린다).
	void GameTick(double Now, bool Visible);

	// 그 영역의 항목들을 그린다.
	void DrawArea(NlCore::Area Where);
	// "시간" 영역: 게임 속도.
	void DrawTime();

	// 그 영역에 표의 항목이 있는가.
	bool HasItems(NlCore::Area Where);

	// 켠 것이 바뀌었으면 상태 파일에 적을 것을 채우고 참을 돌려준다.
	bool TakeChanges(std::set<std::string>& On, std::map<std::string, double>& Numbers);

	// 켜 둔 항목의 수(게임 속도를 걸었으면 하나 더).
	int ActiveCount();
	// 모두 끄고 원래 값으로 되돌린다.
	void ReleaseAll();
}
```

- [ ] **Step 2: 구현한다**

`src/Cheats.cpp`:

```cpp
#include "Cheats.hpp"

#include "Access.hpp"
#include "core/Rate.hpp"
#include "core/SpeedTrial.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

using NlCore::Area;
using NlCore::Cheat;
using NlCore::CheatKind;
using NlCore::Fixed;
using NlCore::Shortest;
using NlCore::SpeedStep;
using NlCore::SpeedTrial;

namespace
{
	struct Item
	{
		const Cheat* Def = nullptr;
		bool On = false;			// Toggle: 켰다. Number: 값을 정했다
		double Number = 0;			// Number 가 써 넣는 값
		bool Restore = false;		// 방금 껐다. 다음 틱에 원래 값을 한 번 써 넣는다
		bool HasBase = false;
		double Base = 0;			// Number 의 처음 본 값
		// 아래는 GameTick 이 채우는 스냅샷
		bool Found = false;
		double Current = 0;
		std::string Note, Logged;
	};

	// 게임 속도의 후보(스펙 §9). 시험하는 차례대로. 이름은 덤프의 o_time_controller 에서 봤다. 어느 것이 속도를 정하는지는 모른다.
	struct Handle
	{
		const char* Label;
		const char* Path;			// nullptr 이면 time_speed_variants 의 지금 자리
	};
	constexpr Handle k_Handles[] = {
		{ "time_warp_new", "inst:o_time_controller.time_warp_new" },
		{ "time_warp", "inst:o_time_controller.time_warp" },
		{ "__debug_custom_wrap", "inst:o_time_controller.__debug_custom_wrap" },
		{ "time_speed_variants[time_speed_index]", nullptr },
	};
	constexpr int k_HandleCount = 4;
	constexpr const char* k_GameTime = "inst:o_time_controller.__game_time";
	constexpr const char* k_Warp = "inst:o_time_controller.time_warp";
	constexpr const char* k_SpeedIndex = "inst:o_time_controller.time_speed_index";
	constexpr double k_Factors[] = { 0.25, 0.5, 1, 2, 5, 10, 50 };		// time_warp_max 는 100 이다(덤프)

	std::recursive_mutex g_Mutex;	// 아래 전부를 지킨다
	NlCheats::LogFn g_Log;
	std::vector<Item> g_Items;
	bool g_Changed = false;			// 상태 파일에 적을 것이 바뀌었다
	bool g_Dirty = true;			// 다음 틱에 바로 적용한다
	double g_NextApply = 0;

	// 게임 속도
	NlCore::Rate g_Rate(1.0);
	SpeedTrial g_Trial(k_HandleCount);
	double g_Wanted = 0;			// 고른 배율. 0 이면 손대지 않는다
	bool g_Pressed = false, g_ReleaseAsked = false;		// 창이 올리고 틱이 내린다
	bool g_Holding = false;			// 계속 써 넣는 중인가
	std::string g_HoldPath;
	double g_HoldValue = 0;
	double g_NextSpeed = 0, g_NextHold = 0;
	size_t g_AttemptsLogged = 0;
	bool g_TimeFound = false;		// 스냅샷
	double g_Flow = 0, g_Warp = 0;
	std::string g_SpeedNote;

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// ---- 게임 스레드 ----

	void Apply(Item& It, bool Visible)
	{
		if (!It.On && !It.Restore && !Visible)
			return;			// 켜지 않았고 보는 사람도 없으면 읽지도 않는다

		double current = 0;
		It.Found = NlAccess::ReadNumber(It.Def->Path, current);
		if (!It.Found)
		{
			// 대상이 없다(메뉴에서는 o_debug 가 없다). 되돌릴 것도 대상과 함께 사라졌다.
			It.Restore = false;
			It.Note = It.On ? "게임을 시작하면 적용" : "";
			return;
		}
		It.Current = current;

		const bool number = It.Def->Kind == CheatKind::Number;
		if (number && !It.HasBase)
		{
			It.Base = current;
			It.HasBase = true;
		}

		double wanted = current;
		if (It.On)
			wanted = number ? It.Number : It.Def->On;
		else if (It.Restore)
			wanted = number ? It.Base : It.Def->Off;
		It.Restore = false;
		if (wanted == current)
		{
			It.Note = It.On ? "써 넣음" : "";
			return;
		}

		std::string why;
		const bool ok = NlAccess::WriteNumber(It.Def->Path, wanted, why);
		if (ok)
			It.Current = wanted;
		It.Note = ok ? (It.On ? "써 넣음" : "") : "써지지 않음: " + why;

		// 같은 결과를 되풀이해 적지 않는다(게임이 값을 되돌리면 0.5초마다 다시 쓴다).
		const std::string line = std::string("cheat ") + It.Def->Id + " = " + Shortest(wanted) + (ok ? ": ok" : ": " + why);
		if (line != It.Logged)
		{
			It.Logged = line;
			Log(line);
		}
	}

	std::string HandlePath(int Index)
	{
		if (Index < 0 || Index >= k_HandleCount)
			return "";
		if (k_Handles[Index].Path)
			return k_Handles[Index].Path;

		double at = 0;
		if (!NlAccess::ReadNumber(k_SpeedIndex, at) || at < 0)
			return "";
		return "inst:o_time_controller.time_speed_variants[" + Shortest(std::floor(at)) + "]";
	}

	// 고른 손잡이로 바라는 배율을 건다.
	void ApplyWanted()
	{
		if (g_Wanted <= 0 || g_Trial.State() != SpeedTrial::Phase::Done)
			return;

		const int chosen = g_Trial.Candidate();
		const std::string path = HandlePath(chosen);
		std::string why;
		const bool ok = !path.empty() && NlAccess::WriteNumber(path, g_Wanted, why);
		g_Holding = ok && !g_Trial.Sticky();
		g_HoldPath = path;
		g_HoldValue = g_Wanted;
		g_SpeedNote = ok ? "x" + Shortest(g_Wanted) + " 을 걸었습니다." : "써지지 않음: " + why;
		Log("speed x" + Shortest(g_Wanted) + " via " + k_Handles[chosen].Label + (ok ? "" : ": " + why));
	}

	void Release()
	{
		g_ReleaseAsked = false;
		if (g_Trial.Running())
			return;
		g_Wanted = 0;
		g_Holding = false;
		if (g_Trial.State() != SpeedTrial::Phase::Done)
		{
			g_SpeedNote.clear();
			return;
		}

		const int chosen = g_Trial.Candidate();
		const std::string path = HandlePath(chosen);
		std::string why;
		const bool ok = !path.empty() && NlAccess::WriteNumber(path, g_Trial.SavedOld(), why);
		g_SpeedNote = ok ? "게임의 속도로 되돌렸습니다." : "되돌리지 못했습니다: " + why;
		Log(std::string("speed release via ") + k_Handles[chosen].Label + " = " + Shortest(g_Trial.SavedOld()) + (ok ? "" : ": " + why));
	}

	// 배율 단추를 눌렀다.
	void Begin()
	{
		if (g_Trial.Running())
			return;			// 시험이 끝나면 g_Wanted 를 건다
		if (g_Trial.State() == SpeedTrial::Phase::Done)
		{
			ApplyWanted();
			return;
		}
		if (!g_Rate.Ready() || !g_Trial.Start(g_Flow, g_Warp))
		{
			g_SpeedNote = "시간이 흐르지 않아 손잡이를 시험할 수 없습니다. 게임 화면에서 일시정지를 풀고 다시 누르세요.";
			g_Wanted = 0;
			return;
		}
		g_AttemptsLogged = 0;
		g_SpeedNote = "손잡이를 시험하는 중입니다(길어도 10초). 그동안 게임 속도가 잠깐 바뀝니다.";
		Log("speed trial start: flow " + Fixed(g_Flow, 2) + "/s, time_warp " + Shortest(g_Warp) + ", probe " + Shortest(g_Trial.ProbeWarp()));
	}

	// 시험을 한 걸음 나아간다(0.1초마다).
	void StepTrial(double Now)
	{
		const std::string path = HandlePath(g_Trial.Candidate());
		double current = 0;
		const bool readable = !path.empty() && NlAccess::ReadNumber(path, current);
		const SpeedStep step = g_Trial.Tick(Now, readable, current, g_Flow);

		g_Holding = step.What == SpeedStep::Kind::Write;
		if (step.What == SpeedStep::Kind::Write)
		{
			g_HoldPath = HandlePath(step.Candidate);
			g_HoldValue = step.Value;
			g_NextHold = 0;
		}
		else if (step.What == SpeedStep::Kind::Restore)
		{
			std::string why;
			NlAccess::WriteNumber(HandlePath(step.Candidate), step.Value, why);
		}

		for (; g_AttemptsLogged < g_Trial.Attempts().size(); g_AttemptsLogged++)
		{
			const NlCore::SpeedAttempt& attempt = g_Trial.Attempts()[g_AttemptsLogged];
			Log(std::string("speed trial ") + k_Handles[attempt.Candidate].Label + ": "
				+ (attempt.Readable ? "old " + Shortest(attempt.Old) + ", flow " + Fixed(attempt.Rate, 2) + "/s"
					+ (attempt.Matched ? " -> matched" : " -> no effect, restored") : "not readable"));
		}

		switch (g_Trial.State())
		{
		case SpeedTrial::Phase::Done:
			Log(std::string("speed trial done: ") + k_Handles[g_Trial.Candidate()].Label + (g_Trial.Sticky() ? " (write once)" : " (keep writing)"));
			ApplyWanted();
			break;
		case SpeedTrial::Phase::Failed:
			g_SpeedNote = "네 후보 모두 속도를 바꾸지 못했습니다. 시도마다의 값은 NlToyBox.log 에 있습니다.";
			g_Wanted = 0;
			Log("speed trial failed");
			break;
		case SpeedTrial::Phase::Aborted:
			g_SpeedNote = "시험 중에 시간이 멈췄습니다. 일시정지를 풀고 다시 누르세요.";
			g_Wanted = 0;
			Log("speed trial aborted: time stopped");
			break;
		default:
			break;
		}
	}

	void TickSpeed(double Now, bool Visible)
	{
		// 계속 쓰기는 프레임마다 한 번쯤 한다. 게임이 프레임마다 되돌리는 값이면 이래야 먹는다.
		if (g_Holding && Now >= g_NextHold)
		{
			g_NextHold = Now + 0.015;
			std::string why;
			NlAccess::WriteNumber(g_HoldPath, g_HoldValue, why);
		}

		const bool busy = g_Trial.Running() || g_Wanted > 0 || g_Pressed || g_ReleaseAsked;
		if (Now < g_NextSpeed || (!Visible && !busy))
			return;
		g_NextSpeed = Now + 0.1;

		double game_time = 0;
		g_TimeFound = NlAccess::ReadNumber(k_GameTime, game_time);
		if (!g_TimeFound)
		{
			// 시간 컨트롤러가 없다(로딩 중). 하던 것을 놓는다. 써 둔 값은 컨트롤러와 함께 사라졌다.
			g_Rate.Reset();
			g_Flow = 0;
			g_Holding = false;
			g_Pressed = g_ReleaseAsked = false;
			if (g_Trial.Running())
				g_Trial = SpeedTrial(k_HandleCount);
			return;
		}
		g_Rate.Add(Now, game_time);
		g_Flow = g_Rate.PerSecond();
		NlAccess::ReadNumber(k_Warp, g_Warp);

		if (g_ReleaseAsked)
			Release();
		if (g_Pressed)
		{
			g_Pressed = false;
			Begin();
		}
		if (g_Trial.Running())
			StepTrial(Now);
	}

	// ---- 그리는 쪽 (러너를 부르지 않는다) ----

	// 이름 옆의 "(?)": 효과를 아직 확인하지 않은 항목이다. 올리면 주소와 설명이 보인다.
	void DrawHelp(const Cheat& Def)
	{
		if (!Def.Verified)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s\n%s%s", Def.Path, Def.Help, Def.Verified ? "" : "\n효과 확인 전");
	}

	void TurnOff(Item& It)
	{
		It.On = false;
		It.Restore = true;
		g_Changed = g_Dirty = true;
	}

	void DrawToggle(Item& It)
	{
		bool on = It.On;
		if (ImGui::Checkbox(It.Def->Label, &on))
		{
			if (on)
			{
				It.On = true;
				It.Restore = false;
				g_Changed = g_Dirty = true;
			}
			else
				TurnOff(It);
		}
		DrawHelp(*It.Def);
		if (!It.Note.empty())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("%s", It.Note.c_str());
		}
	}

	void DrawNumber(Item& It)
	{
		double value = It.On ? It.Number : It.Current;
		ImGui::SetNextItemWidth(130);
		if (ImGui::InputDouble("##v", &value, 0, 0, "%.6g", ImGuiInputTextFlags_EnterReturnsTrue))
		{
			It.Number = std::clamp(value, It.Def->Min, It.Def->Max);
			It.On = true;
			It.Restore = false;
			g_Changed = g_Dirty = true;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!It.On);
		if (ImGui::Button("원래대로"))
			TurnOff(It);
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextUnformatted(It.Def->Label);
		DrawHelp(*It.Def);

		ImGui::SameLine();
		if (It.On)
			ImGui::TextDisabled("%s", It.Note.c_str());
		else if (It.Found)
			ImGui::TextDisabled("지금 %s", Shortest(It.Current).c_str());
		else
			ImGui::TextDisabled("게임을 시작하면 보입니다");
	}
}

void NlCheats::Init(LogFn Log_, const NlCore::CheatState& State)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	g_Items.clear();

	std::string loaded;
	for (const Cheat& cheat : NlCore::Cheats())
	{
		Item item;
		item.Def = &cheat;
		if (cheat.Kind == CheatKind::Toggle)
			item.On = State.On.count(cheat.Id) > 0;
		else if (const auto it = State.Numbers.find(cheat.Id); it != State.Numbers.end())
		{
			item.On = true;
			item.Number = it->second;
		}
		if (item.On)
			loaded += std::string(" ") + cheat.Id + (cheat.Kind == CheatKind::Number ? "=" + Shortest(item.Number) : "");
		g_Items.push_back(std::move(item));
	}
	g_Dirty = true;
	Log("cheat state:" + (loaded.empty() ? std::string(" none") : loaded));
}

void NlCheats::GameTick(double Now, bool Visible)
{
	std::lock_guard lock(g_Mutex);
	TickSpeed(Now, Visible);

	if (!g_Dirty && Now < g_NextApply)
		return;
	g_Dirty = false;
	g_NextApply = Now + 0.5;		// 게임이 값을 다시 만들면(새 게임, 불러오기) 다시 써 넣는다
	for (Item& item : g_Items)
		Apply(item, Visible);
}

void NlCheats::DrawArea(Area Where)
{
	std::lock_guard lock(g_Mutex);
	bool any = false, any_on = false;
	for (Item& item : g_Items)
	{
		if (item.Def->Where != Where)
			continue;
		any = true;
		any_on = any_on || item.On;
		ImGui::PushID(item.Def->Id);
		if (item.Def->Kind == CheatKind::Toggle)
			DrawToggle(item);
		else
			DrawNumber(item);
		ImGui::PopID();
	}
	if (!any)
	{
		ImGui::TextDisabled("이 영역은 %d단계에서 채웁니다.", NlCore::GetArea(Where).Stage);
		return;
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::BeginDisabled(!any_on);
	if (ImGui::Button("이 영역 모두 끄기"))
		for (Item& item : g_Items)
			if (item.Def->Where == Where && item.On)
				TurnOff(item);
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("(?) 는 효과를 아직 확인하지 않은 항목입니다. 수는 Enter 로 써 넣습니다.");
}

void NlCheats::DrawTime()
{
	std::lock_guard lock(g_Mutex);
	ImGui::TextWrapped("배율을 누르면 게임 속도를 바꿉니다. 처음 누를 때는 어느 값이 속도를 정하는지 시험으로 찾습니다"
		"(게임 화면에서, 일시정지를 푼 채로 누르세요).");
	ImGui::Spacing();

	if (!g_TimeFound)
		ImGui::TextDisabled("시간 컨트롤러(o_time_controller)가 아직 없습니다.");
	else
	{
		const double unit = g_Trial.UnitRate();
		if (unit > 0)
			ImGui::Text("게임 시간의 흐름: 실제 1초에 %.1f  (기준의 %.2f배)", g_Flow, g_Flow / unit);
		else
			ImGui::Text("게임 시간의 흐름: 실제 1초에 %.1f", g_Flow);
		ImGui::SameLine();
		ImGui::TextDisabled("time_warp %s", Shortest(g_Warp).c_str());
	}

	ImGui::BeginDisabled(g_Trial.Running() || !g_TimeFound);
	for (const double factor : k_Factors)
	{
		const std::string label = "x" + Shortest(factor);
		if (ImGui::Button(label.c_str(), ImVec2(64, 0)))
		{
			g_Wanted = factor;
			g_Pressed = true;
		}
		ImGui::SameLine();
	}
	if (ImGui::Button("게임에 맡김"))
		g_ReleaseAsked = true;
	ImGui::EndDisabled();

	if (!g_SpeedNote.empty())
		ImGui::TextWrapped("%s", g_SpeedNote.c_str());
	if (g_Trial.State() == SpeedTrial::Phase::Done)
		ImGui::TextDisabled("손잡이: %s (%s)", k_Handles[g_Trial.Candidate()].Label, g_Trial.Sticky() ? "한 번 쓰기" : "계속 쓰기");
}

bool NlCheats::HasItems(Area Where)
{
	for (const Cheat& cheat : NlCore::Cheats())
		if (cheat.Where == Where)
			return true;
	return false;
}

bool NlCheats::TakeChanges(std::set<std::string>& On, std::map<std::string, double>& Numbers)
{
	std::lock_guard lock(g_Mutex);
	if (!g_Changed)
		return false;
	g_Changed = false;

	On.clear();
	Numbers.clear();
	for (const Item& item : g_Items)
	{
		if (!item.On)
			continue;
		if (item.Def->Kind == CheatKind::Toggle)
			On.insert(item.Def->Id);
		else
			Numbers[item.Def->Id] = item.Number;
	}
	return true;
}

int NlCheats::ActiveCount()
{
	std::lock_guard lock(g_Mutex);
	const auto on = std::count_if(g_Items.begin(), g_Items.end(), [](const Item& item) { return item.On; });
	return static_cast<int>(on) + (g_Wanted > 0 ? 1 : 0);
}

void NlCheats::ReleaseAll()
{
	std::lock_guard lock(g_Mutex);
	for (Item& item : g_Items)
		if (item.On)
			TurnOff(item);
	if (g_Wanted > 0)
		g_ReleaseAsked = true;
}
```

`CMakeLists.txt`의 `nltoybox` 목록에서 `src/Access.cpp` 다음 줄에 `  src/Cheats.cpp`를 더한다.

- [ ] **Step 3: 빌드한다**

Run: `pwsh -File tools/build.ps1`
Expected: `build ok -> …\NlToyBox.dll`

- [ ] **Step 4: 커밋**

```powershell
git -C E:\NlToyBox add src/Cheats.hpp src/Cheats.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "feat(module): 치트 표를 게임에 적용하고 영역 패널을 그린다. 게임 속도는 손잡이를 시험으로 고른다"
```

---

### Task 10: 메뉴를 붙인다 — 왼쪽 목록, 상태 파일, 시험 설정

**Files:**
- Create: `src/Menu.hpp`, `src/Menu.cpp`
- Modify: `src/Ui.hpp`, `src/Ui.cpp` (시험 설정의 나머지 줄, 준비된 뒤의 시간, 창 크기)
- Modify: `src/ModuleMain.cpp` (버전 `0.4.0`, `NlMenu`를 붙인다)
- Modify: `CMakeLists.txt` (`nltoybox`에 `src/Menu.cpp`)

**Interfaces:**
- Consumes: `NlExplorer::*` (Task 8), `NlCheats::*` (Task 9), `NlTweaks::Draw` (`src/Tweaks.hpp`), `NlAccess::Read`·`Describe`·`ReadNumber`·`WriteNumber`·`InGame` (Task 6), `NlCore::ParseCheatState`·`FormatCheatState` (Task 2), `NlCore::KeepKnown`·`Areas`·`FindArea` (Task 3)
- Produces:
  - `void NlMenu::Init(const std::filesystem::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log)`
  - `void NlMenu::GameTick()`, `void NlMenu::Draw()`
  - `std::vector<std::string> NlUi::TestValues(const std::string& Key)` — 시험 설정의 `Key=값` 줄들의 값
  - `double NlUi::ShotSeconds()` — 화면을 뜨는 때(준비된 뒤의 초). 뜨지 않으면 음수
  - `double NlUi::SecondsSinceReady()` — 모드창이 준비된 뒤의 초. 아직이면 음수
  - 로그 줄: `ask <주소> = <종류> <값>` / `ask <주소> : <까닭>`, `poke <주소>: old <a> -> wrote <b>, read <c> (stuck|not stuck: <까닭>); restored <d>`, `ui tests done`

- [ ] **Step 1: `Ui`가 시험 설정의 나머지 줄을 내어 주게 한다**

`src/Ui.hpp`의 `TestSets` 선언 뒤에 더한다.

```cpp

	// 시험용 설정의 그 밖의 줄: "Key=값" 들의 값. 평소에는 비어 있다(page, path, ask, poke 를 Menu 가 읽는다).
	std::vector<std::string> TestValues(const std::string& Key);

	// 시험용 설정이 정한, 화면을 뜨는 때(모드창이 준비된 뒤의 초). 뜨지 않으면 음수.
	double ShotSeconds();

	// 모드창이 준비된 뒤의 초. 아직 준비되지 않았으면 음수.
	double SecondsSinceReady();
```

`src/Ui.cpp`:

1. include 에 `#include <utility>`를 더한다(`<sstream>` 다음).
2. `std::vector<std::string> g_TestSets;` 다음 줄에 더한다.

```cpp
	std::vector<std::pair<std::string, std::string>> g_TestExtra;	// 이 파일이 모르는 줄(키, 값). Menu 가 읽는다
```

3. `NlUi::Init`의 줄을 읽는 곳에서 `else if (key == "set")` 가지 뒤에 가지를 하나 더한다.

```cpp
		else if (key == "set")
			g_TestSets.push_back(value.substr(0, value.find_last_not_of(" \r\n") + 1));
		else
			g_TestExtra.emplace_back(key, value.substr(0, value.find_last_not_of(" \r\n") + 1));
```

4. 창의 처음 크기를 넓힌다. `ImGui::SetNextWindowSize(ImVec2(760, 400), ImGuiCond_FirstUseEver);`를 다음으로 바꾼다.

```cpp
			ImGui::SetNextWindowSize(ImVec2(980, 620), ImGuiCond_FirstUseEver);
```

5. `NlUi::TestSets` 정의 뒤에 더한다.

```cpp

std::vector<std::string> NlUi::TestValues(const std::string& Key)
{
	std::vector<std::string> values;
	for (const auto& [key, value] : g_TestExtra)
		if (key == Key)
			values.push_back(value);
	return values;
}

double NlUi::ShotSeconds()
{
	return g_ShotSeconds;
}

double NlUi::SecondsSinceReady()
{
	return g_Ready ? std::chrono::duration<double>(Clock::now() - g_ReadyAt).count() : -1;
}
```

- [ ] **Step 2: `Menu`를 쓴다**

`src/Menu.hpp`:

```cpp
#pragma once
// 모드창의 안쪽: 왼쪽에 영역의 목록, 오른쪽에 고른 영역의 패널. 스펙: 치트 메뉴 §8.
// 치트의 상태 파일(NlToyBox.cheats.txt)을 읽고 쓰며, 시험 설정의 ask= 와 poke= 를 실행한다.

#include <filesystem>
#include <functional>
#include <string>

namespace NlMenu
{
	// ModuleInitialize 에서 한 번. NlUi::Init 뒤에 부른다(시험 설정을 NlUi 가 읽어 둔다).
	void Init(const std::filesystem::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log);

	// 게임 스레드의 콜백에서 부른다. 탐색기와 치트의 틱을 돌리고, 바뀐 상태를 파일에 적는다.
	void GameTick();

	// 모드창 안을 그린다.
	void Draw();
}
```

`src/Menu.cpp`:

```cpp
#include "Menu.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Explorer.hpp"
#include "Tweaks.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/CheatState.hpp"
#include "core/CheatTable.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <vector>

using NlCore::Area;
using NlCore::Shortest;
using Clock = std::chrono::steady_clock;

namespace
{
	std::filesystem::path g_StatePath;
	std::string g_Version;
	std::function<void(const std::string&)> g_Log;
	Clock::time_point g_Start;

	std::atomic<int> g_Page = static_cast<int>(Area::Explorer);
	std::atomic<bool> g_InGame = false;

	NlCore::CheatState g_State;		// 상태 파일에 적힌(적을) 것. 게임 스레드만 만진다
	bool g_SavePending = false;
	double g_SaveAt = 0, g_NextState = 0;

	// 시험 설정의 ask= 와 poke=. 화면을 뜨기 5초 전에 한 번 한다.
	std::vector<std::string> g_Asks, g_Pokes;
	bool g_TestsDone = true;

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	double Seconds()
	{
		return std::chrono::duration<double>(Clock::now() - g_Start).count();
	}

	void Ask(const std::string& Path)
	{
		YYTK::RValue value;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath(Path), value, why))
		{
			Log("ask " + Path + " : " + why);
			return;
		}
		const NlAccess::Row row = NlAccess::Describe({}, value);
		Log("ask " + Path + " = " + row.Type + " " + row.Text);
	}

	// "<주소>=<수>": 써 넣고, 다시 읽고, 원래 값으로 되돌린다.
	void Poke(const std::string& Line)
	{
		const size_t eq = Line.rfind('=');
		double wanted = 0, old = 0, read = 0, back = 0;
		if (eq == std::string::npos || !NlCore::ParseNumber(NlCore::Trim(Line.substr(eq + 1)), wanted))
		{
			Log("poke " + Line + " : cannot read the line");
			return;
		}
		const std::string path = NlCore::Trim(Line.substr(0, eq));
		if (!NlAccess::ReadNumber(path, old))
		{
			Log("poke " + path + " : not a readable number");
			return;
		}

		std::string why, why_back;
		const bool stuck = NlAccess::WriteNumber(path, wanted, why);
		const bool have = NlAccess::ReadNumber(path, read);
		const bool restored = NlAccess::WriteNumber(path, old, why_back);
		const bool have_back = NlAccess::ReadNumber(path, back);
		Log("poke " + path + ": old " + Shortest(old) + " -> wrote " + Shortest(wanted) + ", read " + (have ? Shortest(read) : "?")
			+ (stuck ? " (stuck)" : " (not stuck: " + why + ")") + "; restored " + (have_back ? Shortest(back) : "?")
			+ (restored ? "" : " (restore failed: " + why_back + ")"));
	}

	void RunTests()
	{
		if (g_TestsDone)
			return;
		const double ready = NlUi::SecondsSinceReady();
		if (ready < 0 || ready < NlUi::ShotSeconds() - 5)
			return;
		g_TestsDone = true;

		for (const std::string& path : g_Asks)
			Ask(path);
		for (const std::string& line : g_Pokes)
			Poke(line);
		Log("ui tests done");
	}

	void Save()
	{
		std::ofstream out(g_StatePath, std::ios::trunc);
		out << NlCore::FormatCheatState(g_State);
	}

	// 그 영역에 보여 줄 것이 있는가. 없는 영역은 목록에서 흐리게 보인다.
	bool HasContent(Area Where)
	{
		return Where == Area::Explorer || Where == Area::Time || Where == Area::Tweaks || NlCheats::HasItems(Where);
	}
}

void NlMenu::Init(const std::filesystem::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log_)
{
	g_StatePath = ModuleDir / "NlToyBox.cheats.txt";
	g_Version = Version;
	g_Log = std::move(Log_);
	g_Start = Clock::now();

	if (std::ifstream in(g_StatePath); in)
		g_State = NlCore::KeepKnown(NlCore::ParseCheatState(in));
	NlCheats::Init(g_Log, g_State);
	NlExplorer::Init(g_Log, g_State.Pins, g_State.Locks);

	// 시험 설정(NlToyBox.ui.txt). 평소에는 아무 줄도 없다.
	for (const std::string& page : NlUi::TestValues("page"))
		if (const NlCore::AreaInfo* area = NlCore::FindArea(page))
			g_Page = static_cast<int>(area->Id);
	for (const std::string& path : NlUi::TestValues("path"))
		NlExplorer::Navigate(path);
	g_Asks = NlUi::TestValues("ask");
	g_Pokes = NlUi::TestValues("poke");
	g_TestsDone = g_Asks.empty() && g_Pokes.empty();
}

void NlMenu::GameTick()
{
	const double now = Seconds();
	const bool visible = NlUi::Visible();
	const Area page = static_cast<Area>(g_Page.load());

	NlExplorer::GameTick(now, visible && page == Area::Explorer);
	NlCheats::GameTick(now, visible);
	if (visible && now >= g_NextState)
	{
		g_NextState = now + 1;
		g_InGame = NlAccess::InGame();
	}
	RunTests();

	// 바뀐 것은 0.8초 뒤에 적는다(값을 끄는 동안 파일을 되풀이해 쓰지 않는다).
	const bool cheats = NlCheats::TakeChanges(g_State.On, g_State.Numbers);
	const bool explorer = NlExplorer::TakeChanges(g_State.Pins, g_State.Locks);
	if (cheats || explorer)
	{
		g_SavePending = true;
		g_SaveAt = now + 0.8;
	}
	if (g_SavePending && now >= g_SaveAt)
	{
		g_SavePending = false;
		Save();
	}
}

void NlMenu::Draw()
{
	// 상태 줄
	ImGui::TextDisabled("NlToyBox %s", g_Version.c_str());
	ImGui::SameLine();
	ImGui::TextUnformatted(g_InGame ? "게임 화면" : "게임 화면이 아님 (스위치는 게임을 시작하면 적용됩니다)");
	const int active = NlCheats::ActiveCount() + NlExplorer::ActiveLocks();
	ImGui::SameLine();
	ImGui::TextDisabled("켠 것 %d", active);
	ImGui::SameLine();
	ImGui::BeginDisabled(active == 0);
	if (ImGui::SmallButton("모두 끄기"))
	{
		NlCheats::ReleaseAll();
		NlExplorer::ReleaseAll();
	}
	ImGui::EndDisabled();
	ImGui::Separator();

	// 왼쪽: 영역의 목록. 아직 채우지 않은 영역은 흐리게, 몇 단계인지 적는다.
	const Area page = static_cast<Area>(g_Page.load());
	ImGui::BeginChild("areas", ImVec2(150, 0), ImGuiChildFlags_Borders);
	for (const NlCore::AreaInfo& area : NlCore::Areas())
	{
		const bool ready = HasContent(area.Id);
		const std::string label = ready ? area.Label : std::string(area.Label) + " (" + std::to_string(area.Stage) + "단계)";
		ImGui::BeginDisabled(!ready);
		if (ImGui::Selectable(label.c_str(), page == area.Id) && ready)
			g_Page = static_cast<int>(area.Id);
		ImGui::EndDisabled();
	}
	ImGui::EndChild();

	// 오른쪽: 고른 영역
	ImGui::SameLine();
	ImGui::BeginChild("panel", ImVec2(0, 0));
	switch (page)
	{
	case Area::Explorer: NlExplorer::Draw(); break;
	case Area::Time: NlCheats::DrawTime(); break;
	case Area::Tweaks: NlTweaks::Draw(); break;
	default: NlCheats::DrawArea(page); break;
	}
	ImGui::EndChild();
}
```

- [ ] **Step 3: 모듈에 붙인다**

`src/ModuleMain.cpp`:

1. 맨 위의 주석 세 줄을 다음으로 바꾼다.

```cpp
// NlToyBox 모듈의 들머리. 모듈 옆 NlToyBox.log 에 적재, YYToolkit 버전, 프로브 두 개의 결과를 쓰고
// 게임 스레드의 콜백에서 덤프, 모드창, 배율, 치트 메뉴의 틱을 돌린다.
// 처음 여섯 줄의 형식은 tools/common.ps1 의 Get-NlLoadFailures 가 그대로 찾는다 (Phase 0 스펙 §4.3). 바꾸면 그쪽도 바꾼다.
```

2. include 에 `#include "Menu.hpp"`를 더한다(`"Game.hpp"` 다음).
3. `constexpr const char* k_Version = "0.3.0";`을 `"0.4.0"`으로 바꾼다.
4. `CodeCallback`에서 `NlUi::GameTick();` 다음 줄에 `NlMenu::GameTick();`을 더한다.

```cpp
	void CodeCallback(FWCodeEvent& CodeContext)
	{
		ProbeOnce("object_call");
		NlDump::Tick(std::get<2>(CodeContext.Arguments()));
		NlUi::GameTick();
		NlMenu::GameTick();
		NlTweaks::GameTick();
	}
```

5. `ModuleInitialize`에서 `NlUi::SetContent(NlTweaks::Draw);`를 다음 두 줄로 바꾼다.

```cpp
	NlMenu::Init(module_dir, k_Version, [](const std::string& Line) { LogLine(Line); });
	NlUi::SetContent(NlMenu::Draw);
```

`CMakeLists.txt`의 `nltoybox` 소스 목록이 이렇게 된다.

```cmake
add_library(nltoybox SHARED
  src/ModuleMain.cpp
  src/Access.cpp
  src/Cheats.cpp
  src/Dump.cpp
  src/Explorer.cpp
  src/Finder.cpp
  src/Game.cpp
  src/Menu.cpp
  src/Search.cpp
  src/Tweaks.cpp
  src/Ui.cpp
  "${YYTK_SHARED}/YYTK_Shared_Types.cpp"
  "${IMGUI_ROOT}/imgui.cpp"
  "${IMGUI_ROOT}/imgui_draw.cpp"
  "${IMGUI_ROOT}/imgui_tables.cpp"
  "${IMGUI_ROOT}/imgui_widgets.cpp"
  "${IMGUI_ROOT}/backends/imgui_impl_dx11.cpp"
  "${IMGUI_ROOT}/backends/imgui_impl_win32.cpp"
)
```

- [ ] **Step 4: 빌드하고 네이티브 시험을 돌린다**

Run: `pwsh -File tools/build.ps1; pwsh -File tools/test-native.ps1`
Expected: `build ok -> …\NlToyBox.dll`, `core tests: N passed`, 종료 코드 0

- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add src/Menu.hpp src/Menu.cpp src/Ui.hpp src/Ui.cpp src/ModuleMain.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "feat(module): 치트 메뉴 0.4.0 — 왼쪽 목록에 탐색기, 영역별 스위치, 게임 속도, 배율"
```

---

### Task 11: 도구 — 적재 판정을 한 곳에 두고 `ui-check`가 함께 낸다

게임을 켜는 횟수를 줄인다: `ui-check.ps1` 한 번이 화면, 주소 읽기·쓰기, 적재 판정을 모두 본다.

**Files:**
- Modify: `tools/common.ps1` (끝에 `Get-NlLoadFailures`)
- Modify: `tools/check-load.ps1:39-50`
- Modify: `tools/ui-check.ps1`
- Test: `tools/tests/safety.tests.ps1`

**Interfaces:**
- Produces: `Get-NlLoadFailures([string[]]$Lines)` — 빠진 판정의 이름들(`loaded`, `builtin`, `script`, `done`). 비어 있으면 통과
- `ui-check.ps1`의 새 매개변수: `-Page <영역 Key>`, `-Path <주소>`, `-Ask <주소>[,…]`, `-Poke <주소=수>[,…]`

- [ ] **Step 1: 실패하는 시험을 쓴다**

`tools/tests/safety.tests.ps1`의 `try {` 바로 다음(첫 `Test-Case` 앞)에 넣는다.

```powershell
    Test-Case '적재 판정은 빠진 줄의 이름을 돌려준다' {
        $good = 'NlToyBox 0.4.0 loaded', 'yytk 5.0.0', 'trigger object_call', 'builtin code_is_compiled = true',
                'script gml_Script_command_line_parameters_init = found', 'probe done'
        Assert-Equal @(Get-NlLoadFailures $good).Count 0 '네 줄이 다 있으면 통과다'
        Assert-Equal (@(Get-NlLoadFailures @($good | Where-Object { $_ -ne 'probe done' })) -join ',') 'done' '빠진 줄의 이름'
        Assert-Equal (@(Get-NlLoadFailures @()) -join ',') 'loaded,builtin,script,done' '로그가 비면 넷 다 빠진다'
        Assert-True (@(Get-NlLoadFailures @('builtin code_is_compiled = false')) -contains 'builtin') '거짓이면 통과가 아니다'
    }

```

- [ ] **Step 2: 시험이 실패하는지 본다**

Run: `pwsh -File tools/tests/safety.tests.ps1`
Expected: `Get-NlLoadFailures` 를 찾지 못한다는 오류로 끝난다(`The term 'Get-NlLoadFailures' is not recognized`)

- [ ] **Step 3: 구현한다**

`tools/common.ps1`의 끝에 더한다.

```powershell

# 모듈이 붙었다는 판정. 줄 형식은 src/ModuleMain.cpp 가 쓴다 (Phase 0 스펙 §4.3). 'yytk' 줄은 판정에 쓰지 않는다.
# 돌려주는 값: 빠진 것의 이름들. 비어 있으면 통과다.
function Get-NlLoadFailures([string[]]$Lines) {
    $checks = [ordered]@{
        'loaded'  = [bool]($Lines -match '^NlToyBox \S+ loaded$')
        'builtin' = $Lines -contains 'builtin code_is_compiled = true'
        'script'  = $Lines -contains 'script gml_Script_command_line_parameters_init = found'
        'done'    = $Lines -contains 'probe done'
    }
    @($checks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object { $_.Key })
}
```

`tools/check-load.ps1`의 끝부분(`# 줄 형식은 …` 주석부터 `$failed = …`까지)을 다음으로 바꾼다. 그 아래의 `if ($failed.Count) { … }`와 `PASS`는 그대로 둔다.

```powershell
# 판정은 common.ps1 의 Get-NlLoadFailures 가 한다(ui-check.ps1 과 같이 쓴다).
$failed = @(Get-NlLoadFailures $lines)
```

`tools/ui-check.ps1`:

1. `param(` 의 `[string[]]$Saved = @()` 줄 끝에 쉼표를 붙이고 그 뒤에 더한다.

```powershell
    [string]$Page,         # 왼쪽 목록에서 열 영역의 이름(explorer, time, util, …). src/core/CheatTable.cpp 의 Key
    [string]$Path,         # 탐색기가 처음 열 주소(예: inst:o_time_controller)
    [string[]]$Ask = @(),  # 화면을 뜨기 5초 전에 그 주소의 값을 로그에 적는다
    [string[]]$Poke = @()  # '주소=수': 써 넣고, 다시 읽고, 원래 값으로 되돌린다. 결과를 로그에 적는다
```

2. `$Set = @($Set | …)` 줄 다음에 더한다.

```powershell
$Ask = @($Ask | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$Poke = @($Poke | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
```

3. 배율 설정을 치워 두는 곳(`if ($Saved.Count) { … }` 줄) 다음에 치트 상태도 치워 둔다.

```powershell
# 치트의 상태 파일도 같다.
$cheats = Join-Path $modDir 'NlToyBox.cheats.txt'
$cheatsKept = "$cheats.kept"
if (Test-Path -LiteralPath $cheats) { Move-Item -LiteralPath $cheats -Destination $cheatsKept -Force }
```

4. `$result = $null` 다음 줄에 `$allLines = @()`를 더한다.
5. 시험 설정을 쓰는 줄(`Set-Content -LiteralPath $config …`)을 다음으로 바꾼다.

```powershell
    $configLines = @("open=$([int](-not $Closed))", "shot_seconds=$ShotSeconds") + @($Set | ForEach-Object { "set=$_" }) +
        @(if ($TestDrag) { "drag=$TestDrag" }) + @(if ($Page) { "page=$Page" }) + @(if ($Path) { "path=$Path" }) +
        @($Ask | ForEach-Object { "ask=$_" }) + @($Poke | ForEach-Object { "poke=$_" })
    Set-Content -LiteralPath $config -Value $configLines -Encoding ascii
```

6. `Write-Host '--- NlToyBox.log ---'` 줄 바로 앞에 더하고, 그 다음 줄이 다시 읽지 않고 `$allLines`를 쓰게 한다.

```powershell
    $allLines = @(if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log })
    Write-Host '--- NlToyBox.log ---'
    if ($allLines.Count) { $allLines | ForEach-Object { Write-Host $_ } } else { Write-Host '(로그 없음)' }
    Write-Host '--------------------'
```

7. `finally` 안에서 배율 설정을 되돌리는 줄 다음에 더한다.

```powershell
    if (Test-Path -LiteralPath $cheats) {
        Write-Host '--- 시험이 저장한 치트 상태 ---'
        Get-Content -LiteralPath $cheats | ForEach-Object { Write-Host $_ }
        Remove-Item -LiteralPath $cheats -Force
    }
    if (Test-Path -LiteralPath $cheatsKept) { Move-Item -LiteralPath $cheatsKept -Destination $cheats -Force }
```

8. 끝의 판정(`if ($result -match '^ui shot done' …` 줄) 앞에 적재 판정을 더한다.

```powershell
# 적재 판정도 함께 낸다(check-load.ps1 과 같은 줄). 실행 한 번으로 둘을 본다.
$loadFailed = @(Get-NlLoadFailures $allLines)
if ($loadFailed.Count) { Write-Host "FAIL: 적재 판정 - $($loadFailed -join ', ')"; exit 1 }
Write-Host '적재 판정: 통과'
```

- [ ] **Step 4: 시험이 통과하는지 본다**

Run: `pwsh -File tools/tests/safety.tests.ps1`
Expected: `ok - 적재 판정은 빠진 줄의 이름을 돌려준다`, 끝에 `safety tests: N passed`, 종료 코드 0 (게임을 켜지 않는다)

- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add tools/common.ps1 tools/check-load.ps1 tools/ui-check.ps1 tools/tests/safety.tests.ps1
git -C E:\NlToyBox commit -m "feat(tools): ui-check 가 영역·주소·읽기·쓰기 시험을 받고 적재 판정도 함께 낸다"
```

---

### Task 12: 실행 한 번으로 확인하고, 적고, 넣는다

게임을 켜는 것은 이 Task 의 Step 3 한 번이다. 메인 메뉴까지만 간다(새 게임을 시작하지 않는다).

**Files:**
- Create: `research/06-cheat-menu.md`
- Modify: `CLAUDE.md`, `README.md`, `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` (상태 줄과 §13)

- [ ] **Step 1: 게임을 켜지 않는 확인을 모두 돌린다**

```powershell
pwsh -File tools/build.ps1
pwsh -File tools/test-native.ps1
pwsh -File tools/tests/safety.tests.ps1
py -3.14 -m unittest discover -s tools/re/tests
py -3.14 -m unittest discover -s tools/overlay/tests
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"
```

Expected: 넷 모두 통과(종료 코드 0), 마지막 명령의 출력은 비어 있다.

- [ ] **Step 2: 배포하고 세이브의 사본을 뜬다**

```powershell
pwsh -File tools/game-status.ps1
pwsh -File tools/deploy.ps1
pwsh -File tools/saves-backup.ps1
```

Expected: exe 가 패치돼 있다(`Patched`), `deployed -> …\mods\Aurie\NlToyBox.dll`, 세이브 사본의 경로.

- [ ] **Step 3: 게임을 한 번 켠다**

켜기 전에 사용자에게 알린다: "게임을 켭니다. 2분쯤 걸리고 끝나면 스스로 꺼집니다. 그동안 게임 창을 누르지 마세요."

`o_time_controller`는 메인 메뉴에서 생긴다(모듈이 뜬 뒤 28~50초. `research/02`, `research/04`). 그래서 80초에 화면을 뜬다.

```powershell
pwsh -File tools/ui-check.ps1 -Name cheat-menu-1 -ShotSeconds 80 -TimeoutSec 240 -Page explorer -Path inst:o_time_controller `
  -Ask inst:o_time_controller.time_warp,inst:o_time_controller.__game_time,inst:o_time_controller.__debug_custom_wrap,inst:o_time_controller.time_speed_index,inst:o_time_controller.time_speed_variants[0],inst:o_time_controller.time_speed_variants[1],inst:o_time_controller.time_speed_variants[2],inst:o_time_controller.time_speed_variants[3],inst:o_data.current_debug_mode,global.__gameplay_vars.bribe_give_rings,inst:o_debug.is_debug_enabled,inst:o_time_controller.set_time_speed `
  -Poke inst:o_time_controller.time_warp_max=101,inst:o_time_controller.time_speed_variants[3]=7.5,global.__gameplay_vars.bribe_give_rings=11
```

Expected:
- `적재 판정: 통과`, `PASS`, 종료 코드 0, `게임 종료: closed`
- 로그에 `NlToyBox 0.4.0 loaded`, `cheat state: none`, `ui ready …`
- `ask inst:o_time_controller.time_warp = number 0` (메뉴에서는 0 이다. `research/02`)
- `ask inst:o_debug.is_debug_enabled : no instance of o_debug at 0` (메뉴에는 `o_debug`가 없다)
- `ask inst:o_time_controller.set_time_speed = method 함수`
- `poke inst:o_time_controller.time_warp_max: old 100 -> wrote 101, read 101 (stuck); restored 100` — `variable_instance_set`이 이 러너에서 된다
- `poke inst:o_time_controller.time_speed_variants[3]: old … -> wrote 7.5, read 7.5 (stuck); restored …` — `array_set`이 된다
- `poke global.__gameplay_vars.bribe_give_rings: old 10 -> wrote 11, read 11 (stuck); restored 10`
- `ui tests done`, `ui shot done …`
- 화면 `refs\ui\cheat-menu-1.png`: 왼쪽에 영역의 목록(인물·영주·지식·아이템·프리셋은 흐리고 단계가 적혀 있다), 오른쪽에 `o_time_controller`의 변수 표

기대와 다른 줄이 있으면 그것이 이 실행의 발견이다. 고칠 수 있는 것은 고치되 **게임을 다시 켜지 않는다**(승인받은 것은 한 번이다). 다시 켜야 확인되는 것은 사용자에게 알리고 승인을 받는다.
`poke`가 `not stuck`이면 그 빌트인은 이 러너에서 그 꼴로는 안 되는 것이다. `research/06`에 적고, 탐색기는 그 자리를 "써지지 않음"으로 보인다(이미 그렇게 만들어져 있다).

- [ ] **Step 4: 화면을 눈으로 본다**

`refs\ui\cheat-menu-1.png`를 열어 Step 3 의 마지막 줄대로인지, 한글이 깨지지 않았는지, 표가 창 안에 들어오는지 본다.

- [ ] **Step 5: 잰 것을 적는다**

`research/06-cheat-menu.md`를 `research/05-mod-window.md`의 꼴로 쓴다. 들어갈 것:

- 머리: 조사일, 대상(게임 버전, 모듈 `0.4.0`), 게임을 켠 횟수(1), 메뉴까지만 갔다는 것
- "된 것": Step 3 의 로그에서 실제로 나온 `ask`·`poke` 줄을 그대로 옮긴 표(주소, 결과)
- "처음 잰 것": `variable_instance_exists`·`variable_instance_set`·`array_set`·`is_method`·`instance_exists`가 이 러너에서 됐는지(로그의 `stuck`/`not stuck`, `method`로 판정), `time_speed_variants`의 네 값, `current_debug_mode`의 값
- "확인하지 못한 것": 스위치 31개의 효과, 게임 속도의 손잡이, 탐색기의 찾기에 걸리는 시간, 잠금, 게임 화면에서의 모든 것(이 실행은 메뉴까지만 갔다)
- "사용자가 플레이에서 볼 것": 스펙 §10 의 2단계 "끝났다는 것"을 차례로(탐색기로 값 하나를 고친다 → 건설·생산의 "건물 즉시 건설"을 켜고 건물을 놓는다 → 시간에서 x5 를 누른다 → 결과를 알린다)

`docs/superpowers/specs/2026-10-05-cheat-menu-design.md`의 상태 줄에 "1·2단계 구현(2026-10-05). 메뉴에서 잰 것은 `research/06-cheat-menu.md`"를 더하고, §13 에서 이 실행으로 답이 나온 줄(`variable_instance_set`, `array_set`이 되는가)을 결과로 바꾼다.

- [ ] **Step 6: 쓰는 법과 규칙을 적는다**

`CLAUDE.md`:

1. "브랜치 전략"의 CI 줄에서 `` `tools/check-load.ps1` 종료 코드 0을 확인한다 ``를 `` `tools/check-load.ps1`(또는 같은 판정을 함께 내는 `tools/ui-check.ps1`) 종료 코드 0을 확인한다 ``로 바꾼다.
2. "개발 루프"의 명령 목록에서 `ui-check.ps1` 줄의 설명을 "게임을 켜서 모드창이 그려진 프레임을 refs\ui\ 로 받아 온다. 적재 판정도 함께 낸다. `-Page`·`-Path`·`-Ask`·`-Poke`로 영역, 주소, 읽기, 쓰기를 시험한다"로 바꾼다.
3. "모듈을 쓸 때"에 묶음을 더한다(Step 3 에서 `not stuck`이 나온 빌트인이 있으면 그 줄을 결과대로 고친다).

```markdown
- 치트 메뉴(`src/Menu.cpp`)는 왼쪽 목록의 영역마다 패널을 그린다. 스펙은 `docs/superpowers/specs/2026-10-05-cheat-menu-design.md`.
  - 게임의 값은 주소(`src/core/AskPath`) 하나로 가리킨다: `global.a.b[3]`, `inst:o_debug.is_x`, `inst:o_building:1.generic`, `map:150@key#0`.
    읽고 쓰는 것은 `src/Access.cpp`가 한다. 없는 것을 만들지 않고, 쓴 뒤에는 다시 읽어 확인한다.
  - **그리는 쪽(`Draw*`)에서 러너를 부르지 않는다.** 창은 바라는 상태나 명령만 남기고, 러너는 `GameTick`이 건드린다.
    `RValue`를 틱 너머로 들지 않는다(스냅샷은 글이다).
  - 치트 항목을 더하는 일은 `src/core/CheatTable.cpp`의 표에 한 줄을 더하는 일이다. 주소에 ds 번호를 적지 않는다.
    `Verified`는 플레이에서 효과를 본 뒤에만 참으로 바꾸고 `research/`에 적는다. **이름에서 읽은 뜻은 추정이다.**
  - 켠 치트와 즐겨찾기·잠금은 `mods\Aurie\NlToyBox.cheats.txt`에 저장된다(사용자의 설정이다. 도구가 지우지 않는다).
    파일에서 불러온 잠금은 꺼진 채로 시작한다(`inst:<오브젝트>:<n>`의 n 이 실행마다 다른 인스턴스일 수 있다).
  - 게임 속도는 후보 넷을 차례로 써 보고 `__game_time`의 흐름으로 판정한다(`src/core/SpeedTrial`). 메뉴에서는 시간이 흐르지 않아 시험할 수 없다.
  - 모드창의 글꼴에는 한글과 라틴-1 만 있다. 창의 글에 화살표나 별 같은 기호를 쓰지 않는다.
```

`README.md`의 "게임 안에서 배율 조절하기" 절을 다음으로 바꾼다.

```markdown
## 게임 안의 치트 메뉴

모듈을 놓고(`setup-aurie.ps1`, `build.ps1`, `deploy.ps1`) 게임을 켠 뒤 **F8**을 누르면 모드창이 뜬다. 왼쪽에서 영역을 고른다.

- **탐색기**: 게임의 아무 값이나 보고 고친다. 값을 잠그거나(0.1초마다 다시 써 넣는다) 이름·값으로 찾을 수 있다.
- **경제, 건설·생산, 인구·욕구, 군대·전투, 외교, 종교, 월드, 이벤트, 유틸**: 게임에 들어 있는 개발자 스위치와 수.
  이름 옆의 `(?)`는 효과를 아직 확인하지 않았다는 뜻이다.
- **시간**: 게임 속도. 처음 누를 때 어느 값이 속도를 정하는지 시험으로 찾는다(게임 화면에서, 일시정지를 푼 채로).
- **배율**: 건설 비용 같은 값의 배율(0.3.0 의 것).

켠 것은 `mods\Aurie\NlToyBox.cheats.txt`에 저장된다. 위쪽의 "모두 끄기"가 켠 것을 원래 값으로 되돌린다.
```

README 의 "문서" 목록에 `- `research/06-cheat-menu.md` — 치트 메뉴에서 잰 것`을 더한다.

- [ ] **Step 7: 문서를 커밋하고 `develop`에 넣는다**

```powershell
git -C E:\NlToyBox add research/06-cheat-menu.md CLAUDE.md README.md docs/superpowers/specs/2026-10-05-cheat-menu-design.md
git -C E:\NlToyBox commit -m "docs(cheat): 치트 메뉴 1·2단계에서 잰 것과 쓰는 법"
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"
git -C E:\NlToyBox checkout develop
git -C E:\NlToyBox merge --no-ff feat/cheat-menu -m "Merge branch 'feat/cheat-menu' into develop"
git -C E:\NlToyBox branch -d feat/cheat-menu
```

Expected: `ls-files` 의 출력이 비어 있다. 머지 커밋이 생기고 브랜치가 지워진다.

---

## Self-Review

- **스펙 대조**: §4(구조: Task 6~10), §5(주소: Task 1, 6), §6(치트 표·적용·상태 파일: Task 2, 3, 9, 10), §7(탐색기: Task 7, 8), §8(메뉴 구조와 2단계 항목: Task 3, 10), §9(게임 속도: Task 4, 5, 9), §11(오류와 안전: Task 6 의 쓰기 규칙, Task 9 의 끄기·"모두 끄기"), §12(시험: Task 1~5, 11, 12). §3 의 수단 C·D(함수 호출, 훅)는 3단계의 것이라 이 계획에 없다.
- **빈칸**: 없다. Task 12 Step 5 의 `research/06`은 그 실행의 로그를 옮기는 일이라 내용을 미리 적을 수 없다. 들어갈 항목을 적었다.
- **이름의 일치**: `Cheat::Where`(스펙 §6.1 도 같게 고쳤다), `NlExplorer::GameTick(Now, Shown)`, `NlCheats::GameTick(Now, Visible)`, `SpeedTrial::Start(BaseRate, BaseWarp)`, `NlUi::TestValues`·`ShotSeconds`·`SecondsSinceReady`가 쓰는 곳과 만드는 곳에서 같다.
- **Review Focus**: 다섯 줄 모두 Task 2~5 의 시험이 잡는다.
