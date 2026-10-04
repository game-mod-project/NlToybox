# 데이터 오버레이 단계 0b (새 게임 상태 실측) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 새 게임에 들어가기 전후의 런타임 상태를 기록하고 되풀이 덤프해서, 데이터 파일의 값이 런타임에 반영되는지와 "게임 안"을 가르는 신호를 실측한다.

**Architecture:** 모듈이 0.5초마다 룸 이름, 오브젝트별 인스턴스 수, 지정한 전역 값을 재서 바뀔 때마다 로그에 적고, 메인 메뉴에서 한 번·그 뒤로 일정 간격으로 덤프한다(마지막 몇 개만 남긴다). **조건을 미리 정하지 않는다.** 사용자가 새 게임을 시작해 잠시 둔 뒤 평소처럼 끄면 도구가 덤프를 모은다. 찾기는 너비 우선으로 고치고 ds_map·ds_list와 인스턴스 변수까지 넓힌다. 러너에 기대지 않는 부분(요청 읽기, 덤프 일정, 글 처리)은 `src/core/`로 나눠 게임 없이 시험한다. 게임에 들어간 것을 알아보는 로직은 이 실측에서 나온 신호로 다음 계획에서 만든다.

**Tech Stack:** C++ (MSVC `/std:c++latest`, CMake + Ninja), YYToolkit v5.0.0c 인터페이스, PowerShell 7, Python 3.14 (`unittest`).

**Spec:** `docs/superpowers/specs/2026-10-04-data-overlay-design.md` §3.7. 앞 단계의 결과는 `research/01-data-overlay.md`.

## Global Constraints

- **추측으로 작업하지 않는다.** 이 계획이 기대는 러너·게임·라이브러리의 사실은 스펙 §3.7의 "근거" 표에 출처가 있다. 실행 중에 표에 없는 사실이 필요해지면 코드보다 먼저 그것을 확인하고(로컬 데이터, Context7의 공식 문서, 인터넷 리서치), 출처를 원장과 스펙의 표에 적는다. 확인할 수 없으면 "모른다"로 두고 그 위에 다음 단계를 쌓지 않는다.
- 게임을 켜는 횟수는 **최대 2회**(실행 1 + 예비 1). 더 필요하면 멈추고 사용자에게 묻는다.
- 게임을 켜기 전에 사용자에게 알린다. 이번에는 사용자가 할 일이 있다: 메뉴 덤프 뒤에 새 게임을 시작하고, 게임 화면에서 2분쯤 둔 뒤 평소처럼 게임을 끈다.
- 세이브 폴더(`%LOCALAPPDATA%\Strategy`)에는 **쓰지 않는다.** 읽어서 사본을 뜰 뿐이다. 새 게임이 만든 세이브를 지우는 것은 사용자가 정한다.
- `refs/`, `backups/`, `downloads/`, `build/`는 커밋하지 않는다. 커밋 전에 `git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"`가 비어 있어야 한다.
- 서브모듈 `external/YYToolkit`의 파일은 고치지 않는다.
- 러너에 닿는 호출은 빌트인(`CallBuiltinEx`)과 문서·소스·실행으로 확인한 인터페이스(`EnumInstanceMembers`, `GetGlobalInstance`, `GetBuiltin`, `GetRunnerInterface().StructGetKeys`, `CallGameScriptEx`)만 쓴다. `GetInstanceObject`와 `CRoom`은 러너 내부 구조체의 배치에 기대므로 쓰지 않는다. `GetInstanceMember`(없는 이름일 때)와 `GetInstanceMemberCount`는 이 러너에서 확인하지 못한 빌트인을 구조체에 대고 부르므로(서브모듈 `MI_Public.cpp`) 쓰지 않는다.
- 정적 저장 기간의 `RValue`를 두지 않는다(프로세스가 끝날 때 소멸자가 YYToolkit을 부른다). `RValue`는 함수 안에서만 든다.
- 인자의 형을 모르는 게임 스크립트는 부르지 않는다. 실행 1의 요청에는 `script=` 줄이 없다.
- 스크립트는 `pwsh`(PowerShell 7), Python은 `py -3.14`.
- 데이터 파일은 `tools/data-snapshot.ps1`으로 스냅샷을 뜬 뒤에만 고친다. 이번에 고치는 파일은 다섯이다: `debug_params.json`, `gameplay_variables.json`, `battle_params.json`, `director_params.json`, `knowledge\technology\cultural_knowledge\addiction_resist.json`.
- 브랜치: `develop`에서 `feat/overlay-stage0b`를 만들어 작업하고 `git merge --no-ff`로 `develop`에 합친다. `main`은 건드리지 않는다. git 명령은 `git -C E:\NlToyBox`.
- 커밋 메시지는 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`으로 끝낸다.

**TDD의 예외(계획 승인에 포함해 달라):** 러너에 닿는 모듈 코드(`src/Game.cpp`, `src/Finder.cpp`, `src/Dump.cpp`)는 게임 없이는 시험할 수 없다. 이 코드의 관문은 빌드 성공이고, 시험은 실행 1의 다섯 확인(Task 5 Step 6)이다. 게임 없이 시험할 수 있는 부분은 모두 `src/core/`와 도구로 빼서 먼저 실패하는 시험을 쓴다.

**실행 전 검토에서 고친 것 (2026-10-04):** Task 1~4를 마친 뒤, 게임을 켜기 전에 전체 검토를 받고 고쳤다(커밋 `fix(review): …`). 아래 Task 1~4의 코드 블록은 처음 쓴 그대로이고 **지금의 소스가 정본이다.** 달라진 것: 찾기의 한도를 요청 키(`max_*`)로 받는다, 한도와 통계를 구역마다 따로 센다, ds 형 상수를 만들고 지워서 먼저 확인한다, 중첩 ds는 `ds_exists`를 본 뒤에만 연다, 히트 경로를 자르지 않는다, 멤버 열거가 끊긴 수(`enum_failed`, `enum_short`)와 길어서 들어가지 않은 것(`too_long`)을 적는다, `watch`는 열거로 찾는다, 첫 표본의 호출마다 로그를 남긴다, 기록 줄의 한도를 올린다, `probe.ps1`은 복사한 덤프만 지운다, `dump_tool.py controls`가 `hits_cut`과 열거 끊김을 본다. 시험 수는 네이티브 18, 파이썬 9, 안전 19다.

## Review Focus

1. **요청 파일의 오타나 잘못된 값.** 조용히 기본값으로 돌면 게임 실행 한 번과 사용자의 수고를 버린다. 모르는 키와 잘못된 값은 오류여야 하고, 오류가 있으면 모듈은 아무것도 하지 않아야 하며, `probe.ps1`은 기다리지 않고 실패해야 한다. → Task 1 Step 1(읽기 시험, `tools/probes/*.txt` 전부 읽기), Task 3 Step 1(`dump failed` 시험)
2. **되풀이 덤프의 일정.** 덤프가 오래 걸려도 다음 덤프가 바로 겹치면 안 되고, 사용자가 언제 끄든 마지막 몇 개가 남아야 한다. → Task 1 Step 1(일정 시험 넷)
3. **도구나 게임이 도중에 죽는 경우.** 요청 파일이나 덤프가 게임 폴더에 남으면 안 된다. → Task 3 Step 1(`probe`와 `restore`의 시험)
4. **"없다"를 "보지 못했다"와 가르지 못하는 경우.** 찾기가 한도에 걸렸거나, ds 자가 점검이 실패했거나, ds 번호가 본 범위를 넘었으면 "없다"를 답으로 쓰면 안 된다. → Task 2 Step 1(`controls` 시험), Task 5 Step 6
5. **세이브 폴더.** 사본을 뜨는 도구가 세이브 폴더에 무엇이든 쓰면 안 된다. → Task 3 Step 1(`saves-backup` 시험)

## File Structure

| 파일 | 책임 |
|---|---|
| `src/core/Text.hpp`, `Text.cpp` | 글 처리: `Trim`, `Quote`(JSON 문자열), `Number`, `Fixed`, `ParseNumber`, `Contains`. 러너에 기대지 않는다 |
| `src/core/Request.hpp`, `Request.cpp` | 요청 파일을 읽어 `Request`로 만든다. 모르는 키와 잘못된 값은 `Errors`에 쌓는다 |
| `src/core/Schedule.hpp`, `Schedule.cpp` | 덤프의 일정: 언제 덤프하고 그 덤프의 이름이 무엇인가(`menu`, `late0` …) |
| `src/core/PathTable.hpp` | 찾기가 지나온 길. 맞은 것이 나올 때만 경로 글을 만든다 |
| `tests/native/core_tests.cpp` | 위 넷의 시험. `nlcore_tests.exe`로 빌드된다 |
| `tools/test-native.ps1` | `nlcore_tests.exe`를 돌린다 |
| `src/Game.hpp`, `Game.cpp` | 러너에 닿는 얇은 층: 빌트인 호출, 오브젝트 목록, 룸 이름, `global.a.b` 따라가기, 값 적기 |
| `src/Finder.hpp`, `Finder.cpp` | 값·이름으로 찾기: 전역(너비 우선), ds_map·ds_list, 인스턴스 변수 |
| `src/Dump.hpp`, `Dump.cpp` | 요청 읽기, 상태 재기, 일정에 따라 덤프 쓰기 |
| `src/ModuleMain.cpp` | 버전 `0.2.0`. 콜백보다 먼저 덤프를 준비하고, 콜백에서 코드 객체를 넘긴다 |
| `tools/probe.ps1` | 덤프를 이름대로 가져온다. `dump failed`면 바로 실패한다. 되풀이 요청이면 사용자가 게임을 끌 때까지 기다린다 |
| `tools/saves-backup.ps1` | 세이브 폴더의 사본과 차이 |
| `tools/common.ps1` | `Get-NlSavesDir` |
| `tools/restore-game.ps1` | 이름이 여럿인 덤프를 모두 지운다 |
| `tools/re/dump_tool.py`, `tools/re/tests/test_dump_tool.py` | 새 덤프 형식, `hits`, `controls` |
| `tools/probes/stage0b-run1.txt` | 실행 1의 요청 |
| `research/02-new-game-state.md` | 결과 |

---

### Task 0: 작업 브랜치

- [ ] **Step 1: 브랜치를 만든다**

```powershell
git -C E:\NlToyBox switch develop
git -C E:\NlToyBox switch -c feat/overlay-stage0b
git -C E:\NlToyBox status --short
```

Expected: 마지막 명령의 출력이 비어 있다.

---

### Task 1: 러너에 기대지 않는 코어와 네이티브 시험

**Files:**
- Create: `E:\NlToyBox\src\core\Text.hpp`, `Text.cpp`, `Request.hpp`, `Request.cpp`, `Schedule.hpp`, `Schedule.cpp`, `PathTable.hpp`
- Create: `E:\NlToyBox\tests\native\core_tests.cpp`
- Create: `E:\NlToyBox\tools\test-native.ps1`
- Modify: `E:\NlToyBox\CMakeLists.txt`

**Interfaces:**
- Consumes: 없음
- Produces (모두 `namespace NlCore`):
  - `std::string Trim(const std::string&)`, `std::string Quote(std::string Text, size_t MaxBytes = 200)`, `std::string Number(double)`, `std::string Fixed(double Value, int Digits)`, `bool ParseNumber(const std::string&, double& Out)`, `bool Contains(const std::string& Name, const std::string& Part)`
  - `struct ScriptCall { std::string Name; std::vector<std::string> Args; }`
  - `struct Request { int DelaySeconds; double RepeatSeconds; int KeepLast; bool TraceEvents; std::vector<double> FindValues; std::vector<std::string> FindNames, Watches, Skip, Errors; std::vector<ScriptCall> Scripts; }`
  - `Request ParseRequest(std::istream&)`
  - `class Schedule { Schedule(); Schedule(double DelaySeconds, double RepeatSeconds, int KeepLast); std::string Due(double Now) const; void Finished(double Now); bool Repeats() const; bool Exhausted() const; int Count() const; }`
  - `class PathTable { int Add(int Parent, std::string Segment); std::string Path(int Node) const; }`
  - `pwsh -File tools/test-native.ps1` → 끝줄 `core tests: <n> passed`(종료 코드 0) 또는 `core tests: <n> FAILED`(1)

- [ ] **Step 1: 실패하는 시험을 쓴다 — `tests/native/core_tests.cpp`**

```cpp
// 러너에 기대지 않는 코어(src/core)의 시험. 게임을 켜지 않는다.
// 사용: nlcore_tests.exe <요청 파일 폴더>     (tools/test-native.ps1 이 부른다)

#include "core/PathTable.hpp"
#include "core/Request.hpp"
#include "core/Schedule.hpp"
#include "core/Text.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

using namespace NlCore;

namespace
{
	int g_Failed = 0;
	int g_Passed = 0;
	std::string g_ProbesDir;

	void Check(bool Ok, const char* What, int Line)
	{
		if (Ok)
			return;
		std::printf("  FAIL line %d: %s\n", Line, What);
		g_Failed++;
	}

	void CheckStr(const std::string& Got, const std::string& Want, const char* What, int Line)
	{
		if (Got == Want)
			return;
		std::printf("  FAIL line %d: %s\n    got  <%s>\n    want <%s>\n", Line, What, Got.c_str(), Want.c_str());
		g_Failed++;
	}

#define CHECK(cond) Check((cond), #cond, __LINE__)
#define CHECK_STR(got, want) CheckStr((got), (want), #got, __LINE__)

	void Test(const char* Name, void (*Body)())
	{
		const int before = g_Failed;
		Body();
		std::printf("%s - %s\n", g_Failed == before ? "ok" : "not ok", Name);
		if (g_Failed == before)
			g_Passed++;
	}

	Request Parse(const std::string& Text)
	{
		std::istringstream in(Text);
		return ParseRequest(in);
	}
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::printf("usage: nlcore_tests <probes dir>\n");
		return 2;
	}
	g_ProbesDir = argv[1];

	Test("Trim 은 양끝의 공백과 줄바꿈을 뗀다", [] {
		CHECK_STR(Trim("  a b \r\n"), "a b");
		CHECK_STR(Trim(" \t "), "");
	});

	Test("Quote 는 따옴표, 역슬래시, 제어 문자를 JSON 으로 쓴다", [] {
		CHECK_STR(Quote("a\"b\\c\n\x01"), "\"a\\\"b\\\\c\\n\\u0001\"");
	});

	Test("Quote 는 긴 글을 UTF-8 글자 경계에서 자른다", [] {
		// 3바이트 글자 넷(12바이트). 7바이트에서 자르면 두 글자(6바이트)만 남아야 한다.
		const std::string text = "\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80";
		CHECK_STR(Quote(text, 7), "\"\xEA\xB0\x80\xEA\xB0\x80...\"");
	});

	Test("Quote 는 잘못된 UTF-8 바이트를 물음표로 바꾼다", [] {
		CHECK_STR(Quote("a\xFF" "b"), "\"a?b\"");
	});

	Test("Number 는 정수는 정수로, 유한하지 않은 수는 글로 쓴다", [] {
		CHECK_STR(Number(2345), "2345");
		CHECK_STR(Number(0.5), "0.5");
		CHECK_STR(Number(std::nan("")), "\"nan\"");
		CHECK_STR(Number(std::numeric_limits<double>::infinity()), "\"inf\"");
		CHECK_STR(Number(-std::numeric_limits<double>::infinity()), "\"-inf\"");
	});

	Test("Fixed 는 소수 자릿수를 맞춘다", [] {
		CHECK_STR(Fixed(12.345, 1), "12.3");
		CHECK_STR(Fixed(7, 1), "7.0");
	});

	Test("ParseNumber 는 글 전체가 수일 때만 받는다", [] {
		double value = 0;
		CHECK(ParseNumber("60", value) && value == 60);
		CHECK(ParseNumber("-1.5", value) && value == -1.5);
		CHECK(!ParseNumber("60s", value));
		CHECK(!ParseNumber("", value));
		CHECK(!ParseNumber("abc", value));
	});

	Test("Contains 는 부분 일치를 본다. 빈 낱말은 아무것에도 맞지 않는다", [] {
		CHECK(Contains("battle_battle_dodge_base", "dodge_base"));
		CHECK(!Contains("dodge", "dodge_base"));
		CHECK(!Contains("abc", ""));
	});

	Test("요청: 줄이 없으면 기본값이다", [] {
		const Request r = Parse("# 주석\n\n");
		CHECK(r.Errors.empty());
		CHECK(r.DelaySeconds == 60 && r.RepeatSeconds == 0 && r.KeepLast == 3 && !r.TraceEvents);
		CHECK(r.FindValues.empty() && r.FindNames.empty() && r.Watches.empty() && r.Skip.empty() && r.Scripts.empty());
	});

	Test("요청: 모든 키를 읽는다", [] {
		const Request r = Parse(
			"delay_seconds=45\r\n"
			"repeat_seconds=30\n"
			"keep_last=2\n"
			"trace_events=1\n"
			"skip=ds\n"
			"skip=room\n"
			"find=2345\n"
			"find_name= dodge_base \n"
			"watch=global.a.b\n");
		CHECK(r.Errors.empty());
		CHECK(r.DelaySeconds == 45 && r.RepeatSeconds == 30 && r.KeepLast == 2 && r.TraceEvents);
		CHECK(r.Skip.size() == 2 && r.Skip[0] == "ds" && r.Skip[1] == "room");
		CHECK(r.FindValues.size() == 1 && r.FindValues[0] == 2345);
		CHECK(r.FindNames.size() == 1 && r.FindNames[0] == "dodge_base");
		CHECK(r.Watches.size() == 1 && r.Watches[0] == "global.a.b");

		const Request s = Parse("script=gml_Script_x|1|wood\n");
		CHECK(s.Errors.empty());
		CHECK(s.Scripts.size() == 1 && s.Scripts[0].Name == "gml_Script_x" && s.Scripts[0].Args.size() == 2 && s.Scripts[0].Args[1] == "wood");
	});

	Test("요청: 모르는 키와 잘못된 값은 오류로 남는다", [] {
		CHECK(Parse("dealy_seconds=60\n").Errors.size() == 1);		// 오타
		CHECK(Parse("delay_seconds=abc\n").Errors.size() == 1);
		CHECK(Parse("delay_seconds=-1\n").Errors.size() == 1);
		CHECK(Parse("repeat_seconds=5\n").Errors.size() == 1);		// 너무 짧다. 덤프가 이어 붙는다
		CHECK(Parse("keep_last=0\n").Errors.size() == 1);
		CHECK(Parse("keep_last=10\n").Errors.size() == 1);
		CHECK(Parse("keep_last=1.5\n").Errors.size() == 1);
		CHECK(Parse("trace_events=yes\n").Errors.size() == 1);
		CHECK(Parse("find=wood\n").Errors.size() == 1);
		CHECK(Parse("find_name=\n").Errors.size() == 1);
		CHECK(Parse("watch=o_main_menu.x\n").Errors.size() == 1);	// global. 로 시작해야 한다
		CHECK(Parse("skip=everything\n").Errors.size() == 1);
		CHECK(Parse("script=|1\n").Errors.size() == 1);
		CHECK(Parse("no equals sign\n").Errors.size() == 1);
		// 되풀이 덤프와 스크립트 호출은 함께 쓸 수 없다(같은 스크립트를 여러 번 부르지 않는다).
		CHECK(Parse("repeat_seconds=45\nscript=gml_Script_x\n").Errors.size() == 1);
	});

	Test("일정: 첫 덤프는 delay 뒤의 menu 다", [] {
		Schedule s(60, 0, 3);
		CHECK_STR(s.Due(59.9), "");
		CHECK_STR(s.Due(60), "menu");
		CHECK(s.Count() == 0 && !s.Exhausted());
	});

	Test("일정: 되풀이하지 않으면 첫 덤프 뒤에는 없다", [] {
		Schedule s(60, 0, 3);
		s.Finished(62);
		CHECK(s.Exhausted() && !s.Repeats() && s.Count() == 1);
		CHECK_STR(s.Due(100000), "");
	});

	Test("일정: 되풀이는 앞 덤프가 끝난 때부터 센다", [] {
		Schedule s(60, 45, 3);
		s.Finished(70);		// menu 덤프가 10초 걸렸다
		CHECK(!s.Exhausted() && s.Repeats());
		CHECK_STR(s.Due(114.9), "");
		CHECK_STR(s.Due(115), "late0");
	});

	Test("일정: 되풀이 덤프는 마지막 keep_last 개의 이름을 돌려 쓴다", [] {
		Schedule s(60, 45, 3);
		s.Finished(60);		// menu
		CHECK_STR(s.Due(105), "late0");
		s.Finished(105);
		CHECK_STR(s.Due(150), "late1");
		s.Finished(150);
		CHECK_STR(s.Due(195), "late2");
		s.Finished(195);
		CHECK_STR(s.Due(240), "late0");
		s.Finished(240);
		CHECK(s.Count() == 5);
	});

	Test("PathTable 은 뿌리부터 이어 붙인 경로를 돌려준다", [] {
		PathTable paths;
		const int root = paths.Add(-1, "global");
		const int a = paths.Add(root, ".a");
		const int item = paths.Add(a, "[3]");
		CHECK_STR(paths.Path(item), "global.a[3]");
		CHECK_STR(paths.Path(root), "global");
		CHECK_STR(paths.Path(-1), "");
	});

	// 요청 파일의 오타로 게임 실행 한 번을 버리지 않는다.
	Test("tools/probes 의 요청 파일은 모두 오류 없이 읽힌다", [] {
		int files = 0;
		for (const auto& entry : std::filesystem::directory_iterator(g_ProbesDir))
		{
			if (entry.path().extension() != ".txt")
				continue;
			std::ifstream in(entry.path());
			const Request r = ParseRequest(in);
			files++;
			for (const std::string& error : r.Errors)
			{
				std::printf("  FAIL %s: %s\n", entry.path().filename().string().c_str(), error.c_str());
				g_Failed++;
			}
		}
		CHECK(files > 0);
	});

	if (g_Failed)
	{
		std::printf("core tests: %d FAILED\n", g_Failed);
		return 1;
	}
	std::printf("core tests: %d passed\n", g_Passed);
	return 0;
}
```

- [ ] **Step 2: `CMakeLists.txt`에 코어와 시험을 더한다**

`add_library(nltoybox SHARED` 줄 **위에** 넣는다:

```cmake
# 러너에 기대지 않는 부분. 모듈과 네이티브 시험이 같이 쓴다.
add_library(nlcore STATIC
  src/core/Text.cpp
  src/core/Request.cpp
  src/core/Schedule.cpp
)
set_target_properties(nlcore PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
target_compile_options(nlcore PUBLIC /std:c++latest /W3 /utf-8)

# src/core 의 시험. 게임을 켜지 않는다. tools/test-native.ps1 이 돌린다.
add_executable(nlcore_tests tests/native/core_tests.cpp)
set_target_properties(nlcore_tests PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
target_include_directories(nlcore_tests PRIVATE src)
target_link_libraries(nlcore_tests PRIVATE nlcore)

```

- [ ] **Step 3: `tools/test-native.ps1`을 쓴다**

```powershell
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# src/core 의 시험을 돌린다. 게임을 켜지 않는다. 먼저 tools/build.ps1 로 빌드한다.
$repo = Get-NlRepoRoot
$exe = Join-Path $repo 'build\nlcore_tests.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "시험 실행 파일이 없습니다: $exe. 먼저 tools\build.ps1 을 실행하세요." }

# 소스보다 오래된 실행 파일로 통과했다고 하지 않는다.
$newest = Get-ChildItem -LiteralPath (Join-Path $repo 'src\core'), (Join-Path $repo 'tests\native') -Recurse -File |
          Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($newest -and (Get-Item -LiteralPath $exe).LastWriteTime -lt $newest.LastWriteTime) {
    throw "시험 실행 파일이 소스보다 오래되었습니다 ($($newest.Name)). tools\build.ps1 을 다시 실행하세요."
}

try { [Console]::OutputEncoding = [Text.Encoding]::UTF8 } catch { }     # 시험 이름이 UTF-8 로 나온다
& $exe (Join-Path $repo 'tools\probes')
exit $LASTEXITCODE
```

- [ ] **Step 4: 빌드해서 실패를 본다**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1 2>&1 | Select-Object -Last 8`
Expected: 실패. 출력에 `Cannot find source file` 과 `src/core/Text.cpp` 가 있고 끝이 `빌드 실패`다(코어가 아직 없다).

- [ ] **Step 5: `src/core/Text.hpp`**

```cpp
#pragma once
// 러너에 기대지 않는 글 처리. tests/native 가 직접 부른다.

#include <string>

namespace NlCore
{
	std::string Trim(const std::string& Text);

	// JSON 문자열로 쓴다. MaxBytes 보다 길면 UTF-8 글자 경계에서 자르고 "..." 를 붙인다. 잘못된 바이트는 '?' 로 바꾼다.
	std::string Quote(std::string Text, size_t MaxBytes = 200);

	// JSON 수로 쓴다. 유한하지 않으면 "nan" / "inf" / "-inf" 문자열로 쓴다. 로캘과 무관하게 소수점은 '.' 이다.
	std::string Number(double Value);

	// 소수 Digits 자리로 쓴다. 로그의 시각에 쓴다.
	std::string Fixed(double Value, int Digits);

	// 글 전체가 수일 때만 참.
	bool ParseNumber(const std::string& Text, double& Out);

	// Name 안에 Part 가 들어 있는가(대소문자를 가린다). 빈 Part 는 아무것에도 맞지 않는다.
	bool Contains(const std::string& Name, const std::string& Part);
}
```

- [ ] **Step 6: `src/core/Text.cpp`**

```cpp
#include "Text.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>

namespace NlCore
{
	namespace
	{
		// printf 는 로캘에 따라 소수점을 쉼표로 쓸 수 있다. JSON 과 로그는 항상 점이어야 한다.
		std::string Dotted(std::string Text)
		{
			for (char& c : Text)
				if (c == ',')
					c = '.';
			return Text;
		}
	}

	std::string Trim(const std::string& Text)
	{
		const size_t begin = Text.find_first_not_of(" \t\r\n");
		if (begin == std::string::npos)
			return "";
		return Text.substr(begin, Text.find_last_not_of(" \t\r\n") - begin + 1);
	}

	std::string Quote(std::string Text, size_t MaxBytes)
	{
		if (Text.size() > MaxBytes)
		{
			size_t cut = MaxBytes;
			while (cut > 0 && (static_cast<unsigned char>(Text[cut]) & 0xC0) == 0x80)
				cut--;
			Text.resize(cut);
			Text += "...";
		}

		std::string out = "\"";
		for (size_t i = 0; i < Text.size();)
		{
			const unsigned char c = static_cast<unsigned char>(Text[i]);
			const size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
			bool valid = len != 0 && i + len <= Text.size();
			for (size_t k = 1; valid && k < len; k++)
				valid = (static_cast<unsigned char>(Text[i + k]) & 0xC0) == 0x80;

			if (!valid) { out += '?'; i++; continue; }
			if (len > 1) { out.append(Text, i, len); i += len; continue; }

			switch (c)
			{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20) { char buf[8]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
				else out += static_cast<char>(c);
			}
			i++;
		}
		return out + "\"";
	}

	std::string Number(double Value)
	{
		if (!std::isfinite(Value))
			return Value != Value ? "\"nan\"" : (Value > 0 ? "\"inf\"" : "\"-inf\"");
		char buf[40];
		snprintf(buf, sizeof(buf), "%.17g", Value);
		return Dotted(buf);
	}

	std::string Fixed(double Value, int Digits)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "%.*f", Digits, Value);
		return Dotted(buf);
	}

	bool ParseNumber(const std::string& Text, double& Out)
	{
		if (Text.empty())
			return false;
		double parsed = 0;
		const char* end = Text.data() + Text.size();
		const std::from_chars_result result = std::from_chars(Text.data(), end, parsed);
		if (result.ec != std::errc() || result.ptr != end)
			return false;
		Out = parsed;
		return true;
	}

	bool Contains(const std::string& Name, const std::string& Part)
	{
		return !Part.empty() && Name.find(Part) != std::string::npos;
	}
}
```

- [ ] **Step 7: `src/core/Request.hpp`**

```cpp
#pragma once
// 요청 파일(NlToyBox.probe.txt)의 형식. 스펙: 데이터 오버레이 §3.2, §3.7.

#include <istream>
#include <string>
#include <vector>

namespace NlCore
{
	struct ScriptCall
	{
		std::string Name;
		std::vector<std::string> Args;
	};

	struct Request
	{
		int DelaySeconds = 60;			// 첫 덤프(menu)까지
		double RepeatSeconds = 0;		// 0 이면 첫 덤프만 한다. 아니면 앞 덤프가 끝난 뒤 이만큼 지나 다시 덤프한다
		int KeepLast = 3;				// 되풀이 덤프는 마지막 이만큼만 남긴다
		bool TraceEvents = false;
		std::vector<double> FindValues;
		std::vector<std::string> FindNames;
		std::vector<ScriptCall> Scripts;
		std::vector<std::string> Watches;	// "global.a.b"
		std::vector<std::string> Skip;		// "ds", "instances", "room"
		std::vector<std::string> Errors;	// 비어 있지 않으면 이 요청으로 아무것도 하지 않는다
	};

	// 줄 단위 키=값. '#' 으로 시작하는 줄과 빈 줄은 건너뛴다. 모르는 키와 잘못된 값은 Errors 에 쌓는다.
	Request ParseRequest(std::istream& In);
}
```

- [ ] **Step 8: `src/core/Request.cpp`**

```cpp
#include "Request.hpp"

#include "Text.hpp"

#include <cmath>

namespace NlCore
{
	namespace
	{
		std::vector<std::string> Split(const std::string& Text, char Separator)
		{
			std::vector<std::string> parts;
			size_t begin = 0;
			while (true)
			{
				const size_t end = Text.find(Separator, begin);
				parts.push_back(Trim(Text.substr(begin, end == std::string::npos ? std::string::npos : end - begin)));
				if (end == std::string::npos)
					return parts;
				begin = end + 1;
			}
		}

		bool ParseSeconds(const std::string& Value, double& Out)
		{
			double parsed = 0;
			if (!ParseNumber(Value, parsed) || parsed < 0)
				return false;
			Out = parsed;
			return true;
		}
	}

	Request ParseRequest(std::istream& In)
	{
		Request request;
		std::string line;
		int number = 0;
		while (std::getline(In, line))
		{
			number++;
			line = Trim(line);
			if (line.empty() || line[0] == '#')
				continue;

			const std::string where = "line " + std::to_string(number) + ": ";
			const size_t eq = line.find('=');
			if (eq == std::string::npos)
			{
				request.Errors.push_back(where + "no '=' in '" + line + "'");
				continue;
			}

			const std::string key = Trim(line.substr(0, eq));
			const std::string value = Trim(line.substr(eq + 1));
			double parsed = 0;

			if (key == "delay_seconds")
			{
				if (ParseSeconds(value, parsed))
					request.DelaySeconds = static_cast<int>(parsed);
				else
					request.Errors.push_back(where + "delay_seconds needs a number >= 0");
			}
			else if (key == "repeat_seconds")
			{
				// 너무 짧으면 덤프가 이어 붙어 게임이 멈춘 채로 있게 된다.
				if (ParseSeconds(value, parsed) && (parsed == 0 || parsed >= 10))
					request.RepeatSeconds = parsed;
				else
					request.Errors.push_back(where + "repeat_seconds needs 0 or a number >= 10");
			}
			else if (key == "keep_last")
			{
				if (ParseNumber(value, parsed) && parsed == std::floor(parsed) && parsed >= 1 && parsed <= 9)
					request.KeepLast = static_cast<int>(parsed);
				else
					request.Errors.push_back(where + "keep_last needs a whole number from 1 to 9");
			}
			else if (key == "trace_events")
			{
				if (value == "1" || value == "0")
					request.TraceEvents = value == "1";
				else
					request.Errors.push_back(where + "trace_events needs 0 or 1");
			}
			else if (key == "skip")
			{
				if (value == "ds" || value == "instances" || value == "room")
					request.Skip.push_back(value);
				else
					request.Errors.push_back(where + "skip needs ds, instances or room");
			}
			else if (key == "find")
			{
				if (ParseNumber(value, parsed))
					request.FindValues.push_back(parsed);
				else
					request.Errors.push_back(where + "find needs a number");
			}
			else if (key == "find_name")
			{
				if (!value.empty())
					request.FindNames.push_back(value);
				else
					request.Errors.push_back(where + "find_name is empty");
			}
			else if (key == "watch")
			{
				const bool ok = value.rfind("global.", 0) == 0 && value.size() > 7
					&& value.back() != '.' && value.find("..") == std::string::npos;
				if (ok)
					request.Watches.push_back(value);
				else
					request.Errors.push_back(where + "watch must look like global.a.b");
			}
			else if (key == "script")
			{
				// 이름|인자|인자…
				const std::vector<std::string> parts = Split(value, '|');
				if (parts[0].empty())
					request.Errors.push_back(where + "script needs a name");
				else
				{
					ScriptCall call;
					call.Name = parts[0];
					call.Args.assign(parts.begin() + 1, parts.end());
					request.Scripts.push_back(std::move(call));
				}
			}
			else
				request.Errors.push_back(where + "unknown key '" + key + "'");
		}

		// 되풀이 덤프마다 같은 스크립트를 부르지 않는다. 스크립트는 값을 바꿀 수도, 게임을 끝낼 수도 있다.
		if (request.RepeatSeconds > 0 && !request.Scripts.empty())
			request.Errors.push_back("script cannot be used with repeat_seconds");

		return request;
	}
}
```

- [ ] **Step 9: `src/core/Schedule.hpp`**

```cpp
#pragma once
// 덤프의 일정. 첫 덤프는 delay 뒤의 "menu" 이고, 되풀이하면 그 뒤로 "late0", "late1" … 를 돌려 쓴다.
// 스펙: 데이터 오버레이 §3.7.

#include <string>

namespace NlCore
{
	class Schedule
	{
	public:
		Schedule() = default;
		Schedule(double DelaySeconds, double RepeatSeconds, int KeepLast);

		// 지금 덤프할 때인가. 때가 됐으면 그 덤프의 이름을, 아니면 빈 글을 돌려준다.
		std::string Due(double Now) const;

		// 덤프 하나가 끝났다고 알린다. 다음 덤프는 이 시각에서 RepeatSeconds 뒤다
		// (덤프가 오래 걸려도 다음 덤프가 바로 겹치지 않는다).
		void Finished(double Now);

		bool Repeats() const { return m_Repeat > 0; }

		// 더 할 덤프가 없는가(되풀이하지 않는 요청에서 첫 덤프가 끝났다).
		bool Exhausted() const { return m_Count > 0 && m_Repeat <= 0; }

		// 지금까지 끝난 덤프의 수.
		int Count() const { return m_Count; }

	private:
		double m_Repeat = 0;
		int m_KeepLast = 1;
		int m_Count = 0;
		double m_NextAt = 0;
	};
}
```

- [ ] **Step 10: `src/core/Schedule.cpp`**

```cpp
#include "Schedule.hpp"

namespace NlCore
{
	Schedule::Schedule(double DelaySeconds, double RepeatSeconds, int KeepLast)
		: m_Repeat(RepeatSeconds), m_KeepLast(KeepLast < 1 ? 1 : KeepLast), m_NextAt(DelaySeconds)
	{
	}

	std::string Schedule::Due(double Now) const
	{
		if (Exhausted() || Now < m_NextAt)
			return "";
		if (m_Count == 0)
			return "menu";
		return "late" + std::to_string((m_Count - 1) % m_KeepLast);
	}

	void Schedule::Finished(double Now)
	{
		m_Count++;
		m_NextAt = Now + m_Repeat;
	}
}
```

- [ ] **Step 11: `src/core/PathTable.hpp`**

```cpp
#pragma once
// 찾기가 지나온 길을 적어 둔다. 맞은 것이 나올 때만 경로 글을 만든다(값마다 글을 들고 다니지 않으려고).

#include <string>
#include <utility>
#include <vector>

namespace NlCore
{
	class PathTable
	{
	public:
		// Segment 는 앞의 구분자를 포함한다: "global", ".name", "[3]". 뿌리의 Parent 는 -1 이다. 돌려주는 값은 노드 번호.
		int Add(int Parent, std::string Segment)
		{
			m_Nodes.push_back({ Parent, std::move(Segment) });
			return static_cast<int>(m_Nodes.size()) - 1;
		}

		// 뿌리부터 Node 까지 이어 붙인 글. Node 가 -1 이면 빈 글.
		std::string Path(int Node) const
		{
			std::vector<const std::string*> parts;
			for (int at = Node; at >= 0; at = m_Nodes[at].Parent)
				parts.push_back(&m_Nodes[at].Segment);

			std::string path;
			for (auto it = parts.rbegin(); it != parts.rend(); ++it)
				path += **it;
			return path;
		}

	private:
		struct Node
		{
			int Parent;
			std::string Segment;
		};

		std::vector<Node> m_Nodes;
	};
}
```

- [ ] **Step 12: 빌드하고 시험을 돌린다**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1 2>&1 | Select-Object -Last 3; pwsh -NoProfile -File E:\NlToyBox\tools\test-native.ps1; "exit=$LASTEXITCODE"`
Expected: `build ok -> …NlToyBox.dll`, `ok - …` 17줄, `core tests: 17 passed`, `exit=0`.

- [ ] **Step 13: 변이로 시험이 규칙을 잡는지 본다**

`src/core/Schedule.cpp`의 `return "late" + std::to_string((m_Count - 1) % m_KeepLast);`에서 `% m_KeepLast`를 잠깐 지우고 Step 12의 명령을 다시 돌린다.
Expected: `not ok - 일정: 되풀이 덤프는 마지막 keep_last 개의 이름을 돌려 쓴다`, `core tests: … FAILED`, `exit=1`.
되돌리고 다시 돌려 `core tests: 17 passed`를 확인한다.

- [ ] **Step 14: 커밋**

```powershell
git -C E:\NlToyBox add -- CMakeLists.txt src/core tests/native tools/test-native.ps1
git -C E:\NlToyBox commit -m "feat(core): 요청 읽기, 덤프 일정, 글 처리를 러너에서 떼어 시험한다" -m "src/core 는 YYToolkit 에 기대지 않는다. nlcore_tests.exe 가 요청 파일의 형식, 되풀이 덤프의 일정, JSON 글 처리를 게임 없이 시험하고, tools/probes 의 요청 파일이 모두 오류 없이 읽히는지 본다." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: `dump_tool.py`가 새 덤프 형식을 읽는다

**Files:**
- Modify: `E:\NlToyBox\tools\re\dump_tool.py` (전체를 바꾼다)
- Test: `E:\NlToyBox\tools\re\tests\test_dump_tool.py`

**Interfaces:**
- Consumes: 덤프의 형식(Task 4가 쓴다). 최상위 키: `module_dump`(2), `module_version`, `name`(`menu`|`late<k>`), `seq`, `elapsed_seconds`, `room`, `delay_seconds`, `repeat_seconds`, `limits`{…, `max_ds_id`}, `present`{이름:수}, `watch`{경로:{kind,value}}, `global_status`, `globals`, `find_global`, `find_ds`, `instances`{이름:{count,own,members}}, `find_instances`, `scripts`. 찾기 구역은 `hits`[{path,why,kind,value,members}], `visited`, `truncated`를 가지며 건너뛴 구역은 `skipped: true`다. `find_ds`에는 `maps`, `lists`, `highest_map`, `highest_list`, `selfcheck`{made,ds_map,ds_list}가 있다.
- Produces:
  - `dump_tool.hits(dump)` → `(구역 이름, 항목)`을 내는 제너레이터
  - `dump_tool.flatten(dump)` → `{경로: 값}`. 인스턴스 변수는 `instance:<오브젝트>.<이름>`
  - `dump_tool.controls(dump, expectations)` → `[(이름, 통과, 설명)]`. `expectations`는 `[(경로, 값)]`
  - 명령: `summary <덤프>`, `find <덤프> <낱말|수>…`, `hits <덤프> [<낱말>…]`, `diff <A> <B>`, `controls <덤프> [<경로>=<값>…]`(하나라도 어긋나면 종료 코드 1)

- [ ] **Step 1: 실패하는 시험을 쓴다 — `tools/re/tests/test_dump_tool.py`**

```python
"""dump_tool.py 의 시험. 사용: py -3.14 -m unittest discover -s tools/re/tests -v"""
import contextlib
import importlib.util
import io
import json
import pathlib
import unittest

_spec = importlib.util.spec_from_file_location("dump_tool", pathlib.Path(__file__).resolve().parents[1] / "dump_tool.py")
dump_tool = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(dump_tool)

NEW = {
    "module_dump": 2, "module_version": "0.2.0", "name": "late1", "seq": 4, "elapsed_seconds": 200.5,
    "room": "rm_game", "delay_seconds": 60, "repeat_seconds": 45, "limits": {"max_ds_id": 100000},
    "present": {"o_data": 1}, "watch": {"global.a.b": {"kind": "bool", "value": 1}},
    "global_status": "AURIE_SUCCESS",
    "globals": {"x": {"kind": "number", "value": 1},
                "s": {"kind": "struct", "members": {"m": {"kind": "number", "value": 745}}}},
    "find_global": {"hits": [{"path": "global.s.m", "why": "value", "kind": "number", "value": 745}],
                    "visited": 10, "truncated": False},
    "find_ds": {"hits": [{"path": "ds_map[19].budget_money", "why": "name", "kind": "number", "value": 2345}],
                "maps": 3, "lists": 1, "highest_map": 412, "highest_list": 97,
                "selfcheck": {"made": True, "ds_map": True, "ds_list": True},
                "visited": 20, "truncated": False},
    "instances": {"o_data": {"count": 1, "own": 1, "members": {"state": {"kind": "number", "value": 3}}}},
    "find_instances": {"hits": [], "instances": 1, "visited": 25, "truncated": False},
    "scripts": [],
}

OLD = {
    "module_dump": 1, "elapsed_seconds": 60.0, "global_status": "AURIE_SUCCESS",
    "globals": {"x": {"kind": "number", "value": 1}},
    "find": {"hits": [{"path": "global.x", "why": "value", "kind": "number", "value": 1}], "visited": 1, "truncated": False},
    "scripts": [],
}


class DumpToolTests(unittest.TestCase):
    def test_hits_come_from_every_find_section(self):
        self.assertEqual([(section, hit["path"]) for section, hit in dump_tool.hits(NEW)],
                         [("find_global", "global.s.m"), ("find_ds", "ds_map[19].budget_money")])

    def test_hits_read_the_old_format(self):
        self.assertEqual([(section, hit["path"]) for section, hit in dump_tool.hits(OLD)], [("find", "global.x")])

    def test_flatten_includes_globals_and_instance_members(self):
        flat = dump_tool.flatten(NEW)
        self.assertEqual(flat["global.s.m"], 745)
        self.assertEqual(flat["instance:o_data.state"], 3)

    def test_summary_reports_name_room_selfcheck_and_hits(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            dump_tool.summary(NEW)
        text = out.getvalue()
        self.assertIn("name=late1", text)
        self.assertIn("room=rm_game", text)
        self.assertIn("selfcheck", text)
        self.assertIn("ds_map[19].budget_money", text)

    def test_controls_pass_on_a_trustworthy_dump(self):
        results = dump_tool.controls(NEW, [("global.s.m", 745.0)])
        self.assertTrue(all(ok for _, ok, _ in results), results)

    def test_controls_flag_what_makes_an_absence_untrustworthy(self):
        bad = json.loads(json.dumps(NEW))
        bad["find_ds"]["selfcheck"]["ds_list"] = False          # ds_list 안을 보지 못했다
        bad["find_ds"]["highest_map"] = 99999                   # ds 번호가 본 범위의 끝에 닿았다
        bad["find_instances"]["truncated"] = True               # 한도에 걸렸다
        bad["instances"]["o_data"]["members"] = {}              # 인스턴스 변수를 하나도 읽지 못했다
        failed = {name for name, ok, _ in dump_tool.controls(bad, [("global.nope", 1.0)]) if not ok}
        self.assertEqual(failed, {"expect global.nope", "selfcheck", "ds_range", "truncated", "instances"})

    def test_controls_flag_a_skipped_section(self):
        bad = json.loads(json.dumps(NEW))
        bad["find_ds"] = {"hits": [], "skipped": True}
        failed = {name for name, ok, _ in dump_tool.controls(bad, []) if not ok}
        self.assertEqual(failed, {"skipped"})


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 시험이 실패하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\re\tests -v 2>&1 | Select-Object -Last 6`
Expected: 실패. `AttributeError: module 'dump_tool' has no attribute 'hits'`(와 `controls`)가 보이고 끝 줄이 `FAILED (`로 시작한다.

- [ ] **Step 3: `tools/re/dump_tool.py` 전체를 바꾼다**

```python
"""모듈 덤프(NlToyBox.dump.*.json)를 '경로 → 값' 표로 펴서 요약하고, 찾고, 비교하고, 믿을 만한지 점검한다.

사용:
  py -3.14 dump_tool.py summary <덤프>
  py -3.14 dump_tool.py find <덤프> <낱말 또는 수>...
  py -3.14 dump_tool.py hits <덤프> [<경로에 든 낱말>...]
  py -3.14 dump_tool.py diff <덤프 A> <덤프 B>
  py -3.14 dump_tool.py controls <덤프> [<경로>=<값>...]
"""
import collections
import json
import sys

FIND_SECTIONS = ("find", "find_global", "find_ds", "find_instances")  # "find" 는 옛 형식(module_dump 1)
FIND_STATS = ("visited", "containers", "depth_cut", "array_skipped", "hits_cut", "truncated",
              "maps", "lists", "highest_map", "highest_list", "skipped_large", "instances", "selfcheck", "skipped")


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def flatten(dump):
    """전역, 인스턴스 변수, 스크립트 결과를 {경로: 값} 으로 편다. 값이 없는 것은 '<형>' 으로 적는다."""
    flat = {}

    def put(path, node):
        kind = node.get("kind")
        if kind == "struct":
            flat[path] = "<struct>"
            for key, child in node.get("members", {}).items():
                put(f"{path}.{key}", child)
        elif kind == "array":
            flat[path] = f"<array {node.get('length')}>"
        elif "value" in node:
            flat[path] = node["value"]
        else:
            flat[path] = f"<{kind}>"

    for name, node in dump.get("globals", {}).items():
        put(f"global.{name}", node)
    for name, node in dump.get("instances", {}).items():
        flat[f"instance:{name}"] = f"<count {node.get('count')}>"
        for key, child in node.get("members", {}).items():
            put(f"instance:{name}.{key}", child)
    for call in dump.get("scripts", []):
        key = f"script:{call['name']}({','.join(str(a) for a in call.get('args', []))})"
        if call.get("status") == "AURIE_SUCCESS":
            put(key, call.get("result", {"kind": "none"}))
        else:
            flat[key] = f"<{call.get('status')}>"
    return flat


def hits(dump):
    """모든 찾기 구역의 맞은 것을 (구역, 항목) 으로 낸다."""
    for section in FIND_SECTIONS:
        for hit in dump.get(section, {}).get("hits", []):
            yield section, hit


def describe_hit(hit):
    members = hit.get("members")
    extra = f" members={list(members)[:40]}" if members is not None else ""
    value = hit.get("value", "<" + str(hit.get("kind")) + ">")
    return f"[{hit.get('why')}] {hit.get('path')} = {value}{extra}"


def summary(dump):
    print(f"name={dump.get('name', '-')} seq={dump.get('seq', '-')} room={dump.get('room', '-')} "
          f"elapsed_seconds={dump.get('elapsed_seconds')} module={dump.get('module_version', '-')} "
          f"global_status={dump.get('global_status')}")
    if "present" in dump:
        print("present: " + " ".join(f"{name}:{count}" for name, count in dump["present"].items()))
    for path, node in dump.get("watch", {}).items():
        print(f"watch {path} = {node.get('value', '<' + str(node.get('kind')) + '>')}")
    kinds = collections.Counter(node.get("kind") for node in dump.get("globals", {}).values())
    print(f"globals={len(dump.get('globals', {}))} " + " ".join(f"{k}={n}" for k, n in kinds.most_common()))
    if "instances" in dump:
        print("instances (수/적힌 변수): " + " ".join(
            f"{name}:{node.get('count')}/{len(node.get('members', {}))}" for name, node in dump["instances"].items()))
    for call in dump.get("scripts", []):
        print(f"script {call['name']}({call.get('args')}) -> {call.get('status')} {call.get('result')}")
    for section in FIND_SECTIONS:
        if section not in dump:
            continue
        found = dump[section]
        stats = " ".join(f"{key}={found[key]}" for key in FIND_STATS if key in found)
        print(f"{section}: hits={len(found.get('hits', []))} {stats}")
        for hit in found.get("hits", []):
            print("  " + describe_hit(hit))


def find(dump, terms):
    flat = flatten(dump)
    for term in terms:
        try:
            number = float(term)
        except ValueError:
            number = None
        print(f"--- {term} ---")
        shown = 0
        for path, value in flat.items():
            by_value = number is not None and isinstance(value, (int, float)) and not isinstance(value, bool) and value == number
            by_name = number is None and term.lower() in path.lower()
            if by_value or by_name:
                print(f"  {path} = {value}")
                shown += 1
                if shown >= 60:
                    print("  ... (60개에서 끊음)")
                    break
        if shown == 0:
            print("  (없음)")


def show_hits(dump, terms):
    shown = 0
    for section, hit in hits(dump):
        path = str(hit.get("path"))
        if terms and not any(term.lower() in path.lower() for term in terms):
            continue
        print(f"{section} {describe_hit(hit)}")
        for key, child in (hit.get("members") or {}).items():
            print(f"    {key} = {child.get('value', '<' + str(child.get('kind')) + '>')}")
        shown += 1
    print(f"hits shown={shown}")


def diff(a, b):
    fa, fb = flatten(a), flatten(b)
    changed = [(p, fa[p], fb[p]) for p in fa if p in fb and fa[p] != fb[p]]
    print(f"changed={len(changed)} only_a={len(fa.keys() - fb.keys())} only_b={len(fb.keys() - fa.keys())}")
    for path, va, vb in changed[:200]:
        print(f"  {path}: {va} -> {vb}")
    for path in sorted(fb.keys() - fa.keys())[:50]:
        print(f"  + {path} = {fb[path]}")
    for path in sorted(fa.keys() - fb.keys())[:50]:
        print(f"  - {path} = {fa[path]}")


def controls(dump, expectations):
    """덤프의 '없다'를 믿어도 되는지 본다. (이름, 통과, 설명) 의 목록을 돌려준다.

    expectations: [(경로, 값)]. 찾기가 실제로 찾아야 하는 것(양성 대조).
    그 덤프가 게임 안에서 뜬 것인지는 여기서 판정하지 않는다. 사용자가 알려 준 때와 덤프의 시각을 맞춰 본다.
    """
    results = []

    found = set()
    for _, hit in hits(dump):
        value = hit.get("value")
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            value = float(value)
        found.add((hit.get("path"), value))
    for path, value in expectations:
        results.append((f"expect {path}", (path, value) in found, f"want {value}"))

    skipped = [s for s in FIND_SECTIONS if dump.get(s, {}).get("skipped")]
    if skipped:
        results.append(("skipped", False, f"sections={skipped}"))

    ds = dump.get("find_ds", {})
    selfcheck = ds.get("selfcheck")
    if selfcheck is not None:
        results.append(("selfcheck", all(selfcheck.get(key) for key in ("made", "ds_map", "ds_list")), str(selfcheck)))
    limit = dump.get("limits", {}).get("max_ds_id")
    if limit is not None and "highest_map" in ds:
        highest = max(ds.get("highest_map", -1), ds.get("highest_list", -1))
        results.append(("ds_range", highest < limit - 1, f"highest id={highest} scanned below {limit}"))

    cut = [s for s in FIND_SECTIONS if dump.get(s, {}).get("truncated")]
    results.append(("truncated", not cut, f"sections={cut}"))

    if "instances" in dump:
        with_members = [name for name, node in dump["instances"].items() if node.get("members")]
        results.append(("instances", bool(with_members), f"objects with members={len(with_members)}"))
    return results


def parse_expectation(text):
    path, _, value = text.rpartition("=")
    try:
        return path, float(value)
    except ValueError:
        return path, value


def main(argv):
    if len(argv) >= 3 and argv[1] == "summary":
        summary(load(argv[2]))
    elif len(argv) >= 4 and argv[1] == "find":
        find(load(argv[2]), argv[3:])
    elif len(argv) >= 3 and argv[1] == "hits":
        show_hits(load(argv[2]), argv[3:])
    elif len(argv) == 4 and argv[1] == "diff":
        diff(load(argv[2]), load(argv[3]))
    elif len(argv) >= 3 and argv[1] == "controls":
        results = controls(load(argv[2]), [parse_expectation(text) for text in argv[3:]])
        for name, ok, detail in results:
            print(f"{'ok  ' if ok else 'FAIL'} {name}: {detail}")
        return 0 if all(ok for _, ok, _ in results) else 1
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
```

- [ ] **Step 4: 시험이 통과하는 것을 본다**

Run: `py -3.14 -m unittest discover -s E:\NlToyBox\tools\re\tests -v 2>&1 | Select-Object -Last 4`
Expected: `Ran 7 tests`, `OK`.

- [ ] **Step 5: 옛 덤프도 읽히는지 본다**

Run: `py -3.14 E:\NlToyBox\tools\re\dump_tool.py summary E:\NlToyBox\refs\runtime\dump-added.json | Select-Object -First 3`
Expected: 첫 줄에 `name=-`, 둘째 줄에 `globals=4737`, 셋째 줄이 `find: hits=…`로 시작한다. (`refs\runtime\dump-added.json`이 없으면 이 Step은 건너뛰고 건너뛴 것을 원장에 적는다.)

- [ ] **Step 6: 커밋**

```powershell
git -C E:\NlToyBox add -- tools/re/dump_tool.py tools/re/tests/test_dump_tool.py
git -C E:\NlToyBox commit -m "feat(tools): dump_tool 이 새 덤프 형식을 읽고 믿을 만한지 점검한다" -m "찾기 구역이 셋(find_global, find_ds, find_instances)으로 나뉜 새 형식과 옛 형식을 함께 읽는다. controls 는 양성 대조가 찾아졌는지, ds 자가 점검과 본 범위, 한도, 인스턴스 변수를 본다." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: 도구 — probe, saves-backup, restore

**Files:**
- Modify: `E:\NlToyBox\tools\tests\safety.tests.ps1`
- Modify: `E:\NlToyBox\tools\restore-game.ps1`
- Modify: `E:\NlToyBox\tools\probe.ps1` (전체를 바꾼다)
- Modify: `E:\NlToyBox\tools\common.ps1` (`Get-NlSavesDir`)
- Create: `E:\NlToyBox\tools\saves-backup.ps1`

**Interfaces:**
- Consumes: 모듈이 쓰는 파일과 로그 줄(Task 4가 쓴다). 파일 `mods\Aurie\NlToyBox.dump.<이름>.json`(이름은 `menu`, `late0` …). 로그: `request consumed`, `dump requested: …`, `dump <이름> done seq <n> …`, `dump done`(되풀이하지 않는 요청에서만), `dump failed: <이유>`.
- Produces:
  - `pwsh -File tools/probe.ps1 -Request <파일> -Out <x.json> [-TimeoutSec 240] [-GraceSec 15]` → 덤프를 `<x>.<이름>.json`으로 가져온다. 끝줄 `PASS`(0) 또는 `FAIL: …`(1). 끝나면 게임은 꺼져 있고 요청 파일과 덤프는 게임 폴더에 없다. 되풀이 요청이면 게임이 꺼질 때까지 기다리고, 뒤의 덤프(`late*`)를 하나도 받지 못하면 실패한다
  - `pwsh -File tools/saves-backup.ps1 [-To <폴더>]` → 끝줄 `saves backup ok (<n>) -> <폴더>`. `-Diff <사본 폴더>` → `새로 생김: …` / `바뀜: …` / `없어짐: …` 줄과 끝줄 `saves diff (<n>)`
  - `Get-NlSavesDir` (`NORLAND_SAVES_DIR` 우선, 없으면 `%LOCALAPPDATA%\Strategy`)

- [ ] **Step 1: 실패하는 시험을 더한다 — `tools/tests/safety.tests.ps1`**

(a) `Test-Case 'restore 는 온전한 백업으로 원본을 되살린다'` 안에서, `Copy-Item -LiteralPath $pristine -Destination $backupPath -Force` 줄 **바로 아래에** 넣는다:

```powershell
        foreach ($n in 'NlToyBox.dump.menu.json', 'NlToyBox.dump.late2.json') {      # probe 가 도중에 죽어 남은 덤프
            [IO.File]::WriteAllText((Join-Path $fake "mods\Aurie\$n"), '{}')
        }
```

(b) `Test-Case 'probe 는 켜는 데 실패해도 요청 파일을 게임 폴더에 남기지 않는다'` 블록의 닫는 `}` **아래**, `Write-Host "safety tests: …"` 줄 **위에** 넣는다:

```powershell
    # 모듈과 게임 프로세스를 흉내 낸다. 게임을 켜는 대신 로그와 덤프를 mods\Aurie 에 써 넣고,
    # AliveSec 동안 게임이 떠 있는 것처럼 보이게 한다. Get-Process 와 Stop-Process 를 가로챌 뿐 실제 프로세스는 없다.
    $modDir = Join-Path $fake 'mods\Aurie'
    function New-FakeModule([string[]]$LogLines, [hashtable]$Dumps = @{}, [int]$AliveSec = 0) {
        $body = "Set-Content -LiteralPath '$modDir\NlToyBox.log' -Value @($(($LogLines | ForEach-Object { "'$_'" }) -join ', '));"
        foreach ($name in $Dumps.Keys) { $body += " Set-Content -LiteralPath '$modDir\$name' -Value '$($Dumps[$name])';" }
        $body += " `$global:NlFakeUntil = (Get-Date).AddSeconds($AliveSec);"
        "function Start-Process { $body }; " +
        "function Get-Process { if (`$global:NlFakeUntil -and (Get-Date) -lt `$global:NlFakeUntil) { [pscustomobject]@{ Name = 'Norland-fake' } } }; " +
        "function Stop-Process { };"
    }

    Test-Case 'probe 는 덤프를 이름대로 -Out 옆으로 가져오고 게임 폴더에서 지운다' {
        $req = Join-Path $fake 'req.txt'
        [IO.File]::WriteAllText($req, "delay_seconds=1`n")
        $prelude = New-FakeModule @('request consumed', 'dump requested: x', 'dump menu done seq 1', 'dump done') @{ 'NlToyBox.dump.menu.json' = 'MENU' }
        $r = Invoke-Tool 'probe.ps1' "-Request '$req' -Out '$(Join-Path $fake 'out\one.json')' -TimeoutSec 20" $prelude
        Assert-Equal $r.Exit 0 "probe 종료 코드`n$($r.Out)"
        Assert-True (Test-Path -LiteralPath (Join-Path $fake 'out\one.menu.json')) "덤프를 이름대로 가져와야 한다`n$($r.Out)"
        Assert-Equal ([IO.File]::ReadAllText((Join-Path $fake 'out\one.menu.json')).Trim()) 'MENU' '덤프의 내용'
        Assert-Equal @(Get-ChildItem -LiteralPath $modDir -Filter 'NlToyBox.dump*').Count 0 '덤프가 게임 폴더에 남으면 안 된다'
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $modDir 'NlToyBox.probe.txt'))) '요청 파일이 남으면 안 된다'
    }

    Test-Case 'probe 는 되풀이 요청이면 게임이 꺼질 때까지 기다렸다가 덤프를 모두 가져온다' {
        $req = Join-Path $fake 'req.txt'
        [IO.File]::WriteAllText($req, "delay_seconds=1`nrepeat_seconds=45`n")
        $prelude = New-FakeModule @('request consumed', 'dump requested: x', 'dump menu done seq 1', 'dump late0 done seq 2') @{
            'NlToyBox.dump.menu.json' = 'MENU'; 'NlToyBox.dump.late0.json' = 'L0'; 'NlToyBox.dump.late1.json' = 'L1' } 6
        $watch = [Diagnostics.Stopwatch]::StartNew()
        $r = Invoke-Tool 'probe.ps1' "-Request '$req' -Out '$(Join-Path $fake 'out\rep.json')' -TimeoutSec 30" $prelude
        Assert-Equal $r.Exit 0 "probe 종료 코드`n$($r.Out)"
        Assert-True ($watch.Elapsed.TotalSeconds -ge 5) "게임이 꺼지기 전에 끝내면 안 된다 ($([int]$watch.Elapsed.TotalSeconds)초)"
        Assert-True ($watch.Elapsed.TotalSeconds -lt 25) "게임이 꺼진 뒤에는 기다리지 않는다 ($([int]$watch.Elapsed.TotalSeconds)초)"
        foreach ($pair in @{ 'menu' = 'MENU'; 'late0' = 'L0'; 'late1' = 'L1' }.GetEnumerator()) {
            Assert-Equal ([IO.File]::ReadAllText((Join-Path $fake "out\rep.$($pair.Key).json")).Trim()) $pair.Value "덤프 $($pair.Key) 의 내용"
        }
        Assert-True ($r.Out -match '새 게임') "되풀이 요청이면 새 게임을 시작하라고 알려야 한다`n$($r.Out)"
        Assert-Equal @(Get-ChildItem -LiteralPath $modDir -Filter 'NlToyBox.dump*').Count 0 '덤프가 게임 폴더에 남으면 안 된다'
    }

    Test-Case 'probe 는 되풀이 요청인데 뒤의 덤프를 하나도 받지 못하면 실패한다' {
        $req = Join-Path $fake 'req.txt'
        [IO.File]::WriteAllText($req, "delay_seconds=1`nrepeat_seconds=45`n")
        $prelude = New-FakeModule @('request consumed', 'dump requested: x', 'dump menu done seq 1') @{ 'NlToyBox.dump.menu.json' = 'MENU' } 4
        $r = Invoke-Tool 'probe.ps1' "-Request '$req' -Out '$(Join-Path $fake 'out\none.json')' -TimeoutSec 30" $prelude
        Assert-True ($r.Exit -eq 1 -and $r.Out -match 'FAIL: 되풀이 덤프') "메뉴 덤프만으로 통과하면 안 된다`n$($r.Out)"
    }

    Test-Case 'probe 는 모듈이 dump failed 를 적으면 기다리지 않고 실패한다' {
        $req = Join-Path $fake 'req.txt'
        [IO.File]::WriteAllText($req, "delay_seconds=1`n")
        $prelude = New-FakeModule @('request consumed', 'dump failed: bad request')
        $watch = [Diagnostics.Stopwatch]::StartNew()
        $r = Invoke-Tool 'probe.ps1' "-Request '$req' -Out '$(Join-Path $fake 'out\bad.json')' -TimeoutSec 40" $prelude
        Assert-True ($r.Exit -eq 1 -and $r.Out -match 'FAIL: dump failed: bad request') "모듈이 적은 이유로 실패해야 한다`n$($r.Out)"
        Assert-True ($watch.Elapsed.TotalSeconds -lt 25) "시간이 다 될 때까지 기다리면 안 된다 ($([int]$watch.Elapsed.TotalSeconds)초)"
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $modDir 'NlToyBox.probe.txt'))) '요청 파일이 남으면 안 된다'
    }

    Test-Case 'saves-backup 은 사본을 뜨고 달라진 것을 알려 주며, 세이브 폴더에는 쓰지 않는다' {
        $saves = Join-Path $fake 'saves-src'
        New-Item -ItemType Directory -Force -Path (Join-Path $saves 'saves') | Out-Null
        [IO.File]::WriteAllText((Join-Path $saves 'game_settings.json'), 'A')
        [IO.File]::WriteAllText((Join-Path $saves 'saves\one.norland'), 'B')
        $copy = Join-Path $fake 'saves-copy'
        $env:NORLAND_SAVES_DIR = $saves
        try {
            $r = Invoke-Tool 'saves-backup.ps1' "-To '$copy'"
            Assert-Equal $r.Exit 0 "saves-backup 종료 코드`n$($r.Out)"
            Assert-Equal ([IO.File]::ReadAllText((Join-Path $copy 'saves\one.norland'))) 'B' '사본의 내용이 같아야 한다'
            Assert-Equal @(Get-ChildItem -LiteralPath $saves -Recurse -File).Count 2 '세이브 폴더에 파일을 만들면 안 된다'

            [IO.File]::WriteAllText((Join-Path $saves 'game_settings.json'), 'A2')      # 게임이 고친 파일
            [IO.File]::WriteAllText((Join-Path $saves 'saves\auto.norland'), 'C')       # 게임이 새로 쓴 세이브
            $r = Invoke-Tool 'saves-backup.ps1' "-Diff '$copy'"
            Assert-True ($r.Exit -eq 0 -and $r.Out -match '바뀜: game_settings\.json' -and $r.Out -match '새로 생김: saves\\auto\.norland' -and $r.Out -match 'saves diff \(2\)') "달라진 두 파일을 알려야 한다`n$($r.Out)"
            Assert-Equal ([IO.File]::ReadAllText((Join-Path $saves 'game_settings.json'))) 'A2' '세이브 폴더의 파일은 그대로여야 한다'
            Assert-Equal @(Get-ChildItem -LiteralPath $saves -Recurse -File).Count 3 '세이브 폴더에 파일을 만들면 안 된다'
        }
        finally { $env:NORLAND_SAVES_DIR = $null }
    }
```

- [ ] **Step 2: 시험이 실패하는 것을 본다 (restore)**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\tests\safety.tests.ps1 2>&1 | Select-Object -Last 4`
Expected: 실패. `mods 가 없어야 한다 : condition false`. (복원이 모르는 파일을 남겨 `mods`가 지워지지 않는다.)

- [ ] **Step 3: `tools/restore-game.ps1`이 이름이 여럿인 덤프를 지우게 한다**

`foreach ($rel in $ours) { … }` 블록 **바로 아래에** 넣는다:

```powershell
# 덤프는 이름이 여럿이다(menu, late0 …). 모듈이 쓰는 이름 꼴로 지운다.
$aurieDir = Join-Path $gameDir 'mods\Aurie'
if (Test-Path -LiteralPath $aurieDir) {
    Get-ChildItem -LiteralPath $aurieDir -File -Filter 'NlToyBox.dump*.json' | ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
}
```

- [ ] **Step 4: 다시 돌려 다음 실패를 본다 (probe)**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\tests\safety.tests.ps1 2>&1 | Select-Object -Last 4`
Expected: 실패. `덤프를 이름대로 가져와야 한다`. (지금의 `probe.ps1`은 `NlToyBox.dump.json` 하나만 가져온다.)

- [ ] **Step 5: `tools/probe.ps1` 전체를 바꾼다**

```powershell
param(
    [Parameter(Mandatory)][string]$Request,
    [Parameter(Mandatory)][string]$Out,
    [int]$TimeoutSec = 240,
    [int]$GraceSec = 15
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 요청 파일을 놓고 게임을 켜서 모듈의 덤프를 받아 온다. 끝나면 요청 파일과 덤프를 게임 폴더에서 지운다.
# 덤프는 이름대로 온다: -Out x.json 이면 x.menu.json, x.late0.json …
#  - 되풀이하지 않는 요청: 'dump done' 을 기다렸다가 게임을 끈다.
#  - 되풀이 요청(repeat_seconds): 사용자가 새 게임을 시작하고, 사용자가 게임을 끌 때까지 기다린다.
#    게임 안에서는 이 도구가 끄지 않는다. 제한 시간이 지났을 때만 끈다.
Assert-NlReadyToLaunch
if (-not (Test-Path -LiteralPath $Request -PathType Leaf)) { throw "요청 파일이 없습니다: $Request" }
$Out = [IO.Path]::GetFullPath($Out)
$outBase = Join-Path (Split-Path -Parent $Out) ([IO.Path]::GetFileNameWithoutExtension($Out))

$modDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
$reqDst = Join-Path $modDir 'NlToyBox.probe.txt'
$log = Join-Path $modDir 'NlToyBox.log'
function Get-NlDumps { @(Get-ChildItem -LiteralPath $modDir -File -Filter 'NlToyBox.dump*.json' -ErrorAction SilentlyContinue) }
foreach ($f in @(Get-NlDumps | ForEach-Object { $_.FullName }) + $log) { if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force } }

$repeat = [bool](Select-String -LiteralPath $Request -Pattern '^\s*repeat_seconds\s*=\s*[1-9]' -Quiet)

$done = $false
$exited = $false
$failed = $null
$toldMenu = $false
$copied = @()
try {
    Copy-Item -LiteralPath $Request -Destination $reqDst -Force
    if ($repeat) {
        Write-Host "게임을 켭니다 (steam://rungameid/$($script:NlAppId)). 메인 메뉴가 뜨고 약 1분 뒤 화면이 잠깐 멈췄다 풀리면 새 게임을 시작해 주세요. 게임 화면에서 2분쯤 둔 뒤 평소처럼 게임을 꺼 주세요."
    } else {
        Write-Host "게임을 켭니다 (steam://rungameid/$($script:NlAppId)). 게임 창을 누르지 마세요."
    }
    Start-Process "steam://rungameid/$($script:NlAppId)"

    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $seenProcess = $false
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        if (Test-NlGameRunning) { $seenProcess = $true }
        elseif ($seenProcess) { Write-Host "게임 프로세스가 사라졌습니다 ($(Get-Date -Format 'HH:mm:ss'))."; $exited = $true; break }
        $lines = @(if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -ErrorAction SilentlyContinue })
        # 모듈은 요청을 읽은 뒤 스스로 지운다. 지우지 못했을 때를 대비해 여기서도 지운다.
        if (($lines -match '^(dump requested|dump failed)') -and (Test-Path -LiteralPath $reqDst)) { Remove-Item -LiteralPath $reqDst -Force -ErrorAction SilentlyContinue }
        # 모듈이 그만뒀으면 더 기다려도 덤프는 오지 않는다.
        $bad = @($lines -match '^dump failed')
        if ($bad.Count) { $failed = $bad[0]; break }
        if ($repeat -and -not $toldMenu -and ($lines -match '^dump menu done')) {
            $toldMenu = $true
            Write-Host "메뉴 덤프가 끝났습니다 ($(Get-Date -Format 'HH:mm:ss')). 이제 새 게임을 시작해 주세요."
            try { [Console]::Beep(880, 300) } catch { }
        }
        if (-not $repeat -and ($lines -contains 'dump done')) { $done = $true; break }
    }
    if (-not $seenProcess) { Write-Host '게임 프로세스를 한 번도 보지 못했습니다.' }

    Write-Host '--- NlToyBox.log ---'
    if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log | ForEach-Object { Write-Host $_ } } else { Write-Host '(로그 없음)' }
    Write-Host '--------------------'

    # 반쯤 쓰인 덤프도 가져온다. 어디서 멈췄는지가 증거다.
    foreach ($d in Get-NlDumps) {
        $suffix = $d.BaseName.Substring('NlToyBox.dump'.Length)      # ".menu" / ".late0" / 옛 형식이면 빈 글
        $dst = "$outBase$suffix.json"
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
        Copy-Item -LiteralPath $d.FullName -Destination $dst -Force
        $copied += $dst
        Write-Host "덤프: $dst ($((Get-Item -LiteralPath $dst).Length) B)"
    }
}
finally {
    # 요청 파일이 남으면 평소 플레이 때도 덤프가 돈다. 게임을 끄는 일보다 먼저 지운다(끄다가 실패해도 남지 않게).
    if (Test-Path -LiteralPath $reqDst) { Remove-Item -LiteralPath $reqDst -Force -ErrorAction SilentlyContinue }
    try { Write-Host "게임 종료: $(Stop-NlGame $GraceSec)" }
    finally {
        # 덤프는 모듈이 쓰는 중이면 잠겨 있다. 게임을 끈 뒤에 지운다.
        foreach ($f in @($reqDst) + @(Get-NlDumps | ForEach-Object { $_.FullName })) {
            if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue }
        }
    }
}

if ($failed) { Write-Host "FAIL: $failed"; exit 1 }
if ($repeat) {
    if (-not @($copied | Where-Object { $_ -match '\.late\d+\.json$' }).Count) { Write-Host 'FAIL: 되풀이 덤프를 하나도 받지 못했습니다.'; exit 1 }
    if (-not $exited) { Write-Host '주의: 제한 시간이 지나 이 도구가 게임을 껐습니다.' }
}
elseif (-not $done) { Write-Host 'FAIL: dump done 을 보지 못했습니다.'; exit 1 }
Write-Host 'PASS'
exit 0
```

- [ ] **Step 6: 다시 돌려 다음 실패를 본다 (saves-backup)**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\tests\safety.tests.ps1 2>&1 | Select-Object -Last 8`
Expected: `ok - probe 는 …` 네 줄 뒤에 실패. `saves-backup 종료 코드`(도구가 아직 없다).

- [ ] **Step 7: `tools/common.ps1`에 `Get-NlSavesDir`를 더한다** (`Get-NlDataSnapshotDir` 함수 아래)

```powershell
# 세이브·설정 폴더. 이 레포의 도구는 여기에 쓰지 않는다(읽어서 사본을 뜰 뿐이다). 시험은 NORLAND_SAVES_DIR 로 바꾼다.
function Get-NlSavesDir {
    if ($env:NORLAND_SAVES_DIR) { $env:NORLAND_SAVES_DIR } else { Join-Path $env:LOCALAPPDATA 'Strategy' }
}
```

- [ ] **Step 8: `tools/saves-backup.ps1`을 쓴다**

```powershell
param(
    [string]$To,
    [string]$Diff
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 세이브·설정 폴더의 사본을 뜬다(기본: backups\saves\<시각>\). -Diff <사본 폴더> 를 주면 사본과 지금의 차이만 보여 준다.
# 세이브 폴더에는 쓰지 않는다.
$src = Get-NlSavesDir
if (-not (Test-Path -LiteralPath $src -PathType Container)) { throw "세이브 폴더가 없습니다: $src" }
$src = (Resolve-Path -LiteralPath $src).Path

# 상대경로 → SHA256
function Get-NlTree([string]$Root) {
    $map = @{}
    foreach ($f in Get-ChildItem -LiteralPath $Root -Recurse -File -Force) {
        $map[$f.FullName.Substring($Root.Length + 1)] = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
    }
    $map
}

if ($Diff) {
    if (-not (Test-Path -LiteralPath $Diff -PathType Container)) { throw "사본 폴더가 없습니다: $Diff" }
    $old = Get-NlTree (Resolve-Path -LiteralPath $Diff).Path
    $new = Get-NlTree $src
    $n = 0
    foreach ($rel in ($new.Keys | Sort-Object)) {
        if (-not $old.ContainsKey($rel)) { Write-Host "새로 생김: $rel"; $n++ }
        elseif ($old[$rel] -ne $new[$rel]) { Write-Host "바뀜: $rel"; $n++ }
    }
    foreach ($rel in ($old.Keys | Sort-Object)) {
        if (-not $new.ContainsKey($rel)) { Write-Host "없어짐: $rel"; $n++ }
    }
    Write-Host "saves diff ($n)"
    exit 0
}

Assert-NlGameNotRunning      # 게임이 쓰는 도중의 파일을 사본으로 뜨지 않는다
if (-not $To) { $To = Join-Path (Get-NlRepoRoot) "backups\saves\$(Get-Date -Format 'yyyyMMdd-HHmmss')" }
$To = [IO.Path]::GetFullPath($To)
if ($To -eq $src -or $To.StartsWith("$src\", [StringComparison]::OrdinalIgnoreCase)) { throw "사본을 세이브 폴더 안에 둘 수 없습니다: $To" }
if ((Test-Path -LiteralPath $To) -and @(Get-ChildItem -LiteralPath $To -Force).Count) { throw "사본 폴더가 비어 있지 않습니다: $To" }

$tree = Get-NlTree $src
foreach ($rel in $tree.Keys) {
    $dst = Join-Path $To $rel
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
    Copy-Item -LiteralPath (Join-Path $src $rel) -Destination $dst
    if ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -ne $tree[$rel]) { throw "사본이 원본과 다릅니다: $rel" }
}
New-Item -ItemType Directory -Force -Path $To | Out-Null      # 세이브 폴더가 비어 있어도 사본 폴더는 남긴다
Write-Host "saves backup ok ($($tree.Count)) -> $To"
```

- [ ] **Step 9: 시험이 모두 통과하는 것을 본다**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\tests\safety.tests.ps1 2>&1 | Select-Object -Last 4`
Expected: `safety tests: 18 passed`.

- [ ] **Step 10: 변이로 시험이 그 줄을 잡는지 본다**

두 줄을 하나씩 잠깐 막고 Step 9의 명령을 돌린다. 볼 때마다 되돌린다.

1. `tools/probe.ps1`의 `if ($bad.Count) { $failed = $bad[0]; break }`를 주석으로 막는다.
   Expected: 실패. `모듈이 적은 이유로 실패해야 한다`.
2. `tools/probe.ps1`의 `if (-not @($copied | Where-Object …).Count) { … }` 줄을 주석으로 막는다.
   Expected: 실패. `메뉴 덤프만으로 통과하면 안 된다`.

둘 다 되돌린 뒤 `safety tests: 18 passed`를 확인한다.

- [ ] **Step 11: 커밋**

```powershell
git -C E:\NlToyBox add -- tools/probe.ps1 tools/saves-backup.ps1 tools/common.ps1 tools/restore-game.ps1 tools/tests/safety.tests.ps1
git -C E:\NlToyBox commit -m "feat(tools): probe 가 이름이 여럿인 덤프를 가져오고, 세이브 폴더의 사본을 뜬다" -m "probe 는 덤프를 이름대로 -Out 옆으로 가져오고 모듈이 dump failed 를 적으면 기다리지 않는다. 되풀이 요청이면 사용자가 게임을 끌 때까지 기다린다. saves-backup 은 세이브 폴더를 읽어 사본과 차이를 만든다(쓰지 않는다). restore 는 덤프를 이름 꼴로 지운다." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: 모듈 — 상태 재기, 되풀이 덤프, 고친 찾기

**Files:**
- Create: `E:\NlToyBox\src\Game.hpp`, `Game.cpp`, `Finder.hpp`, `Finder.cpp`
- Modify: `E:\NlToyBox\src\Dump.hpp`, `Dump.cpp` (전체를 바꾼다)
- Modify: `E:\NlToyBox\src\ModuleMain.cpp`
- Modify: `E:\NlToyBox\CMakeLists.txt`
- Create: `E:\NlToyBox\tools\probes\stage0b-run1.txt`

**Interfaces:**
- Consumes: Task 1의 `NlCore::*` 전부.
- Produces:
  - 로그 줄(`NlToyBox.log`): `request consumed`, `request error: <글>`, `dump failed: <이유>`, `dump requested: delay <n>s, repeat <n>s, keep <n>, find <n>, scripts <n>`, `objects <n>`, `state t=<초> [room=<이름>] [+<이름>] [-<이름>]…`, `watch t=<초> <경로> {<kind…>}`, `event t=<초> <코드 이름>`, `state trace truncated` / `watch trace truncated` / `event trace truncated`, `dump <이름> start seq <n> t=<초> at <HH:MM:SS>`, `dump <이름>: globals`, `dump <이름>: find_global visited <n>`, `dump <이름>: find_ds visited <n>`, `dump <이름>: find_instances visited <n>`, `dump <이름> done seq <n> t=<초> took <초> at <HH:MM:SS>`, `dump done`(되풀이하지 않는 요청에서만)
  - 파일: `mods\Aurie\NlToyBox.dump.<이름>.json`(이름은 `menu`, `late0` …). 형식은 Task 2의 Consumes와 같다
  - Phase 0의 줄(`NlToyBox <버전> loaded`, `yytk …`, `trigger object_call|wndproc`, `builtin … = true`, `script … = found`, `probe done`)은 그대로다. `check-load.ps1`은 고치지 않는다

게임 없이는 시험할 수 없다(Global Constraints의 예외). 관문은 빌드 성공과 Step 10의 세 시험 묶음이다.
이 Task의 코드가 기대는 빌트인과 인터페이스의 출처는 스펙 §3.7의 근거 표에 있다.

- [ ] **Step 1: `src/Game.hpp`**

```cpp
#pragma once
// 러너에 닿는 얇은 층. 빌트인 호출과 이미 써 본 YYTK 인터페이스만 쓴다(러너 내부 구조체의 배치에 기대지 않는다).
// Init 을 뺀 나머지는 게임 스레드에서만 부른다.

#include <YYTK_Shared.hpp>

#include <ostream>
#include <string>
#include <vector>

namespace NlGame
{
	struct Object
	{
		std::string Name;
		double Index = 0;	// object_index. 빌트인에 수로 넘긴다
	};

	void Init(YYTK::YYTKInterface* Yytk);
	YYTK::YYTKInterface* Yytk();

	// 전역 인스턴스. 못 얻으면 nullptr.
	YYTK::CInstance* Global();

	// 빌트인을 부른다. 실패하면 거짓.
	bool Call(const char* Name, const std::vector<YYTK::RValue>& Args, YYTK::RValue& Result);

	// 수를 돌려주는 빌트인. 실패하거나 수가 아니면 Fallback.
	double CallNumber(const char* Name, const std::vector<YYTK::RValue>& Args, double Fallback);

	// 수(불리언 포함)인가. 문자열·구조체·배열·undefined 는 아니다.
	bool IsNumber(const YYTK::RValue& Value);

	// 게임의 오브젝트 전부. 처음 부를 때 0 번부터 object_exists 가 참인 동안 이름을 모은다.
	const std::vector<Object>& Objects();

	// 지금 룸의 이름. 못 얻으면 빈 글이다(한 번 실패하면 다시 시도하지 않는다).
	std::string RoomName();

	// "global.a.b" 를 따라간다. 없으면 거짓.
	bool Resolve(const std::string& Path, YYTK::RValue& Out);

	// 배열 길이. 못 얻으면 음수.
	double ArrayLength(const YYTK::RValue& Value);

	// 값 하나를 "kind":…[,"value":…] 로 적는다(중괄호 없이). 구조체와 배열의 속은 보지 않는다.
	std::string Describe(const YYTK::RValue& Value);

	// 구조체의 멤버를 "이름":{…},… 로 적는다. Expand 면 구조체 멤버를 한 단계 더 편다.
	void WriteMembers(std::ostream& Out, const YYTK::RValue& Object, bool Expand, bool FlushEach);
}
```

- [ ] **Step 2: `src/Game.cpp`**

```cpp
#include "Game.hpp"

#include "core/Text.hpp"

using namespace Aurie;
using namespace YYTK;
using NlCore::Number;
using NlCore::Quote;

namespace
{
	YYTKInterface* g_Yytk = nullptr;
	std::vector<NlGame::Object> g_Objects;	// RValue 를 담지 않는다(정적 저장 기간의 RValue 를 두지 않는다)
	bool g_ObjectsBuilt = false;
	bool g_RoomFailed = false;
}

void NlGame::Init(YYTKInterface* Yytk)
{
	g_Yytk = Yytk;
}

YYTKInterface* NlGame::Yytk()
{
	return g_Yytk;
}

CInstance* NlGame::Global()
{
	CInstance* global = nullptr;
	return AurieSuccess(g_Yytk->GetGlobalInstance(&global)) ? global : nullptr;
}

bool NlGame::Call(const char* Name, const std::vector<RValue>& Args, RValue& Result)
{
	CInstance* global = Global();
	return global && AurieSuccess(g_Yytk->CallBuiltinEx(Result, Name, global, global, Args));
}

bool NlGame::IsNumber(const RValue& Value)
{
	return !Value.IsString() && !Value.IsStruct() && !Value.IsArray() && Value.IsNumberConvertible();
}

double NlGame::CallNumber(const char* Name, const std::vector<RValue>& Args, double Fallback)
{
	RValue result;
	if (!Call(Name, Args, result) || !IsNumber(result))
		return Fallback;
	return result.ToDouble();
}

const std::vector<NlGame::Object>& NlGame::Objects()
{
	if (g_ObjectsBuilt)
		return g_Objects;
	g_ObjectsBuilt = true;

	for (int i = 0; i < 4096; i++)
	{
		const RValue index(static_cast<double>(i));
		if (CallNumber("object_exists", { index }, 0) <= 0)
			break;

		RValue name;
		if (!Call("object_get_name", { index }, name) || !name.IsString())
			break;
		g_Objects.push_back({ name.ToString(), static_cast<double>(i) });
	}
	return g_Objects;
}

std::string NlGame::RoomName()
{
	if (g_RoomFailed)
		return "";

	// 전역 빌트인 변수는 인스턴스 없이 읽는다(YYToolkit 위키 GetBuiltin). 이 게임에서 빌트인 변수 표의
	// 초기화가 성공한 것은 aurie.log 의 GetBuiltinInformation => AURIE_SUCCESS 로 확인했다.
	RValue room, name;
	if (!AurieSuccess(g_Yytk->GetBuiltin("room", nullptr, NULL_INDEX, room))
		|| !Call("room_get_name", { room }, name) || !name.IsString())
	{
		g_RoomFailed = true;
		return "";
	}
	return name.ToString();
}

bool NlGame::Resolve(const std::string& Path, RValue& Out)
{
	CInstance* global = Global();
	if (!global || Path.rfind("global.", 0) != 0)
		return false;

	RValue current(global);
	size_t begin = 7;	// "global." 다음
	while (true)
	{
		const size_t dot = Path.find('.', begin);
		const std::string name = Path.substr(begin, dot == std::string::npos ? std::string::npos : dot - begin);
		if (!current.IsStruct())
			return false;

		RValue* member = nullptr;
		if (!AurieSuccess(g_Yytk->GetInstanceMember(current, name.c_str(), member)) || !member)
			return false;
		current = *member;

		if (dot == std::string::npos)
			break;
		begin = dot + 1;
	}
	Out = current;
	return true;
}

double NlGame::ArrayLength(const RValue& Value)
{
	return CallNumber("array_length", { Value }, -1);
}

std::string NlGame::Describe(const RValue& Value)
{
	if (Value.IsStruct())
		return "\"kind\":\"struct\"";
	if (Value.IsArray())
		return "\"kind\":\"array\",\"length\":" + Number(ArrayLength(Value));
	if (Value.IsString())
		return "\"kind\":\"string\",\"value\":" + Quote(Value.ToString());
	if (Value.IsNumberConvertible())
		return "\"kind\":" + Quote(Value.GetKindName()) + ",\"value\":" + Number(Value.ToDouble());
	return "\"kind\":" + Quote(Value.GetKindName());
}

void NlGame::WriteMembers(std::ostream& Out, const RValue& Object, bool Expand, bool FlushEach)
{
	bool first = true;
	g_Yytk->EnumInstanceMembers(Object, [&](const char* Name, RValue* Value) -> bool
	{
		Out << (first ? "" : ",") << "\n" << Quote(Name ? Name : "") << ":{";
		first = false;

		if (!Value)
			Out << "\"kind\":\"error\"";
		else if (Expand && Value->IsStruct())
		{
			Out << "\"kind\":\"struct\",\"members\":{";
			WriteMembers(Out, *Value, false, false);
			Out << "}";
		}
		else
			Out << Describe(*Value);

		Out << "}";
		if (FlushEach)
			Out.flush();	// 도중에 죽어도 어디까지 왔는지 남는다
		return false;		// 거짓을 돌려줘야 다음 멤버로 넘어간다
	});
}
```

- [ ] **Step 3: `src/Finder.hpp`**

```cpp
#pragma once
// 값과 이름으로 찾기. 스펙: 데이터 오버레이 §3.7.
// 구조체와 배열은 너비 우선으로 내려가고, 닿은 가장 얕은 깊이를 적어 둔다. 깊은 길로 먼저 닿아서 빠지는 것이 없다.

#include "Game.hpp"
#include "core/PathTable.hpp"
#include "core/Request.hpp"

#include <deque>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace NlDump
{
	struct Limits
	{
		int MaxDepth = 6;				// 찾기가 내려가는 깊이
		size_t MaxVisited = 2000000;	// 찾기가 방문하는 값의 수(구역을 통틀어)
		double MaxArray = 64;			// 이보다 긴 배열과 ds_list 는 들어가지 않는다
		size_t MaxHits = 300;			// 구역마다 적는 수
		double MaxDsKeys = 20000;		// 키가 이보다 많은 ds_map 은 들어가지 않는다
		int MaxDsId = 100000;			// ds 번호를 0 부터 여기까지 모두 본다. 가장 큰 번호를 덤프에 적어 범위가 모자라지 않았는지 본다
		int MaxInstances = 16;			// 오브젝트마다 찾기가 들어가는 인스턴스 수
	};

	// 찾기가 들어갈 인스턴스 하나.
	struct InstanceRef
	{
		std::string Object;
		int Number = 0;			// 그 오브젝트의 인스턴스 가운데 몇 번째인가
		YYTK::RValue Id;		// instance_find 가 돌려준 값. 빌트인에 그대로 넘긴다
	};

	class Finder
	{
	public:
		Finder(std::ostream& Out, const NlCore::Request& Request, const Limits& Bounds);

		// 저마다 JSON 구역 하나를 통째로 쓴다: ,"find_global":{…}  ,"find_ds":{…}  ,"find_instances":{…}
		void FindGlobal(YYTK::CInstance* GlobalInstance);
		void FindDataStructures();
		void FindInstances(const std::vector<InstanceRef>& Refs);

		// 구역을 건너뛰었다고 쓴다.
		void Skip(const char* Section);

		size_t Visited() const { return m_Visited; }

	private:
		struct Pending
		{
			YYTK::RValue Value;
			int Node;
			int Depth;
		};

		bool Wanted() const;
		bool MatchesName(const std::string& Name) const;
		void Begin(const char* Section);
		void End(const std::string& Extra);
		void Hit(int Parent, const std::string& Segment, const char* Why, const YYTK::RValue& Value);
		void NestedHit(int Parent, const std::string& Segment, bool IsMap, double Id);
		void Visit(const YYTK::RValue& Value, int Parent, const std::string& Segment, const std::string& Name, int Depth);
		void Expand(const Pending& Item);
		void Drain();
		bool WalkMap(int Id, int Root, bool& SelfSeen);
		bool WalkList(int Id, int Root, bool& SelfSeen);

		std::ostream& m_Out;
		const NlCore::Request& m_Request;
		Limits m_Limits;
		NlCore::PathTable m_Paths;
		std::deque<Pending> m_Queue;
		std::unordered_map<const void*, int> m_Depth;	// 구조체·배열마다 닿은 가장 얕은 깊이
		size_t m_Visited = 0;
		size_t m_Containers = 0;
		size_t m_DepthCut = 0;			// 깊이 한도에서 멈춘 구조체·배열의 수
		size_t m_ArraySkipped = 0;		// 길어서 들어가지 않은 배열의 수
		size_t m_SectionHits = 0;
		bool m_HitsCut = false;
		bool m_Truncated = false;
	};
}
```

- [ ] **Step 4: `src/Finder.cpp`**

```cpp
#include "Finder.hpp"

#include "core/Text.hpp"

using namespace YYTK;
using NlCore::Number;
using NlCore::Quote;

namespace
{
	// 자가 점검의 표식. 찾기가 ds_map·ds_list 안을 실제로 보는지 확인하는 데 쓴다.
	constexpr const char* k_SelfKey = "nltoybox_selfcheck";
	constexpr double k_SelfMapValue = 918273645;
	constexpr double k_SelfListValue = 918273646;
	constexpr size_t k_MaxNested = 200;		// 이름으로 찾은 ds_map·ds_list 의 속을 적는 수

	const char* Bool(bool Value)
	{
		return Value ? "true" : "false";
	}

	// ds_map 의 키를 글로. 키는 문자열이 아닐 수도 있다.
	std::string KeyName(const RValue& Key)
	{
		if (Key.IsString())
			return Key.ToString();
		if (NlGame::IsNumber(Key))
			return Number(Key.ToDouble());
		return Key.GetKindName();
	}
}

namespace NlDump
{
	Finder::Finder(std::ostream& Out, const NlCore::Request& Request, const Limits& Bounds)
		: m_Out(Out), m_Request(Request), m_Limits(Bounds)
	{
	}

	bool Finder::Wanted() const
	{
		return !m_Request.FindValues.empty() || !m_Request.FindNames.empty();
	}

	bool Finder::MatchesName(const std::string& Name) const
	{
		for (const std::string& wanted : m_Request.FindNames)
			if (NlCore::Contains(Name, wanted))
				return true;
		return false;
	}

	void Finder::Begin(const char* Section)
	{
		m_SectionHits = 0;
		m_HitsCut = false;
		m_Out << ",\"" << Section << "\":{\"hits\":[";
	}

	void Finder::End(const std::string& Extra)
	{
		m_Out << "\n],\"visited\":" << m_Visited << ",\"containers\":" << m_Containers
			<< ",\"depth_cut\":" << m_DepthCut << ",\"array_skipped\":" << m_ArraySkipped
			<< ",\"hits_cut\":" << Bool(m_HitsCut) << ",\"truncated\":" << Bool(m_Truncated) << Extra << "}";
		m_Out.flush();
	}

	void Finder::Skip(const char* Section)
	{
		m_Out << ",\"" << Section << "\":{\"hits\":[],\"skipped\":true}";
		m_Out.flush();
	}

	void Finder::Hit(int Parent, const std::string& Segment, const char* Why, const RValue& Value)
	{
		if (m_SectionHits >= m_Limits.MaxHits)
		{
			m_HitsCut = true;
			return;
		}

		m_Out << (m_SectionHits == 0 ? "" : ",") << "\n{\"path\":" << Quote(m_Paths.Path(Parent) + Segment)
			<< ",\"why\":\"" << Why << "\"," << NlGame::Describe(Value);
		if (Value.IsStruct())
		{
			m_Out << ",\"members\":{";
			NlGame::WriteMembers(m_Out, Value, false, false);
			m_Out << "}";
		}
		m_Out << "}";
		m_Out.flush();
		m_SectionHits++;
	}

	// 이름으로 찾은 ds_map 항목이 중첩된 map·list 이면 그 속을 한 단계 적는다.
	void Finder::NestedHit(int Parent, const std::string& Segment, bool IsMap, double Id)
	{
		if (m_SectionHits >= m_Limits.MaxHits)
		{
			m_HitsCut = true;
			return;
		}

		const RValue ds(Id);
		m_Out << (m_SectionHits == 0 ? "" : ",") << "\n{\"path\":" << Quote(m_Paths.Path(Parent) + Segment)
			<< ",\"why\":\"nested\",\"kind\":\"" << (IsMap ? "ds_map" : "ds_list") << "\",\"id\":" << Number(Id) << ",\"members\":{";

		size_t written = 0;
		if (IsMap)
		{
			RValue keys;
			if (NlGame::Call("ds_map_keys_to_array", { ds }, keys) && keys.IsArray())
			{
				for (const RValue& key : keys.ToVector())
				{
					if (written >= k_MaxNested)
						break;
					RValue value;
					if (!NlGame::Call("ds_map_find_value", { ds, key }, value))
						continue;
					m_Out << (written ? "," : "") << "\n" << Quote(KeyName(key)) << ":{" << NlGame::Describe(value) << "}";
					written++;
				}
			}
		}
		else
		{
			const double size = NlGame::CallNumber("ds_list_size", { ds }, 0);
			for (int i = 0; i < size && written < k_MaxNested; i++)
			{
				RValue value;
				if (!NlGame::Call("ds_list_find_value", { ds, RValue(static_cast<double>(i)) }, value))
					continue;
				m_Out << (written ? "," : "") << "\n" << Quote(std::to_string(i)) << ":{" << NlGame::Describe(value) << "}";
				written++;
			}
		}

		m_Out << "}}";
		m_Out.flush();
		m_SectionHits++;
	}

	// 값 하나를 본다. 이름·값이 맞으면 적고, 구조체와 배열이면 큐에 넣는다.
	void Finder::Visit(const RValue& Value, int Parent, const std::string& Segment, const std::string& Name, int Depth)
	{
		if (m_Truncated)
			return;
		if (++m_Visited > m_Limits.MaxVisited)
		{
			m_Truncated = true;
			return;
		}

		if (!Name.empty() && MatchesName(Name))
			Hit(Parent, Segment, "name", Value);

		if (Value.IsStruct() || Value.IsArray())
		{
			if (Depth >= m_Limits.MaxDepth)
			{
				m_DepthCut++;
				return;
			}

			// 같은 것에 더 얕은 길로 다시 닿으면 다시 들어간다. 더 깊거나 같은 길이면 건너뛴다.
			const void* identity = Value.m_Pointer;
			if (identity)
			{
				const auto [it, inserted] = m_Depth.try_emplace(identity, Depth);
				if (!inserted)
				{
					if (it->second <= Depth)
						return;
					it->second = Depth;
				}
			}
			m_Queue.push_back({ Value, m_Paths.Add(Parent, Segment), Depth });
			return;
		}

		if (!NlGame::IsNumber(Value))
			return;
		const double number = Value.ToDouble();
		for (const double wanted : m_Request.FindValues)
			if (number == wanted)
				Hit(Parent, Segment, "value", Value);
	}

	void Finder::Expand(const Pending& Item)
	{
		m_Containers++;
		if (Item.Value.IsStruct())
		{
			NlGame::Yytk()->EnumInstanceMembers(Item.Value, [&](const char* MemberName, RValue* Member) -> bool
			{
				const std::string name = MemberName ? MemberName : "";
				if (Member)
					Visit(*Member, Item.Node, "." + name, name, Item.Depth + 1);
				return false;	// 거짓을 돌려줘야 다음 멤버로 넘어간다
			});
			return;
		}

		const double length = NlGame::ArrayLength(Item.Value);
		if (length <= 0)
			return;
		if (length > m_Limits.MaxArray)
		{
			m_ArraySkipped++;
			return;
		}
		const std::vector<RValue> items = Item.Value.ToVector();
		for (size_t i = 0; i < items.size(); i++)
			Visit(items[i], Item.Node, "[" + std::to_string(i) + "]", "", Item.Depth + 1);
	}

	void Finder::Drain()
	{
		while (!m_Queue.empty() && !m_Truncated)
		{
			const Pending item = m_Queue.front();	// Expand 가 큐에 더 넣으므로 사본으로 든다
			m_Queue.pop_front();
			Expand(item);
		}
		m_Queue.clear();
	}

	void Finder::FindGlobal(CInstance* GlobalInstance)
	{
		Begin("find_global");
		if (GlobalInstance && Wanted())
		{
			Visit(RValue(GlobalInstance), -1, "global", "", 0);
			Drain();
		}
		End("");
	}

	// ds_map 하나. 키가 너무 많아 건너뛰었으면 거짓.
	bool Finder::WalkMap(int Id, int Root, bool& SelfSeen)
	{
		const RValue ds(static_cast<double>(Id));
		if (NlGame::CallNumber("ds_map_size", { ds }, 0) > m_Limits.MaxDsKeys)
			return false;

		RValue keys;
		if (!NlGame::Call("ds_map_keys_to_array", { ds }, keys) || !keys.IsArray())
			return true;

		const int node = m_Paths.Add(Root, "_map[" + std::to_string(Id) + "]");
		for (const RValue& key : keys.ToVector())
		{
			RValue value;
			if (!NlGame::Call("ds_map_find_value", { ds, key }, value))
				continue;

			const std::string name = KeyName(key);
			if (name == k_SelfKey)
			{
				SelfSeen = true;
				continue;
			}

			const std::string segment = "." + name;
			Visit(value, node, segment, name, 1);
			if (NlGame::IsNumber(value) && MatchesName(name))
			{
				if (NlGame::CallNumber("ds_map_is_map", { ds, key }, 0) > 0)
					NestedHit(node, segment, true, value.ToDouble());
				else if (NlGame::CallNumber("ds_map_is_list", { ds, key }, 0) > 0)
					NestedHit(node, segment, false, value.ToDouble());
			}
		}
		return true;
	}

	// ds_list 하나. 너무 길어 건너뛰었으면 거짓.
	bool Finder::WalkList(int Id, int Root, bool& SelfSeen)
	{
		const RValue ds(static_cast<double>(Id));
		const double size = NlGame::CallNumber("ds_list_size", { ds }, 0);
		if (size > m_Limits.MaxArray)
			return false;

		const int node = m_Paths.Add(Root, "_list[" + std::to_string(Id) + "]");
		for (int i = 0; i < size; i++)
		{
			RValue value;
			if (!NlGame::Call("ds_list_find_value", { ds, RValue(static_cast<double>(i)) }, value))
				continue;
			if (NlGame::IsNumber(value) && value.ToDouble() == k_SelfListValue)
			{
				SelfSeen = true;
				continue;
			}
			Visit(value, node, "[" + std::to_string(i) + "]", "", 1);
		}
		return true;
	}

	void Finder::FindDataStructures()
	{
		Begin("find_ds");
		int maps = 0, lists = 0, skipped = 0;
		int highest_map = -1, highest_list = -1;
		bool self_made = false, self_map = false, self_list = false;

		if (Wanted())
		{
			// 자가 점검: 표식을 넣은 ds_map 과 ds_list 를 하나씩 만들었다가 지운다. 게임의 값은 건드리지 않는다.
			RValue made_map, made_list, ignored;
			const bool have_map = NlGame::Call("ds_map_create", {}, made_map);
			const bool have_list = NlGame::Call("ds_list_create", {}, made_list);
			if (have_map)
				NlGame::Call("ds_map_add", { made_map, RValue(k_SelfKey), RValue(k_SelfMapValue) }, ignored);
			if (have_list)
				NlGame::Call("ds_list_add", { made_list, RValue(k_SelfListValue) }, ignored);
			self_made = have_map && have_list;

			// 번호 0 부터 한도까지 모두 본다. 같은 번호가 map 과 list 에 따로 있을 수 있다(매뉴얼 ds_exists).
			// 형 상수: ds_type_map = 1, ds_type_list = 2 (YoYoGames/GameMaker-HTML5 Function_YoYo.js 36~41행).
			// 이 러너에서도 맞는지는 위의 표식을 찾았는가로 드러난다.
			const int root = m_Paths.Add(-1, "ds");
			for (int id = 0; id < m_Limits.MaxDsId && !m_Truncated; id++)
			{
				const RValue ds(static_cast<double>(id));
				if (NlGame::CallNumber("ds_exists", { ds, RValue(1.0) }, 0) > 0)
				{
					maps++;
					highest_map = id;
					if (!WalkMap(id, root, self_map))
						skipped++;
				}
				if (NlGame::CallNumber("ds_exists", { ds, RValue(2.0) }, 0) > 0)
				{
					lists++;
					highest_list = id;
					if (!WalkList(id, root, self_list))
						skipped++;
				}
			}
			Drain();

			if (have_map)
				NlGame::Call("ds_map_destroy", { made_map }, ignored);
			if (have_list)
				NlGame::Call("ds_list_destroy", { made_list }, ignored);
		}

		End(",\"maps\":" + std::to_string(maps) + ",\"lists\":" + std::to_string(lists)
			+ ",\"highest_map\":" + std::to_string(highest_map) + ",\"highest_list\":" + std::to_string(highest_list)
			+ ",\"skipped_large\":" + std::to_string(skipped)
			+ ",\"selfcheck\":{\"made\":" + Bool(self_made) + ",\"ds_map\":" + Bool(self_map) + ",\"ds_list\":" + Bool(self_list) + "}");
	}

	void Finder::FindInstances(const std::vector<InstanceRef>& Refs)
	{
		Begin("find_instances");
		size_t walked = 0;

		if (Wanted())
		{
			const int root = m_Paths.Add(-1, "instance");
			for (const InstanceRef& ref : Refs)
			{
				if (m_Truncated)
					break;
				if (ref.Number >= m_Limits.MaxInstances)
					continue;

				RValue names;
				if (!NlGame::Call("variable_instance_get_names", { ref.Id }, names) || !names.IsArray())
					continue;
				walked++;

				const int node = m_Paths.Add(root, ":" + ref.Object + "#" + std::to_string(ref.Number));
				for (const RValue& name : names.ToVector())
				{
					if (!name.IsString())
						continue;
					RValue value;
					if (!NlGame::Call("variable_instance_get", { ref.Id, name }, value))
						continue;
					const std::string text = name.ToString();
					Visit(value, node, "." + text, text, 1);
				}
			}
			Drain();
		}

		End(",\"instances\":" + std::to_string(walked));
	}
}
```

- [ ] **Step 5: `src/Dump.hpp` 전체를 바꾼다**

```cpp
#pragma once
// 요청 파일(NlToyBox.probe.txt)이 있을 때만 도는 기록과 덤프. 형식은 스펙 §3.2, §3.7.
// 읽기만 한다. 예외는 ds 자가 점검의 표식 둘(Finder.cpp)과 요청의 script= 호출이다.

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>

namespace NlDump
{
	// 모듈 폴더에서 요청 파일을 읽고 지운다. 없으면 아무 일도 하지 않는다. 콜백을 등록하기 전에 부른다.
	void Init(const Aurie::fs::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log);

	// 게임 스레드의 콜백에서 매번 부른다. Code 는 지금 도는 이벤트의 코드 객체다(없으면 nullptr).
	// 0.5초마다 상태를 재고, 일정에 따라 덤프한다.
	void Tick(YYTK::CCode* Code);
}
```

- [ ] **Step 6: `src/Dump.cpp` 전체를 바꾼다**

```cpp
#include "Dump.hpp"

#include "Finder.hpp"
#include "Game.hpp"
#include "core/Request.hpp"
#include "core/Schedule.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

using namespace Aurie;
using namespace YYTK;
using NlCore::Fixed;
using NlCore::Number;
using NlCore::Quote;

namespace
{
	constexpr double k_SampleSeconds = 0.5;		// 상태를 재는 간격
	// 기록 줄의 한도. 종류마다 따로 센다(이벤트나 자주 바뀌는 watch 가 state 줄을 밀어내지 않게).
	constexpr int k_MaxStateLines = 300;
	constexpr int k_MaxWatchLines = 200;
	constexpr int k_MaxEventLines = 250;
	constexpr int k_MaxOwnedPerObject = 512;	// 오브젝트마다 훑는 인스턴스 수

	NlCore::Request g_Request;
	NlCore::Schedule g_Schedule;
	const NlDump::Limits g_Limits{};
	std::function<void(const std::string&)> g_Log;
	fs::path g_Dir;
	std::string g_Version;
	std::chrono::steady_clock::time_point g_Start;
	std::atomic<bool> g_Active = false;

	// 아래는 게임 스레드에서만 만진다.
	bool g_Finished = false;
	bool g_Busy = false;			// 덤프 도중 이벤트가 일어나 Tick 이 다시 들어오는 것을 막는다
	bool g_Announced = false;
	double g_LastSample = -1;
	int g_StateLines = 0;
	int g_WatchLines = 0;
	int g_EventLines = 0;
	bool g_RoomKnown = false;
	std::string g_Room;										// 지난번의 룸 이름. 못 얻었으면 빈 글
	std::vector<std::string> g_Present;						// 지난번에 인스턴스가 있던 오브젝트
	std::unordered_map<std::string, int> g_Counts;			// 지난번의 오브젝트별 인스턴스 수
	std::vector<std::string> g_WatchLast;					// watch 마다 지난번에 적은 글
	std::unordered_set<const void*> g_SeenCode;

	double Elapsed()
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_Start).count();
	}

	// 벽시계 시각 HH:MM:SS. 사용자가 알려 준 때와 덤프의 때를 맞춰 보는 데 쓴다.
	std::string Clock()
	{
		const std::time_t now = std::time(nullptr);
		std::tm local{};
		localtime_s(&local, &now);
		char buf[16];
		std::strftime(buf, sizeof(buf), "%H:%M:%S", &local);
		return buf;
	}

	bool Skipped(const char* Section)
	{
		return std::find(g_Request.Skip.begin(), g_Request.Skip.end(), Section) != g_Request.Skip.end();
	}

	// 기록 한 줄을 적는다. Lines 가 Max 에 닿으면 "<What> trace truncated" 를 한 번 적고 그 뒤로는 적지 않는다.
	void Trace(const std::string& Line, int& Lines, int Max, const char* What)
	{
		if (Lines > Max)
			return;
		if (Lines++ == Max)
		{
			g_Log(std::string(What) + " trace truncated");
			return;
		}
		g_Log(Line);
	}

	// 코드 객체의 이름을 읽는다. YYToolkit 의 CCode 배치가 이 러너와 맞는지는 확인하지 못했다.
	// 잘못된 포인터를 읽어도 게임이 죽지 않게 SEH 로 감싸고, 읽은 것이 글처럼 생겼을 때만 쓴다.
	// (이 함수에는 소멸자가 있는 지역 변수를 두지 않는다.)
	bool SafeCodeName(CCode* Code, char* Buffer, size_t Size)
	{
		__try
		{
			const char* name = Code->GetName();
			if (!name)
				return false;
			size_t i = 0;
			for (; i + 1 < Size && name[i]; i++)
			{
				if (name[i] < 0x20 || name[i] > 0x7E)
					return false;
				Buffer[i] = name[i];
			}
			Buffer[i] = '\0';
			return i > 0;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	// 룸 이름, 오브젝트별 인스턴스 수, watch 의 값을 재고, 달라진 것만 로그에 적는다.
	void Sample(double Now)
	{
		if (!g_Announced)
		{
			g_Announced = true;
			g_Log("objects " + std::to_string(NlGame::Objects().size()));
		}

		std::string changes;

		if (!Skipped("room"))
		{
			const std::string room = NlGame::RoomName();
			if (!g_RoomKnown || room != g_Room)
			{
				changes += " room=" + (room.empty() ? std::string("?") : room);
				g_Room = room;
				g_RoomKnown = true;
			}
		}

		std::vector<std::string> present;
		g_Counts.clear();
		for (const NlGame::Object& object : NlGame::Objects())
		{
			const int count = static_cast<int>(NlGame::CallNumber("instance_number", { RValue(object.Index) }, 0));
			g_Counts[object.Name] = count;
			if (count > 0)
				present.push_back(object.Name);
		}

		for (const std::string& name : present)
			if (std::find(g_Present.begin(), g_Present.end(), name) == g_Present.end())
				changes += " +" + name;
		for (const std::string& name : g_Present)
			if (std::find(present.begin(), present.end(), name) == present.end())
				changes += " -" + name;
		if (!changes.empty())
			Trace("state t=" + Fixed(Now, 1) + changes, g_StateLines, k_MaxStateLines, "state");
		g_Present = std::move(present);

		for (size_t i = 0; i < g_Request.Watches.size(); i++)
		{
			RValue value;
			const std::string text = NlGame::Resolve(g_Request.Watches[i], value) ? NlGame::Describe(value) : "\"kind\":\"missing\"";
			if (text == g_WatchLast[i])
				continue;
			g_WatchLast[i] = text;
			Trace("watch t=" + Fixed(Now, 1) + " " + g_Request.Watches[i] + " {" + text + "}", g_WatchLines, k_MaxWatchLines, "watch");
		}
	}

	// 인스턴스를 오브젝트별로 모은다. 인스턴스 수가 적은 오브젝트부터 본다. instance_number 와 instance_find 는
	// 자식 오브젝트의 인스턴스도 포함하므로(매뉴얼), 그래야 인스턴스가 부모가 아니라 자기 오브젝트의 이름으로 적힌다.
	std::vector<NlDump::InstanceRef> CollectInstances()
	{
		std::vector<std::pair<int, const NlGame::Object*>> order;
		for (const NlGame::Object& object : NlGame::Objects())
		{
			const auto it = g_Counts.find(object.Name);
			if (it != g_Counts.end() && it->second > 0)
				order.push_back({ it->second, &object });
		}
		std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

		std::vector<NlDump::InstanceRef> refs;
		std::unordered_set<int64_t> seen;
		for (const auto& [count, object] : order)
		{
			int own = 0;
			for (int n = 0; n < count && n < k_MaxOwnedPerObject; n++)
			{
				RValue id;
				if (!NlGame::Call("instance_find", { RValue(object->Index), RValue(static_cast<double>(n)) }, id))
					continue;
				if (NlGame::IsNumber(id) && id.ToDouble() < 0)	// noone (-4)
					continue;
				if (!seen.insert(id.m_i64).second)
					continue;
				refs.push_back({ object->Name, own++, id });
			}
		}
		return refs;
	}

	// 오브젝트마다 인스턴스 수와, 첫 인스턴스의 변수를 한 단계 적는다.
	void WriteInstances(std::ostream& Out, const std::vector<NlDump::InstanceRef>& Refs)
	{
		Out << ",\"instances\":{";
		bool first = true;
		for (const NlGame::Object& object : NlGame::Objects())
		{
			const auto it = g_Counts.find(object.Name);
			const int count = it == g_Counts.end() ? 0 : it->second;
			if (count <= 0)
				continue;

			int own = 0;
			const NlDump::InstanceRef* sample = nullptr;
			for (const NlDump::InstanceRef& ref : Refs)
			{
				if (ref.Object != object.Name)
					continue;
				if (!sample)
					sample = &ref;
				own++;
			}

			Out << (first ? "" : ",") << "\n" << Quote(object.Name) << ":{\"count\":" << count << ",\"own\":" << own << ",\"members\":{";
			first = false;

			RValue names;
			if (sample && NlGame::Call("variable_instance_get_names", { sample->Id }, names) && names.IsArray())
			{
				bool first_member = true;
				for (const RValue& name : names.ToVector())
				{
					if (!name.IsString())
						continue;
					RValue value;
					if (!NlGame::Call("variable_instance_get", { sample->Id, name }, value))
						continue;
					Out << (first_member ? "" : ",") << "\n" << Quote(name.ToString()) << ":{" << NlGame::Describe(value) << "}";
					first_member = false;
				}
			}
			Out << "}}";
			Out.flush();
		}
		Out << "\n}";
	}

	// 덤프 하나를 쓴다. 덜 위험한 것부터 쓴다: 전역 목록, 전역 찾기, ds 찾기, 인스턴스, 스크립트 호출.
	// 구역이 끝날 때마다 파일을 비우고 로그에 적는다. 도중에 죽어도 어디까지 왔는지 남는다.
	bool Run(const std::string& Name, int Seq, double Now, bool WithScripts)
	{
		const std::string file = "NlToyBox.dump." + Name + ".json";
		std::ofstream out(g_Dir / file, std::ios::trunc | std::ios::binary);
		if (!out.is_open())
		{
			g_Log("dump failed: cannot open " + file);
			return false;
		}
		const std::string tag = "dump " + Name;
		g_Log(tag + " start seq " + std::to_string(Seq) + " t=" + Fixed(Now, 1) + " at " + Clock());

		out << "{\"module_dump\":2,\"module_version\":" << Quote(g_Version) << ",\"name\":" << Quote(Name)
			<< ",\"seq\":" << Seq << ",\"elapsed_seconds\":" << Number(Now) << ",\"room\":" << Quote(g_Room)
			<< ",\"delay_seconds\":" << g_Request.DelaySeconds << ",\"repeat_seconds\":" << Number(g_Request.RepeatSeconds)
			<< ",\"limits\":{\"max_depth\":" << g_Limits.MaxDepth << ",\"max_visited\":" << g_Limits.MaxVisited
			<< ",\"max_array\":" << Number(g_Limits.MaxArray) << ",\"max_hits\":" << g_Limits.MaxHits
			<< ",\"max_ds_keys\":" << Number(g_Limits.MaxDsKeys) << ",\"max_ds_id\":" << g_Limits.MaxDsId
			<< ",\"max_instances\":" << g_Limits.MaxInstances << "}";

		out << ",\"present\":{";
		for (size_t i = 0; i < g_Present.size(); i++)
			out << (i ? "," : "") << Quote(g_Present[i]) << ":" << g_Counts[g_Present[i]];
		out << "}";

		out << ",\"watch\":{";
		for (size_t i = 0; i < g_Request.Watches.size(); i++)
			out << (i ? "," : "") << Quote(g_Request.Watches[i]) << ":{" << g_WatchLast[i] << "}";
		out << "}";

		CInstance* global = NlGame::Global();
		out << ",\"global_status\":" << Quote(global ? "AURIE_SUCCESS" : "no global instance");

		out << ",\"globals\":{";
		if (global)
			NlGame::WriteMembers(out, RValue(global), true, true);
		out << "\n}";
		out.flush();
		g_Log(tag + ": globals");

		NlDump::Finder finder(out, g_Request, g_Limits);
		finder.FindGlobal(global);
		g_Log(tag + ": find_global visited " + std::to_string(finder.Visited()));

		if (Skipped("ds"))
			finder.Skip("find_ds");
		else
			finder.FindDataStructures();
		g_Log(tag + ": find_ds visited " + std::to_string(finder.Visited()));

		if (Skipped("instances"))
		{
			out << ",\"instances\":{}";
			finder.Skip("find_instances");
		}
		else
		{
			const std::vector<NlDump::InstanceRef> refs = CollectInstances();
			WriteInstances(out, refs);
			finder.FindInstances(refs);
		}
		g_Log(tag + ": find_instances visited " + std::to_string(finder.Visited()));

		// 스크립트 호출은 맨 뒤다. 게임 스크립트는 인자가 맞지 않으면 GML 오류로 게임을 끝낸다(단계 0 실측).
		out << ",\"scripts\":[";
		bool first = true;
		for (const NlCore::ScriptCall& call : g_Request.Scripts)
		{
			if (!WithScripts)
				break;

			std::vector<RValue> args;
			std::string args_json;
			for (const std::string& arg : call.Args)
			{
				double number = 0;
				const bool numeric = NlCore::ParseNumber(arg, number);
				args.push_back(numeric ? RValue(number) : RValue(std::string_view(arg)));
				args_json += std::string(args_json.empty() ? "" : ",") + (numeric ? Number(number) : Quote(arg));
			}

			out << (first ? "" : ",") << "\n{\"name\":" << Quote(call.Name) << ",\"args\":[" << args_json << "]";
			out.flush();	// 호출이 게임을 죽이면 어느 스크립트였는지 남는다
			first = false;

			if (!global)
			{
				out << ",\"status\":\"no global instance\"}";
				continue;
			}

			RValue result;
			const AurieStatus status = NlGame::Yytk()->CallGameScriptEx(result, call.Name, global, global, args);
			out << ",\"status\":" << Quote(AurieStatusToString(status));
			if (AurieSuccess(status))
				out << ",\"result\":{" << NlGame::Describe(result) << "}";
			out << "}";
			out.flush();
		}
		out << "\n]}\n";
		out.close();

		const double end = Elapsed();
		g_Log(tag + " done seq " + std::to_string(Seq) + " t=" + Fixed(end, 1) + " took " + Fixed(end - Now, 1) + " at " + Clock());
		return true;
	}

	void Step(double Now)
	{
		Sample(Now);

		const std::string name = g_Schedule.Due(Now);
		if (name.empty())
			return;

		const bool last = !g_Schedule.Repeats();	// 되풀이하지 않으면 이 덤프가 마지막이다
		const bool ok = Run(name, g_Schedule.Count() + 1, Now, last);
		g_Schedule.Finished(Elapsed());
		if (!ok)
		{
			g_Finished = true;
			return;
		}
		if (last)
		{
			g_Finished = true;
			g_Log("dump done");
		}
	}
}

void NlDump::Init(const fs::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log)
{
	const fs::path request_path = ModuleDir / "NlToyBox.probe.txt";
	std::ifstream in(request_path);
	if (!in.is_open())
		return;

	g_Log = std::move(Log);
	g_Dir = ModuleDir;
	g_Version = Version;
	g_Start = std::chrono::steady_clock::now();
	g_Request = NlCore::ParseRequest(in);
	in.close();

	// 요청은 한 번만 쓴다. 지워 두면 이 실행이 어떻게 끝나든 다음 실행에서 덤프가 돌지 않는다.
	std::error_code ec;
	fs::remove(request_path, ec);
	g_Log(ec ? "request not removed: " + ec.message() : std::string("request consumed"));

	if (!g_Request.Errors.empty())
	{
		for (const std::string& error : g_Request.Errors)
			g_Log("request error: " + error);
		g_Log("dump failed: bad request");
		return;
	}

	g_Schedule = NlCore::Schedule(g_Request.DelaySeconds, g_Request.RepeatSeconds, g_Request.KeepLast);
	g_WatchLast.assign(g_Request.Watches.size(), "");
	g_Log("dump requested: delay " + std::to_string(g_Request.DelaySeconds) + "s, repeat " + Fixed(g_Request.RepeatSeconds, 0)
		+ "s, keep " + std::to_string(g_Request.KeepLast)
		+ ", find " + std::to_string(g_Request.FindValues.size() + g_Request.FindNames.size())
		+ ", scripts " + std::to_string(g_Request.Scripts.size()));
	g_Active = true;
}

void NlDump::Tick(CCode* Code)
{
	if (!g_Active.load(std::memory_order_relaxed) || g_Finished || g_Busy)
		return;

	const double now = Elapsed();

	if (g_Request.TraceEvents && Code && g_SeenCode.insert(Code).second)
	{
		char name[160];
		if (SafeCodeName(Code, name, sizeof(name)))
			Trace("event t=" + Fixed(now, 1) + " " + name, g_EventLines, k_MaxEventLines, "event");
	}

	if (now - g_LastSample < k_SampleSeconds)
		return;
	g_LastSample = now;

	g_Busy = true;
	Step(now);
	g_Busy = false;
}
```

- [ ] **Step 7: `src/ModuleMain.cpp`를 고친다**

(a) 머리의 포함 줄 `#include "Dump.hpp"` 아래에 한 줄을 더한다:

```cpp
#include "Game.hpp"
```

(b) 버전을 올린다: `constexpr const char* k_Version = "0.1.0";` → `"0.2.0"`

(c) `CodeCallback`의 몸통을 바꾼다:

```cpp
	// 오브젝트 이벤트 코드가 실행될 때마다 온다. 래퍼의 인자 가운데 코드 객체(세 번째)만 쓴다.
	void CodeCallback(FWCodeEvent& CodeContext)
	{
		ProbeOnce("object_call");
		NlDump::Tick(std::get<2>(CodeContext.Arguments()));
	}
```

(d) `ModuleInitialize`에서, `LogLine("yytk " + …);` 줄 아래 **콜백을 등록하기 전에** 덤프를 준비하고, 맨 끝의 옛 `NlDump::Init(…)` 호출과 그 위의 주석 한 줄을 지운다:

```cpp
	// 콜백보다 먼저 준비한다. 콜백은 등록하자마자 게임 스레드에서 오기 시작한다.
	// 요청 파일이 있을 때만 덤프를 준비한다 (스펙: 데이터 오버레이 §3.2, §3.7).
	NlGame::Init(g_Yytk);
	NlDump::Init(module_dir, k_Version, [](const std::string& Line) { LogLine(Line); });
```

- [ ] **Step 8: `CMakeLists.txt`의 `nltoybox`에 소스와 코어를 더한다**

`add_library(nltoybox SHARED` 블록을 이렇게 바꾼다:

```cmake
add_library(nltoybox SHARED
  src/ModuleMain.cpp
  src/Dump.cpp
  src/Finder.cpp
  src/Game.cpp
  "${YYTK_SHARED}/YYTK_Shared_Types.cpp"
)
```

그리고 `target_compile_definitions(nltoybox PRIVATE UNICODE _UNICODE)` 줄 아래에 한 줄을 더한다:

```cmake
target_link_libraries(nltoybox PRIVATE nlcore)
```

- [ ] **Step 9: 실행 1의 요청 파일 — `tools/probes/stage0b-run1.txt`**

```
# 단계 0b 실행 1 (기록 실행): 다섯 값을 바꾼 뒤, 메인 메뉴에서 한 번 덤프하고 그 뒤로 45초마다 덤프한다(마지막 3개만 남는다).
# 사용자가 메뉴 덤프 뒤에 새 게임을 시작하고, 게임 화면에서 2분쯤 둔 뒤 평소처럼 게임을 끈다.
# 조건을 미리 정하지 않는다. 무엇이 "게임 안"을 가르는지는 이 실행의 기록에서 뽑는다.
delay_seconds=60
repeat_seconds=45
keep_last=3
trace_events=1
# 메인 메뉴 덤프(단계 0)에 있던 상태 변수. 게임에 들어갈 때 바뀌는지 본다.
watch=global.__new_game_initializer.__is_active
watch=global.__new_game_initializer.__current_step
watch=global.__new_game_initializer.__step_index
watch=global.__game_load_operator.__main_menu_state
watch=global.__game_load_operator.__game_is_loading
watch=global.__game_load_operator.__game_load_type
# 바꾼 값: budget_money, initial_budget(양성 대조), group_cooldown_days.EPIDEMY, addiction_resist 의 population
find=2345
find=745
find=3391
find=3767
find_name=budget_money
find_name=initial_budget
find_name=dodge_base
find_name=production_cost
find_name=building_resources
find_name=building_duration_factor
find_name=product_count
find_name=fair_trade
find_name=group_cooldown_days
find_name=EPIDEMY
find_name=addiction_resist
```

- [ ] **Step 10: 빌드하고 세 시험 묶음을 돌린다**

Run:
```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1 2>&1 | Select-Object -Last 3
pwsh -NoProfile -File E:\NlToyBox\tools\test-native.ps1 | Select-Object -Last 1
py -3.14 -m unittest discover -s E:\NlToyBox\tools\re\tests 2>&1 | Select-Object -Last 1
pwsh -NoProfile -File E:\NlToyBox\tools\tests\safety.tests.ps1 2>&1 | Select-Object -Last 1
```
Expected: `build ok -> …NlToyBox.dll`(경고는 있어도 오류는 없다), `core tests: 17 passed`(새 요청 파일이 오류 없이 읽혔다), `OK`, `safety tests: 18 passed`.

빌드가 실패하면 오류의 원인을 찾아 고친다(superpowers:systematic-debugging). YYToolkit 헤더와 어긋난 호출이면 `external/YYToolkit/YYToolkit/source/YYTK/Shared/YYTK_Shared_Interface.hpp`와 `YYTK_Shared_Types.hpp`의 선언을 **읽고** 맞춘다. 기억으로 고치지 않는다. 고친 것과 본 선언의 위치를 원장에 `Ruling:`으로 적는다.

- [ ] **Step 11: 커밋**

```powershell
git -C E:\NlToyBox add -- CMakeLists.txt src tools/probes/stage0b-run1.txt
git -C E:\NlToyBox commit -m "feat(module): 상태를 기록하고 되풀이 덤프한다, 찾기를 넓힌다" -m "0.5초마다 룸 이름과 오브젝트의 인스턴스 수, 지정한 전역 값을 재서 바뀔 때마다 로그에 적고, 메인 메뉴에서 한 번, 그 뒤로 일정 간격으로 덤프한다. 찾기는 너비 우선으로 바꾸고 ds_map, ds_list, 인스턴스 변수까지 본다. ds 자가 점검으로 찾기가 실제로 보는지 확인한다. 요청 파일은 읽은 뒤 스스로 지운다." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: 실행 (게임 1회 + 예비 1회)

**Files:**
- Create(추적 안 함): `E:\NlToyBox\refs\runtime\stage0b-run1.menu.json`, `stage0b-run1.late<k>.json`, `stage0b-run1.probe.txt`, `backups\saves\<시각>\`, `backups\data\0.5588.9777.0\director_params.json`, `…\knowledge\technology\cultural_knowledge\addiction_resist.json`
- Create(예비 실행을 쓸 때만): `E:\NlToyBox\tools\probes\stage0b-run2.txt`

**Interfaces:**
- Consumes: Task 1~4의 전부, `setup-aurie.ps1`, `deploy.ps1`, `data-snapshot.ps1`, `data-edit.ps1`, `data-restore.ps1`, `restore-game.ps1`
- Produces: 덤프들과 로그. Task 6이 읽는다

- [ ] **Step 1: 시작 상태를 확인한다**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\game-status.ps1`
Expected: `패치 여부  : 바닐라`, `실행 여부  : 꺼져 있음`, `mods\      : (없음)`, 백업이 `exe 와 일치`. 다르면 멈추고 원인을 본다.

- [ ] **Step 2: 세이브 폴더의 사본을 뜬다**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\saves-backup.ps1`
Expected: `saves backup ok (<n>) -> E:\NlToyBox\backups\saves\<시각>`. 이 경로를 원장에 적는다(Step 9와 Task 6이 쓴다).

- [ ] **Step 3: 스냅샷을 뜨고 다섯 값을 바꾼다**

```powershell
& E:\NlToyBox\tools\data-snapshot.ps1 -Files debug_params.json, gameplay_variables.json, battle_params.json, director_params.json, 'knowledge\technology\cultural_knowledge\addiction_resist.json'
& E:\NlToyBox\tools\data-edit.ps1 -File debug_params.json       -Find '"budget_money": 2000'     -Replace '"budget_money": 2345'
& E:\NlToyBox\tools\data-edit.ps1 -File gameplay_variables.json -Find '"initial_budget": 700,'   -Replace '"initial_budget": 745,'
& E:\NlToyBox\tools\data-edit.ps1 -File battle_params.json      -Find '"battle_dodge_base": 20,' -Replace '"battle_dodge_base": 23,'
& E:\NlToyBox\tools\data-edit.ps1 -File director_params.json    -Find '"EPIDEMY": 7,'            -Replace '"EPIDEMY": 3391,'
& E:\NlToyBox\tools\data-edit.ps1 -File 'knowledge\technology\cultural_knowledge\addiction_resist.json' -Find '"population": 110' -Replace '"population": 3767'
```

Expected: 스냅샷은 앞의 셋이 이미 있어 건너뛰고 뒤의 둘을 새로 뜬다. `data-edit`는 다섯 번 모두 성공한다(찾는 글이 파일마다 정확히 한 번 나오는 것은 2026-10-04에 세어 확인했다). 하나라도 거부되면 멈춘다. 게임이 갱신됐을 수 있다.

- [ ] **Step 4: Aurie와 모듈을 놓는다**

```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\setup-aurie.ps1 | Select-Object -Last 1
pwsh -NoProfile -File E:\NlToyBox\tools\deploy.ps1 | Select-Object -Last 1
```

Expected: `setup ok -> …`와 `deployed -> …\mods\Aurie\NlToyBox.dll`.

- [ ] **Step 5: 사용자에게 알리고 게임을 켠다 (실행 1)**

사용자에게 이 글을 보낸다:

> 지금 게임을 켭니다. 이번에는 해 주실 일이 있습니다.
> 1. 메인 메뉴가 뜨면 **1분쯤 기다려 주세요.** 화면이 잠깐 멈췄다가 풀립니다(메뉴 덤프).
> 2. 그 뒤에 **새 게임을 기본 설정으로 시작**해 주세요. 튜토리얼은 받지 않습니다.
> 3. 게임 화면이 나오면 **2분쯤 그대로 두세요.** 그 사이 화면이 45초쯤마다 잠깐씩 멈춥니다(덤프).
> 4. **평소처럼 게임을 꺼 주세요.**
> 5. 끝나면 두 가지를 알려 주세요: 게임 화면이 나온 것이 끄기 몇 분 전쯤이었는지, 시작 금화가 얼마로 보였는지(파일에는 2345로 바꿔 두었습니다).
>
> 새 게임이 자동 저장을 만들면 Steam 클라우드에 올라갈 수 있습니다. 세이브 폴더의 사본은 떠 두었습니다.

그런 다음 **백그라운드로**(`run_in_background`, 제한 시간 30분) 돌린다:

```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\probe.ps1 -Request E:\NlToyBox\tools\probes\stage0b-run1.txt -Out E:\NlToyBox\refs\runtime\stage0b-run1.json -TimeoutSec 1500 -GraceSec 30 *>&1 | Tee-Object -FilePath E:\NlToyBox\refs\runtime\stage0b-run1.probe.txt; "probe exit=$LASTEXITCODE"
```

도는 동안 게임 폴더의 `mods\Aurie\NlToyBox.log`를 읽어 `dump menu done`이 보이면 사용자에게 한 줄을 보낸다: "메뉴 덤프가 끝났습니다. 이제 새 게임을 시작해 주세요." 로그에 `dump failed`가 보이거나 게임이 덤프 도중 꺼지면 도구가 실패로 끝난다. 그때는 Step 6의 표를 본다.

Expected: 끝줄 `PASS`, `probe exit=0`. `refs\runtime\stage0b-run1.menu.json`과 `stage0b-run1.late*.json`이 있다. 출력의 로그에 Phase 0의 줄(`NlToyBox 0.2.0 loaded`, `builtin code_is_compiled = true`, `script gml_Script_command_line_parameters_init = found`, `probe done`)과 `request consumed`, `sample: objects`, `objects 64`, `sample: room`, `sample: counts`, `sample: watch`, `sample: ok`, `dump menu done …`, `dump late… done …`이 있다.

- [ ] **Step 6: 어느 덤프가 게임 안의 것인지 정하고, 믿을 만한지 본다**

```powershell
Get-ChildItem -LiteralPath E:\NlToyBox\refs\runtime -Filter 'stage0b-run1.*.json' | ForEach-Object { py -3.14 E:\NlToyBox\tools\re\dump_tool.py summary $_.FullName | Select-Object -First 2 }
Select-String -LiteralPath E:\NlToyBox\refs\runtime\stage0b-run1.probe.txt -Pattern '^(state|watch|objects|dump|request) |사라졌습니다|메뉴 덤프가 끝났습니다' | ForEach-Object { $_.Line }
```

사용자가 알려 준 "게임 화면이 나온 때"(끈 때에서 거꾸로 센다. 끈 때는 `게임 프로세스가 사라졌습니다 (HH:mm:ss)` 줄에 있다)와 `dump late… done … at HH:MM:SS` 줄을 맞춰, **게임 화면이 나온 뒤에 시작한 덤프 가운데 마지막 것**을 고른다. 그것을 `<late>`라 부른다. 사용자의 말과 상태 기록(그 무렵의 `state` 줄)이 어긋나면 어긋났다고 적고 사용자에게 다시 묻는다. 짐작으로 고르지 않는다.

```powershell
py -3.14 E:\NlToyBox\tools\re\dump_tool.py controls E:\NlToyBox\refs\runtime\stage0b-run1.menu.json global.__gameplay_vars.global_map_ai_economy_initial_budget=745
py -3.14 E:\NlToyBox\tools\re\dump_tool.py controls E:\NlToyBox\refs\runtime\stage0b-run1.<late>.json global.__gameplay_vars.global_map_ai_economy_initial_budget=745
```

Expected: 두 `controls` 모두 줄마다 `ok`이고 종료 코드 0.

판정:

| 본 것 | 뜻 | 다음 |
|---|---|---|
| 게임 화면이 나온 뒤에 시작한 덤프가 있고 `controls`가 모두 `ok` | 게임 안의 덤프가 있고 믿을 만하다 | Step 8 |
| 게임 화면이 나온 뒤에 시작한 덤프가 없다(너무 빨리 껐다, 덤프가 오래 걸렸다) | 게임 안의 덤프가 없다 | 로그의 `took`으로 덤프에 걸린 시간을 보고, 게임 화면에서 기다릴 시간을 다시 정해 Step 7 |
| `expect …initial_budget` 실패 | 찾기가 알려진 값을 못 찾았다. 찾기가 고장이다 | 원인을 찾는다. 요청으로 못 고치면 멈추고 사용자에게 알린다 |
| `selfcheck` 실패 | ds 안을 보지 못했다(형 상수가 이 러너에서 다르거나 빌트인이 실패했다) | ds에 대한 "없다"는 "확인하지 못함"으로 적는다. Step 8 |
| `ds_range` 실패 | ds 번호가 본 범위의 끝에 닿았다 | ds에 대한 "없다"는 "확인하지 못함"으로 적는다. Step 8 |
| `truncated` 실패 | 한도에 걸렸다 | 그 구역의 "없다"는 "확인하지 못함"으로 적는다. 요청의 `max_visited`를 올려 Step 7을 쓸지 정한다 |
| `hits_cut` 실패 | 맞은 것을 다 적지 못했다(이름으로 맞은 것이 많다) | 그 구역의 "없다"는 "확인하지 못함"이다. 로그의 `matches`를 보고 요청의 `max_hits`를 올리거나 `find_name`을 좁혀 Step 7 |
| `enumeration` 실패 | 멤버를 다 보지 못한 구조체가 있다 | 그 구역의 "없다"는 "확인하지 못함"으로 적는다. 요청으로는 못 고친다. Step 8 |
| 로그가 `sample: …` 줄에서 끊기고 게임이 죽었다 | 첫 표본의 그 호출이 게임을 죽였다 | `sample: room`이면 `skip=room`으로 빼고 Step 7. `objects`·`counts`·`watch`면 요청으로 못 고친다(`watch`는 `watch=` 줄을 빼면 된다). 멈추고 사용자에게 알린다 |
| 게임이 덤프 도중 죽었다(로그가 어느 구역 줄에서 끊김) | 그 구역의 호출이 게임을 죽였다 | 끊긴 구역을 `skip=`으로 빼고 Step 7 |
| 로그에 `state … room=?`만 있다 | 룸 이름을 읽지 못했다 | 질문 6에서 룸은 "확인하지 못함"으로 적는다. 다른 것은 그대로 쓴다 |

- [ ] **Step 7: 예비 실행 (필요할 때만)**

Step 6의 표가 여기로 보냈을 때만 한다. `tools/probes/stage0b-run1.txt`를 `stage0b-run2.txt`로 복사해 **요청만** 고친다(`repeat_seconds`, `keep_last`, `skip=`, `trace_events`, `watch=`, `find_name=`, 한도 `max_*`). 무엇을 왜 고쳤는지, 그 근거가 된 로그 줄을 원장에 `Ruling:`으로 적는다. 모듈 코드를 고쳐야 하면 예비 실행을 쓰지 말고 멈춰서 사용자에게 알린다.

```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\test-native.ps1 | Select-Object -Last 1
```

Expected: `core tests: 18 passed`(새 요청 파일이 오류 없이 읽힌다).

사용자에게 Step 5의 글을 다시 보내고(달라진 점이 있으면 덧붙인다), Step 5의 명령에서 `stage0b-run1`을 `stage0b-run2`로 바꿔 돌린다. 끝나면 Step 6을 `stage0b-run2`로 다시 한다. 이번에도 어긋나면 더 켜지 않는다. 얻은 것까지로 Task 6을 쓴다.

- [ ] **Step 8: 질문의 답을 뽑는다**

(`<run>`은 쓰기로 한 실행, `<late>`는 Step 6에서 고른 덤프다.)

```powershell
$m = 'E:\NlToyBox\refs\runtime\<run>.menu.json'; $g = 'E:\NlToyBox\refs\runtime\<run>.<late>.json'
py -3.14 E:\NlToyBox\tools\re\dump_tool.py summary $m | Select-Object -First 12
py -3.14 E:\NlToyBox\tools\re\dump_tool.py summary $g | Select-Object -First 12
py -3.14 E:\NlToyBox\tools\re\dump_tool.py hits $m budget_money dodge_base EPIDEMY group_cooldown addiction_resist production_cost building_resources building_duration product_count fair_trade
py -3.14 E:\NlToyBox\tools\re\dump_tool.py hits $g budget_money dodge_base EPIDEMY group_cooldown addiction_resist production_cost building_resources building_duration product_count fair_trade
py -3.14 E:\NlToyBox\tools\re\dump_tool.py diff $m $g | Select-Object -First 80
```

값으로 맞은 것(`[value]`)은 경로의 이름으로 우연한 일치를 가린다(단계 0에서 745가 소리 배열에, 2345가 스프라이트 배열에 있었다). 메뉴 덤프의 같은 경로와 비교한다.

파일마다 이렇게 판정한다:

| 답 | 조건 |
|---|---|
| 그렇다 | 바꾼 값이 그 키에 대응하는 이름의 경로에 있다. 경로와 값을 적는다 |
| 아니다 | 그 키의 이름이 런타임에 있는데 값이 **바꾸기 전 값**이다(코드의 기본값이거나 다른 출처다) |
| 아니다 (찾은 범위에서) | Step 6의 `controls`가 모두 통과했고, 값으로도 이름으로도 없다. 범위의 한계(길이 64를 넘는 배열·ds_list, 키가 2만을 넘는 ds_map, ds_grid, 깊이 6, 열거가 끊기는지 모름)를 함께 적는다 |
| 확인하지 못함 | `controls` 가운데 하나라도 어긋났거나, 게임 안의 덤프가 없다 |

질문 5(언제 읽히는가)는 같은 경로가 메뉴 덤프에도 있는지로 답한다.
질문 6(게임 안을 무엇으로 알아볼 수 있는가)은 사용자가 알려 준 "게임 화면이 나온 때" 앞뒤의 `state`·`watch`·`event` 줄에서, 그 무렵에 생기고 사라진 것으로 답한다. 메인 메뉴에서도 참인 것은 신호가 아니다. 후보마다 "메뉴에서는 어땠고 게임 안에서는 어땠다"를 적는다.

- [ ] **Step 9: 세이브 폴더에 생긴 것을 본다**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\saves-backup.ps1 -Diff <Step 2의 경로>`
Expected: `새로 생김: …` / `바뀜: …` 줄과 `saves diff (<n>)`. 이 출력을 원장에 그대로 적는다. **아무것도 지우지 않는다.**

- [ ] **Step 10: 되돌린다**

먼저 복사하지 못한 덤프가 게임 폴더에 남았는지 본다. `restore-game.ps1`은 그것을 지운다.

```powershell
Get-ChildItem -LiteralPath 'E:\SteamLibrary\steamapps\common\Norland Story Generating Strategy\mods\Aurie' -Filter 'NlToyBox.dump*.json' -ErrorAction SilentlyContinue | ForEach-Object { "{0}  {1} B" -f $_.Name, $_.Length }
```

Expected: 출력이 없다. 있으면 `refs\runtime\`로 직접 복사한 뒤에 다음으로 간다.

```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\data-restore.ps1;  "data-restore exit=$LASTEXITCODE"
pwsh -NoProfile -File E:\NlToyBox\tools\restore-game.ps1;  "restore-game exit=$LASTEXITCODE"
pwsh -NoProfile -File E:\NlToyBox\tools\game-status.ps1
```

Expected: `data restore ok (5)`, `restore ok`, 그리고 `패치 여부  : 바닐라`, `mods\      : (없음)`, 백업이 `exe 와 일치`.

스냅샷과 게임 파일이 같은지 확인한다:

```powershell
$root = 'E:\NlToyBox\backups\data\0.5588.9777.0'
$game = 'E:\SteamLibrary\steamapps\common\Norland Story Generating Strategy'
Get-ChildItem -LiteralPath $root -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($root.Length + 1)
    $same = (Get-FileHash -LiteralPath $_.FullName).Hash -eq (Get-FileHash -LiteralPath (Join-Path $game $rel)).Hash
    "{0}  {1}" -f $(if ($same) { 'same' } else { 'DIFF' }), $rel
}
Test-Path -LiteralPath (Join-Path $game 'aurie.log')
```

Expected: 다섯 줄 모두 `same`, 마지막 줄 `False`.

이 Task에는 커밋이 없다(산출물이 모두 추적하지 않는 폴더에 있다. 예비 실행을 썼으면 `tools/probes/stage0b-run2.txt`를 Task 6에서 함께 커밋한다).

---

### Task 6: 결과 문서, 병합

**Files:**
- Create: `E:\NlToyBox\research\02-new-game-state.md`
- Modify: `E:\NlToyBox\docs\superpowers\specs\2026-10-04-data-overlay-design.md` (머리의 상태 줄, §4 첫 문단)
- Modify: `E:\NlToyBox\research\01-data-overlay.md` (머리에 한 줄)
- Modify: `E:\NlToyBox\CLAUDE.md`

**Interfaces:**
- Consumes: Task 5의 덤프, 로그, 원장의 기록
- Produces: 질문 4~6의 답. 단계 1의 카탈로그 범위와, 감지 로직을 만들 다음 계획의 근거

- [ ] **Step 1: `research/02-new-game-state.md`를 쓴다**

덤프와 로그에서 읽은 것만 적는다. 추정은 추정이라고 적고 결론에 쓰지 않는다. 다음 절을 이 순서로 둔다.

1. **머리.** 조사일, 대상 버전, 게임을 켠 횟수, 쓴 실행과 덤프, 덤프의 위치(`refs/runtime/…`, 추적 안 함), 다시 재는 법(`tools/probe.ps1 -Request tools/probes/stage0b-run1.txt`).
2. **답.** 표: 질문 4를 파일 다섯 개로 나눈 다섯 줄, 질문 5, 질문 6. 열은 "질문 / 답 / 근거(경로와 값)". 답은 Task 5 Step 8의 표에 있는 네 가지 가운데 하나다.
3. **어느 덤프를 게임 안의 것으로 봤는가.** 사용자가 알려 준 때, 덤프의 시각, 그 무렵의 상태 기록. 셋이 맞았는지.
4. **확인의 결과.** `controls`의 출력을 메뉴 덤프와 고른 덤프 모두 그대로 옮긴다. 어긋난 것이 있으면 그것이 어느 답을 "확인하지 못함"으로 만들었는지 적는다.
5. **게임 안을 알아볼 수 있는 신호.** 룸 이름이 바뀌었는가, 어떤 오브젝트가 생기고 사라졌는가, `watch`의 값이 어떻게 바뀌었는가, 처음 본 이벤트 가운데 게임에 들어간 뒤의 것. 후보마다 메뉴와 게임 안의 값을 나란히 적는다. 새 게임 설정 화면에서의 상태도 적는다(메뉴와 게임 사이에서 헷갈릴 수 있는 것).
6. **파일 키와 런타임 경로의 대응.** 반영이 확인된 키마다 파일의 경로 → 런타임의 경로. 값이 ds_map에 있으면 번호가 실행마다 같은지는 모른다고 적는다.
7. **찾기가 본 범위.** 두 덤프의 `visited`, `containers`, `depth_cut`, `array_skipped`, `maps`, `lists`, `highest_map`, `highest_list`, `skipped_large`, 인스턴스 수. 보지 않는 것(길이 64를 넘는 배열과 ds_list, 키가 2만을 넘는 ds_map, ds_grid·stack·queue·priority, 오브젝트마다 16개를 넘는 인스턴스, 깊이 6 아래)과 알 수 없는 것(멤버 열거가 중간에 끊기는지).
8. **이번에 처음 잰 것.** 덤프에 걸린 시간(로그의 `took`), 형 상수 1·2가 이 러너에서 맞았는지(자가 점검), 룸 이름을 읽었는지, 코드 이름을 읽었는지(`event` 줄이 있는지), 새로 쓴 빌트인이 모두 동작했는지, 사용자가 본 시작 금화. 스펙 §3.7의 "모른다" 항목마다 결과를 적는다.
9. **세이브 폴더.** Task 5 Step 9의 출력. 지우지 않았다는 것과 사본의 위치. 게임의 로그(`catched_errors_…txt`)에 이번 실행의 `Save game:` 줄이 있는지.
10. **단계 1에 주는 결론.** 카탈로그에 올릴 파일과 올리지 않을 파일. 값이 새 게임에서 읽히는 파일은 "프리셋을 입힌 뒤 새 게임부터 적용"이라고 적는다.
11. **확인하지 못한 것.**

- [ ] **Step 2: 스펙과 앞 문서를 고친다**

- `docs/superpowers/specs/2026-10-04-data-overlay-design.md`
  - 머리의 `- 상태: …` 줄을 `- 상태: 단계 0, 0b 완료(\`research/01-data-overlay.md\`, \`research/02-new-game-state.md\`). 단계 1은 계획 대기`로 바꾼다.
  - §3.7의 "모른다" 항목 가운데 이번에 잰 것은 근거 표로 옮기고 출처를 `research/02-new-game-state.md`로 적는다.
  - §4의 첫 문단 끝 문장 "다른 파일은 새 게임을 시작한 상태에서 반영을 잰 뒤에 카탈로그에 올린다."를 Step 1의 10번 결론 한두 문장으로 바꾼다.
- `research/01-data-overlay.md`의 첫 문단 아래에 한 줄: `새 게임 상태에서 다시 잰 결과는 \`research/02-new-game-state.md\`에 있다. 이 문서의 "확인하지 못함"은 그쪽을 본다.`

- [ ] **Step 3: `CLAUDE.md`를 고친다**

(a) "브랜치 전략"의 CI 줄을 바꾼다:

```
- CI가 없다. 코드가 바뀌면 머지 전에 `tools/build.ps1` 성공, `tools/test-native.ps1`·
  `tools/tests/safety.tests.ps1` 통과, `py -3.14 -m unittest discover -s tools/re/tests` 통과,
  `tools/check-load.ps1` 종료 코드 0을 확인한다. 문서만 바뀌면 생략해도 된다.
  앞의 셋은 게임을 켜지 않는다.
```

(b) "개발 루프"의 명령 블록에 한 줄을 더한다(`build.ps1` 줄 아래):

```
    pwsh -File tools/test-native.ps1   # src/core 의 시험. 게임을 켜지 않는다
```

(c) "모듈을 쓸 때"에 줄을 더한다:

```
- 러너에 기대지 않는 로직은 `src/core/`에 두고 `tests/native/`에서 시험한다. 러너에 닿는 호출은
  `src/Game.cpp`를 거친다. 빌트인과 문서·소스·실행으로 확인한 인터페이스만 쓴다. `GetInstanceObject`와
  `CRoom`은 러너 내부 구조체의 배치에 기대므로 쓰지 않는다. 없는 이름으로 부르는 `GetInstanceMember`와
  `GetInstanceMemberCount`는 확인하지 못한 빌트인을 구조체에 대고 부르므로 쓰지 않는다(이름은 열거로 찾는다).
- 정적 저장 기간의 `RValue`를 두지 않는다. 함수 안에서만 든다.
- 빌트인을 새로 쓸 때는 먼저 이름이 `refs/exe_strings.txt`에 있는지, 인자와 반환값이 매뉴얼
  (Context7 `/yoyogames/gamemaker-manual`)에 어떻게 적혀 있는지 확인한다. GML 상수는 C++에서 이름으로 쓸 수
  없으니 값의 출처를 주석에 적는다.
```

Step 1의 5번에서 실측으로 나온 신호가 있으면 한 줄을 더한다: `- 게임 안인지는 <실측한 신호>로 안다 (\`research/02-new-game-state.md\`).` 신호가 나오지 않았으면 이 줄을 넣지 않는다.

(d) "게임에 가하는 변경"의 `tools/probe.ps1` 줄을 바꾸고 한 줄을 더한다:

```
- `tools/probe.ps1`은 요청 파일(`tools/probes/*.txt`)을 놓고 게임을 켜서 런타임 덤프를 받아 온다.
  요청에 `repeat_seconds`가 있으면 메인 메뉴에서 한 번, 그 뒤로 일정 간격으로 덤프하고, 사용자가 새 게임을
  시작해 잠시 둔 뒤 직접 게임을 끈다. 덤프는 `refs\runtime\`에 둔다(추적 안 함). 분석은
  `py -3.14 tools/re/dump_tool.py`, "없다"를 믿어도 되는지는 `dump_tool.py controls`로 본다.
  요청 파일을 고치면 `tools/test-native.ps1`로 읽어 본다.
- 새 게임을 시작하는 실행 전에는 `tools/saves-backup.ps1`으로 세이브 폴더의 사본을 뜬다. 도구는 세이브 폴더에
  쓰지 않는다. 게임이 만든 세이브를 지우는 것은 사용자가 정한다.
```

(e) "작업 규칙"의 첫 줄을 바꾼다:

```
- 추측으로 작업하지 않는다. 계획과 코드에 쓰는 사실은 실측(로그·해시·덤프·게임 파일), 공식 문서, 서브모듈 소스로
  확인하고 출처를 적는다. 확인하지 못한 것은 "모른다"로 두고 먼저 그것을 재는 단계를 만든다.
  추정을 결론처럼 쓰지 않는다.
```

- [ ] **Step 4: 커밋 전 확인과 커밋**

```powershell
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"
git -C E:\NlToyBox status --short
```

Expected: 첫 명령의 출력이 비어 있다. 둘째 명령에는 `research/02-new-game-state.md`, 스펙, `research/01-data-overlay.md`, `CLAUDE.md`(예비 실행을 썼으면 `tools/probes/stage0b-run2.txt`)만 있다.

```powershell
git -C E:\NlToyBox add -- research docs/superpowers/specs/2026-10-04-data-overlay-design.md CLAUDE.md tools/probes
git -C E:\NlToyBox commit -m "docs(research): 새 게임 상태에서 잰 결과" -m "질문 4~6 의 답, 확인의 결과, 게임 안을 알아볼 수 있는 신호, 찾기가 본 범위를 적는다. CLAUDE.md 에 네이티브 시험과 세이브 사본, 러너에 닿는 호출과 근거 확인의 규칙을 더한다." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 5: 병합 전 확인**

```powershell
pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1 2>&1 | Select-Object -Last 1
pwsh -NoProfile -File E:\NlToyBox\tools\test-native.ps1 | Select-Object -Last 1
py -3.14 -m unittest discover -s E:\NlToyBox\tools\re\tests 2>&1 | Select-Object -Last 1
pwsh -NoProfile -File E:\NlToyBox\tools\tests\safety.tests.ps1 2>&1 | Select-Object -Last 1
```

Expected: `build ok -> …`, `core tests: 18 passed`, `OK`, `safety tests: 19 passed`.

`tools/check-load.ps1`은 따로 돌리지 않는다. 게임 실행 횟수를 아끼려는 것이고, 실행 1의 로그에 Phase 0의 판정 줄이 모두 있었던 것(Task 5 Step 5)으로 갈음한다. 그 줄이 없었으면 병합하지 않고 사용자에게 알린다.

- [ ] **Step 6: 병합한다**

최종 검토(superpowers:executing-plans의 Final Review)와 그 수정이 끝난 뒤에 한다.

```powershell
git -C E:\NlToyBox switch develop
git -C E:\NlToyBox merge --no-ff feat/overlay-stage0b -m "Merge branch 'feat/overlay-stage0b' into develop" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git -C E:\NlToyBox branch -d feat/overlay-stage0b
git -C E:\NlToyBox status --short
```

Expected: 병합 커밋이 생기고 마지막 명령의 출력이 비어 있다.

- [ ] **Step 7: 사용자에게 보고한다**

질문 4~6의 답, 세이브 폴더에 생긴 파일과 그 경로(지울지는 사용자가 정한다), 게임이 바닐라라는 것, 쓴 게임 실행 횟수, 그리고 다음 단계를 제안한다:

- 게임 안을 가르는 신호가 나왔다 → 그 신호로 "게임에 들어간 것을 알아보는" 로직의 계획을 쓴다(한 번 켜서 확인).
- 반영되는 파일이 늘었다 → 그 파일들을 넣어 단계 1(오버레이 도구)의 계획을 쓴다.
- `gameplay_variables.json`만 반영된다 → 단계 1을 94개 키로 하고, MVP의 금화·건설 비용·전투는 하위 프로젝트 2(런타임 접근)로 옮긴다.
- 확인하지 못한 것이 남았다 → 무엇이 막았는지와, 그것을 풀려면 게임 실행이 몇 번 더 필요한지 적는다.
