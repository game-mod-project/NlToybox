# 치트 메뉴 3나-1: 경제 패널(금화, 자원) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 모드창의 "경제" 영역에서 금화를 더하고 맞추며, 영지 창고의 자원을 더하고 맞춘다. 값을 바로 쓰지 않고 게임의 함수로 바꾼다
(금화는 화면이 함께 바뀌는 것을 봤다. 자원의 화면은 Task 4 에서 잰다).

**Architecture:** 창(`NlEconomy::Draw`)은 명령만 쌓고, 게임 스레드의 틱(`NlEconomy::GameTick`)이 지금 값을 읽어 명령을 변화량으로 풀고(`NlCore::PlanEconomy`. 러너와 무관, 시험한다) 게임의 함수를 부른다: 금화는 스크립트 `gml_Script_budget_money_change(변화량)`, 자원은 영지 창고의 메서드 `change(자원 번호, 변화량)`. 원격 명령 `economy`가 같은 길을 창 없이 태운다(게임을 켠 확인에서 쓴다).

**Tech Stack:** C++20(MSVC), YYToolkit v5 인터페이스, Dear ImGui, 네이티브 시험(`tests/native/core_tests.cpp`).

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §3(수단 C), §8(경제), §10(3나), §14. 잰 것: `research/07-remote.md`.

## 무엇을 넣고 무엇을 미루는가

스펙 §10 의 3나는 "경제, 건설·생산, 첫 훅, 기존 배율 7개를 표로"다. 이 계획은 그 가운데 **자리와 함수를 잰 것만** 넣는다(추측으로 만들지 않는다).

| 것 | 수단 | 이 계획 |
|---|---|---|
| 금화 더하기·맞추기 | C: `budget_money_change` (화면에서 확인) | 넣는다 |
| 자원 더하기·맞추기, 모든 자원 | C: `…__warehouse.change` (건물 창고에서 같은 함수를 확인. 영지 창고는 묶어 불러야 한다. 확인 전) | 넣는다. Task 4 에서 먼저 원격으로 확인 |
| 창고 용량 | B: `…__cached_total_capacity_for_storage_type.<갈래>` (읽기만 했다) | **미룬다**(코드 검토: 게임이 다시 채우는 캐시라 "원래대로"가 낡은 값을 쓴다). 패널은 읽어서 보이기만 한다. Task 4 에서 잰다 |
| 거래 가격, 임금, 건설비, 생산 | 자리의 이름만 봤다. 어느 값이 쓰이는지 모른다 | **미룬다.** Task 4 의 실행에서 자리를 재고 3나-2 로 만든다 |
| 기존 배율 7개를 표로 | — | **미룬다**(3나-2. 사용자에게 새로 되는 것이 없다) |
| 즉시 건설 | A: `o_debug.is_instant_build_buildings` | 사용자가 플레이에서 봤다 → `Verified` |
| 모든 건물 건설 가능 | A: `o_debug.is_can_build_all_buildings` | 목록만 풀린다(사용자가 봤다). 이름과 설명을 고친다. 조건을 푸는 것은 3나-2 |
| 자원 편집 모드 | A: `o_debug.is_resources_edit_mode` | 쓸 수 없다(사용자가 봤다). 표에서 뺀다. 이 패널이 맡는다 |

## 계획 검토와 구현 뒤에 바뀐 것

Task 1~3 은 구현했다(커밋 `a8f06b6`, `04b7936`, Task 3 의 커밋). 독립 검토("고친 뒤 진행", BLOCKER 1)와 사용자의 확인을 반영해
아래 Task 1~3 의 본문과 달라진 곳은 다음과 같다. **`src/Economy.cpp`의 블록은 실제 파일과 같게 맞췄다.** 나머지는 커밋이 기준이다.

- 브랜치: Task 1 앞에서 `git -C E:\NlToyBox switch -c feat/cheat-economy`(`feat/cheat-remote` 위에 쌓는다).
- Task 1: `NlCore::Thousands`(`src/core/Text`)와 그 시험을 더했다. `Shortest`는 100000 을 `1e+05`로 쓴다. 창의 수는 `Thousands`, 로그의 수는 `Fixed(…, 0)`. 시험 65 → 69.
- Task 2: `economy`의 `resource`에 상한(1000 미만)을 뒀다(`resource=1e300`이 정수로 바뀌지 않게). 원격 명령 `page <영역의 키>`를 더했다(`NlMenu::SetPage`). 패널의 화면을 뜨려면 영역을 골라야 한다.
- Task 3: 치트 표의 수는 31 → 30 이다(`resources_edit_mode`를 뺐다. `cap_*` 여섯은 코드 검토 뒤에 다시 뺐다). 시험 파일의 그 줄을 고치고 `instant_build`의 `Verified`,
  `resources_edit_mode`와 `cap_food`가 없는 것을 확인하는 줄을 더했다. 갈래의 차례는 게임의 `__categories_names`를 따른다. 부르기 전의 로그를 호출마다 남기고,
  `change`가 돌려준 적용된 변화량이 청한 것과 다르면 로그와 창에 알린다. 다시 읽지 못하면 쌓인 명령을 버린다. `Do`의 답에 자원의 앞뒤 수를 넣었다.
- `NlAccess::CallMethod`는 묶인 곳도 묶을 곳도 없는 메서드를 부르지 않는다(`feat/cheat-remote`의 `083ed4f`).
- 코드 검토(Critical 0, Important 8) 뒤: `PlanEconomy(Command, Gold, Counts, Free, Stocked)` — 넘기는 변화량은 언제나 유한한 정수다(`Settle`). 읽은 수가 수가 아니면 하지 않는다.
  줄일 때는 예약되지 않은 수(`__no_reserve__`)까지만. 갈래에 없는 자원(0번)은 하나씩으로도 건드리지 않는다. 같은 자원을 두 번 하지 않는다.
  청한 만큼 바뀌었는지는 함수의 반환값이 아니라 앞뒤의 수로 본다(`EconomyShortfall`). 금화의 입력 칸은 지금 금화로 채운다(0 인 채 "맞추기"를 누르면 금화가 사라진다).
  읽지 못한 까닭을 가려서 보인다(게임 화면이 아니다 / 금화의 자리 / 창고의 자리). `economy gold_add|gold_set|all`은 `resource=`를 받지 않는다. 시험 69 → 70.

## Global Constraints

- 러너를 건드리는 호출은 게임 스레드의 틱에서만 한다. `Draw*`에서 러너를 부르지 않는다. `RValue`를 정적으로 두거나 틱 너머로 들지 않는다.
- 게임 스크립트는 `gml_Script_` 이름으로만 부른다. 인자의 수와 형은 `research/07`에서 본 그대로만 쓴다:
  `budget_money_change(수)`, `change(수, 수)`. 다른 꼴로 부르지 않는다.
- 부르기 전에 로그에 한 줄을 남긴다(틀린 호출이 게임을 끝내면 어디였는지 남는다).
- 없는 것을 만들지 않는다. 자리나 메서드가 읽히지 않으면 하지 않고 까닭을 보인다.
- 모드창의 글에는 한글과 라틴-1 만 쓴다(화살표, 별 같은 기호를 쓰지 않는다).
- 주소에 ds 번호를 적지 않는다. 게임 경로를 스크립트에 적지 않는다.
- `Verified`는 플레이에서 효과를 본 뒤에만 참으로 바꾼다. 자원의 이름(한글)은 이 레포가 붙인 것이다(게임 파일의 번역을 옮기지 않는다).
- 스크립트는 `pwsh`, Python 은 `py -3.14`, git 은 `git -C E:\NlToyBox`. 커밋 끝에 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- 게임을 켜는 횟수는 사용자에게 승인받은 만큼만 쓴다.

## Review Focus

1. 금화가 0 인 채 "-1,000"처럼 줄이는 명령 → 0 아래로 내리지 않는다(Task 1 의 시험).
2. 자원 번호가 범위를 벗어난 원격 명령(`resource=99`) → 아무것도 부르지 않는다(Task 1 의 시험. `PlanEconomy`가 빈 목록을 준다).
3. `amount=nan`·`inf`·`1e12` → 아무것도 부르지 않는다(Task 1 의 시험).
4. 메뉴(게임 화면이 아님)에서 버튼이나 원격 명령 → 부르지 않고 "게임 화면이 아닙니다"(Task 3. 게임을 켠 확인에서 본다).
5. "모든 자원"이 갈래에 없는 자원(0번 `rune`)을 건드리지 않는다(Task 1 의 시험. `Stocked`에 든 것만).

---

### Task 1: 코어 — 자원의 이름과 명령을 변화량으로 푸는 일

**Files:**
- Create: `src/core/EconomyPlan.hpp`, `src/core/EconomyPlan.cpp`
- Modify: `CMakeLists.txt`(nlcore 에 `src/core/EconomyPlan.cpp`), `tests/native/core_tests.cpp`

**Interfaces:**
- Produces: `NlCore::ResourceKey`, `ResourceLabel`, `CategoryLabel`, `EconomyAct`, `EconomyCommand`, `ParseEconomyAct`, `NeedsResource`, `EconomyChange`, `PlanEconomy`.

- [ ] **Step 1: 실패하는 시험을 쓴다**

`tests/native/core_tests.cpp`의 include 에 `#include "core/EconomyPlan.hpp"`를 더하고, `Test("치트 상태: 읽고 쓰면 같다"` 앞에 끼운다:

```cpp
	Test("경제: 자원의 이름과 갈래의 이름", [] {
		CHECK_STR(ResourceKey("resource.wood"), "wood");
		CHECK_STR(ResourceKey("wood"), "wood");
		CHECK_STR(ResourceKey(""), "");
		CHECK_STR(ResourceLabel("wood"), "나무 (wood)");
		CHECK_STR(ResourceLabel("something_new"), "something_new");
		CHECK_STR(CategoryLabel("food"), "음식");
		CHECK_STR(CategoryLabel("unknown"), "unknown");
	});

	Test("경제: 명령을 변화량으로 푼다", [] {
		const std::vector<double> counts = { 0, 300, 0, 5 };
		const std::vector<int> stocked = { 1, 2, 3 };
		const auto one = [&](EconomyAct act, int resource, double amount, double gold = 3000) {
			return PlanEconomy({ act, resource, amount }, gold, counts, stocked);
		};
		// 금화: 더하기는 그대로, 맞추기는 차이만큼
		const auto add = one(EconomyAct::GoldAdd, -1, 1000);
		CHECK(add.size() == 1 && add[0].Resource == -1 && add[0].Delta == 1000);
		CHECK(one(EconomyAct::GoldSet, -1, 5000)[0].Delta == 2000 && one(EconomyAct::GoldSet, -1, 100)[0].Delta == -2900);
		CHECK(one(EconomyAct::GoldSet, -1, 3000).empty());					// 이미 그 수다
		CHECK(one(EconomyAct::GoldSet, -1, -50)[0].Delta == -3000);			// 음수로 맞추지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, -5000)[0].Delta == -3000);		// 0 아래로 내리지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, -1000, 0).empty());				// 0 에서 더 내리지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, -10, -50).empty());				// 이미 음수면 더 내리지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, 100, -50)[0].Delta == 100);
		CHECK(one(EconomyAct::GoldAdd, -1, 0.4).empty() && one(EconomyAct::GoldAdd, -1, 1.6)[0].Delta == 2);	// 정수로
		// 자원
		const auto wood = one(EconomyAct::ResourceAdd, 1, 50);
		CHECK(wood.size() == 1 && wood[0].Resource == 1 && wood[0].Delta == 50);
		CHECK(one(EconomyAct::ResourceAdd, 3, -100)[0].Delta == -5);
		CHECK(one(EconomyAct::ResourceSet, 1, 120)[0].Delta == -180 && one(EconomyAct::ResourceSet, 2, 7)[0].Delta == 7);
		CHECK(one(EconomyAct::ResourceAdd, 4, 10).empty() && one(EconomyAct::ResourceAdd, -1, 10).empty() && one(EconomyAct::ResourceSet, 99, 10).empty());
		// 모든 자원: 갈래에 든 것만(0번은 들지 않았다), 변화가 있는 것만
		const auto all = one(EconomyAct::AllAdd, -1, 100);
		CHECK(all.size() == 3 && all[0].Resource == 1 && all[1].Resource == 2 && all[1].Delta == 100 && all[2].Resource == 3);
		const auto less = one(EconomyAct::AllAdd, -1, -5);
		CHECK(less.size() == 2 && less[0].Resource == 1 && less[0].Delta == -5 && less[1].Resource == 3 && less[1].Delta == -5);
		// 수가 아니거나 터무니없으면 아무것도 하지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, std::numeric_limits<double>::quiet_NaN()).empty());
		CHECK(one(EconomyAct::GoldAdd, -1, std::numeric_limits<double>::infinity()).empty() && one(EconomyAct::AllAdd, -1, 1e12).empty());
	});

	Test("경제: 원격 명령의 낱말", [] {
		EconomyAct act = EconomyAct::GoldAdd;
		CHECK(ParseEconomyAct("gold_set", act) && act == EconomyAct::GoldSet && !NeedsResource(act));
		CHECK(ParseEconomyAct("add", act) && act == EconomyAct::ResourceAdd && NeedsResource(act));
		CHECK(ParseEconomyAct("set", act) && act == EconomyAct::ResourceSet && NeedsResource(act));
		CHECK(ParseEconomyAct("all", act) && act == EconomyAct::AllAdd && !NeedsResource(act));
		CHECK(ParseEconomyAct("gold_add", act) && act == EconomyAct::GoldAdd && !ParseEconomyAct("gold", act) && !ParseEconomyAct("", act));
	});

```

- [ ] **Step 2: 빌드해서 실패를 본다**

Run: `pwsh -NoProfile -File tools/build.ps1`
Expected: FAIL — `core/EconomyPlan.hpp`를 열 수 없다(C1083).

- [ ] **Step 3: 구현한다**

`src/core/EconomyPlan.hpp`:

```cpp
#pragma once
// 경제 패널의 러너에 기대지 않는 부분: 자원의 이름, 명령을 변화량으로 푸는 일. tests/native 가 직접 부른다.
// 스펙: 치트 메뉴 §8(경제). 자리와 함수는 research/07.

#include <string>
#include <vector>

namespace NlCore
{
	// "resource.wood" → "wood". 접두가 없으면 그대로.
	std::string ResourceKey(const std::string& Caption);
	// 창에 보일 이름: "나무 (wood)". 모르는 키는 키 그대로. 한글 이름은 이 레포가 붙인 것이다.
	std::string ResourceLabel(const std::string& Key);
	// 창고 갈래의 이름: "food" → "음식". 모르는 키는 키 그대로.
	std::string CategoryLabel(const std::string& Key);

	enum class EconomyAct { GoldAdd, GoldSet, ResourceAdd, ResourceSet, AllAdd };

	struct EconomyCommand
	{
		EconomyAct Act = EconomyAct::GoldAdd;
		int Resource = -1;		// ResourceAdd, ResourceSet 의 자원 번호
		double Amount = 0;		// 더할 수, 또는 맞출 수
	};

	// 원격 명령의 낱말: gold_add, gold_set, add, set, all. 모르는 낱말이면 거짓.
	bool ParseEconomyAct(const std::string& Word, EconomyAct& Out);
	// 그 명령이 자원 번호를 받는가(add, set).
	bool NeedsResource(EconomyAct Act);

	struct EconomyChange
	{
		int Resource = -1;		// -1 이면 금화
		double Delta = 0;
	};

	// 지금의 수를 보고 명령을 변화량들로 푼다. 수는 정수로 맞추고, 0 인 변화는 뺀다. 줄일 때 0 아래로 내려가지 않게 한다.
	// Counts: 자원 번호 → 지금 수. Stocked: "모든 자원"에 드는 자원 번호들(창고의 갈래에 든 것).
	std::vector<EconomyChange> PlanEconomy(const EconomyCommand& Command, double Gold, const std::vector<double>& Counts,
		const std::vector<int>& Stocked);
}
```

`src/core/EconomyPlan.cpp`:

```cpp
#include "EconomyPlan.hpp"

#include <algorithm>
#include <cmath>

namespace NlCore
{
	namespace
	{
		struct Named
		{
			const char* Key;
			const char* Label;
		};

		// 키는 global.__resource_caption 에서 봤다(research/07). 한글 이름은 이 레포가 붙였다.
		constexpr Named k_Resources[] = {
			{ "rune", "룬" }, { "wood", "나무" }, { "food", "음식" }, { "beer", "맥주" }, { "iron", "철" }, { "instruments", "도구" },
			{ "light_armor", "경갑" }, { "heavy_armor", "중갑" }, { "bow", "활" }, { "crossbow", "석궁" }, { "wooden_hammer", "나무 망치" },
			{ "wooden_spear", "나무 창" }, { "sword", "검" }, { "battle_axe", "전투 도끼" }, { "knife", "단검" }, { "shield", "방패" },
			{ "medicine", "약" }, { "coal", "석탄" }, { "nectar", "넥타" }, { "paper", "종이" }, { "hop", "홉" }, { "rye", "호밀" },
			{ "steel", "강철" }, { "meat", "고기" }, { "herb", "약초" }, { "sweden", "순무" }, { "moonshine", "밀주" }, { "ale", "에일" },
			{ "carrot", "당근" }, { "stone", "돌" }, { "instruments_steel", "강철 도구" }, { "firewood", "장작" }, { "wood_blanks", "목재" },
			{ "berry", "열매" }, { "herb_drink", "약초 음료" }, { "clay_tile", "점토 타일" }, { "clay_pot", "점토 항아리" },
			{ "clay_roof_tile", "점토 기와" }, { "stone_tile", "석재 타일" },
		};

		// 키는 o_data.__resource_categories_data.__categories 에서 봤다(research/07).
		constexpr Named k_Categories[] = {
			{ "food", "음식" }, { "liquid", "액체" }, { "resources", "물자" }, { "armory", "전쟁 물자" }, { "herbs", "약초·작물" }, { "raw", "원자재" },
		};

		constexpr double k_MaxAmount = 1e9;		// 이보다 큰 수는 잘못 친 것으로 본다

		// 정수로 맞춘 변화량. 줄일 때 지금 수가 0 아래로 내려가지 않게 한다(이미 음수면 더 내리지 않는다).
		double Bounded(double Current, double Delta)
		{
			Delta = std::round(Delta);
			if (Delta < 0 && Current + Delta < 0)
				return std::min(0.0, -Current);
			return Delta;
		}
	}

	std::string ResourceKey(const std::string& Caption)
	{
		static const std::string prefix = "resource.";
		return Caption.compare(0, prefix.size(), prefix) == 0 ? Caption.substr(prefix.size()) : Caption;
	}

	std::string ResourceLabel(const std::string& Key)
	{
		for (const Named& named : k_Resources)
			if (Key == named.Key)
				return std::string(named.Label) + " (" + Key + ")";
		return Key;
	}

	std::string CategoryLabel(const std::string& Key)
	{
		for (const Named& named : k_Categories)
			if (Key == named.Key)
				return named.Label;
		return Key;
	}

	bool ParseEconomyAct(const std::string& Word, EconomyAct& Out)
	{
		static const struct
		{
			const char* Word;
			EconomyAct Act;
		} acts[] = {
			{ "gold_add", EconomyAct::GoldAdd }, { "gold_set", EconomyAct::GoldSet }, { "add", EconomyAct::ResourceAdd },
			{ "set", EconomyAct::ResourceSet }, { "all", EconomyAct::AllAdd },
		};
		for (const auto& act : acts)
			if (Word == act.Word)
			{
				Out = act.Act;
				return true;
			}
		return false;
	}

	bool NeedsResource(EconomyAct Act)
	{
		return Act == EconomyAct::ResourceAdd || Act == EconomyAct::ResourceSet;
	}

	std::vector<EconomyChange> PlanEconomy(const EconomyCommand& Command, double Gold, const std::vector<double>& Counts,
		const std::vector<int>& Stocked)
	{
		std::vector<EconomyChange> changes;
		if (!std::isfinite(Command.Amount) || std::fabs(Command.Amount) > k_MaxAmount)
			return changes;

		const auto add = [&](int resource, double delta) {
			if (delta != 0)
				changes.push_back({ resource, delta });
		};
		const auto valid = [&](int resource) { return resource >= 0 && static_cast<size_t>(resource) < Counts.size(); };
		const double target = std::max(0.0, std::round(Command.Amount));

		switch (Command.Act)
		{
		case EconomyAct::GoldAdd:
			add(-1, Bounded(Gold, Command.Amount));
			break;
		case EconomyAct::GoldSet:
			add(-1, target - Gold);
			break;
		case EconomyAct::ResourceAdd:
			if (valid(Command.Resource))
				add(Command.Resource, Bounded(Counts[Command.Resource], Command.Amount));
			break;
		case EconomyAct::ResourceSet:
			if (valid(Command.Resource))
				add(Command.Resource, target - Counts[Command.Resource]);
			break;
		case EconomyAct::AllAdd:
			for (const int resource : Stocked)
				if (valid(resource))
					add(resource, Bounded(Counts[resource], Command.Amount));
			break;
		}
		return changes;
	}
}
```

`CMakeLists.txt`의 nlcore 목록에서 `src/core/CheatTable.cpp` 다음 줄에 `  src/core/EconomyPlan.cpp`를 더한다.

- [ ] **Step 4: 시험이 통과하는지 본다**

Run: `pwsh -NoProfile -File tools/build.ps1; pwsh -NoProfile -File tools/test-native.ps1`
Expected: `core tests: 66 passed`

- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add src/core/EconomyPlan.hpp src/core/EconomyPlan.cpp CMakeLists.txt tests/native/core_tests.cpp
git -C E:\NlToyBox commit -m "feat(core): 경제 명령을 변화량으로 푼다 — 금화·자원 더하기와 맞추기, 자원의 이름"
```

---

### Task 2: 원격 명령 `economy`

**Files:**
- Modify: `src/core/RemoteCommand.hpp`, `src/core/RemoteCommand.cpp`, `tests/native/core_tests.cpp`

**Interfaces:**
- Consumes: `NlCore::ParseEconomyAct`, `NeedsResource`(Task 1).
- Produces: `ParseRemoteLine("economy <낱말> amount=<수> [resource=<번호>]")` → `Verb == "economy"`, `Target == <낱말>`, `Number == <수>`, `Options["resource"]`.

- [ ] **Step 1: 실패하는 시험을 쓴다**

`Test("원격 명령: 줄을 읽는다"`의 끝(`about` 검사 뒤)에 더한다:

```cpp
		const RemoteCommand gold = ParseRemoteLine("economy gold_add amount=1000");
		CHECK(gold.Error.empty() && gold.Verb == "economy" && gold.Target == "gold_add" && gold.Number == 1000);
		const RemoteCommand wood = ParseRemoteLine("economy add resource=1 amount=-5");
		CHECK(wood.Error.empty() && wood.Target == "add" && wood.Number == -5 && OptionNumber(wood, "resource", -1) == 1);
		CHECK(ParseRemoteLine("economy all amount=100").Error.empty());
```

`Test("원격 명령: 읽을 수 없으면 오류를 낸다"`의 목록 끝에 더한다:
`"economy", "economy gold", "economy gold_add", "economy gold_add amount=x", "economy add amount=5", "economy add resource=1.5 amount=5", "economy set resource=-1 amount=5"`

- [ ] **Step 2: 시험이 실패하는지 본다**

Run: `pwsh -NoProfile -File tools/build.ps1; pwsh -NoProfile -File tools/test-native.ps1`
Expected: FAIL — `원격 명령: 줄을 읽는다` (`unknown command: economy`)

- [ ] **Step 3: 구현한다**

`src/core/RemoteCommand.cpp`: include 에 `#include "EconomyPlan.hpp"`와 `#include <cmath>`를 더하고, `else if (verb == "call" || verb == "method")` 앞에 끼운다:

```cpp
		else if (verb == "economy")
		{
			EconomyAct act = EconomyAct::GoldAdd;
			if (count < 2 || !ParseEconomyAct(tokens[1], act))
				return fail("economy needs gold_add, gold_set, add, set or all");
			command.Target = tokens[1];
			if (!options(2))
				return command;
			const auto amount = command.Options.find("amount");
			if (amount == command.Options.end() || !ParseNumber(amount->second, command.Number))
				return fail("economy needs amount=<number>");
			double resource = 0;
			const auto given = command.Options.find("resource");
			if (NeedsResource(act)
				&& (given == command.Options.end() || !ParseNumber(given->second, resource) || resource < 0 || resource != std::floor(resource)))
				return fail("economy " + tokens[1] + " needs resource=<index>");
		}
```

`src/core/RemoteCommand.hpp`의 머리 주석에서 `//   state,  shot <이름>,  window open|close` 앞에 더한다:

```cpp
//   economy <gold_add|gold_set|all> amount=<수>     금화를 더한다·맞춘다, 모든 자원을 더한다(경제 패널과 같은 길)
//   economy <add|set> resource=<번호> amount=<수>   자원 하나를 더한다·맞춘다
```

- [ ] **Step 4: 시험이 통과하는지 본다**

Run: `pwsh -NoProfile -File tools/build.ps1; pwsh -NoProfile -File tools/test-native.ps1`
Expected: `core tests: 66 passed`

- [ ] **Step 5: 커밋**

```powershell
git -C E:\NlToyBox add src/core/RemoteCommand.hpp src/core/RemoteCommand.cpp tests/native/core_tests.cpp
git -C E:\NlToyBox commit -m "feat(core): 원격 명령 economy — 경제 패널의 길을 창 없이 태운다"
```

---

### Task 3: 모듈 — 경제 패널 (0.6.0)

**Files:**
- Create: `src/Economy.hpp`, `src/Economy.cpp`
- Modify: `src/Game.hpp`, `src/Game.cpp`(`CallScript`), `src/Menu.cpp`, `src/Remote.cpp`, `src/ModuleMain.cpp`, `src/core/CheatTable.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `NlCore::PlanEconomy` 등(Task 1), `RemoteCommand`의 `economy`(Task 2), `NlAccess::CallMethod`·`List`·`ReadNumber`·`InGame`, `NlCore::ScriptRoutineName`.
- Produces: `NlGame::CallScript(Name, Args, Result)`, `NlEconomy::Init(Log)`, `GameTick(Now, Active)`, `Draw()`, `Do(Command)`.

러너에 닿는 코드라 네이티브 시험이 없다. 논리는 Task 1 이 시험했고, 이 Task 는 빌드와 Task 4 의 실행으로 확인한다.

- [ ] **Step 1: 게임 스크립트를 부르는 층**

`src/Game.hpp`의 `double CallNumber(` 선언 뒤에 더한다:

```cpp

	// 게임 스크립트를 정식 이름("gml_Script_x". 접두가 없으면 붙인다)으로 부른다. self 와 other 는 전역이다.
	// 인자의 수와 형은 부르는 쪽이 책임진다: 틀리면 게임이 GML 오류로 끝난다. 없는 스크립트면 거짓.
	bool CallScript(const std::string& Name, const std::vector<YYTK::RValue>& Args, YYTK::RValue& Result);
```

`src/Game.cpp`의 `bool NlGame::IsNumber(` 앞에 끼운다:

```cpp
bool NlGame::CallScript(const std::string& Name, const std::vector<RValue>& Args, RValue& Result)
{
	// 번호가 100000 미만이면 빌트인, 500000 이상이면 확장 함수다(YYToolkit MI_Public.cpp 55~66행).
	const std::string name = NlCore::ScriptRoutineName(Name);
	CInstance* global = Global();
	int index = -1;
	if (!global || name.empty() || !AurieSuccess(g_Yytk->GetNamedRoutineIndex(name.c_str(), &index)) || index < 100000 || index >= 500000)
		return false;
	return AurieSuccess(g_Yytk->CallGameScriptEx(Result, name, global, global, Args));
}
```

- [ ] **Step 2: 머리 파일을 쓴다**

`src/Economy.hpp`:

```cpp
#pragma once
// "경제" 영역의 위쪽: 금화와 영지 창고의 자원. 값을 바로 쓰지 않고 게임의 함수로 바꾼다(그래야 화면이 따라온다. research/07).
// 창(Draw)은 명령만 쌓고, 게임 스레드의 틱이 지금 값을 읽어 명령을 변화량으로 풀고(core/EconomyPlan) 부른다.

#include "core/EconomyPlan.hpp"

#include <functional>
#include <string>

namespace NlEconomy
{
	using LogFn = std::function<void(const std::string&)>;

	void Init(LogFn Log);

	// 게임 스레드의 틱. Active: 경제 패널이 보이는가(보일 때만 값을 다시 읽는다). 쌓인 명령은 보이지 않아도 한다.
	void GameTick(double Now, bool Active);

	// 패널을 그린다. 러너를 부르지 않는다.
	void Draw();

	// 명령 하나를 지금 한다. 게임 스레드에서만 부른다(원격 명령 economy). 돌려주는 글: 한 일.
	std::string Do(const NlCore::EconomyCommand& Command);
}
```

- [ ] **Step 3: 구현한다**

`src/Economy.cpp`:

```cpp
#include "Economy.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <deque>
#include <mutex>
#include <vector>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::EconomyAct;
using NlCore::EconomyCommand;
using NlCore::Fixed;
using NlCore::Thousands;

namespace
{
	// 자리와 함수는 research/07 에서 잰 것이다(새 게임, 0.5588.9777.0). 불러온 세이브에서 같은 자리인지는 재지 않았다.
	constexpr const char* k_Gold = "inst:o_game_map_controller.__province.__budget.__budget.__no_reserve__";
	constexpr const char* k_GoldChange = "gml_Script_budget_money_change";		// (변화량). +100 으로 화면까지 확인했다
	constexpr const char* k_Counts = "inst:o_game_map_controller.__province.__warehouse.__warehouse.__total__";
	constexpr const char* k_Free = "inst:o_game_map_controller.__province.__warehouse.__warehouse.__no_reserve__";	// 예약되지 않은 수
	// (자원 번호, 변화량). 건물 창고에서 같은 함수가 (1, 5) -> 5, (1, -10) -> -10 이었다. 영지 창고에서는 아직 부르지 않았다.
	// 묶인 곳이 없으면 CallMethod 가 창고(주소의 부모)에 묶어 부른다.
	constexpr const char* k_Change = "inst:o_game_map_controller.__province.__warehouse.change";
	constexpr const char* k_Captions = "global.__resource_caption";
	constexpr const char* k_Categories = "inst:o_data.__resource_categories_data.__categories";
	constexpr const char* k_CategoryNames = "inst:o_data.__resource_categories_data.__categories_names";	// 갈래의 차례(게임이 둔 배열)
	constexpr const char* k_Capacity = "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type";

	struct Group
	{
		std::string Key, Label;
		double Capacity = -1;				// 갈래의 용량(읽기만 한다). 못 읽으면 음수
		std::vector<int> Resources;
	};

	struct Snapshot			// 틱이 채우고 Draw 가 읽는다. RValue 를 담지 않는다
	{
		bool Ready = false;
		std::string Why;					// Ready 가 아닌 까닭
		double Gold = 0;
		std::vector<double> Counts;			// 자원 번호 → 영지 창고의 수
		std::vector<double> Free;			// 자원 번호 → 예약되지 않은 수
		std::vector<std::string> Labels;	// 자원 번호 → 창에 보일 이름
		std::vector<Group> Groups;
		std::vector<int> Stocked;			// 갈래에 든 자원 번호들
		std::string Last;					// 마지막으로 한 일
	};

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlEconomy::LogFn g_Log;
	Snapshot g_Now;
	std::deque<EconomyCommand> g_Queue;	// 창이 쌓고 틱이 한다
	double g_NextRead = 0;
	double g_GoldInput = 0;				// 창의 입력 칸
	bool g_GoldInputSet = false;		// 입력 칸을 지금 금화로 채웠는가(0 으로 맞추는 실수를 막는다)

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 로그에 쓰는 정수("+1000", "-5"). 넘기는 변화량은 정수다(core/EconomyPlan).
	std::string Signed(double Value)
	{
		return (Value >= 0 ? "+" : "") + Fixed(Value, 0);
	}

	// ---- 게임 스레드 ----

	bool ListRows(const NlCore::AskPath& Path, std::vector<NlAccess::Row>& Rows)
	{
		size_t total = 0;
		std::string why;
		Rows.clear();
		return NlAccess::List(Path, Holder::None, 256, Rows, total, why);
	}

	bool ReadNumbers(const char* Path, std::vector<double>& Out)
	{
		std::vector<NlAccess::Row> rows;
		Out.clear();
		if (!ListRows(NlCore::ParseAskPath(Path), rows))
			return false;
		for (const NlAccess::Row& row : rows)
			Out.push_back(row.IsNumber ? row.Number : 0);
		return !Out.empty();
	}

	// 이름과 갈래. 게임마다 한 번 읽는다(자원의 수가 달라지면 다시).
	void LoadTables(size_t Count)
	{
		std::vector<NlAccess::Row> rows, members;
		ListRows(NlCore::ParseAskPath(k_Captions), rows);
		g_Now.Labels.assign(Count, "");
		for (size_t i = 0; i < Count; i++)
			g_Now.Labels[i] = i < rows.size() && rows[i].IsString ? NlCore::ResourceLabel(NlCore::ResourceKey(rows[i].Raw)) : "#" + std::to_string(i);

		g_Now.Groups.clear();
		g_Now.Stocked.clear();
		const NlCore::AskPath categories = NlCore::ParseAskPath(k_Categories);
		ListRows(NlCore::ParseAskPath(k_CategoryNames), rows);
		for (const NlAccess::Row& row : rows)
		{
			if (!row.IsString)
				continue;
			Group group;
			group.Key = row.Raw;
			group.Label = NlCore::CategoryLabel(row.Raw);
			const NlCore::AskPath category = NlCore::ChildPath(categories, { '.', row.Raw, 0 });
			if (!ListRows(NlCore::ChildPath(category, { '.', "__resources_in_category", 0 }), members))
				continue;
			for (const NlAccess::Row& member : members)
				if (member.IsNumber && member.Number >= 0 && member.Number < static_cast<double>(Count))
				{
					group.Resources.push_back(static_cast<int>(member.Number));
					g_Now.Stocked.push_back(static_cast<int>(member.Number));
				}
			if (!group.Resources.empty())
				g_Now.Groups.push_back(std::move(group));
		}
		Log("economy tables: " + std::to_string(Count) + " resources, " + std::to_string(g_Now.Groups.size()) + " categories, "
			+ std::to_string(g_Now.Stocked.size()) + " stocked");
	}

	// 지금 값을 읽는다. 읽지 못하면 거짓이고 g_Now.Why 에 까닭(게임 화면이 아니다, 어느 자리가 없다).
	bool Refresh()
	{
		double gold = 0;
		std::vector<double> counts, free;
		std::string why;
		if (!NlAccess::InGame())
			why = "게임 화면이 아닙니다";
		else if (!NlAccess::ReadNumber(k_Gold, gold))
			why = "금화의 자리를 읽지 못했습니다";
		else if (!ReadNumbers(k_Counts, counts) || !ReadNumbers(k_Free, free) || free.size() != counts.size())
			why = "영지 창고의 자리를 읽지 못했습니다";
		if (!why.empty())
		{
			if (why != g_Now.Why && NlAccess::InGame())
				Log("economy: cannot read (" + std::string(why == "금화의 자리를 읽지 못했습니다" ? "gold" : "warehouse") + ")");
			g_Now.Ready = false;
			g_Now.Why = why;
			return false;
		}

		if (!g_Now.Ready || g_Now.Labels.size() != counts.size())
			LoadTables(counts.size());

		g_Now.Gold = gold;
		g_Now.Counts = std::move(counts);
		g_Now.Free = std::move(free);
		for (Group& group : g_Now.Groups)
			if (!NlAccess::ReadNumber(std::string(k_Capacity) + "." + group.Key, group.Capacity))
				group.Capacity = -1;
		g_Now.Ready = true;
		g_Now.Why.clear();
		return true;
	}

	// 명령 하나를 하고 다시 읽는다. Refresh 가 참을 돌려준 바로 뒤에 부른다. 돌려주는 글: 한 일.
	// 다시 읽지 못하면 g_Now.Ready 가 거짓이 된다(부른 쪽이 본다).
	std::string Execute(const EconomyCommand& Command)
	{
		const std::vector<NlCore::EconomyChange> changes = NlCore::PlanEconomy(Command, g_Now.Gold, g_Now.Counts, g_Now.Free, g_Now.Stocked);
		if (changes.empty())
			return "바꿀 것이 없습니다";

		const double gold = g_Now.Gold;
		const std::vector<double> counts = g_Now.Counts;
		const NlCore::AskPath change_method = NlCore::ParseAskPath(k_Change);
		std::vector<NlCore::EconomyChange> done;
		std::string why;
		for (const NlCore::EconomyChange& change : changes)
		{
			RValue result;		// 이 함수 안에서만 든다
			bool ok = false;
			// 부르기 전에 남긴다(틀린 호출이 게임을 끝내면 어디였는지 남는다).
			if (change.Resource < 0)
			{
				Log("economy call budget_money_change(" + Signed(change.Delta) + ")");
				ok = NlGame::CallScript(k_GoldChange, { RValue(change.Delta) }, result);
				if (!ok)
					why = "no such script";
			}
			else
			{
				Log("economy call warehouse.change(" + std::to_string(change.Resource) + ", " + Signed(change.Delta) + ")");
				ok = NlAccess::CallMethod(change_method, { RValue(static_cast<double>(change.Resource)), RValue(change.Delta) }, result, why);
			}
			if (!ok)
				break;			// 하나가 안 되면 나머지도 하지 않는다
			done.push_back(change);
		}
		Log("economy: called " + std::to_string(done.size()) + "/" + std::to_string(changes.size()) + (why.empty() ? "" : ": " + why));

		std::string text = std::to_string(done.size()) + "/" + std::to_string(changes.size()) + " 개를 불렀습니다" + (why.empty() ? "" : " (" + why + ")");
		if (!Refresh())
			return text + "; 다시 읽지 못했습니다";

		// 청한 만큼 바뀌었는지는 앞뒤의 수로 본다(함수의 반환값이 적용된 양인지는 모른다).
		const std::vector<NlCore::EconomyShort> shorts = NlCore::EconomyShortfall(done, gold, counts, g_Now.Gold, g_Now.Counts);
		for (const NlCore::EconomyShort& one : shorts)
			Log("economy: " + (one.Resource < 0 ? std::string("gold") : "resource " + std::to_string(one.Resource)) + " asked "
				+ Signed(one.Asked) + ", changed " + Signed(one.Applied));
		if (!shorts.empty())
			text += "; " + std::to_string(shorts.size()) + " 개는 청한 만큼 바뀌지 않았습니다(용량을 넘겼을 수 있습니다)";
		return text;
	}

	// ---- 그리는 쪽 (러너를 부르지 않는다) ----

	void Push(EconomyAct Act, int Resource, double Amount)
	{
		g_Queue.push_back({ Act, Resource, Amount });
	}
}

void NlEconomy::Init(LogFn Log_)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
}

std::string NlEconomy::Do(const EconomyCommand& Command)
{
	std::lock_guard lock(g_Mutex);
	if (!Refresh())
		return g_Now.Why;

	const bool one = NlCore::NeedsResource(Command.Act) && Command.Resource >= 0 && static_cast<size_t>(Command.Resource) < g_Now.Counts.size();
	const double gold = g_Now.Gold, count = one ? g_Now.Counts[Command.Resource] : 0;
	g_Now.Last = Execute(Command);
	if (!g_Now.Ready)
		return g_Now.Last;

	std::string line = g_Now.Last + "; gold " + Fixed(gold, 0) + " -> " + Fixed(g_Now.Gold, 0);
	if (one && static_cast<size_t>(Command.Resource) < g_Now.Counts.size())
		line += "; resource " + std::to_string(Command.Resource) + " " + Fixed(count, 0) + " -> " + Fixed(g_Now.Counts[Command.Resource], 0);
	return line;
}

void NlEconomy::GameTick(double Now, bool Active)
{
	std::lock_guard lock(g_Mutex);
	if (g_Queue.empty() && (!Active || Now < g_NextRead))
		return;
	g_NextRead = Now + 0.5;

	if (!Refresh())
	{
		if (!g_Queue.empty())
		{
			Log("economy: cannot read, dropped " + std::to_string(g_Queue.size()) + " command(s)");
			g_Queue.clear();
			g_Now.Last = g_Now.Why;
		}
		return;
	}

	while (!g_Queue.empty())
	{
		const EconomyCommand command = g_Queue.front();
		g_Queue.pop_front();
		g_Now.Last = Execute(command);
		if (!g_Now.Ready)		// 다음 명령은 바뀐 수를 보고 푼다. 읽지 못했으면 낡은 수로 하지 않는다
		{
			Log("economy: could not read back, dropped " + std::to_string(g_Queue.size()) + " command(s)");
			g_Queue.clear();
			break;
		}
	}
}

void NlEconomy::Draw()
{
	std::lock_guard lock(g_Mutex);
	if (!g_Now.Ready)
	{
		g_GoldInputSet = false;
		ImGui::TextDisabled("%s", g_Now.Why.empty() ? "게임을 시작하면 금화와 자원이 보입니다." : g_Now.Why.c_str());
		if (!g_Now.Last.empty() && g_Now.Last != g_Now.Why)
			ImGui::TextDisabled("%s", g_Now.Last.c_str());
		return;
	}
	if (!g_GoldInputSet)
	{
		g_GoldInput = g_Now.Gold;		// 빈 칸(0)인 채 "맞추기"를 누르면 금화가 모두 사라진다. 지금 금화로 채워 둔다
		g_GoldInputSet = true;
	}

	ImGui::Text("금화 %s", Thousands(g_Now.Gold).c_str());
	ImGui::SameLine();
	if (ImGui::Button("+1,000"))
		Push(EconomyAct::GoldAdd, -1, 1000);
	ImGui::SameLine();
	if (ImGui::Button("+10,000"))
		Push(EconomyAct::GoldAdd, -1, 10000);
	ImGui::SameLine();
	if (ImGui::Button("+100,000"))
		Push(EconomyAct::GoldAdd, -1, 100000);
	ImGui::SetNextItemWidth(130);
	ImGui::InputDouble("##gold", &g_GoldInput, 0, 0, "%.0f");
	ImGui::SameLine();
	if (ImGui::Button("이 금화로 맞추기"))
		Push(EconomyAct::GoldSet, -1, g_GoldInput);

	ImGui::Separator();
	ImGui::TextUnformatted("영지 창고의 자원");
	ImGui::SameLine();
	if (ImGui::Button("모든 자원 +100"))
		Push(EconomyAct::AllAdd, -1, 100);
	ImGui::SameLine();
	if (ImGui::Button("모든 자원 +1,000"))
		Push(EconomyAct::AllAdd, -1, 1000);
	ImGui::TextDisabled("게임의 함수로 더합니다. 용량을 넘기면 청한 만큼 바뀌지 않을 수 있습니다.");
	if (!g_Now.Last.empty())
		ImGui::TextDisabled("%s", g_Now.Last.c_str());

	if (ImGui::BeginTable("resources", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
	{
		for (const Group& group : g_Now.Groups)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(group.Label.c_str());
			ImGui::TableNextColumn();
			if (group.Capacity >= 0)
				ImGui::TextDisabled("용량 %s", Thousands(group.Capacity).c_str());
			ImGui::TableNextColumn();

			for (const int resource : group.Resources)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("  %s", g_Now.Labels[resource].c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(Thousands(g_Now.Counts[resource]).c_str());
				ImGui::TableNextColumn();
				ImGui::PushID(resource);
				if (ImGui::SmallButton("+10"))
					Push(EconomyAct::ResourceAdd, resource, 10);
				ImGui::SameLine();
				if (ImGui::SmallButton("+100"))
					Push(EconomyAct::ResourceAdd, resource, 100);
				ImGui::SameLine();
				if (ImGui::SmallButton("0 으로"))
					Push(EconomyAct::ResourceSet, resource, 0);
				ImGui::PopID();
			}
		}
		ImGui::EndTable();
	}
}
```

- [ ] **Step 4: 창과 원격 명령에 잇는다**

`src/Menu.cpp`: include 에 `#include "Economy.hpp"`. `NlMenu::GameTick`에서 `NlCheats::GameTick(now, visible);` 다음 줄에 `	NlEconomy::GameTick(now, visible && page == Area::Economy);`.
`NlMenu::Draw`의 `switch`에서 `case Area::Tweaks:` 줄 다음에:

```cpp
	case Area::Economy:
		NlEconomy::Draw();
		ImGui::Separator();
		NlCheats::DrawArea(page);
		break;
```

`src/Remote.cpp`: include 에 `#include "Economy.hpp"`. `void DoState()` 앞에 끼운다:

```cpp
	// 경제 패널과 같은 길로 금화·자원을 바꾼다(NlEconomy::Do).
	void DoEconomy(const RemoteCommand& C)
	{
		NlCore::EconomyCommand command;
		if (!NlCore::ParseEconomyAct(C.Target, command.Act))
		{
			Say("  : unknown economy command");
			return;
		}
		command.Amount = C.Number;
		command.Resource = static_cast<int>(NlCore::OptionNumber(C, "resource", -1));
		Say("  running economy " + C.Target + " " + Shortest(C.Number));		// 죽으면 여기까지 남는다
		Say("  " + NlEconomy::Do(command));
	}

```

`Execute`에서 `else if (C.Verb == "about")` 가지 다음에:

```cpp
		else if (C.Verb == "economy")
			DoEconomy(C);
```

`src/ModuleMain.cpp`: include 에 `#include "Economy.hpp"`, `k_Version`을 `"0.6.0"`으로, `NlMenu::Init(` 줄 앞에 `	NlEconomy::Init([](const std::string& Line) { LogLine(Line); });`.

`CMakeLists.txt`의 nltoybox 목록에서 `src/Dump.cpp` 다음 줄에 `  src/Economy.cpp`.

`src/core/CheatTable.cpp`: `resources_edit_mode` 항목(설명 줄 포함) 다음에 더한다:

```cpp
			// 창고의 갈래별 용량(저장 건물들의 합을 게임이 캐시해 둔 값). 자리는 research/07 에서 읽기만 했다. 게임이 다시 계산하면 0.5초마다 다시 써 넣는다.
			{ "cap_food", Area::Economy, "용량: 음식", "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type.food",
				N, 0, 0, 0, 1000000, false, "음식 갈래의 창고 용량으로 보인다(새 게임 250)" },
			{ "cap_liquid", Area::Economy, "용량: 액체", "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type.liquid",
				N, 0, 0, 0, 1000000, false, "액체 갈래의 창고 용량으로 보인다(새 게임 100)" },
			{ "cap_resources", Area::Economy, "용량: 물자", "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type.resources",
				N, 0, 0, 0, 1000000, false, "물자 갈래의 창고 용량으로 보인다(새 게임 50)" },
			{ "cap_armory", Area::Economy, "용량: 전쟁 물자", "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type.armory",
				N, 0, 0, 0, 1000000, false, "전쟁 물자 갈래의 창고 용량으로 보인다(새 게임 20)" },
			{ "cap_herbs", Area::Economy, "용량: 약초·작물", "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type.herbs",
				N, 0, 0, 0, 1000000, false, "약초·작물 갈래의 창고 용량으로 보인다(새 게임 300)" },
			{ "cap_raw", Area::Economy, "용량: 원자재", "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type.raw",
				N, 0, 0, 0, 1000000, false, "원자재 갈래의 창고 용량으로 보인다(새 게임 300)" },
```

- [ ] **Step 5: 빌드하고 게임을 켜지 않는 확인을 모두 돌린다**

```powershell
pwsh -NoProfile -File tools/build.ps1
pwsh -NoProfile -File tools/test-native.ps1
pwsh -NoProfile -File tools/tests/safety.tests.ps1
py -3.14 -m unittest discover -s tools/re/tests
py -3.14 -m unittest discover -s tools/overlay/tests
```

Expected: 빌드 성공, `core tests: 66 passed`(치트 표의 시험이 새 항목 6개의 주소와 Id 를 본다), `safety tests: 28 passed`, `OK` 둘.

- [ ] **Step 6: 커밋**

```powershell
git -C E:\NlToyBox add src/Economy.hpp src/Economy.cpp src/Game.hpp src/Game.cpp src/Menu.cpp src/Remote.cpp src/ModuleMain.cpp src/core/CheatTable.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "feat(module): 경제 패널 0.6.0 — 금화와 영지 창고의 자원을 게임의 함수로 더하고 맞춘다"
```

---

### Task 4: 실행 묶음 하나로 확인하고, 다음 것의 자리를 재고, 적고, 넣는다

**Files:**
- Create: `research/08-economy.md`
- Modify: `CLAUDE.md`, `README.md`, `src/core/CheatTable.cpp`(본 것만 `Verified`), 스펙 §10·§13

게임을 켠다. **사용자의 승인이 있어야 한다**: 실행 묶음 1회. 모듈이 적재되기 전에 멈추면(`CLAUDE.md`의 "켜지다 멈추는 일") 한 번 더 켜도 되는지도 함께 묻는다.
켜기 전에 사용자에게 알린다: 메뉴가 뜰 때까지 게임 창을 누르지 말 것, 알림을 받으면 새 게임을 시작해 **일시정지**해 둘 것, Step 5 에서 모드창의 단추를 눌러 줄 것.

이 실행은 `feat/cheat-remote`(0.5.1 의 수정)의 적재 확인을 겸한다. 0.5.1 이 처음 부르는 빌트인(`method_get_self`, `method`, `array_create`)과
0.6.0 이 처음 부르는 길을 여기서 잰다.

- [ ] **Step 1: 배포하고 세이브의 사본을 뜬 뒤 켠다**

```powershell
pwsh -NoProfile -File tools/deploy.ps1
pwsh -NoProfile -File tools/saves-backup.ps1
pwsh -NoProfile -File tools/session.ps1 -Action start
```

메뉴가 뜬 뒤(`& tools\ask.ps1 -Lines 'state'`가 `o_main_menu 1`) 적재 판정을 받는다. **종료 코드 0 이 머지의 조건이다.**

```powershell
pwsh -NoProfile -File tools/session.ps1 -Action status
```

Expected: `적재 판정: 통과`, 로그의 첫 줄 `NlToyBox 0.6.0 loaded`.

- [ ] **Step 2: (메뉴) 0.5.1 의 수정과 게임 화면이 아닐 때의 거절을 본다**

```powershell
& tools\ask.ps1 -Lines 'economy gold_add amount=1000', 'call budget_default_money_get', 'about inst:o_time_controller.set_time_speed', 'about inst:o_time_controller.is_paused'
& tools\ask.ps1 -Lines 'record inst:o_time_controller.is_paused', 'method inst:o_time_controller.is_paused', 'records'
```

Expected: `economy` → `게임 화면이 아닙니다`(아무것도 부르지 않는다). `call` → `calling gml_Script_budget_default_money_get`(접두가 붙는다).
`about` 둘 → `script gml_Script_anon_gml_Object_o_time_controller_Create_0_…`, `self bound`, `method would call it as bound`.
`method … is_paused` → 기록에 `() -> <불리언>`이 남고 답도 `-> bool`. **기록에 호출이 남지 않거나 `undefined`이면** 인자 없는 호출이 이 러너에서 되지 않는 것이다:
그 사실을 적고, 인자 없는 메서드는 부르지 않는 것으로 둔다(이 계획의 호출은 모두 인자가 있다. 계속한다).

사용자에게 새 게임을 시작해 일시정지해 달라고 알리고, `state`가 `in_game 1`이 될 때까지 기다린다(`ask.ps1`로 30초마다).

- [ ] **Step 3: (게임 화면, 일시정지) 영지 창고의 메서드를 원격으로 먼저 확인한다**

위험한 호출이다. 한 요청씩 보낸다. 자원은 당근(28)으로 한다: 새 게임에서 200/250 이라 용량에 걸리지 않는다(나무는 300/300 이다).

```powershell
& tools\ask.ps1 -Lines 'state', 'ask inst:o_game_map_controller.__province.__warehouse.__warehouse.__total__[28]', 'ask inst:o_game_map_controller.__province.__warehouse.__warehouse.__no_reserve__[28]', 'about inst:o_game_map_controller.__province.__warehouse.change', 'window close', 'shot before-change'
& tools\ask.ps1 -Lines 'record inst:o_game_map_controller.__province.__warehouse.change'
& tools\ask.ps1 -Lines 'method inst:o_game_map_controller.__province.__warehouse.change n:28 n:5'
& tools\ask.ps1 -Lines 'ask inst:o_game_map_controller.__province.__warehouse.__warehouse.__total__[28]', 'ask inst:o_game_map_controller.__province.__warehouse.__warehouse.__no_reserve__[28]', 'records', 'shot after-change'
```

Expected: `about` → `self unbound`, `method would bind it to the owner`. 호출의 답 → `calling gml_Script_anon_Warehouse_…_3640123903_… (self bound to the owner) with 2 arguments`, `-> number 5`.
그 뒤 `__total__[28]`과 `__no_reserve__[28]`이 5 늘고, 기록에 `(28, 5) -> 5`, 화면의 음식 수가 5 는다.

멈추는 조건과 그때 할 일:

| 본 것 | 뜻 | 할 일 |
|---|---|---|
| `about`이 `self bound` | 무엇에 묶였는지 모른다 | 부르지 않는다. 자원은 이 실행에서 하지 않는다 |
| `about`이 `refuse` | 주소의 부모가 구조체가 아니다 | 부르지 않는다. 주소를 다시 잰다 |
| 호출 뒤 게임이 끝남 | self 나 인자가 틀렸다 | 답 파일의 마지막 줄을 적는다. 자원은 넣지 않는다 |
| 값이 그대로(`-> 0`이나 `undefined`) | 다른 창고이거나 용량 | 기록의 반환값을 적는다. 자원은 넣지 않는다 |
| 값은 늘었는데 화면이 그대로 | 화면은 이벤트가 따로 있다 | 시간을 흘려 다시 뜬다. 그래도 그대로면 "화면은 따라오지 않는다"로 적고 넣는다 |

자원을 넣지 않게 되면: 금화(Step 4 의 앞 두 줄)만 확인하고, `Economy.cpp`에서 자원의 단추를 가린 채(다음 실행의 승인을 받아 다시 확인) 넣는다.

줄이는 호출은 잰 적이 없다(금화는 `+100` 한 번, 영지 창고는 한 번도. 건물 창고의 `(1, -10) -> -10`뿐이다). 패널이 줄이기 전에 한 줄씩 먼저 잰다:

```powershell
& tools\ask.ps1 -Lines 'call gml_Script_budget_money_get'
& tools\ask.ps1 -Lines 'call gml_Script_budget_money_change n:-1'
& tools\ask.ps1 -Lines 'call gml_Script_budget_money_get', 'shot gold-minus1'
& tools\ask.ps1 -Lines 'method inst:o_game_map_controller.__province.__warehouse.change n:28 n:-5'
& tools\ask.ps1 -Lines 'ask inst:o_game_map_controller.__province.__warehouse.__warehouse.__total__[28]', 'ask inst:o_game_map_controller.__province.__warehouse.__warehouse.__no_reserve__[28]', 'records', 'shot carrot-minus5'
```

Expected: 금화가 1 줄고 화면이 따라온다. 당근이 5 줄고(두 배열 모두) 기록에 `(28, -5)`. 게임이 끝나거나 값이 음수가 되면 줄이는 길(`gold_set`의 낮추기, `set`, `0 으로`)을 넣지 않는다.
재고보다 큰 음수는 보내지 않는다(패널이 예약되지 않은 수까지만 줄인다. `PlanEconomy`).

- [ ] **Step 4: (게임 화면) 경제 명령의 길을 태운다**

```powershell
& tools\ask.ps1 -Lines 'economy gold_add amount=1000', 'economy gold_set amount=5000', 'economy gold_set amount=4000', 'call gml_Script_budget_money_get', 'shot economy-gold'
& tools\ask.ps1 -Lines 'economy add resource=28 amount=10', 'economy set resource=28 amount=120', 'economy add resource=99 amount=5', 'economy add resource=1 amount=50', 'shot economy-resource'
```

Expected: 금화 5000 → 4000(줄이는 변화도 된다), 읽기 함수가 4000. 당근 +10, 120 으로 맞춤. `resource=99` → `바꿀 것이 없습니다`.
나무 +50 은 용량(300)에 걸린다: 답의 `resource 1 <앞> -> <뒤>`와 로그의 `asked +50, applied …`를 그대로 적는다(용량을 넘길 때의 동작을 여기서 잰다).
화면(HUD)의 수가 답과 같다.

- [ ] **Step 5: (게임 화면) 패널을 보고, 사용자가 단추를 누른다**

```powershell
& tools\ask.ps1 -Lines 'window open', 'page economy', 'shot economy-panel'
```

화면에서 본다: 금화, 갈래 여섯(음식, 액체, 물자, 전쟁 물자, 약초·작물, 원자재)과 용량, 자원 38개의 이름과 수, 글이 깨지지 않는가,
금화의 입력 칸이 지금 금화로 채워져 있는가(0 이 아니다).
사용자에게 부탁한다: 모드창의 "경제"에서 **금화 +1,000** 한 번, **당근 +10** 한 번. 그 뒤:

```powershell
& tools\ask.ps1 -Lines 'call gml_Script_budget_money_get', 'ask inst:o_game_map_controller.__province.__warehouse.__warehouse.__total__[28]', 'shot economy-clicked'
```

Expected: 금화 5000, 당근 130. 로그에 `economy call budget_money_change(+1000)`, `economy call warehouse.change(28, +10)`(창의 단추 → 큐 → 틱의 길).
맨 끝에 따로 보낸다(38번 부른다. 전쟁 물자는 용량 20 이라 다 들어가지 않을 것이다):

```powershell
& tools\ask.ps1 -Lines 'economy all amount=5', 'shot economy-all' -TimeoutSec 60
```

- [ ] **Step 6: (게임 화면) 3나-2 의 자리를 잰다 — 읽기와 기록만**

```powershell
& tools\ask.ps1 -Lines 'tree inst:o_game_map_controller.__trade_manager.__caravan_resources[1] depth=2 max=60', 'tree inst:o_game_map_controller.__trade_manager.__prices_manager depth=2 max=80', 'tree inst:o_game_map_controller.__trade_manager.__trade_table depth=2 max=80', 'tree inst:o_game_map_controller.__trade_manager.__caravan.__trade_products[0] depth=2 max=60' -TimeoutSec 60
& tools\ask.ps1 -Lines 'tree global.__building_asset_storage depth=1 max=40', 'list inst:o_game_map_controller.buildings_data.__cache_map_by_name as=map max=80', 'tree inst:o_game_map_controller.__province.__production_appoint depth=2 max=60', 'tree inst:o_province_controller.production_cost depth=1 max=45' -TimeoutSec 60
& tools\ask.ps1 -Lines 'about inst:o_time_controller.__set_warp', 'record inst:o_time_controller.__set_warp', 'record inst:o_time_controller.set_time_speed', 'record inst:o_time_controller.pause', 'record inst:o_time_controller.resume'
```

사용자에게 부탁한다: 게임의 속도 단추(1배, 2배, 3배)와 일시정지를 한 번씩 누른다. 그 뒤 `records`로 인자와 `time_warp`의 변화를 받는다(게임 속도를 함수로 거는 길).
생산 건물이 있으면 `tree inst:o_building:<n>.c_production depth=2`(n 은 `list`로 `c_production`이 구조체인 것을 찾는다).
건설 조건: 사용자가 "조건에 걸려 못 짓는" 건물을 하나 골라 놓으려 할 때 화면을 뜨고(`shot build-blocked`), `tree inst:o_building.generic.__allow_to_build_function_list`와
건물의 `is_building_locked`를 `about`으로 본다. 여기서는 부르지 않는다.
상단이 와 있으면 사용자에게 거래 창을 열어 달라고 하고 화면을 뜬 뒤, 위에서 본 가격의 자리 한 칸에 `write`로 눈에 띄는 수를 쓰고 다시 떠서 화면의 값이 바뀌는지 본다.

창고 용량(표에서 뺐다. 여기서 잰다): 나무는 용량에 차 있다. `write …__cached_total_capacity_for_storage_type.raw=999` 뒤 화면을 뜨고(`원자재: n/999`로 보이는가),
`economy add resource=1 amount=50`의 앞뒤를 Step 4 의 것과 견준다(용량을 올리면 더 들어가는가). 시간을 흘린 뒤 그 값을 다시 읽는다(게임이 다시 만드는가, 언제).

즉시 건설만 켜고 본다: 사용자가 모드창의 "건설·생산"에서 **"건물 즉시 건설"만** 켜고(다른 둘은 끈 채) 건물 하나를 놓는다. 놓자마자 완성되는가
(지난 실행에서는 스위치 셋이 함께 켜져 있었다). 아니면 표의 `Verified`를 거짓으로 되돌린다.

저장하고 불러온다(맨 끝): 사용자가 게임을 저장하고 그 세이브를 불러온다. `state`, `call gml_Script_budget_money_get`, `economy gold_add amount=1`이 불러온 게임에서도 되는가
(자리와 함수가 불러온 세이브에서 같은가. `research/07`의 "확인하지 못한 것"). 금화와 자원의 수가 저장 전과 같은가.

- [ ] **Step 7: 끈다**

```powershell
pwsh -NoProfile -File tools/session.ps1 -Action stop -Name stage3b-session1
```

Expected: `게임 종료: closed`, 사용자의 설정 파일이 켜기 전과 같다(없던 파일은 없다).

- [ ] **Step 8: 잰 것을 적는다**

`research/08-economy.md`(`research/07`의 꼴): 된 것(Step 1~5 의 답 그대로), 처음 잰 것(정적 메서드의 `method_get_self`, 인자 없는 `method`, 용량을 넘길 때, 화면이 따라오는가),
3나-2 의 자리(Step 6), 확인하지 못한 것. `Verified`는 화면에서 효과를 본 항목만 참으로 둔다(즉시 건설을 따로 켜고 본 결과에 맞춘다).
`CLAUDE.md`의 "모듈을 쓸 때"에 경제 패널의 규칙(금화와 자원은 함수로. 자리와 함수의 출처는 `research/07`·`08`)을 더하고, README 의 "게임 안의 치트 메뉴"에 경제 패널을 적는다.
스펙 §10 의 3나를 3나-1(한 것)과 3나-2(남은 것: 거래, 임금, 건설비·건설 조건, 생산, 게임 속도의 함수, 배율 7개를 표로)로 나눈다.

- [ ] **Step 9: 커밋하고 `develop`에 넣는다**

```powershell
git -C E:\NlToyBox add research/08-economy.md CLAUDE.md README.md src/core/CheatTable.cpp tests/native/core_tests.cpp docs/superpowers/specs/2026-10-05-cheat-menu-design.md
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"
git -C E:\NlToyBox commit -m "docs(economy): 경제 패널에서 잰 것과 3나-2 의 자리"
git -C E:\NlToyBox checkout develop
git -C E:\NlToyBox merge --no-ff feat/cheat-remote -m "Merge branch 'feat/cheat-remote' into develop"
git -C E:\NlToyBox merge --no-ff feat/cheat-economy -m "Merge branch 'feat/cheat-economy' into develop"
git -C E:\NlToyBox branch -d feat/cheat-remote feat/cheat-economy
```

`ls-files`의 결과는 비어 있어야 한다. `feat/cheat-economy`는 `feat/cheat-remote` 위에 쌓았다. 차례대로 넣는다.
코드가 바뀌었으면(자원의 단추를 가렸다, `Verified`) 머지 전에 빌드와 게임을 켜지 않는 시험을 다시 돌린다. 적재 판정은 Step 1 의 것을 쓴다(그 뒤의 변경이 표의 불리언과 문서뿐일 때).

---

## Self-Review

- **스펙 대조**: §8 경제의 금화·자원(Task 1, 3), §3 수단 C(게임의 함수. Task 3 의 `CallScript`·`CallMethod`), §14 부르기의 규칙(Global Constraints, Task 3 의 부르기 전 로그, Task 4 Step 3 의 `about` 먼저). 거래·임금·건설비·생산과 배율 7개는 "무엇을 미루는가"에 적었다.
- **빈칸**: `research/08`의 내용은 실행의 답이라 미리 적을 수 없다. 들어갈 항목을 적었다.
- **이름의 일치**: `EconomyAct`·`EconomyCommand{Act, Resource, Amount}`·`EconomyChange{Resource, Delta}`·`PlanEconomy(Command, Gold, Counts, Stocked)`가 Task 1(만듦)과 Task 2·3(씀)에서 같다. `NlEconomy::Do`가 Task 3 의 머리 파일과 `Remote.cpp`에서 같다. `NlGame::CallScript(Name, Args, Result)`가 머리 파일과 `Economy.cpp`에서 같다.
- **Review Focus**: 1·2·3·5 는 Task 1 의 시험, 4 는 Task 4 Step 2.
- **계획 검토**: BLOCKER(치트 표의 수) 와 SHOULD-FIX 일곱을 반영했다("계획 검토와 구현 뒤에 바뀐 것", Task 4). NIT 가운데 "자원은 `__total__`, 금화는 `__no_reserve__`
  기준이다. 예약이 있을 때 화면이 어느 쪽인지 모른다"는 그대로 모르는 것으로 둔다(Task 4 Step 3 이 둘 다 읽는다).
