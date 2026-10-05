# 치트 메뉴 3단계 가 — 원격 질의와 호출 기록기 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 켜져 있는 게임에 도구가 파일로 묻고, 쓰고, 화면을 뜨고, 스크립트의 호출을 기록할 수 있게 한다. 이것으로 게임 화면에서 금화·창고·건설 비용의 자리를 사용자 대신 찾는다(3단계 나).

**Architecture:** 모듈이 0.25초마다 `NlToyBox.ask.txt`를 보고, 있으면 줄들을 게임 스레드에서 실행해 답을 `NlToyBox.answer.txt`에 이어 쓴다. 읽고 쓰는 일은 2단계의 접근 층(`NlAccess`, `NlSearch`)이 한다. 호출 기록기는 스크립트의 함수에 `MmCreateHook`으로 훅을 걸어 인자와 반환값을 적는다. 줄을 읽는 일과 기록을 모으는 일은 `src/core`에 두고 네이티브 시험으로 고정한다.

**Tech Stack:** C++ (`/std:c++latest`, MSVC), Aurie v2.0.2 (`MmCreateHook`) + YYToolkit v5.0.0c (`GetNamedRoutineIndex`, `GetScriptData`, `CallGameScriptEx`), PowerShell 7.

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §3(수단 C), §10(3가), §14

## Global Constraints

- 작업 브랜치는 `feat/cheat-remote`. `main`·`develop`에 직접 커밋하지 않는다. 끝나면 `git -C E:\NlToyBox merge --no-ff`로 `develop`에 넣는다.
- git 명령은 `git -C E:\NlToyBox`. `refs/`, `backups/`, `downloads/`, `build/`를 커밋하지 않는다. 스크립트는 `pwsh`, Python 은 `py -3.14`.
- 러너를 건드리는 호출은 게임 스레드의 틱에서만 한다. `RValue`를 정적 저장 기간이나 틱 너머로 두지 않는다.
- 빌트인에 넘기는 수는 `double`로 만든다. ds 함수는 `ds_exists`가 참인 번호에만 부른다. 없는 것을 만들지 않는다.
- **게임 스크립트는 인자의 수와 형을 기록이나 실측으로 확인한 것만 부른다.** 이 계획이 부르는 스크립트는 `gml_Script_budget_default_money_get` 하나다:
  인자 없이 메인 메뉴에서 불러 `undefined`를 받았다(`research/01-data-overlay.md`).
- 한 번 건 훅은 떼지 않는다(그 함수가 호출 스택에 있을 때 떼면 죽는다). 기록만 멈춘다.
- 훅 안(`Handle`)에서는 빌트인을 부르지 않는다. `RValue`의 멤버 함수만 쓴다.
- 서브모듈(`external/`) 안의 파일을 고치지 않는다. 도구와 시험은 소리를 내지 않는다.
- 게임을 켜는 것은 Task 7 의 실행 묶음 하나다. 켜기 전에 사용자에게 알리고 승인을 받는다.
- 모듈 버전은 `0.5.0`.

## Review Focus

1. 답 파일의 한 줄이 주소에 든 공백 때문에 잘못 갈린다 → 공백은 중괄호 밖에서만 가른다. Task 1 "줄을 읽는다"(`map:128@{messenger_cost }`).
2. `write`의 주소에 `=`가 들어 있다 → 마지막 `=`에서 가른다. Task 1 (`map:1@{a=b}=-4.5`).
3. 자주 불리는 함수를 기록하면 글을 만드는 비용으로 게임이 느려진다 → 처음 몇 개와 처음 보는 꼴만 글로 남긴다. Task 2 "처음 몇 개와 처음 보는 꼴만".
4. `shot`의 이름이 파일 이름이 된다 → 영문·숫자·`_`·`-`만 받는다. Task 1 "읽을 수 없으면 오류"(`shot ../x`).
5. 앞선 실행 묶음이 죽어 사용자의 설정 사본(`*.kept`)이 남았다 → `session.ps1 start`가 거부하고 `stop`이 되돌린다. Task 6 의 안전 시험.

러너에 닿는 쪽(훅이 걸리는가, 메서드의 스크립트 이름, `method_call`)은 Task 7 의 실행이 잰다.

## File Structure

| 파일 | 하는 일 | Task |
|---|---|---|
| `src/core/RemoteCommand.{hpp,cpp}` | 묻는 파일의 한 줄을 읽는다 | 1 |
| `src/core/CallLog.{hpp,cpp}` | 호출 기록(세기, 표본, 꼴) | 2 |
| `src/Recorder.{hpp,cpp}` | 스크립트의 함수에 훅을 걸어 호출을 기록한다 | 3 |
| `src/Ui.{hpp,cpp}` | 화면 뜨기를 청할 수 있게 하고, 창을 열고 닫는다 | 4 |
| `src/Remote.{hpp,cpp}`, `src/ModuleMain.cpp` | 묻는 파일을 보고 명령을 실행해 답을 쓴다 | 5 |
| `tools/session.ps1`, `tools/ask.ps1`, `tools/tests/safety.tests.ps1` | 실행 묶음과 묻기 | 6 |
| `research/07-remote.md`, `CLAUDE.md`, `README.md` | 잰 것과 쓰는 법 | 7 |

빌드와 시험:

    pwsh -File tools/build.ps1          # 끝에 "build ok -> ...\build\NlToyBox.dll"
    pwsh -File tools/test-native.ps1    # 끝에 "core tests: N passed", 종료 코드 0

---

### Task 1: 묻는 파일의 한 줄을 읽는다

**Files:**
- Create: `src/core/RemoteCommand.hpp`, `src/core/RemoteCommand.cpp`
- Modify: `CMakeLists.txt` (`nlcore`에 `src/core/RemoteCommand.cpp`)
- Test: `tests/native/core_tests.cpp`

**Interfaces:**
- Consumes: `NlCore::ParseAskPath` (`AskPath.hpp`), `Trim`, `ParseNumber` (`Text.hpp`)
- Produces:
  - `struct RemoteArg { char Kind; double Number; std::string Text; }` — `'n'` 수, `'s'` 글, `'b'` 불리언, `'u'` undefined, `'p'` 주소의 값
  - `struct RemoteCommand { std::string Verb, Target; double Number; std::map<std::string, std::string> Options; std::vector<RemoteArg> Args; std::string Error; }`
  - `RemoteCommand ParseRemoteLine(const std::string& Line)` — 빈 줄과 `#` 줄은 `Verb`가 빈 채로(오류 아님)
  - `double OptionNumber(const RemoteCommand& Command, const std::string& Key, double Fallback)`

- [ ] **Step 1: 실패하는 시험을 쓴다**

`tests/native/core_tests.cpp`의 include 에 `#include "core/RemoteCommand.hpp"`를 더하고(`Rate.hpp` 다음), `// 요청 파일의 오타로 게임 실행 한 번을 버리지 않는다.` 줄 앞에 넣는다.

```cpp
	Test("원격 명령: 줄을 읽는다", [] {
		CHECK(ParseRemoteLine("").Verb.empty() && ParseRemoteLine("# 주석").Verb.empty() && ParseRemoteLine("# 주석").Error.empty());
		const RemoteCommand ask = ParseRemoteLine("  ask inst:o_debug.is_x ");
		CHECK(ask.Error.empty() && ask.Verb == "ask" && ask.Target == "inst:o_debug.is_x");
		const RemoteCommand list = ParseRemoteLine("list inst:o_production_manager.ppm_map_of_process as=map max=50");
		CHECK(list.Error.empty() && list.Options.at("as") == "map" && OptionNumber(list, "max", 400) == 50 && OptionNumber(list, "depth", 2) == 2);
		const RemoteCommand tree = ParseRemoteLine("tree map:128@{messenger_cost } depth=3");
		CHECK(tree.Error.empty() && tree.Target == "map:128@{messenger_cost }" && OptionNumber(tree, "depth", 2) == 3);
		const RemoteCommand find = ParseRemoteLine("find name=gold value=3000 in=global,inst");
		CHECK(find.Error.empty() && find.Options.at("name") == "gold" && OptionNumber(find, "value", 0) == 3000 && find.Options.at("in") == "global,inst");
		const RemoteCommand refine = ParseRemoteLine("refine value=2950");
		CHECK(refine.Error.empty() && refine.Number == 2950);
		const RemoteCommand write = ParseRemoteLine("write map:1@{a=b}=-4.5");
		CHECK(write.Error.empty() && write.Target == "map:1@{a=b}" && write.Number == -4.5);
		CHECK(ParseRemoteLine("poke inst:o_debug.x=1").Verb == "poke" && ParseRemoteLine("state").Error.empty());
		CHECK(ParseRemoteLine("record gml_Script_budget_money_get").Target == "gml_Script_budget_money_get");
		CHECK(ParseRemoteLine("records").Error.empty() && ParseRemoteLine("unrecord all").Target == "all");
		CHECK(ParseRemoteLine("shot hud-1").Target == "hud-1" && ParseRemoteLine("window close").Target == "close");
	});

	Test("원격 명령: 부르는 인자를 읽는다", [] {
		const RemoteCommand call = ParseRemoteLine("call gml_Script_x n:-3.5 s:wood s:{two words} b:1 u p:inst:o_building:1");
		CHECK(call.Error.empty() && call.Target == "gml_Script_x" && call.Args.size() == 6);
		CHECK(call.Args[0].Kind == 'n' && call.Args[0].Number == -3.5 && call.Args[1].Kind == 's' && call.Args[1].Text == "wood");
		CHECK(call.Args[2].Text == "two words" && call.Args[3].Kind == 'b' && call.Args[3].Number == 1 && call.Args[4].Kind == 'u');
		CHECK(call.Args[5].Kind == 'p' && call.Args[5].Text == "inst:o_building:1");
		const RemoteCommand method = ParseRemoteLine("method inst:o_building.get_level");
		CHECK(method.Error.empty() && method.Target == "inst:o_building.get_level" && method.Args.empty());
	});

	Test("원격 명령: 읽을 수 없으면 오류를 낸다", [] {
		for (const char* line : { "dance", "ask", "ask o_debug.x", "ask global.a global.b", "list", "list global.a max", "find", "find value=abc",
			"refine", "refine value=x", "write inst:o_debug.x", "write inst:o_debug=3", "write inst:o_debug.x=abc", "poke global=1",
			"record", "record a b", "shot", "shot ../x", "window", "window maybe", "call", "call x n:abc", "call x q:1", "call x b:2",
			"call x p:nowhere.x", "method global", "method o_x.y", "state now" })
		{
			if (ParseRemoteLine(line).Error.empty())
			{
				std::printf("  FAIL accepted: %s\n", line);
				g_Failed++;
			}
		}
	});

```

- [ ] **Step 2: 빌드해서 실패를 본다**

Run: `pwsh -File tools/build.ps1`
Expected: `Cannot open include file: 'core/RemoteCommand.hpp'`

- [ ] **Step 3: 구현한다**

`src/core/RemoteCommand.hpp`:

```cpp
#pragma once
// 켜져 있는 게임에 묻는 파일(NlToyBox.ask.txt)의 한 줄. 러너에 기대지 않는다. 스펙: 치트 메뉴 §14.
//   ask <주소>                                      값 하나
//   list <주소> [as=map|list] [max=N]               그릇의 자식들
//   tree <주소> [depth=N] [max=N]                   자식들을 깊이 N 까지
//   find [name=글] [value=수] [in=global,inst,ds]   이름·값으로 찾기
//   refine value=수                                 앞의 find 결과 가운데 지금 값이 그 수인 것만
//   write <주소>=<수>                               써 넣는다(남긴다)
//   poke <주소>=<수>                                써 넣고, 읽고, 되돌린다
//   record <스크립트 이름|메서드의 주소>            그 함수의 호출을 기록한다
//   unrecord <스크립트 이름|all>,  records [스크립트 이름]
//   call <스크립트 이름> [인자…]                    게임 스크립트를 부른다
//   method <메서드의 주소> [인자…]                  메서드를 그것이 묶인 구조체에서 부른다
//   state,  shot <이름>,  window open|close
// 인자: n:<수>  s:<글> 또는 s:{공백이 든 글}  b:0|1  u(undefined)  p:<주소>(그 주소의 값)

#include <map>
#include <string>
#include <vector>

namespace NlCore
{
	struct RemoteArg
	{
		char Kind = 0;			// 'n' 수, 's' 글, 'b' 불리언, 'u' undefined, 'p' 주소의 값
		double Number = 0;
		std::string Text;
	};

	struct RemoteCommand
	{
		std::string Verb;								// 비어 있으면 빈 줄이나 주석이다
		std::string Target;								// 주소, 스크립트 이름, 화면 이름, open|close
		double Number = 0;								// write, poke, refine 의 수
		std::map<std::string, std::string> Options;		// key=value 들
		std::vector<RemoteArg> Args;					// call, method 의 인자
		std::string Error;								// 비어 있지 않으면 읽지 못했다
	};

	RemoteCommand ParseRemoteLine(const std::string& Line);

	// 옵션의 수. 없거나 수가 아니면 Fallback.
	double OptionNumber(const RemoteCommand& Command, const std::string& Key, double Fallback);
}
```

`src/core/RemoteCommand.cpp`:

```cpp
#include "RemoteCommand.hpp"

#include "AskPath.hpp"
#include "Text.hpp"

namespace NlCore
{
	namespace
	{
		// 공백에서 가른다. 중괄호 안의 공백은 가르지 않는다(주소의 @{키}, s:{글}).
		std::vector<std::string> Split(const std::string& Line)
		{
			std::vector<std::string> tokens;
			std::string token;
			int depth = 0;
			for (const char c : Line)
			{
				if (c == '{')
					depth++;
				else if (c == '}' && depth > 0)
					depth--;
				if ((c == ' ' || c == '\t') && depth == 0)
				{
					if (!token.empty())
						tokens.push_back(token);
					token.clear();
					continue;
				}
				token += c;
			}
			if (!token.empty())
				tokens.push_back(token);
			return tokens;
		}

		bool GoodPath(const std::string& Text, bool NeedSteps)
		{
			const AskPath path = ParseAskPath(Text);
			return path.Error.empty() && (!NeedSteps || !path.Steps.empty());
		}

		// 파일 이름이 되는 글: 영문, 숫자, '_', '-' 만.
		bool GoodName(const std::string& Text)
		{
			if (Text.empty() || Text.size() > 60)
				return false;
			for (const char c : Text)
				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
					return false;
			return true;
		}

		bool ParseArg(const std::string& Token, RemoteArg& Out)
		{
			if (Token == "u")
			{
				Out.Kind = 'u';
				return true;
			}
			if (Token.size() < 2 || Token[1] != ':')
				return false;
			const std::string rest = Token.substr(2);
			Out.Kind = Token[0];
			switch (Token[0])
			{
			case 'n':
				return ParseNumber(rest, Out.Number);
			case 'b':
				Out.Number = rest == "1" ? 1 : 0;
				return rest == "0" || rest == "1";
			case 's':
				Out.Text = rest.size() >= 2 && rest.front() == '{' && rest.back() == '}' ? rest.substr(1, rest.size() - 2) : rest;
				return true;
			case 'p':
				Out.Text = rest;
				return GoodPath(rest, false);
			default:
				return false;
			}
		}
	}

	double OptionNumber(const RemoteCommand& Command, const std::string& Key, double Fallback)
	{
		const auto it = Command.Options.find(Key);
		double value = 0;
		return it != Command.Options.end() && ParseNumber(it->second, value) ? value : Fallback;
	}

	RemoteCommand ParseRemoteLine(const std::string& Line)
	{
		RemoteCommand command;
		const std::string line = Trim(Line);
		if (line.empty() || line[0] == '#')
			return command;

		const std::vector<std::string> tokens = Split(line);
		const size_t count = tokens.size();
		command.Verb = tokens[0];
		const std::string& verb = command.Verb;

		const auto fail = [&](const std::string& why) {
			command.Error = why;
			return command;
		};
		// tokens[From] 부터를 key=value 로 읽는다.
		const auto options = [&](size_t From) {
			for (size_t i = From; i < count; i++)
			{
				const size_t eq = tokens[i].find('=');
				if (eq == std::string::npos || eq == 0)
				{
					command.Error = "expected key=value: " + tokens[i];
					return false;
				}
				command.Options[tokens[i].substr(0, eq)] = tokens[i].substr(eq + 1);
			}
			return true;
		};

		if (verb == "state")
		{
			if (count != 1)
				return fail("state takes nothing");
		}
		else if (verb == "ask" || verb == "list" || verb == "tree")
		{
			if (count < 2 || !GoodPath(tokens[1], false))
				return fail(verb + " needs a path");
			if (verb == "ask" && count != 2)
				return fail("ask takes one path");
			command.Target = tokens[1];
			options(2);
		}
		else if (verb == "find")
		{
			if (!options(1))
				return command;
			double value = 0;
			const auto given = command.Options.find("value");
			if (given != command.Options.end() && !ParseNumber(given->second, value))
				return fail("find value= needs a number");
			if (!command.Options.count("name") && given == command.Options.end())
				return fail("find needs name= or value=");
		}
		else if (verb == "refine")
		{
			if (!options(1))
				return command;
			const auto given = command.Options.find("value");
			if (given == command.Options.end() || !ParseNumber(given->second, command.Number))
				return fail("refine needs value=<number>");
		}
		else if (verb == "write" || verb == "poke")
		{
			const size_t eq = count == 2 ? tokens[1].rfind('=') : std::string::npos;		// 주소의 중괄호 안에 '=' 가 있을 수 있다
			if (eq == std::string::npos || !ParseNumber(tokens[1].substr(eq + 1), command.Number) || !GoodPath(tokens[1].substr(0, eq), true))
				return fail(verb + " needs <path>=<number>");
			command.Target = tokens[1].substr(0, eq);
		}
		else if (verb == "record" || verb == "unrecord")
		{
			if (count != 2)
				return fail(verb + " needs one name");
			command.Target = tokens[1];
		}
		else if (verb == "records")
		{
			if (count > 2)
				return fail("records takes at most one name");
			if (count == 2)
				command.Target = tokens[1];
		}
		else if (verb == "shot")
		{
			if (count != 2 || !GoodName(tokens[1]))
				return fail("shot needs a name of letters, digits, '_' and '-'");
			command.Target = tokens[1];
		}
		else if (verb == "window")
		{
			if (count != 2 || (tokens[1] != "open" && tokens[1] != "close"))
				return fail("window needs open or close");
			command.Target = tokens[1];
		}
		else if (verb == "call" || verb == "method")
		{
			if (count < 2 || (verb == "method" && !GoodPath(tokens[1], true)))
				return fail(verb == "call" ? "call needs a script name" : "method needs the path of a method");
			command.Target = tokens[1];
			for (size_t i = 2; i < count; i++)
			{
				RemoteArg arg;
				if (!ParseArg(tokens[i], arg))
					return fail("cannot read the argument: " + tokens[i]);
				command.Args.push_back(arg);
			}
		}
		else
			return fail("unknown command: " + verb);
		return command;
	}
}
```

`CMakeLists.txt`의 `nlcore` 목록에 `src/core/Rate.cpp` 다음 줄로 `  src/core/RemoteCommand.cpp`를 더한다.

- [ ] **Step 4: 시험이 통과하는지 본다**

Run: `pwsh -File tools/build.ps1; pwsh -File tools/test-native.ps1`
Expected: `ok - 원격 명령: …` 셋, 종료 코드 0

- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add src/core/RemoteCommand.hpp src/core/RemoteCommand.cpp CMakeLists.txt tests/native/core_tests.cpp
git -C E:\NlToyBox commit -m "feat(core): 켜져 있는 게임에 묻는 파일의 한 줄을 읽는다"
```

---

### Task 2: 호출 기록

**Files:**
- Create: `src/core/CallLog.hpp`, `src/core/CallLog.cpp`
- Modify: `CMakeLists.txt` (`nlcore`에 `src/core/CallLog.cpp`)
- Test: `tests/native/core_tests.cpp`

**Interfaces:**
- Produces:
  - `uint64_t ShapeKey(const int* Kinds, int Count)`
  - `struct CallSample { size_t Index; std::string Shape, Args, Result; }`
  - `class CallLog { explicit CallLog(size_t KeepFirst = 6, size_t MaxSamples = 40); size_t Note(uint64_t Key); void Sample(size_t Index, uint64_t Key, std::string Shape, std::string Args, std::string Result); void Clear(); size_t Calls() const; size_t ShapeCount() const; std::string Format(const std::string& Indent) const; }`
  - `Note`는 이 호출을 글로 남겨 달라는 뜻이면 호출의 번호(1 부터)를, 아니면 0 을 돌려준다

- [ ] **Step 1: 실패하는 시험을 쓴다**

include 에 `#include "core/CallLog.hpp"`를 더하고(`AskPath.hpp` 다음), Task 1 의 시험 뒤에 넣는다.

```cpp
	Test("호출 기록: 처음 몇 개와 처음 보는 꼴만 글로 남긴다", [] {
		const int one[] = { 0 }, two[] = { 0, 1 };
		CHECK(ShapeKey(one, 1) != ShapeKey(two, 2) && ShapeKey(one, 1) == ShapeKey(one, 1) && ShapeKey(nullptr, 0) != ShapeKey(one, 1));
		CallLog log(2, 10);
		const uint64_t a = ShapeKey(one, 1), b = ShapeKey(two, 2);
		CHECK(log.Note(a) == 1);
		log.Sample(1, a, "(number)", "(50)", "undefined");
		CHECK(log.Note(a) == 2);
		log.Sample(2, a, "(number)", "(60)", "undefined");
		CHECK(log.Note(a) == 0 && log.Note(a) == 0);				// 셋째부터는 세기만 한다
		CHECK(log.Note(b) == 5);									// 처음 보는 꼴은 남긴다
		log.Sample(5, b, "(number, string)", "(-20, \"tax\")", "1");
		CHECK(log.Calls() == 5 && log.ShapeCount() == 2);
		CHECK_STR(log.Format("  "), "  calls 5, shapes 2\n  shape (number) x4\n  shape (number, string) x1\n"
			"  #1 (50) -> undefined\n  #2 (60) -> undefined\n  #5 (-20, \"tax\") -> 1\n");
		log.Clear();
		CHECK(log.Calls() == 0 && log.Note(a) == 1);
	});

	Test("호출 기록: 글로 남기는 수에 한도가 있다", [] {
		CallLog log(100, 3);
		const int kind[] = { 0 };
		const uint64_t key = ShapeKey(kind, 1);
		for (int i = 0; i < 10; i++)
			if (const size_t index = log.Note(key))
				log.Sample(index, key, "(number)", "(1)", "undefined");
		CHECK(log.Calls() == 10);
		CHECK(log.Format("").find("#3 ") != std::string::npos && log.Format("").find("#4 ") == std::string::npos);
	});

```

- [ ] **Step 2: 빌드해서 실패를 본다**

Run: `pwsh -File tools/build.ps1`
Expected: `Cannot open include file: 'core/CallLog.hpp'`

- [ ] **Step 3: 구현한다**

`src/core/CallLog.hpp`:

```cpp
#pragma once
// 훅을 건 함수의 호출 기록. 러너에 기대지 않는다. 스펙: 치트 메뉴 §14(호출 기록기).
// 호출은 모두 세고, 글로 남기는 것(표본)은 처음 몇 개와 인자의 꼴(수와 형)이 처음 나온 것뿐이다.
// 자주 불리는 함수를 기록해도 글을 만드는 비용이 쌓이지 않는다.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace NlCore
{
	// 인자의 수와 형으로 만든 열쇠. Kinds: 인자마다의 형 번호.
	uint64_t ShapeKey(const int* Kinds, int Count);

	struct CallSample
	{
		size_t Index = 0;			// 몇 번째 호출이었나(1 부터)
		std::string Shape;			// "(number, string)"
		std::string Args;			// "(50, \"wood\")"
		std::string Result;
	};

	class CallLog
	{
	public:
		explicit CallLog(size_t KeepFirst = 6, size_t MaxSamples = 40) : m_KeepFirst(KeepFirst), m_MaxSamples(MaxSamples) {}

		// 호출 하나를 센다. 이 호출을 글로 남겨 달라는 뜻이면 호출의 번호(1 부터)를, 아니면 0 을 돌려준다.
		size_t Note(uint64_t Key);
		// Note 가 번호를 돌려준 호출의 글.
		void Sample(size_t Index, uint64_t Key, std::string Shape, std::string Args, std::string Result);
		void Clear();

		size_t Calls() const { return m_Calls; }
		size_t ShapeCount() const { return m_Shapes.size(); }
		// 사람이 읽는 글. 줄마다 앞에 Indent 를 붙인다.
		std::string Format(const std::string& Indent) const;

	private:
		struct ShapeInfo
		{
			std::string Text;
			size_t Count = 0;
		};

		size_t m_KeepFirst, m_MaxSamples;
		size_t m_Calls = 0;
		std::map<uint64_t, ShapeInfo> m_Shapes;
		std::vector<CallSample> m_Samples;
	};
}
```

`src/core/CallLog.cpp`:

```cpp
#include "CallLog.hpp"

#include <algorithm>
#include <utility>

namespace NlCore
{
	uint64_t ShapeKey(const int* Kinds, int Count)
	{
		// FNV-1a. 수와 형을 차례로 섞는다.
		uint64_t key = 14695981039346656037ull;
		const auto mix = [&](uint64_t value) {
			key ^= value;
			key *= 1099511628211ull;
		};
		mix(static_cast<uint64_t>(Count) + 1);
		for (int i = 0; i < Count && Kinds; i++)
			mix(static_cast<uint64_t>(static_cast<int64_t>(Kinds[i])) + 0x100);
		return key;
	}

	size_t CallLog::Note(uint64_t Key)
	{
		m_Calls++;
		const auto [it, fresh] = m_Shapes.try_emplace(Key);
		it->second.Count++;
		return (fresh || m_Calls <= m_KeepFirst) && m_Samples.size() < m_MaxSamples ? m_Calls : 0;
	}

	void CallLog::Sample(size_t Index, uint64_t Key, std::string Shape, std::string Args, std::string Result)
	{
		const auto it = m_Shapes.find(Key);
		if (it != m_Shapes.end() && it->second.Text.empty())
			it->second.Text = Shape;
		m_Samples.push_back({ Index, std::move(Shape), std::move(Args), std::move(Result) });
	}

	void CallLog::Clear()
	{
		m_Calls = 0;
		m_Shapes.clear();
		m_Samples.clear();
	}

	std::string CallLog::Format(const std::string& Indent) const
	{
		std::string text = Indent + "calls " + std::to_string(m_Calls) + ", shapes " + std::to_string(m_Shapes.size()) + "\n";

		// 많이 나온 꼴부터. 수가 같으면 글의 차례로(열쇠의 차례는 뜻이 없다).
		std::vector<const ShapeInfo*> shapes;
		for (const auto& [key, info] : m_Shapes)
			shapes.push_back(&info);
		std::sort(shapes.begin(), shapes.end(), [](const ShapeInfo* a, const ShapeInfo* b) {
			return a->Count != b->Count ? a->Count > b->Count : a->Text < b->Text;
		});
		for (const ShapeInfo* info : shapes)
			text += Indent + "shape " + (info->Text.empty() ? "(?)" : info->Text) + " x" + std::to_string(info->Count) + "\n";

		// 표본은 호출의 차례로. 겹쳐 불리면(되부름) 안쪽 호출이 먼저 끝나 먼저 들어온다.
		std::vector<const CallSample*> samples;
		for (const CallSample& sample : m_Samples)
			samples.push_back(&sample);
		std::sort(samples.begin(), samples.end(), [](const CallSample* a, const CallSample* b) { return a->Index < b->Index; });
		for (const CallSample* sample : samples)
			text += Indent + "#" + std::to_string(sample->Index) + " " + sample->Args + " -> " + sample->Result + "\n";
		return text;
	}
}
```

`CMakeLists.txt`의 `nlcore` 목록에 `src/core/AskPath.cpp` 다음 줄로 `  src/core/CallLog.cpp`를 더한다.

- [ ] **Step 4: 시험이 통과하는지 본다**

Run: `pwsh -File tools/build.ps1; pwsh -File tools/test-native.ps1`
Expected: `ok - 호출 기록: …` 둘, 종료 코드 0

- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add src/core/CallLog.hpp src/core/CallLog.cpp CMakeLists.txt tests/native/core_tests.cpp
git -C E:\NlToyBox commit -m "feat(core): 훅을 건 함수의 호출 기록 — 모두 세고 처음 몇 개와 새 꼴만 글로 남긴다"
```

---

### Task 3: 호출 기록기

러너에 닿는 코드다. 네이티브 시험이 없다. 검증은 빌드이고, 동작은 Task 7 의 실행이 잰다.

근거:

| 쓰는 것 | 근거 |
|---|---|
| `GetNamedRoutineIndex(이름, &번호)` — 스크립트는 100000 이상 500000 미만 | `MI_Public.cpp` 55~73행, 956~959행 |
| `GetScriptData(번호 - 100000, CScript*&)` → `m_Functions->m_ScriptFunction` | `MI_Public.cpp` 961~996행(`CallGameScriptEx`가 이 길로 부른다). 단계 0 에서 `CallGameScript`로 스크립트를 불렀다(`research/01`) |
| 함수의 꼴 `RValue& (CInstance*, CInstance*, RValue&, int, RValue**)` | `YYTK_Shared_Types.hpp` 237행 `PFUNC_YYGMLScript` |
| `MmCreateHook(Module, 이름, 원래 함수, 바꿀 함수, &트램펄린)` | `Aurie/shared.hpp` 598행. Present 에 걸어 봤다(`src/Ui.cpp`). **게임 스크립트에는 처음 건다** |
| `script_get_name(scr)`은 메서드도 받는다. `method_get_index(method)`는 스크립트 함수를 돌려준다 | 매뉴얼(Context7 `/yoyogames/gamemaker-manual`). 둘 다 exe 문자열에 있다. **이 러너에서는 처음 쓴다** |
| `is_method` | 이 러너에서 된다(`research/06`) |

**Files:**
- Create: `src/Recorder.hpp`, `src/Recorder.cpp`
- Modify: `CMakeLists.txt` (`nltoybox`에 `src/Recorder.cpp`)

**Interfaces:**
- Consumes: `NlCore::CallLog`, `ShapeKey` (Task 2), `NlAccess::Read` (`src/Access.hpp`), `NlGame::Call`·`Yytk` (`src/Game.hpp`), `NlCore::ParseAskPath`, `Quote`, `Shortest`
- Produces (모두 `namespace NlRecorder`):
  - `using LogFn = std::function<void(const std::string&)>`
  - `void Init(Aurie::AurieModule* Module, LogFn Log)`
  - `bool Watch(const std::string& Target, std::string& Name, std::string& Why)` — `Target`: 스크립트 이름(접두 `gml_Script_`는 없어도 된다) 또는 메서드의 주소. `Name`에 찾은 스크립트의 이름
  - `int Unwatch(const std::string& Name)` — `"all"` 또는 스크립트 이름. 멈춘 수
  - `std::string Report(const std::string& Name)` — 빈 글이면 전부

- [ ] **Step 1: 머리 파일을 쓴다**

`src/Recorder.hpp`:

```cpp
#pragma once
// 호출 기록기. 게임 스크립트의 함수에 훅을 걸어, 게임이 스스로 부를 때의 인자와 반환값을 적는다. 스펙: 치트 메뉴 §14.
// 인자의 수와 형을 모르는 스크립트를 부르면 게임이 끝난다(CLAUDE.md). 부르기 전에 이것으로 꼴을 확인한다.
// Watch, Unwatch, Report 는 게임 스레드의 틱에서 부른다.

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>

namespace NlRecorder
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번.
	void Init(Aurie::AurieModule* Module, LogFn Log);

	// 그 함수의 호출을 기록하기 시작한다. Target: 스크립트의 이름("gml_Script_x". 접두는 없어도 된다) 또는 메서드의 주소
	// ("inst:o_building.set_level"). Name 에 찾은 스크립트의 이름을 돌려준다. 이미 기록 중이면 기록을 비우고 다시 시작한다.
	bool Watch(const std::string& Target, std::string& Name, std::string& Why);

	// 기록을 멈춘다. Name: 스크립트의 이름 또는 "all". 훅은 떼지 않는다(그 함수가 호출 스택에 있을 때 떼면 죽는다).
	// 돌려주는 값: 멈춘 수.
	int Unwatch(const std::string& Name);

	// 기록을 글로. Name 이 비어 있으면 전부.
	std::string Report(const std::string& Name);
}
```

- [ ] **Step 2: 구현한다**

`src/Recorder.cpp`:

```cpp
#include "Recorder.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"
#include "core/CallLog.hpp"
#include "core/Text.hpp"

#include <array>
#include <mutex>
#include <utility>

using namespace Aurie;
using namespace YYTK;

namespace
{
	constexpr int k_Slots = 24;				// 동시에 훅을 걸 수 있는 함수의 수
	constexpr int k_MaxKinds = 16;			// 꼴을 볼 때 보는 인자의 수
	constexpr int k_KindMask = 0x0ffffff;	// m_Kind 에서 형만 남긴다(VALUE_UNSET 의 폭. YYTK_Shared_Types.hpp 199행)

	struct Slot
	{
		bool Used = false;
		bool Recording = false;
		std::string Name;
		PFUNC_YYGMLScript Target = nullptr;		// 훅을 건 함수
		PFUNC_YYGMLScript Original = nullptr;	// 원래 함수로 가는 트램펄린
		NlCore::CallLog Log;
	};

	AurieModule* g_Module = nullptr;
	NlRecorder::LogFn g_Log;
	std::recursive_mutex g_Mutex;		// 기록(Slot::Log)을 지킨다
	Slot g_Slots[k_Slots];

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	const char* KindName(int Kind)
	{
		switch (Kind)
		{
		case VALUE_REAL: return "number";
		case VALUE_STRING: return "string";
		case VALUE_ARRAY: return "array";
		case VALUE_PTR: return "ptr";
		case VALUE_UNDEFINED: return "undefined";
		case VALUE_OBJECT: return "struct";		// 메서드와 인스턴스도 여기에 든다
		case VALUE_INT32: return "int32";
		case VALUE_INT64: return "int64";
		case VALUE_BOOL: return "bool";
		case VALUE_REF: return "ref";
		default: return "other";
		}
	}

	// 값 하나를 짧은 글로. 훅 안에서 부른다: 빌트인을 부르지 않고 RValue 의 멤버 함수만 쓴다.
	std::string Brief(const RValue& Value)
	{
		const int kind = static_cast<int>(Value.m_Kind) & k_KindMask;
		if (kind == VALUE_UNDEFINED || kind == k_KindMask)		// k_KindMask 는 VALUE_UNSET 의 값이기도 하다
			return "undefined";
		if (Value.IsString())
			return NlCore::Quote(Value.ToString(), 60);
		if (Value.IsArray())
			return "array";
		if (Value.IsStruct())
			return "struct";
		if (kind == VALUE_REF)
			return "ref";
		if (Value.IsNumberConvertible())
			return NlCore::Shortest(Value.ToDouble());
		return KindName(kind);
	}

	// 훅을 건 함수가 불릴 때마다 온다(게임 스레드).
	RValue& Handle(int Index, CInstance* Self, CInstance* Other, RValue& Result, int Count, RValue** Args)
	{
		Slot& slot = g_Slots[Index];
		const PFUNC_YYGMLScript original = slot.Original;

		size_t sample = 0;
		uint64_t key = 0;
		std::string shape, args;
		if (slot.Recording)
		{
			int kinds[k_MaxKinds];
			const int seen = Count < 0 || !Args ? 0 : (Count > k_MaxKinds ? k_MaxKinds : Count);
			for (int i = 0; i < seen; i++)
				kinds[i] = Args[i] ? static_cast<int>(Args[i]->m_Kind) & k_KindMask : -1;
			key = NlCore::ShapeKey(kinds, seen);
			{
				std::lock_guard lock(g_Mutex);
				sample = slot.Log.Note(key);
			}
			if (sample)
			{
				// 인자의 글은 부르기 전에 만든다(불린 쪽이 인자를 바꿀 수 있다).
				shape = "(";
				args = "(";
				for (int i = 0; i < seen; i++)
				{
					if (i)
					{
						shape += ", ";
						args += ", ";
					}
					shape += kinds[i] < 0 ? "null" : KindName(kinds[i]);
					args += Args[i] ? Brief(*Args[i]) : "null";
				}
				if (Count > seen)
				{
					shape += ", ...";
					args += ", ...";
				}
				shape += ")";
				args += ")";
			}
		}

		RValue& out = original(Self, Other, Result, Count, Args);

		if (sample)
		{
			std::lock_guard lock(g_Mutex);
			slot.Log.Sample(sample, key, std::move(shape), std::move(args), Brief(out));
		}
		return out;
	}

	// 훅마다 제 자리의 번호를 아는 함수가 있어야 한다(같은 함수를 여러 훅에 쓰면 어느 스크립트인지 모른다).
	template <int Index>
	RValue& Detour(CInstance* Self, CInstance* Other, RValue& Result, int Count, RValue** Args)
	{
		return Handle(Index, Self, Other, Result, Count, Args);
	}

	template <int... Index>
	constexpr std::array<PFUNC_YYGMLScript, sizeof...(Index)> MakeDetours(std::integer_sequence<int, Index...>)
	{
		return { &Detour<Index>... };
	}

	constexpr std::array<PFUNC_YYGMLScript, k_Slots> k_Detours = MakeDetours(std::make_integer_sequence<int, k_Slots>{});

	// 스크립트의 함수를 이름으로 찾는다. 이름은 "gml_Script_x" 꼴이다. 접두가 없으면 붙여 본다.
	// 번호가 100000 미만이면 빌트인, 500000 이상이면 확장 함수다(YYToolkit MI_Public.cpp 55~66행). 스크립트만 받는다.
	bool FindScript(const std::string& Given, std::string& Name, PFUNC_YYGMLScript& Fn, std::string& Why)
	{
		for (const std::string& name : { Given, "gml_Script_" + Given })
		{
			int index = -1;
			NlGame::Yytk()->GetNamedRoutineIndex(name.c_str(), &index);
			if (index < 100000 || index >= 500000)
				continue;

			CScript* script = nullptr;
			if (!AurieSuccess(NlGame::Yytk()->GetScriptData(index - 100000, script)) || !script || !script->m_Functions
				|| !script->m_Functions->m_ScriptFunction)
			{
				Why = "the script has no function: " + name;
				return false;
			}
			Name = name;
			Fn = script->m_Functions->m_ScriptFunction;
			return true;
		}
		Why = "no such script: " + Given;
		return false;
	}

	// 메서드가 묶인 스크립트의 이름. script_get_name 은 메서드도 받는다(매뉴얼). 안 되면 method_get_index 를 거친다.
	bool MethodScriptName(const RValue& Method, std::string& Name)
	{
		RValue name;
		if (NlGame::Call("script_get_name", { Method }, name) && name.IsString() && !name.ToString().empty())
		{
			Name = name.ToString();
			return true;
		}
		RValue index;
		if (!NlGame::Call("method_get_index", { Method }, index) || !NlGame::Call("script_get_name", { index }, name) || !name.IsString())
			return false;
		Name = name.ToString();
		return !Name.empty();
	}
}

void NlRecorder::Init(AurieModule* Module, LogFn Log_)
{
	g_Module = Module;
	g_Log = std::move(Log_);
}

bool NlRecorder::Watch(const std::string& Target, std::string& Name, std::string& Why)
{
	std::string given = Target;

	// 주소이면 그 자리의 메서드가 묶인 스크립트의 이름을 묻는다.
	const NlCore::AskPath path = NlCore::ParseAskPath(Target);
	if (path.Error.empty() && !path.Steps.empty())
	{
		RValue value;
		if (!NlAccess::Read(path, value, Why))
			return false;
		if (NlGame::CallNumber("is_method", { value }, 0) <= 0 || !MethodScriptName(value, given))
		{
			Why = "not a method: " + Target;
			return false;
		}
	}

	PFUNC_YYGMLScript fn = nullptr;
	if (!FindScript(given, Name, fn, Why))
		return false;

	std::lock_guard lock(g_Mutex);
	int free_slot = -1;
	for (int i = 0; i < k_Slots; i++)
	{
		if (g_Slots[i].Used && g_Slots[i].Target == fn)
		{
			// 이미 훅이 걸려 있다. 기록을 비우고 다시 시작한다.
			g_Slots[i].Log.Clear();
			g_Slots[i].Recording = true;
			return true;
		}
		if (!g_Slots[i].Used && free_slot < 0)
			free_slot = i;
	}
	if (free_slot < 0)
	{
		Why = "no free hook slot (" + std::to_string(k_Slots) + " in use)";
		return false;
	}

	Slot& slot = g_Slots[free_slot];
	slot.Name = Name;
	slot.Target = fn;
	slot.Original = nullptr;
	slot.Log.Clear();
	const AurieStatus status = MmCreateHook(g_Module, "NlToyBox.rec." + std::to_string(free_slot), reinterpret_cast<PVOID>(fn),
		reinterpret_cast<PVOID>(k_Detours[free_slot]), reinterpret_cast<PVOID*>(&slot.Original));
	if (!AurieSuccess(status) || !slot.Original)
	{
		slot.Name.clear();
		slot.Target = nullptr;
		slot.Original = nullptr;
		Why = std::string("MmCreateHook ") + AurieStatusToString(status);
		return false;
	}
	slot.Used = true;
	slot.Recording = true;
	Log("record " + Name + ": hooked (slot " + std::to_string(free_slot) + ")");
	return true;
}

int NlRecorder::Unwatch(const std::string& Name)
{
	std::lock_guard lock(g_Mutex);
	int stopped = 0;
	for (Slot& slot : g_Slots)
		if (slot.Used && slot.Recording && (Name == "all" || slot.Name == Name || slot.Name == "gml_Script_" + Name))
		{
			slot.Recording = false;
			stopped++;
		}
	return stopped;
}

std::string NlRecorder::Report(const std::string& Name)
{
	std::lock_guard lock(g_Mutex);
	std::string text;
	for (const Slot& slot : g_Slots)
		if (slot.Used && (Name.empty() || slot.Name == Name || slot.Name == "gml_Script_" + Name))
			text += slot.Name + (slot.Recording ? "" : " (stopped)") + "\n" + slot.Log.Format("  ");
	return text;
}
```

`CMakeLists.txt`의 `nltoybox` 목록에서 `src/Menu.cpp` 다음 줄에 `  src/Recorder.cpp`를 더한다.

- [ ] **Step 3: 빌드한다**

Run: `pwsh -File tools/build.ps1`
Expected: `build ok -> …\NlToyBox.dll`. 함수 포인터와 `PVOID` 사이의 `reinterpret_cast`에 경고(C4191 등)가 나오면 그대로 둔다(MSVC 가 받는다).

- [ ] **Step 4: 커밋**

```powershell
git -C E:\NlToyBox add src/Recorder.hpp src/Recorder.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "feat(module): 호출 기록기 — 스크립트의 함수에 훅을 걸어 인자와 반환값을 적는다"
```

---

### Task 4: 화면 뜨기를 청할 수 있게 한다

지금 `Ui.cpp`는 시험 설정이 정한 때에 한 번만 화면을 뜬다. 원격 질의가 이름을 정해 몇 번이든 청할 수 있게 한다. 모드창을 열고 닫는 것도 밖에서 할 수 있게 한다(화면의 게임 HUD 를 가리지 않게).

**Files:**
- Modify: `src/Ui.hpp`, `src/Ui.cpp`

**Interfaces:**
- Produces:
  - `void NlUi::RequestShot(const std::filesystem::path& File)` — 다음 프레임을 그 파일(BMP)로 뜬다. 끝나면 로그에 `ui shot saved <파일 이름>`
  - `void NlUi::SetVisible(bool Visible)`

- [ ] **Step 1: 고친다**

`src/Ui.hpp`의 `SecondsSinceReady` 선언 뒤에 더한다.

```cpp

	// 다음에 그려지는 프레임을 그 파일(BMP)로 뜬다. 창이 닫혀 있어도 뜬다. 끝나면 로그에 "ui shot saved <파일 이름>"을 적는다.
	void RequestShot(const std::filesystem::path& File);

	// 모드창을 열거나 닫는다(F8 과 같다).
	void SetVisible(bool Visible);
```

`src/Ui.cpp`:

1. include 에 `#include <mutex>`를 더한다(`<fstream>` 다음).
2. `std::vector<std::pair<std::string, std::string>> g_TestExtra;` 다음 줄에 더한다.

```cpp
	std::mutex g_ShotMutex;
	std::filesystem::path g_ShotRequest;		// 밖에서 청한 화면. 다음 프레임에 뜬다
```

3. `Capture`가 파일과 알림 방식을 받게 한다. 머리 줄 `void Capture(ID3D11Texture2D* Back)`을 다음으로 바꾼다.

```cpp
	// File: 쓸 파일. Announce: 시험 설정의 화면이다(ui-check.ps1 이 "ui shot done" 줄을 기다린다).
	void Capture(ID3D11Texture2D* Back, const std::filesystem::path& File, bool Announce)
```

   그 함수 안에서 `std::ofstream out(g_Dir / "NlToyBox.ui.bmp", std::ios::binary | std::ios::trunc);`를 `std::ofstream out(File, std::ios::binary | std::ios::trunc);`로 바꾸고,
   끝의 `Log("ui shot done " + …);` 문 전체를 다음으로 감싼다.

```cpp
		if (!Announce)
		{
			Log("ui shot saved " + File.filename().string());
			return;
		}
		Log("ui shot done " + std::to_string(width) + "x" + std::to_string(height) + " format "
			+ std::to_string(static_cast<int>(desc.Format)) + " frames " + std::to_string(g_Frames)
			+ (g_Visible ? " window open" : " window closed")
			+ " mouse " + std::to_string(g_MouseMessages) + " keys " + std::to_string(g_KeyMessages)
			+ " yytk wndproc " + std::to_string(g_YytkWndProcCalls));
```

4. `Frame`에서 `const bool visible = g_Visible;`과 그 다음의 `if (!visible && !hint && !shot) return;`을 다음으로 바꾼다.

```cpp
		const bool visible = g_Visible;
		std::filesystem::path requested;
		{
			std::lock_guard lock(g_ShotMutex);
			requested.swap(g_ShotRequest);
		}
		if (!visible && !hint && !shot && requested.empty())
			return;
```

5. `Frame`의 끝에서 `if (shot) { g_ShotDone = true; Capture(back); }`을 다음으로 바꾼다.

```cpp
		if (shot)
		{
			g_ShotDone = true;
			Capture(back, g_Dir / "NlToyBox.ui.bmp", true);
		}
		if (!requested.empty())
			Capture(back, requested, false);
```

6. 파일 끝(`NlUi::WndProc` 앞)에 더한다.

```cpp
void NlUi::RequestShot(const std::filesystem::path& File)
{
	std::lock_guard lock(g_ShotMutex);
	g_ShotRequest = File;
}

void NlUi::SetVisible(bool Visible)
{
	g_Visible = Visible;
}

```

- [ ] **Step 2: 빌드한다**

Run: `pwsh -File tools/build.ps1`
Expected: `build ok -> …\NlToyBox.dll`

- [ ] **Step 3: 커밋**

```powershell
git -C E:\NlToyBox add src/Ui.hpp src/Ui.cpp
git -C E:\NlToyBox commit -m "feat(module): 화면 뜨기를 이름을 정해 청할 수 있고, 모드창을 밖에서 여닫을 수 있다"
```

---

### Task 5: 원격 질의

**Files:**
- Create: `src/Remote.hpp`, `src/Remote.cpp`
- Modify: `src/ModuleMain.cpp` (버전 `0.5.0`, `NlRecorder`와 `NlRemote`를 붙인다)
- Modify: `CMakeLists.txt` (`nltoybox`에 `src/Remote.cpp`)

**Interfaces:**
- Consumes: `NlCore::ParseRemoteLine`, `RemoteCommand`, `OptionNumber` (Task 1), `NlRecorder::Watch`·`Unwatch`·`Report` (Task 3), `NlUi::RequestShot`·`SetVisible` (Task 4), `NlAccess::*`, `NlSearch::*`, `NlGame::Yytk`·`Global`·`Call`
- Produces:
  - `void NlRemote::Init(const std::filesystem::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log)`
  - `void NlRemote::GameTick()`
  - 파일(모듈 폴더): `NlToyBox.ask.txt`(읽고 지운다), `NlToyBox.answer.txt`(이어 쓴다), `NlToyBox.shot.<이름>.bmp`
  - 답의 꼴: `# <id>` / 명령마다 `> <줄>`과 결과 줄들 / `# done <id>`

- [ ] **Step 1: 머리 파일을 쓴다**

`src/Remote.hpp`:

```cpp
#pragma once
// 원격 질의. 켜져 있는 게임에 도구가 파일로 묻는다. 스펙: 치트 메뉴 §14.
// 모듈 폴더의 NlToyBox.ask.txt 를 0.25초마다 보고, 있으면 줄들을 읽고 지운 뒤 게임 스레드에서 차례로 실행한다.
// 답은 NlToyBox.answer.txt 에 이어 쓴다. 줄의 꼴은 core/RemoteCommand.hpp 에 있다.

#include <filesystem>
#include <functional>
#include <string>

namespace NlRemote
{
	// ModuleInitialize 에서 한 번.
	void Init(const std::filesystem::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log);

	// 게임 스레드의 콜백에서 부른다.
	void GameTick();
}
```

- [ ] **Step 2: 구현한다**

`src/Remote.cpp`:

```cpp
#include "Remote.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "Recorder.hpp"
#include "Search.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/RemoteCommand.hpp"
#include "core/Text.hpp"

#include <chrono>
#include <fstream>
#include <vector>

using namespace Aurie;
using namespace YYTK;
using NlAccess::Holder;
using NlCore::AskPath;
using NlCore::RemoteCommand;
using NlCore::Shortest;
using Clock = std::chrono::steady_clock;

namespace
{
	std::filesystem::path g_Dir, g_Ask, g_Answer;
	std::string g_Version;
	std::function<void(const std::string&)> g_Log;
	Clock::time_point g_Start, g_NextPoll;
	std::ofstream g_Out;						// 지금 쓰는 답 파일. 요청 하나를 처리하는 동안만 열려 있다
	std::vector<NlSearch::Hit> g_Hits;			// 마지막 find 의 결과(refine 이 거른다). 글뿐이다

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 답 한 줄. 줄마다 내보낸다(명령이 게임을 끝내도 그 앞까지는 남는다).
	void Say(const std::string& Line)
	{
		g_Out << Line << '\n';
		g_Out.flush();
	}

	std::string RowText(const NlAccess::Row& Row)
	{
		return Row.Name + "  " + Row.Type + (Row.Text.empty() ? "" : "  " + Row.Text);
	}

	Holder AsOption(const RemoteCommand& C)
	{
		const auto it = C.Options.find("as");
		if (it == C.Options.end())
			return Holder::None;
		return it->second == "map" ? Holder::Map : it->second == "list" ? Holder::List : Holder::None;
	}

	void DoAsk(const RemoteCommand& C)
	{
		RValue value;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath(C.Target), value, why))
		{
			Say("  : " + why);
			return;
		}
		const NlAccess::Row row = NlAccess::Describe({}, value);
		Say("  = " + row.Type + (row.Text.empty() ? "" : " " + row.Text));
	}

	void DoList(const RemoteCommand& C)
	{
		std::vector<NlAccess::Row> rows;
		size_t total = 0;
		std::string why;
		const size_t max = static_cast<size_t>(NlCore::OptionNumber(C, "max", 400));
		if (!NlAccess::List(NlCore::ParseAskPath(C.Target), AsOption(C), max, rows, total, why))
		{
			Say("  : " + why);
			return;
		}
		for (const NlAccess::Row& row : rows)
			Say("  " + RowText(row));
		Say("  (" + std::to_string(total) + " in all)");
	}

	// 자식들을 깊이 MaxDepth 까지. ref(다른 인스턴스)로는 넘어가지 않는다.
	void Tree(const AskPath& Path, Holder As, int Depth, int MaxDepth, size_t& Budget, const std::string& Indent)
	{
		std::vector<NlAccess::Row> rows;
		size_t total = 0;
		std::string why;
		if (!NlAccess::List(Path, As, 400, rows, total, why))
		{
			if (Depth == 0)
				Say("  : " + why);
			return;
		}
		for (const NlAccess::Row& row : rows)
		{
			if (Budget == 0)
			{
				Say(Indent + "(max reached)");
				return;
			}
			Budget--;
			Say(Indent + RowText(row));
			if (row.IsContainer && row.Type != "ref" && Depth + 1 < MaxDepth)
				Tree(NlCore::ChildPath(Path, row.Step), Holder::None, Depth + 1, MaxDepth, Budget, Indent + "  ");
		}
		if (total > rows.size())
			Say(Indent + "(" + std::to_string(total) + " in all)");
	}

	void DoFind(const RemoteCommand& C)
	{
		NlSearch::Spec spec;
		const auto name = C.Options.find("name");
		if (name != C.Options.end())
			spec.Name = name->second;
		spec.HasValue = C.Options.count("value") > 0;
		spec.Value = NlCore::OptionNumber(C, "value", 0);
		const auto in = C.Options.find("in");
		if (in != C.Options.end())
		{
			spec.Globals = in->second.find("global") != std::string::npos;
			spec.Instances = in->second.find("inst") != std::string::npos;
			spec.Ds = in->second.find("ds") != std::string::npos;
		}

		NlSearch::Result result = NlSearch::Run(spec);
		g_Hits = std::move(result.Hits);
		const size_t max = static_cast<size_t>(NlCore::OptionNumber(C, "max", 200));
		for (size_t i = 0; i < g_Hits.size() && i < max; i++)
			Say("  " + g_Hits[i].Path + "  " + g_Hits[i].Type + "  " + g_Hits[i].Text);
		Say("  (" + std::to_string(g_Hits.size()) + " hits, " + std::to_string(result.Visited) + " visited, " + NlCore::Fixed(result.Seconds, 2)
			+ "s, " + std::to_string(result.Skipped) + " skipped" + (result.Truncated ? ", truncated" : "") + ")");
	}

	void DoRefine(const RemoteCommand& C)
	{
		NlSearch::Refine(g_Hits, C.Number);
		for (const NlSearch::Hit& hit : g_Hits)
			Say("  " + hit.Path + "  " + hit.Type + "  " + hit.Text);
		Say("  (" + std::to_string(g_Hits.size()) + " left)");
	}

	// write 는 남기고 poke 는 되돌린다.
	void DoWrite(const RemoteCommand& C, bool Restore)
	{
		double old = 0, read = 0, back = 0;
		if (!NlAccess::ReadNumber(C.Target, old))
		{
			Say("  : not a readable number");
			return;
		}
		Say("  old " + Shortest(old) + ", writing " + Shortest(C.Number));		// 쓰다가 죽으면 여기까지 남는다
		std::string why, why_back;
		const bool stuck = NlAccess::WriteNumber(C.Target, C.Number, why);
		const bool have = NlAccess::ReadNumber(C.Target, read);
		std::string line = "  read " + (have ? Shortest(read) : std::string("?")) + (stuck ? " (stuck)" : " (not stuck: " + why + ")");
		if (Restore)
		{
			const bool restored = NlAccess::WriteNumber(C.Target, old, why_back);
			const bool have_back = NlAccess::ReadNumber(C.Target, back);
			line += "; restored " + (have_back ? Shortest(back) : std::string("?")) + (restored ? "" : " (restore failed: " + why_back + ")");
		}
		Say(line);
		Log("remote " + C.Verb + " " + C.Target + " = " + Shortest(C.Number) + (stuck ? "" : ": " + why));
	}

	bool BuildArgs(const std::vector<NlCore::RemoteArg>& Args, std::vector<RValue>& Out, std::string& Why)
	{
		for (const NlCore::RemoteArg& arg : Args)
		{
			switch (arg.Kind)
			{
			case 'n': Out.push_back(RValue(arg.Number)); break;
			case 's': Out.push_back(RValue(std::string_view(arg.Text))); break;
			case 'b': Out.push_back(RValue(arg.Number != 0)); break;
			case 'u': Out.push_back(RValue()); break;
			case 'p':
			{
				RValue value;
				if (!NlAccess::Read(NlCore::ParseAskPath(arg.Text), value, Why))
					return false;
				Out.push_back(value);
				break;
			}
			default:
				Why = "unknown argument kind";
				return false;
			}
		}
		return true;
	}

	void SayResult(const RValue& Result)
	{
		const NlAccess::Row row = NlAccess::Describe({}, Result);
		Say("  -> " + row.Type + (row.Text.empty() ? "" : " " + row.Text));
	}

	// 게임 스크립트를 부른다. 인자의 수와 형은 기록으로 확인한 것만 쓴다(부르는 쪽의 책임이다).
	void DoCall(const RemoteCommand& C)
	{
		std::vector<RValue> args;		// 이 함수 안에서만 든다
		std::string why;
		CInstance* global = NlGame::Global();
		if (!global || !BuildArgs(C.Args, args, why))
		{
			Say("  : " + (global ? why : std::string("no global instance")));
			return;
		}
		for (const std::string& name : { C.Target, "gml_Script_" + C.Target })
		{
			int index = -1;
			NlGame::Yytk()->GetNamedRoutineIndex(name.c_str(), &index);
			if (index < 100000 || index >= 500000)
				continue;

			Say("  calling " + name + " with " + std::to_string(args.size()) + " arguments");		// 죽으면 여기까지 남는다
			Log("remote call " + name + " (" + std::to_string(args.size()) + " arguments)");
			RValue result;
			const AurieStatus status = NlGame::Yytk()->CallGameScriptEx(result, name, global, global, args);
			if (AurieSuccess(status))
				SayResult(result);
			else
				Say(std::string("  : ") + AurieStatusToString(status));
			return;
		}
		Say("  : no such script: " + C.Target);
	}

	// 메서드를 그것이 묶인 구조체에서 부른다(method_call. 매뉴얼).
	void DoMethod(const RemoteCommand& C)
	{
		RValue method, result;
		std::vector<RValue> args;
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath(C.Target), method, why) || !BuildArgs(C.Args, args, why))
		{
			Say("  : " + why);
			return;
		}
		if (NlGame::CallNumber("is_method", { method }, 0) <= 0)
		{
			Say("  : not a method");
			return;
		}
		Say("  calling the method with " + std::to_string(args.size()) + " arguments");
		Log("remote method " + C.Target + " (" + std::to_string(args.size()) + " arguments)");
		std::vector<RValue> call = { method };
		if (!args.empty())
			call.push_back(RValue(args));		// 인자들의 배열
		if (NlGame::Call("method_call", call, result))
			SayResult(result);
		else
			Say("  : method_call failed");
	}

	void DoState()
	{
		double game_time = 0, warp = 0;
		const bool have_time = NlAccess::ReadNumber("inst:o_time_controller.__game_time", game_time);
		NlAccess::ReadNumber("inst:o_time_controller.time_warp", warp);
		Say("  module " + g_Version + ", up " + NlCore::Fixed(std::chrono::duration<double>(Clock::now() - g_Start).count(), 1) + "s");
		Say(std::string("  in_game ") + (NlAccess::InGame() ? "1" : "0"));
		Say("  game_time " + (have_time ? Shortest(game_time) : std::string("?")) + ", time_warp " + Shortest(warp));
		for (const char* object : { "o_main_menu", "o_debug", "o_province_controller", "o_character", "o_dummy", "o_building" })
			Say(std::string("  ") + object + " " + std::to_string(NlAccess::InstanceCount(object)));
	}

	void Execute(const RemoteCommand& C)
	{
		if (!C.Error.empty())
		{
			Say("  : " + C.Error);
			return;
		}
		if (C.Verb == "ask")
			DoAsk(C);
		else if (C.Verb == "list")
			DoList(C);
		else if (C.Verb == "tree")
		{
			size_t budget = static_cast<size_t>(NlCore::OptionNumber(C, "max", 300));
			Tree(NlCore::ParseAskPath(C.Target), AsOption(C), 0, static_cast<int>(NlCore::OptionNumber(C, "depth", 2)), budget, "  ");
		}
		else if (C.Verb == "find")
			DoFind(C);
		else if (C.Verb == "refine")
			DoRefine(C);
		else if (C.Verb == "write" || C.Verb == "poke")
			DoWrite(C, C.Verb == "poke");
		else if (C.Verb == "record")
		{
			std::string name, why;
			if (NlRecorder::Watch(C.Target, name, why))
				Say("  recording " + name);
			else
				Say("  : " + why);
		}
		else if (C.Verb == "unrecord")
			Say("  stopped " + std::to_string(NlRecorder::Unwatch(C.Target)));
		else if (C.Verb == "records")
		{
			const std::string report = NlRecorder::Report(C.Target);
			Say(report.empty() ? "  (nothing recorded)" : report.substr(0, report.size() - 1));		// 끝의 줄바꿈은 Say 가 붙인다
		}
		else if (C.Verb == "call")
			DoCall(C);
		else if (C.Verb == "method")
			DoMethod(C);
		else if (C.Verb == "state")
			DoState();
		else if (C.Verb == "shot")
		{
			NlUi::RequestShot(g_Dir / ("NlToyBox.shot." + C.Target + ".bmp"));
			Say("  shot requested: NlToyBox.shot." + C.Target + ".bmp");
		}
		else if (C.Verb == "window")
		{
			NlUi::SetVisible(C.Target == "open");
			Say("  window " + C.Target);
		}
	}
}

void NlRemote::Init(const std::filesystem::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log_)
{
	g_Dir = ModuleDir;
	g_Ask = ModuleDir / "NlToyBox.ask.txt";
	g_Answer = ModuleDir / "NlToyBox.answer.txt";
	g_Version = Version;
	g_Log = std::move(Log_);
	g_Start = Clock::now();
	g_NextPoll = g_Start;
}

void NlRemote::GameTick()
{
	const Clock::time_point now = Clock::now();
	if (now < g_NextPoll)
		return;
	g_NextPoll = now + std::chrono::milliseconds(250);

	std::error_code ec;
	if (!std::filesystem::exists(g_Ask, ec))
		return;

	// 도구는 임시 파일에 쓴 뒤 이름을 바꿔 놓는다. 여기서 보이면 다 쓰인 것이다.
	std::vector<std::string> lines;
	{
		std::ifstream in(g_Ask);
		std::string line;
		while (std::getline(in, line))
			lines.push_back(line);
	}
	std::filesystem::remove(g_Ask, ec);

	std::string id = "-";
	g_Out.open(g_Answer, std::ios::app);
	for (const std::string& line : lines)
	{
		const std::string text = NlCore::Trim(line);
		if (text.rfind("id ", 0) == 0)
		{
			id = NlCore::Trim(text.substr(3));
			Say("# " + id);
			continue;
		}
		const RemoteCommand command = NlCore::ParseRemoteLine(text);
		if (command.Verb.empty())
			continue;
		Say("> " + text);
		Execute(command);
	}
	Say("# done " + id);
	g_Out.close();
	Log("remote request " + id + ": " + std::to_string(lines.size()) + " lines");
}
```

`src/ModuleMain.cpp`:

1. include 에 `#include "Recorder.hpp"`와 `#include "Remote.hpp"`를 더한다(`"Menu.hpp"` 다음).
2. `k_Version`을 `"0.5.0"`으로 바꾼다.
3. `CodeCallback`에서 `NlMenu::GameTick();` 다음 줄에 `NlRemote::GameTick();`을 더한다.
4. `ModuleInitialize`에서 `NlMenu::Init(…);` 앞에 더한다.

```cpp
	NlRecorder::Init(Module, [](const std::string& Line) { LogLine(Line); });
	NlRemote::Init(module_dir, k_Version, [](const std::string& Line) { LogLine(Line); });
```

`CMakeLists.txt`의 `nltoybox` 목록에서 `src/Recorder.cpp` 다음 줄에 `  src/Remote.cpp`를 더한다.

- [ ] **Step 3: 빌드하고 네이티브 시험을 돌린다**

Run: `pwsh -File tools/build.ps1; pwsh -File tools/test-native.ps1`
Expected: `build ok -> …\NlToyBox.dll`, `core tests: N passed`

- [ ] **Step 4: 커밋**

```powershell
git -C E:\NlToyBox add src/Remote.hpp src/Remote.cpp src/ModuleMain.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "feat(module): 원격 질의 0.5.0 — 켜져 있는 게임에 파일로 묻고, 쓰고, 기록하고, 화면을 뜬다"
```

---

### Task 6: 도구 — 실행 묶음과 묻기

**Files:**
- Create: `tools/session.ps1`, `tools/ask.ps1`
- Test: `tools/tests/safety.tests.ps1`

**Interfaces:**
- `tools/session.ps1 -Action start|stop|status [-Name <이름>] [-KeepSettings]`
  - `start`: 남은 `*.kept`가 있으면 거부. 사용자의 배율·치트 설정을 `*.kept`로 치우고(`-KeepSettings`면 그대로 둔다) 게임을 켠다. 기다리지 않고 돌아온다
  - `status`: 실행 여부, 적재 판정, 로그의 끝. 판정이 미통과면 종료 코드 1
  - `stop`: 게임을 끄고, 설정을 되돌리고, 답과 로그를 `refs\runtime\<이름>.answer.txt`·`.log`로 옮긴다
- `tools/ask.ps1 -Lines <줄들> | -File <파일> [-TimeoutSec 30]`: 묻고 답을 찍는다. `shot <이름>`이 있으면 `refs\ui\<이름>.png`로 가져온다. 답이 없으면 종료 코드 1

- [ ] **Step 1: 실패하는 시험을 쓴다**

`tools/tests/safety.tests.ps1`에서 `Test-Case 'setup 은 내용이 원본과 다른 기존 백업을 믿지 않고 다시 만든다'` 앞에 넣는다.

```powershell
    Test-Case 'ask 는 게임이 꺼져 있으면 묻지 않는다' {
        $r = Invoke-Tool 'ask.ps1' "-Lines 'state'"
        Assert-True ($r.Exit -ne 0 -and $r.Out -match '게임이 켜져 있지 않습니다') "거부해야 한다`n$($r.Out)"
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $fake 'mods\Aurie\NlToyBox.ask.txt'))) '물음 파일을 남기지 않아야 한다'
    }

    Test-Case 'session 은 남은 설정 사본이 있으면 켜지 않고, stop 이 사용자의 설정을 되돌린다' {
        $modsDir = Join-Path $fake 'mods'
        $hadMods = Test-Path -LiteralPath $modsDir
        $aurieDir = Join-Path $modsDir 'Aurie'
        New-Item -ItemType Directory -Force -Path $aurieDir | Out-Null
        $settingsFile = Join-Path $aurieDir 'NlToyBox.settings.txt'
        try {
            Set-Content -LiteralPath "$settingsFile.kept" -Value 'building_cost=0.50' -Encoding ascii     # 사용자의 것
            Set-Content -LiteralPath $settingsFile -Value 'building_cost=3.00' -Encoding ascii           # 실행 묶음이 쓴 것
            $r = Invoke-Tool 'session.ps1' '-Action start' $noLaunch
            Assert-True ($r.Exit -ne 0 -and $r.Out -match '남긴 사본' -and $r.Out -notmatch 'LAUNCH-ATTEMPTED') "start 는 거부해야 한다`n$($r.Out)"
            $r = Invoke-Tool 'session.ps1' '-Action stop -Name safety-test'
            Assert-Equal $r.Exit 0 "stop 종료 코드`n$($r.Out)"
            Assert-Equal (Get-Content -LiteralPath $settingsFile -Raw).Trim() 'building_cost=0.50' '사용자의 설정이 돌아와야 한다'
            Assert-True (-not (Test-Path -LiteralPath "$settingsFile.kept")) '사본이 남지 않아야 한다'
        }
        finally {
            if (-not $hadMods -and (Test-Path -LiteralPath $modsDir)) { Remove-Item -LiteralPath $modsDir -Recurse -Force }
        }
    }

```

- [ ] **Step 2: 시험이 실패하는지 본다**

Run: `pwsh -File tools/tests/safety.tests.ps1`
Expected: `ask.ps1`을 찾지 못해 첫 새 시험에서 끝난다(`거부해야 한다` 또는 `not recognized`)

- [ ] **Step 3: 구현한다**

`tools/session.ps1`:

```powershell
param(
    [Parameter(Mandatory)][ValidateSet('start', 'stop', 'status')][string]$Action,
    [string]$Name = (Get-Date -Format 'yyyyMMdd-HHmmss'),   # stop: 답과 로그를 refs\runtime\<Name>.* 로 옮긴다
    [switch]$KeepSettings,                                   # start: 사용자의 배율·치트 설정을 치우지 않는다(걸어 둔 채로 본다)
    [int]$GraceSec = 15
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 실행 묶음: 게임을 켜 둔 채 tools\ask.ps1 로 몇 번이든 묻는다(스펙: 치트 메뉴 §14). start 가 켜고 stop 이 끈다.
# 값을 재는 동안 치트가 걸려 있으면 안 되므로 start 는 사용자의 설정을 *.kept 로 치워 두고, stop 이 되돌린다.
$modDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
$settings = Join-Path $modDir 'NlToyBox.settings.txt'
$cheats = Join-Path $modDir 'NlToyBox.cheats.txt'
$log = Join-Path $modDir 'NlToyBox.log'
$ask = Join-Path $modDir 'NlToyBox.ask.txt'
$answer = Join-Path $modDir 'NlToyBox.answer.txt'

# 치워 둔 사용자의 설정을 되돌린다. 실행 묶음이 그 이름으로 쓴 파일은 버린다.
function Restore-NlKept {
    foreach ($file in $settings, $cheats) {
        $kept = "$file.kept"
        if (Test-Path -LiteralPath $kept) {
            if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force }
            Move-Item -LiteralPath $kept -Destination $file
        }
    }
}

function Get-NlShotFiles {
    if (-not (Test-Path -LiteralPath $modDir)) { return @() }
    @(Get-ChildItem -LiteralPath $modDir -Filter 'NlToyBox.shot.*.bmp' | ForEach-Object { $_.FullName })
}

switch ($Action) {
    'start' {
        foreach ($file in $settings, $cheats) {
            if (Test-Path -LiteralPath "$file.kept") {
                throw "앞선 실행이 남긴 사본이 있습니다: $file.kept`n먼저 tools\session.ps1 -Action stop 을 실행해 되돌리세요."
            }
        }
        Assert-NlReadyToLaunch
        foreach ($f in @($ask, "$ask.tmp", $answer, $log) + (Get-NlShotFiles)) {
            if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force }
        }
        if (-not $KeepSettings) {
            foreach ($file in $settings, $cheats) {
                if (Test-Path -LiteralPath $file) { Move-Item -LiteralPath $file -Destination "$file.kept" }
            }
        }
        try {
            Write-Host "게임을 켭니다 (steam://rungameid/$($script:NlAppId))."
            Start-Process "steam://rungameid/$($script:NlAppId)"
        }
        catch {
            Restore-NlKept
            throw
        }
        Write-Host '켜진 뒤에는 tools\ask.ps1 로 묻고, 끝나면 tools\session.ps1 -Action stop 으로 끕니다.'
    }

    'status' {
        Write-Host "게임 실행 중: $(Test-NlGameRunning)"
        $lines = @(if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log })
        $failed = @(Get-NlLoadFailures $lines)
        Write-Host ('적재 판정: ' + $(if ($failed.Count) { "미통과 - $($failed -join ', ')" } else { '통과' }))
        Write-Host "치워 둔 설정: $(@($settings, $cheats | Where-Object { Test-Path -LiteralPath "$_.kept" }).Count)개"
        $lines | Select-Object -Last 12 | ForEach-Object { Write-Host "  $_" }
        if ($failed.Count) { exit 1 }
    }

    'stop' {
        Write-Host "게임 종료: $(Stop-NlGame $GraceSec)"
        Restore-NlKept
        $outDir = Join-Path (Get-NlRepoRoot) 'refs\runtime'
        if (Test-Path -LiteralPath $answer) {
            New-Item -ItemType Directory -Force -Path $outDir | Out-Null
            Move-Item -LiteralPath $answer -Destination (Join-Path $outDir "$Name.answer.txt") -Force
            Write-Host "답: refs\runtime\$Name.answer.txt"
            if (Test-Path -LiteralPath $log) { Copy-Item -LiteralPath $log -Destination (Join-Path $outDir "$Name.log") -Force }
        }
        foreach ($f in @($ask, "$ask.tmp") + (Get-NlShotFiles)) {
            if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force }
        }
    }
}
```

`tools/ask.ps1`:

```powershell
param(
    [string[]]$Lines = @(),     # 명령들(src/core/RemoteCommand.hpp). 한 줄에 하나
    [string]$File,              # 명령이 든 파일(줄마다 하나). -Lines 대신
    [int]$TimeoutSec = 30
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# 켜져 있는 게임에 묻는다(스펙: 치트 메뉴 §14). 모듈이 0.25초마다 NlToyBox.ask.txt 를 보고 답을 NlToyBox.answer.txt 에 이어 쓴다.
# 공백이나 쉼표가 든 줄을 여럿 넘기려면 pwsh 안에서 부른다:   & tools\ask.ps1 -Lines 'state', 'list inst:o_debug max=20'
if ($File) { $Lines = @(Get-Content -LiteralPath $File) }
$Lines = @($Lines | Where-Object { $_ -and $_.Trim() })
if (-not $Lines.Count) { throw '물을 줄이 없습니다. -Lines 나 -File 을 주세요.' }
if (-not (Test-NlGameRunning)) { throw '게임이 켜져 있지 않습니다. 먼저 tools\session.ps1 -Action start 를 실행하세요.' }

$modDir = Join-Path (Get-NlGameDir) 'mods\Aurie'
$ask = Join-Path $modDir 'NlToyBox.ask.txt'
$answer = Join-Path $modDir 'NlToyBox.answer.txt'
$id = [guid]::NewGuid().ToString('N').Substring(0, 8)

# 앞의 물음을 모듈이 아직 가져가지 않았으면 잠깐 기다린다.
$wait = (Get-Date).AddSeconds(5)
while ((Test-Path -LiteralPath $ask) -and (Get-Date) -lt $wait) { Start-Sleep -Milliseconds 200 }
if (Test-Path -LiteralPath $ask) { throw "앞의 물음이 남아 있습니다(모듈이 읽지 않았습니다): $ask" }

# 임시 파일에 쓴 뒤 이름을 바꾼다. 모듈이 반쯤 쓰인 파일을 읽지 않게.
$tmp = "$ask.tmp"
[IO.File]::WriteAllLines($tmp, [string[]](@("id $id") + $Lines), [Text.UTF8Encoding]::new($false))
Move-Item -LiteralPath $tmp -Destination $ask

$deadline = (Get-Date).AddSeconds($TimeoutSec)
$found = $false
$segment = @()
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 250
    if (Test-Path -LiteralPath $answer) {
        $all = @(Get-Content -LiteralPath $answer -Encoding utf8)
        $from = [Array]::IndexOf($all, "# $id")
        $to = [Array]::IndexOf($all, "# done $id")
        if ($from -ge 0 -and $to -gt $from) {
            $segment = @(if ($to -gt $from + 1) { $all[($from + 1)..($to - 1)] })
            $found = $true
            break
        }
    }
    if (-not (Test-NlGameRunning)) { break }
}

if (-not $found) {
    # 답이 끝나지 않았다. 온 데까지 보여 준다(명령이 게임을 끝냈으면 마지막 줄이 그 명령이다).
    if (Test-Path -LiteralPath $answer) {
        $all = @(Get-Content -LiteralPath $answer -Encoding utf8)
        $from = [Array]::IndexOf($all, "# $id")
        if ($from -ge 0 -and $from + 1 -lt $all.Count) { $all[($from + 1)..($all.Count - 1)] | ForEach-Object { Write-Host $_ } }
    }
    if (Test-Path -LiteralPath $ask) { Remove-Item -LiteralPath $ask -Force -ErrorAction SilentlyContinue }
    Write-Host "FAIL: 답을 받지 못했습니다 ($TimeoutSec 초). 게임 실행 중: $(Test-NlGameRunning)"
    exit 1
}
$segment | ForEach-Object { Write-Host $_ }

# shot <이름> 이 있으면 모듈이 뜬 화면을 refs\ui\<이름>.png 로 가져온다.
$shots = @($Lines | ForEach-Object { if ($_ -match '^\s*shot\s+([A-Za-z0-9_-]+)\s*$') { $Matches[1] } })
foreach ($shot in $shots) {
    $bmp = Join-Path $modDir "NlToyBox.shot.$shot.bmp"
    $shotWait = (Get-Date).AddSeconds(8)
    while (-not (Test-Path -LiteralPath $bmp) -and (Get-Date) -lt $shotWait) { Start-Sleep -Milliseconds 200 }
    if (-not (Test-Path -LiteralPath $bmp)) { Write-Host "화면을 받지 못했습니다: $shot"; continue }
    Start-Sleep -Milliseconds 300       # 다 쓰이기를 기다린다
    $outDir = Join-Path (Get-NlRepoRoot) 'refs\ui'
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    $png = Join-Path $outDir "$shot.png"
    Add-Type -AssemblyName System.Drawing
    $image = [System.Drawing.Image]::FromFile($bmp)
    try { $image.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $image.Dispose() }
    Remove-Item -LiteralPath $bmp -Force
    Write-Host "화면: $png"
}
exit 0
```

- [ ] **Step 4: 시험이 통과하는지 본다**

Run: `pwsh -File tools/tests/safety.tests.ps1`
Expected: `ok - ask 는 게임이 꺼져 있으면 묻지 않는다`, `ok - session 은 남은 설정 사본이 있으면 켜지 않고, stop 이 사용자의 설정을 되돌린다`, `safety tests: 28 passed`

- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add tools/session.ps1 tools/ask.ps1 tools/tests/safety.tests.ps1
git -C E:\NlToyBox commit -m "feat(tools): 실행 묶음(session)과 묻기(ask) — 켜 둔 게임에 몇 번이든 묻는다"
```

---

### Task 7: 실행 묶음 하나로 확인하고, 적고, 넣는다

게임을 켜는 것은 이 Task 의 실행 묶음 하나다. 사용자의 승인을 받고 켠다. 이 묶음은 3가의 확인(메인 메뉴)에 이어 3나의 조사(게임 화면)에도 쓴다. 3나의 조사는 이 계획의 범위가 아니다(`research/07-remote.md`에 따로 적는다).

**Files:**
- Create: `research/07-remote.md`
- Modify: `CLAUDE.md`, `README.md`

- [ ] **Step 1: 게임을 켜지 않는 확인을 모두 돌린다**

```powershell
pwsh -File tools/build.ps1
pwsh -File tools/test-native.ps1
pwsh -File tools/tests/safety.tests.ps1
py -3.14 -m unittest discover -s tools/re/tests
py -3.14 -m unittest discover -s tools/overlay/tests
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"
```

Expected: 모두 통과, 마지막 명령의 출력은 비어 있다.

- [ ] **Step 2: 배포하고 세이브의 사본을 뜬 뒤 켠다**

켜기 전에 사용자에게 알린다: 게임을 켜 두고 여러 번 묻는다. 메뉴에서의 확인이 끝나면 새 게임을 시작해 달라고 청한다.

```powershell
pwsh -File tools/deploy.ps1
pwsh -File tools/saves-backup.ps1
pwsh -File tools/session.ps1 -Action start
```

모듈이 뜨고 메인 메뉴에 닿을 때까지 기다린다(`o_time_controller`가 생긴다. 모듈이 뜬 뒤 28~50초). 다음이 `in_game 0`과 `o_main_menu 1`을 낼 때까지 5초마다 되풀이한다.

```powershell
& tools\ask.ps1 -Lines 'state'
```

게임 창이 2분 안에 뜨지 않고 `NlToyBox.log`가 없으면 모듈 적재 전의 멈춤이다(`research/06`). `session.ps1 -Action stop` 뒤 승인받은 횟수 안에서 다시 켠다.

- [ ] **Step 3: 메뉴에서 묻기·쓰기를 확인한다**

```powershell
& tools\ask.ps1 -Lines 'state', 'ask inst:o_time_controller.time_warp', 'list inst:o_time_controller max=5', 'tree global.__gameplay_vars depth=1 max=5', 'poke inst:o_time_controller.time_warp_max=101', 'ask inst:o_debug.is_debug_enabled'
pwsh -File tools/session.ps1 -Action status
```

Expected:
- `module 0.5.0`, `in_game 0`, `o_main_menu 1`, `o_debug 0`
- `> ask inst:o_time_controller.time_warp` → `  = number 0`
- `list`가 다섯 줄과 `(43 in all)`
- `tree`가 다섯 줄과 `(max reached)`
- `poke` → `old 100, writing 101` / `read 101 (stuck); restored 100`
- `ask inst:o_debug…` → `  : no instance of o_debug at 0`
- `적재 판정: 통과`

- [ ] **Step 4: 훅과 기록을 확인한다**

`gml_Script_budget_default_money_get`은 인자 없이 메뉴에서 불러 `undefined`를 받은 스크립트다(`research/01`). 훅을 걸고, 부르고, 기록을 본다.

```powershell
& tools\ask.ps1 -Lines 'record gml_Script_budget_default_money_get', 'call gml_Script_budget_default_money_get', 'records gml_Script_budget_default_money_get', 'record inst:o_time_controller.set_time_speed', 'records'
```

Expected:
- `  recording gml_Script_budget_default_money_get`
- `  calling gml_Script_budget_default_money_get with 0 arguments` / `  -> undefined`
- 기록에 `calls 1, shapes 1`, `shape () x1`, `#1 () -> undefined` — **훅이 게임 스크립트에 걸리고, 원래 함수로 넘어가고, 결과가 그대로 돌아온다**
- `record inst:o_time_controller.set_time_speed` → `  recording <스크립트 이름>`(메서드가 묶인 스크립트의 이름을 처음 본다. 무엇이 나오든 적는다)

`recording`이 아니라 `: MmCreateHook …`이면 이 러너에서 그 함수에 훅이 걸리지 않는 것이다. 수단 C·D 의 방법을 바꿔야 한다. 멈추고 사용자에게 알린다.

- [ ] **Step 5: 화면 뜨기를 확인한다**

```powershell
& tools\ask.ps1 -Lines 'window close', 'shot remote-menu', 'window open'
```

Expected: `화면: …\refs\ui\remote-menu.png`. 그림을 열어 본다: 메인 메뉴가 보이고 모드창이 없다.

- [ ] **Step 6: (3나의 조사) 사용자가 새 게임을 시작한 뒤 게임 화면에서 자리를 찾는다**

이 계획의 범위 밖이다. 찾은 것은 `research/07-remote.md`의 "게임 화면에서 찾은 것"에 적는다. 끝나면:

```powershell
pwsh -File tools/session.ps1 -Action stop -Name stage3-session1
```

Expected: `게임 종료: closed`, `답: refs\runtime\stage3-session1.answer.txt`. 사용자의 설정 파일이 제자리에 있다.

- [ ] **Step 7: 잰 것과 쓰는 법을 적는다**

`research/07-remote.md`를 `research/06-cheat-menu.md`의 꼴로 쓴다: 머리(조사일, 대상, 모듈 0.5.0, 켠 횟수), "된 것"(Step 3~5 의 답을 그대로), "처음 잰 것"(게임 스크립트에 `MmCreateHook`이 걸리는가, `script_get_name`이 메서드에 돌려주는 이름, `CallGameScriptEx`, 화면 뜨기), "확인하지 못한 것".

`CLAUDE.md`:

1. "개발 루프"의 명령 목록 끝에 더한다.

```
    pwsh -File tools/session.ps1 -Action start   # 게임을 켜 둔다(사용자의 배율·치트 설정은 *.kept 로 치운다). 기다리지 않는다
    & tools\ask.ps1 -Lines 'state', 'list inst:o_debug max=20'   # 켜 둔 게임에 묻는다. 몇 번이든
    pwsh -File tools/session.ps1 -Action stop -Name <이름>       # 끄고, 설정을 되돌리고, 답을 refs\runtime\ 으로 옮긴다
```

2. "모듈을 쓸 때"의 치트 메뉴 묶음 뒤에 더한다.

```markdown
- 켜져 있는 게임에 도구가 파일로 묻는다(`src/Remote.cpp`, 스펙 §14). 줄의 꼴은 `src/core/RemoteCommand.hpp`에 있다:
  `ask`, `list`, `tree`, `find`·`refine`, `write`·`poke`, `state`, `shot`, `window`, `record`·`records`, `call`·`method`.
  - 게임 화면의 값을 찾는 일은 사용자에게 넘기지 않는다. 실행 묶음을 켜고 `ask.ps1`로 찾는다. 사용자는 새 게임을 시작해 두기만 한다.
  - `call`과 `method`는 `record`로 인자의 수와 형을 본 함수에만 쓴다. 본 적 없는 꼴로 부르지 않는다.
  - 한 번 건 훅은 떼지 않는다(`src/Recorder.cpp`). 훅 안에서는 빌트인을 부르지 않는다.
  - 실행 묶음이 죽으면 `*.kept`가 남는다. `session.ps1 -Action stop`이 되돌린다.
```

`README.md`의 "상태 보기" 절 앞에 절을 더한다.

```markdown
## 켜 둔 게임에 묻기 (개발용)

    pwsh -File tools/session.ps1 -Action start
    & tools\ask.ps1 -Lines 'state', 'find name=gold value=3000', 'shot hud'
    pwsh -File tools/session.ps1 -Action stop -Name my-session

게임을 켜 둔 채 값을 읽고, 쓰고, 이름·값으로 찾고, 화면을 뜨고, 스크립트의 호출을 기록한다. 명령은 `src/core/RemoteCommand.hpp`에 있다.
```

README 의 "문서" 목록에 `- `research/07-remote.md` — 원격 질의와 호출 기록기`를 더한다.

- [ ] **Step 8: 커밋하고 `develop`에 넣는다**

```powershell
git -C E:\NlToyBox add research/07-remote.md CLAUDE.md README.md
git -C E:\NlToyBox commit -m "docs(remote): 원격 질의와 호출 기록기에서 잰 것과 쓰는 법"
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"
git -C E:\NlToyBox checkout develop
git -C E:\NlToyBox merge --no-ff feat/cheat-remote -m "Merge branch 'feat/cheat-remote' into develop"
git -C E:\NlToyBox branch -d feat/cheat-remote
```

---

## Self-Review

- **스펙 대조**: §14 의 통로(Task 5, 6), 명령(Task 1, 5), 호출 기록기(Task 2, 3), 부르기의 규칙(Task 5 의 `DoCall`·`DoMethod`가 부르기 전에 한 줄을 남긴다. 규칙 자체는 Global Constraints 와 `CLAUDE.md`), 실행 묶음(Task 6). §10 의 3가 "끝났다는 것"은 Task 7 Step 3~5.
- **빈칸**: Task 7 Step 7 의 `research/07`은 그 실행의 답을 옮기는 일이라 내용을 미리 적을 수 없다. 들어갈 항목을 적었다.
- **이름의 일치**: `NlRecorder::Watch(Target, Name, Why)`·`Unwatch(Name)`·`Report(Name)`, `NlUi::RequestShot(File)`·`SetVisible(Visible)`, `NlRemote::Init(ModuleDir, Version, Log)`·`GameTick()`, `ParseRemoteLine`·`OptionNumber`, `CallLog::Note`가 번호를 돌려주는 것이 쓰는 곳과 만드는 곳에서 같다.
- **Review Focus**: 1·2·4 는 Task 1, 3 은 Task 2, 5 는 Task 6 의 시험이 잡는다.
