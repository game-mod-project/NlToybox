# 이벤트 탭 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 모드창의 이벤트 영역에 게임의 이벤트 61개의 표(묶음·갈래·화면 이름·쿨다운)와 줄마다의 일으키기, 예약된 강제 이벤트의 취소, 가족 다섯의 진행 중 상태와 확인된 가족만의 끝내기를 만들고, 원격 `world events|event_cancel|event_end` 를 더한다.

**Architecture:** 러너에 기대지 않는 표·규칙·글은 `src/core/EventPlan`(시험 `tests/native/test_events.cpp`). 새 모듈 `src/Events.cpp`(`NlEvents`)가 `src/World.cpp` 의 이벤트 부분(이름 읽기, 일으키기, 쿨다운 지우기, 그리기)을 그대로 받고 스냅샷·취소·끝내기·표 그리기를 더한다. World 는 주교·저장·종교의 일만 남고, `NlWorld::Do` 는 이벤트의 일을 `NlEvents::Do` 로 바로 넘긴다(계절과 같은 꼴). 그리는 쪽은 청만 쌓고 틱이 러너를 건드린다.

**Tech Stack:** C++20, Aurie + YYToolkit v5.0.0c, Dear ImGui v1.91.9. 빌드 `tools/build.ps1`, 코어 시험 `tools/test-native.ps1`.

**Spec:** `docs/superpowers/specs/2026-10-07-events-tab-design.md`

## Global Constraints

- 그리는 쪽(`Draw*`)에서 러너를 부르지 않는다. 창은 청만 쌓고 틱이 한다. `RValue` 를 틱 너머로 들지 않는다(스냅샷은 글과 수).
- 게임의 함수는 게임이 부르는 꼴을 본 것만 부른다: `reset_debug_forced_event()` 는 인자 없이(research/28·29). 끝내기의 후보 다섯은 **처음에 모두 `Verified = false`** 이고 그동안 부르지 않는다.
- 게임 파일의 글과 값은 레포에 싣지 않는다: 화면 이름은 모듈이 시작할 때 `localization\main.csv` 에서 열쇠로 읽는다. 표에는 이름·묶음·갈래·열쇠·모드가 지은 한국어 이름만.
- 쓴 뒤에는 다시 읽어 판정한다(일으키기, 취소, 끝내기). 부르기 전에 로그를 남긴다.
- 모드창의 글에 기호(화살표, 별)를 쓰지 않는다. 한글과 라틴-1 만.
- 버전 `0.30.0`(`src/ModuleMain.cpp` 의 `k_Version`). 빌드는 `build ok` 이고 `warning C` 가 없어야 한다. 코어 시험은 모두 통과.
- 게임 켜기는 Task 7 에서 한 번(적재 판정 + 창·원격). 켜기 전에 사용자에게 게임 창을 누르지 말라고 알린다.
- 커밋은 가지 `feat/events-tab` 에. `main`·`develop` 에 직접 푸시하지 않는다. 커밋의 작성자 이메일은 noreply.

## Review Focus

- 게임의 이름 목록에 표에 없는 이름이 있을 때(게임이 갱신됐을 때): 그 이름은 표의 맨 아래에 "(표에 없음)"으로 보이고 일으키기는 되어야 한다. 표에만 있는 이름은 보이지 않고 그 수만 도움말에 — Task 1 의 `MergeEventNames` 시험.
- 예약이 비어 있을 때 취소를 누르면: 함수를 부르지 않고 "예약된 이벤트가 없습니다" — Task 2 의 `ChooseCancelStep`·`CancelEventReport('n')` 시험, Task 4 의 코드.
- 확인 전인 가족의 끝내기가 원격으로 와도 부르지 않는다('x' 의 글) — Task 2 의 `EventEndAllowed` 시험, Task 4 의 코드.
- 쿨다운의 칸이 없거나 수가 아닌 이벤트의 쿨다운 칸은 "-" — Task 1 의 `CooldownText(-1)` 시험, Task 4 의 읽기(없으면 -1).
- 묶음을 고르고 찾기 칸에도 글이 있으면 둘 다 맞는 줄만 — Task 1 의 `EventRowShown` 시험.

---

### Task 1: `core/EventPlan` — 표 61줄, 열쇠·이름·가족·묶음, 보일 줄의 판단

**Files:**
- Create: `src/core/EventPlan.hpp`, `src/core/EventPlan.cpp`, `tests/native/test_events.cpp`
- Modify: `CMakeLists.txt`(코어 소스에 `src/core/EventPlan.cpp`, 시험 소스에 `tests/native/test_events.cpp`), `tests/native/common.hpp`(`#include "core/EventPlan.hpp"`, `void RunEventsTests();`), `tests/native/main.cpp`(`RunEventsTests();` 를 `RunCrimeTests();` 뒤에)

**Interfaces:**
- Produces(`namespace NlCore`): `enum class EventFamily { None, Raid, Prophecy, Conspiracy, Guest, Unrest, Rebellion }`; `struct EventRow { const char* Name; const char* Group; int Type; const char* CaptionKey; const char* Korean; EventFamily Family; }`;
  `const std::vector<EventRow>& EventTable()`, `const EventRow* FindEvent(const std::string& Name)`, `std::vector<std::string> EventGroups()`, `const char* EventTypeWord(int Type)`, `const char* EventFamilyWord(EventFamily)`, `const char* EventFamilyKey(EventFamily)`,
  `std::string EventLabel(const EventRow&, const std::unordered_map<std::string, std::string>& Captions)`, `bool EventMatches(std::string_view Filter, const std::string& Name, const std::string& Label)`, `bool EventRowShown(const std::string& Group, std::string_view Filter, const std::string& RowGroup, const std::string& Name, const std::string& Label)`,
  `std::string CooldownText(double DaysLeft)`, `struct EventListing { std::vector<std::string> Known, Extra; size_t MissingFromGame = 0; }`, `EventListing MergeEventNames(const std::vector<std::string>& GameNames)`.

- [ ] **Step 1: 시험을 쓴다** — `tests/native/test_events.cpp`:

```cpp
#include "common.hpp"

#include <set>

void RunEventsTests()
{
	Test("이벤트: 표 61줄, 묶음, 갈래, 가족, 열쇠, 이름", [] {
		const std::vector<EventRow>& table = EventTable();
		CHECK(table.size() == 61);
		std::set<std::string> names;
		const std::set<std::string> groups = { "GLOBAL_MAP", "RAID", "EPIDEMY", "CAMP", "ECONOMICAL", "GUEST", "POLITICAL", "UPRISING", "DISTORTION", "REWARD", "FOREST_BANDIT" };
		for (const EventRow& row : table)
		{
			const std::string name = row.Name;
			CHECK(names.insert(name).second);				// 겹치지 않는다
			CHECK(groups.count(row.Group) == 1);
			CHECK(row.Type >= 0 && row.Type <= 3);
			CHECK(std::string(row.CaptionKey).empty() != std::string(row.Korean).empty());		// 열쇠가 있거나 지은 이름이 있거나(둘 다는 아니다)
			// 가족은 이름의 규칙과 맞다(research/29: 늑대 습격 예언은 Prophecy, 도적의 공격 셋은 Raid, 정치 여섯과 뇌물은 Unrest)
			EventFamily want = EventFamily::None;
			if (name.rfind("u_guest_", 0) == 0) want = EventFamily::Guest;
			else if (name.rfind("prophecy_", 0) == 0) want = EventFamily::Prophecy;
			else if (name.rfind("raid_", 0) == 0 || name.find("_bandits_attack_") != std::string::npos) want = EventFamily::Raid;
			else if (name.rfind("rebellion_", 0) == 0) want = EventFamily::Rebellion;
			else if (name == "conspiracy") want = EventFamily::Conspiracy;
			else if (name.rfind("desire_politician", 0) == 0 || name.rfind("politician_", 0) == 0) want = EventFamily::Unrest;
			CHECK(row.Family == want);
		}
		// 열쇠의 규칙(research/29 에서 본 것). 도적 기지는 게임의 오타 그대로.
		CHECK_STR(FindEvent("u_guest_bard")->CaptionKey, "guest.bard");
		CHECK_STR(FindEvent("raid_bandits")->CaptionKey, "raid.bandits");
		CHECK_STR(FindEvent("prophecy_bandits_camp")->CaptionKey, "prophecy.bandits_capm");
		CHECK_STR(FindEvent("prophecy_wolf_attack")->CaptionKey, "prophecy.wolf_attack");
		CHECK_STR(FindEvent("reward_rich_migrants")->CaptionKey, "map.reward_rich_migrants");
		CHECK_STR(FindEvent("conspiracy")->CaptionKey, "map.conspiracy");
		CHECK(std::string(FindEvent("u_guest_incognito_bishop")->CaptionKey).empty() && !std::string(FindEvent("u_guest_incognito_bishop")->Korean).empty());
		CHECK(std::string(FindEvent("rebellion_slaves")->CaptionKey).empty() && std::string(FindEvent("rebellion_slaves")->Korean) == "노예 반란");
		CHECK(FindEvent("no_such_event") == nullptr && FindEvent("") == nullptr);
		CHECK(FindEvent("u_guest_bard")->Type == 2 && FindEvent("raid_bandits")->Type == 0 && FindEvent("dark_actions")->Type == 1 && FindEvent("ai_preach")->Type == 3);
		// 묶음의 목록: 표에서 모아 이름순(겹치지 않게). FOREST_BANDIT 은 파일의 묶음에만 있고 이벤트에는 없다 → 표의 묶음은 10개.
		const std::vector<std::string> list = EventGroups();
		CHECK(list.size() == 10 && list.front() == "CAMP" && list.back() == "UPRISING");
		CHECK_STR(EventTypeWord(0), "큰 위협"); CHECK_STR(EventTypeWord(1), "작은 위협"); CHECK_STR(EventTypeWord(2), "보통"); CHECK_STR(EventTypeWord(3), "좋음"); CHECK_STR(EventTypeWord(7), "?");
		CHECK_STR(EventFamilyWord(EventFamily::Raid), "습격"); CHECK_STR(EventFamilyWord(EventFamily::Unrest), "소요"); CHECK_STR(EventFamilyWord(EventFamily::None), "");
		CHECK_STR(EventFamilyKey(EventFamily::Prophecy), "prophecy"); CHECK_STR(EventFamilyKey(EventFamily::None), "");
		// 화면 이름: 열쇠의 줄이 있으면 그 글, 없으면 지은 이름, 그것도 없으면 원시 이름
		const std::unordered_map<std::string, std::string> captions = { { "guest.bard", "음유시인" } };
		CHECK_STR(EventLabel(*FindEvent("u_guest_bard"), captions), "음유시인");
		CHECK_STR(EventLabel(*FindEvent("raid_bandits"), captions), "raid_bandits");				// 열쇠는 있는데 줄이 없다
		CHECK_STR(EventLabel(*FindEvent("rebellion_slaves"), captions), "노예 반란");
		// 찾기: 원시 이름과 화면 이름에서, 대소문자 없이. 빈 글은 모두 맞는다
		CHECK(EventMatches("", "u_guest_bard", "음유시인") && EventMatches("BARD", "u_guest_bard", "음유시인") && EventMatches("음유", "u_guest_bard", "음유시인"));
		CHECK(!EventMatches("raid", "u_guest_bard", "음유시인"));
		// 보일 줄: 묶음(빈 글이면 전체)과 찾기 둘 다 맞아야 한다
		CHECK(EventRowShown("", "", "GUEST", "u_guest_bard", "음유시인") && EventRowShown("GUEST", "bard", "GUEST", "u_guest_bard", "음유시인"));
		CHECK(!EventRowShown("RAID", "", "GUEST", "u_guest_bard", "음유시인") && !EventRowShown("GUEST", "raid", "GUEST", "u_guest_bard", "음유시인"));
		// 쿨다운의 칸: 없으면(-1) "-", 아니면 "N일"
		CHECK_STR(CooldownText(-1), "-"); CHECK_STR(CooldownText(18), "18일"); CHECK_STR(CooldownText(0), "0일"); CHECK_STR(CooldownText(2.5), "2.5일");
		// 게임의 이름 목록과 표를 합친다: 둘 다 있는 것(표의 차례), 게임에만 있는 것(이름순), 표에만 있는 것의 수
		const EventListing merged = MergeEventNames({ "raid_bandits", "zzz_new_event", "u_guest_bard", "aaa_new" });
		CHECK(merged.Known.size() == 2 && merged.Known[0] == "raid_bandits" && merged.Known[1] == "u_guest_bard");		// 표의 차례(RAID 가 GUEST 보다 앞)
		CHECK(merged.Extra.size() == 2 && merged.Extra[0] == "aaa_new" && merged.Extra[1] == "zzz_new_event");
		CHECK(merged.MissingFromGame == 59);
		CHECK(MergeEventNames({}).Known.empty() && MergeEventNames({}).MissingFromGame == 61);
	});
}
```

- [ ] **Step 2: 뼈대에 잇고 빌드해 실패를 본다** — `tests/native/common.hpp` 의 포함 목록에 `#include "core/EventPlan.hpp"`(이름순. `core/EconomyPlan.hpp` 뒤), 선언 목록에 `void RunEventsTests();`(`RunCrimeTests` 뒤). `tests/native/main.cpp` 의 `RunCrimeTests();` 뒤에 `RunEventsTests();`. `CMakeLists.txt`: 코어 소스 목록에 `  src/core/EventPlan.cpp`(`src/core/EconomyPlan.cpp` 뒤. 이름순), 시험 소스 목록에 `  tests/native/test_events.cpp`(`tests/native/test_crime.cpp` 뒤).
  빈 헤더 `src/core/EventPlan.hpp`(`#pragma once` 만)와 빈 `src/core/EventPlan.cpp`(`#include "EventPlan.hpp"` 만)를 만든다.

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1`
Expected: `test_events.cpp` 에서 `EventTable`, `EventRow` 들을 찾지 못한다는 컴파일 오류(C3861·C2065). `build ok` 가 없다.

- [ ] **Step 3: 헤더** — `src/core/EventPlan.hpp`:

```cpp
#pragma once
// 이벤트 탭(research/29)에서 러너에 기대지 않는 것: 이벤트 61개의 표(이름·묶음·갈래·화면 이름의 열쇠·모드가 지은 이름·가족), 보일 줄의 판단, 글.
// 이름·묶음은 게임 파일 director_params.json 의 열쇠, 갈래는 런타임의 __type(0 BIG_THREAT, 1 SMALL_THREAT, 2 NEUTRAL, 3 GOOD). 값(tickets, cooldown_days)은 옮기지 않는다.
// 화면 이름의 열쇠는 localization\main.csv 에서 본 꼴(guest.<이름>, raid.<이름>, prophecy.<이름>, map.reward_*, map.conspiracy). 열쇠가 없는 것은 모드가 한국어 이름을 지었다(추정).

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NlCore
{
	// 진행 중의 자리와 끝내기의 단위. Rebellion 은 읽을 자리를 아직 모른다(창에 "모름"). None 은 보상·세계 지도·어두운 행동·도적 이주민.
	enum class EventFamily { None, Raid, Prophecy, Conspiracy, Guest, Unrest, Rebellion };

	struct EventRow
	{
		const char* Name;			// 게임의 시스템 이름
		const char* Group;			// 묶음(GUEST, RAID …)
		int Type;					// 갈래 0..3
		const char* CaptionKey;		// 화면 이름의 열쇠(main.csv). 없으면 빈 글
		const char* Korean;			// 열쇠가 없을 때 모드가 지은 이름(추정). 열쇠가 있으면 빈 글
		EventFamily Family;
	};
	const std::vector<EventRow>& EventTable();
	const EventRow* FindEvent(const std::string& Name);		// 없으면 nullptr
	std::vector<std::string> EventGroups();					// 표의 묶음을 이름순으로(겹치지 않게)
	const char* EventTypeWord(int Type);					// "큰 위협", "작은 위협", "보통", "좋음". 그 밖은 "?"
	const char* EventFamilyWord(EventFamily Family);		// "습격", "예언", "음모", "손님", "소요", "반란". None 은 빈 글
	const char* EventFamilyKey(EventFamily Family);			// "raid", "prophecy", "conspiracy", "guest", "unrest", "rebellion". None 은 빈 글

	// 창에 보일 이름: 열쇠의 줄이 Captions 에 있으면 그 글, 없으면 지은 이름, 그것도 없으면 원시 이름.
	std::string EventLabel(const EventRow& Row, const std::unordered_map<std::string, std::string>& Captions);
	// 찾기: 원시 이름이나 화면 이름에 들어 있다(영문은 대소문자를 가리지 않는다. core/Localization 의 TraitMatches). 빈 글은 모두 맞는다.
	bool EventMatches(std::string_view Filter, const std::string& Name, const std::string& Label);
	// 보일 줄인가: 고른 묶음(빈 글이면 전체)과 찾기가 둘 다 맞는다.
	bool EventRowShown(const std::string& Group, std::string_view Filter, const std::string& RowGroup, const std::string& Name, const std::string& Label);
	// 쿨다운의 칸: 남은 날이 없으면(음수) "-", 아니면 "N일".
	std::string CooldownText(double DaysLeft);

	// 게임의 이름 목록(ds_map 의 열쇠)과 표를 합친 것: Known 은 둘 다 있는 이름(표의 차례), Extra 는 게임에만 있는 이름(이름순. 게임이 갱신되면 생긴다), MissingFromGame 은 표에만 있는 수.
	struct EventListing
	{
		std::vector<std::string> Known, Extra;
		size_t MissingFromGame = 0;
	};
	EventListing MergeEventNames(const std::vector<std::string>& GameNames);
}
```

- [ ] **Step 4: 구현** — `src/core/EventPlan.cpp`:

```cpp
#include "EventPlan.hpp"

#include "Localization.hpp"
#include "Text.hpp"

#include <algorithm>
#include <set>

namespace NlCore
{
	namespace
	{
		using F = EventFamily;
		// research/29 의 표. 차례는 묶음별(파일의 차례): GLOBAL_MAP, RAID, EPIDEMY, CAMP, DISTORTION, ECONOMICAL, GUEST, POLITICAL, UPRISING, REWARD.
		const std::vector<EventRow> k_Table = {
			{ "vassal_player_demand", "GLOBAL_MAP", 0, "", "봉신의 요구", F::None },
			{ "demand_forcing_neutrality", "GLOBAL_MAP", 0, "", "중립 강요", F::None },
			{ "town_king_tribute_light", "GLOBAL_MAP", 0, "", "왕의 조공 요구", F::None },
			{ "faceless_attack_player_town", "GLOBAL_MAP", 0, "", "얼굴 없는 자의 도시 공격", F::None },
			{ "attack_player_village", "GLOBAL_MAP", 0, "", "마을 공격", F::None },
			{ "player_vassal_rebellion", "GLOBAL_MAP", 0, "", "봉신의 반란", F::None },
			{ "forest_bandits_attack_player_village", "RAID", 0, "", "숲 도적의 마을 공격", F::Raid },
			{ "forest_bandits_attack_player_town", "RAID", 0, "", "숲 도적의 도시 공격", F::Raid },
			{ "mountain_bandits_attack_player_town", "RAID", 0, "", "산 도적의 도시 공격", F::Raid },
			{ "prophecy_wolf_attack", "RAID", 0, "prophecy.wolf_attack", "", F::Prophecy },
			{ "raid_bums_in_rags", "RAID", 0, "raid.bums_in_rags", "", F::Raid },
			{ "raid_yellow_fanatics", "RAID", 0, "raid.yellow_fanatics", "", F::Raid },
			{ "raid_flagellants", "RAID", 0, "raid.flagellants", "", F::Raid },
			{ "raid_faceless", "RAID", 0, "raid.faceless", "", F::Raid },
			{ "raid_dogheads", "RAID", 0, "raid.dogheads", "", F::Raid },
			{ "raid_deserters", "RAID", 0, "raid.deserters", "", F::Raid },
			{ "raid_bandits", "RAID", 0, "raid.bandits", "", F::Raid },
			{ "prophecy_cholera_epidemy", "EPIDEMY", 0, "prophecy.cholera_epidemy", "", F::Prophecy },
			{ "prophecy_bandits_camp", "CAMP", 1, "prophecy.bandits_capm", "", F::Prophecy },		// 게임의 오타 그대로(research/29)
			{ "prophecy_xenophoby", "DISTORTION", 1, "prophecy.xenophoby", "", F::Prophecy },
			{ "migrants_bandits", "DISTORTION", 1, "", "도적 이주민", F::None },
			{ "prophecy_tree_bug", "ECONOMICAL", 1, "prophecy.tree_bug", "", F::Prophecy },
			{ "prophecy_rats_invasion", "ECONOMICAL", 1, "prophecy.rats_invasion", "", F::Prophecy },
			{ "reward_increased_prices", "ECONOMICAL", 2, "", "보상: 물가 상승", F::None },
			{ "reward_farm_grow_boost", "ECONOMICAL", 2, "map.reward_farm_grow_boost", "", F::None },
			{ "reward_raw_collect_boost", "ECONOMICAL", 2, "map.reward_raw_collect_boost", "", F::None },
			{ "u_guest_assassin", "GUEST", 2, "guest.assassin", "", F::Guest },
			{ "u_guest_mountain_knight", "GUEST", 2, "guest.mountain_knight", "", F::Guest },
			{ "u_guest_escaped_lord", "GUEST", 2, "guest.escaped_lord", "", F::Guest },
			{ "u_guest_witch", "GUEST", 2, "guest.witch", "", F::Guest },
			{ "u_guest_bandit_and_slave", "GUEST", 2, "guest.bandit_and_slave", "", F::Guest },
			{ "u_guest_cultists", "GUEST", 2, "guest.cultists", "", F::Guest },
			{ "u_guest_joker", "GUEST", 2, "guest.joker", "", F::Guest },
			{ "u_guest_dog_seller", "GUEST", 2, "guest.dog_seller", "", F::Guest },
			{ "u_guest_bard", "GUEST", 2, "guest.bard", "", F::Guest },
			{ "u_guest_nectar_trader", "GUEST", 2, "guest.nectar_trader", "", F::Guest },
			{ "u_guest_piligrims", "GUEST", 2, "guest.piligrims", "", F::Guest },
			{ "u_guest_incognito_philosoph", "GUEST", 2, "", "익명의 손님 (철학자)", F::Guest },
			{ "u_guest_incognito_bishop", "GUEST", 2, "", "익명의 손님 (주교)", F::Guest },
			{ "u_guest_incognito_bandit", "GUEST", 1, "", "익명의 손님 (도적)", F::Guest },
			{ "u_guest_incognito_maniac", "GUEST", 1, "", "익명의 손님 (살인마)", F::Guest },
			{ "desire_politician_for_power_make_me_heir_low", "POLITICAL", 2, "", "후계자 요구 (약)", F::Unrest },
			{ "desire_politician_for_power_make_me_heir_middle", "POLITICAL", 1, "", "후계자 요구 (중)", F::Unrest },
			{ "desire_politician_for_power_make_me_heir_high", "POLITICAL", 0, "", "후계자 요구 (강)", F::Unrest },
			{ "politician_for_power_make_me_king_low", "POLITICAL", 2, "", "왕위 요구 (약)", F::Unrest },
			{ "politician_for_power_make_me_king_middle", "POLITICAL", 1, "", "왕위 요구 (중)", F::Unrest },
			{ "politician_for_power_make_me_king_high", "POLITICAL", 0, "", "왕위 요구 (강)", F::Unrest },
			{ "politician_bribe", "POLITICAL", 1, "", "정치가의 뇌물", F::Unrest },
			{ "dark_actions", "POLITICAL", 1, "", "어두운 행동", F::None },
			{ "conspiracy", "POLITICAL", 0, "map.conspiracy", "", F::Conspiracy },
			{ "rebellion_slaves", "UPRISING", 0, "", "노예 반란", F::Rebellion },
			{ "rebellion_culture", "UPRISING", 0, "", "문화 반란", F::Rebellion },
			{ "rebellion_religiosity", "UPRISING", 0, "", "종교 반란", F::Rebellion },
			{ "rebellion_loyalty", "UPRISING", 0, "", "충성 반란", F::Rebellion },
			{ "ai_preach", "REWARD", 3, "", "AI 영주의 설교", F::None },
			{ "reward_hostage_lord", "REWARD", 3, "", "보상: 인질 영주", F::None },
			{ "reward_neutralise_lord", "REWARD", 3, "", "보상: 영주 중립화", F::None },
			{ "reward_weak_bandits_camp", "REWARD", 3, "", "보상: 약한 도적 기지", F::None },
			{ "reward_rich_migrants", "REWARD", 3, "map.reward_rich_migrants", "", F::None },
			{ "reward_trade_request", "REWARD", 3, "map.reward_trade_request", "", F::None },
			{ "reward_enemy_squad_on_march", "REWARD", 3, "", "보상: 행군 중인 적 분대", F::None },
		};
	}

	const std::vector<EventRow>& EventTable()
	{
		return k_Table;
	}

	const EventRow* FindEvent(const std::string& Name)
	{
		for (const EventRow& row : k_Table)
			if (Name == row.Name)
				return &row;
		return nullptr;
	}

	std::vector<std::string> EventGroups()
	{
		std::set<std::string> groups;
		for (const EventRow& row : k_Table)
			groups.insert(row.Group);
		return std::vector<std::string>(groups.begin(), groups.end());
	}

	const char* EventTypeWord(int Type)
	{
		switch (Type)
		{
		case 0: return "큰 위협";
		case 1: return "작은 위협";
		case 2: return "보통";
		case 3: return "좋음";
		default: return "?";
		}
	}

	const char* EventFamilyWord(EventFamily Family)
	{
		switch (Family)
		{
		case EventFamily::Raid: return "습격";
		case EventFamily::Prophecy: return "예언";
		case EventFamily::Conspiracy: return "음모";
		case EventFamily::Guest: return "손님";
		case EventFamily::Unrest: return "소요";
		case EventFamily::Rebellion: return "반란";
		default: return "";
		}
	}

	const char* EventFamilyKey(EventFamily Family)
	{
		switch (Family)
		{
		case EventFamily::Raid: return "raid";
		case EventFamily::Prophecy: return "prophecy";
		case EventFamily::Conspiracy: return "conspiracy";
		case EventFamily::Guest: return "guest";
		case EventFamily::Unrest: return "unrest";
		case EventFamily::Rebellion: return "rebellion";
		default: return "";
		}
	}

	std::string EventLabel(const EventRow& Row, const std::unordered_map<std::string, std::string>& Captions)
	{
		if (Row.CaptionKey[0])
		{
			const auto found = Captions.find(Row.CaptionKey);
			if (found != Captions.end() && !found->second.empty())
				return found->second;
		}
		if (Row.Korean[0])
			return Row.Korean;
		return Row.Name;
	}

	bool EventMatches(std::string_view Filter, const std::string& Name, const std::string& Label)
	{
		return TraitMatches(Filter, Name, Label);
	}

	bool EventRowShown(const std::string& Group, std::string_view Filter, const std::string& RowGroup, const std::string& Name, const std::string& Label)
	{
		if (!Group.empty() && Group != RowGroup)
			return false;
		return EventMatches(Filter, Name, Label);
	}

	std::string CooldownText(double DaysLeft)
	{
		return DaysLeft < 0 ? "-" : Shortest(DaysLeft) + "일";
	}

	EventListing MergeEventNames(const std::vector<std::string>& GameNames)
	{
		EventListing out;
		std::set<std::string> game(GameNames.begin(), GameNames.end());
		for (const EventRow& row : k_Table)
		{
			if (game.erase(row.Name))
				out.Known.push_back(row.Name);
			else
				out.MissingFromGame++;
		}
		out.Extra.assign(game.begin(), game.end());		// set 이라 이름순
		return out;
	}
}
```

- [ ] **Step 5: 빌드와 시험**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1` 뒤 `pwsh -NoProfile -File E:\NlToyBox\tools\test-native.ps1`
Expected: `build ok`, `warning C` 없음, `ok - 이벤트: 표 61줄, 묶음, 갈래, 가족, 열쇠, 이름`, `core tests: 119 passed`. 실패하는 CHECK 가 있으면 표의 그 줄(갈래·열쇠·가족)을 research/29 와 견줘 고친다.

- [ ] **Step 6: 커밋**

```bash
git -C E:\NlToyBox add src/core/EventPlan.hpp src/core/EventPlan.cpp tests/native/test_events.cpp tests/native/common.hpp tests/native/main.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "feat(core): 이벤트 표 61줄(묶음·갈래·화면 이름의 열쇠·지은 이름·가족)과 보일 줄의 판단 (이벤트 탭, Task 1)"
```

---

### Task 2: `core/EventPlan` — 끝내기의 표, 상태와 결과의 글; `WorldAct` 셋과 원격의 낱말

**Files:**
- Modify: `src/core/EventPlan.hpp`, `src/core/EventPlan.cpp`, `src/core/WorldPlan.hpp`, `src/core/WorldPlan.cpp`, `src/core/RemoteCommand.cpp:476-497`(`ParseWorldLine`), `src/core/RemoteCommand.hpp`(주석 한 줄), `tests/native/test_events.cpp`, `tests/native/test_world.cpp`

**Interfaces:**
- Consumes: Task 1 의 `EventFamily`, `EventFamilyKey`.
- Produces(`NlCore`): `struct EventEndRow { EventFamily Family; const char* StatusPath; const char* EndPath; const char* ArgShape; bool Verified; }`, `const std::vector<EventEndRow>& EventEndTable()`, `const EventEndRow* FindEventEnd(EventFamily)`, `bool EventEndAllowed(const EventEndRow&)`, `bool ParseEventFamily(const std::string& Word, EventFamily& Out)`(끝내기의 다섯만),
  `enum class CancelStep { Nothing, Call }`, `CancelStep ChooseCancelStep(bool HasForced)`, `std::string EventStatusText(EventFamily, bool Read, bool Present, const std::string& Name)`, `std::string ForceEventReport(…)`(WorldPlan 에서 옮긴다), `std::string CancelEventReport(char Outcome, const std::string& Detail)`, `std::string EndEventReport(EventFamily, char Outcome, const std::string& Detail)`;
  `WorldAct` 에 `EventCancel`, `EventEnd`, `EventList`; `bool IsEventAct(WorldAct)`, `bool WorldActNeedsKind(WorldAct)`; 원격 `world events [group=] [find=]`, `world event_cancel`, `world event_end kind=<raid|prophecy|conspiracy|guest|unrest>`.

- [ ] **Step 1: 시험** — `tests/native/test_events.cpp` 의 `RunEventsTests` 에 둘째 `Test` 를 더한다:

```cpp
	Test("이벤트: 끝내기의 표, 취소의 걸음, 상태와 결과의 글, 원격의 낱말", [] {
		// 끝내기의 후보 다섯(research/29). 처음에는 모두 확인 전이라 부르지 않는다.
		const std::vector<EventEndRow>& ends = EventEndTable();
		CHECK(ends.size() == 5);
		for (const EventEndRow& row : ends)
		{
			CHECK(row.Family != EventFamily::None && row.Family != EventFamily::Rebellion);
			CHECK(std::string(row.StatusPath).rfind("inst:o_game_map_controller.", 0) == 0 && std::string(row.EndPath).rfind("inst:o_game_map_controller.", 0) == 0);
			CHECK(!row.Verified && !EventEndAllowed(row));
		}
		CHECK_STR(FindEventEnd(EventFamily::Raid)->StatusPath, "inst:o_game_map_controller.__raids_manager.__current_raid");
		CHECK_STR(FindEventEnd(EventFamily::Raid)->EndPath, "inst:o_game_map_controller.__raids_manager.try_to_remove_raid");
		CHECK_STR(FindEventEnd(EventFamily::Prophecy)->EndPath, "inst:o_game_map_controller.__province.__prophecy_manager.__reset_prophecy");
		CHECK_STR(FindEventEnd(EventFamily::Unrest)->StatusPath, "inst:o_game_map_controller.__province.__politics_manager.__is_unrest_active_struct");
		CHECK(FindEventEnd(EventFamily::Rebellion) == nullptr && FindEventEnd(EventFamily::None) == nullptr);
		const EventEndRow verified{ EventFamily::Raid, "a", "b", "", true };
		CHECK(EventEndAllowed(verified));
		EventFamily family = EventFamily::None;
		CHECK(ParseEventFamily("raid", family) && family == EventFamily::Raid);
		CHECK(ParseEventFamily("unrest", family) && family == EventFamily::Unrest);
		CHECK(!ParseEventFamily("rebellion", family) && !ParseEventFamily("", family) && !ParseEventFamily("RAID", family));
		// 예약 취소: 예약이 없으면 함수를 부르지 않는다
		CHECK(ChooseCancelStep(false) == CancelStep::Nothing && ChooseCancelStep(true) == CancelStep::Call);
		// 상태의 글
		CHECK_STR(EventStatusText(EventFamily::Raid, true, false, ""), "없음");
		CHECK_STR(EventStatusText(EventFamily::Raid, true, true, "raid_bandits"), "진행 중: raid_bandits");
		CHECK_STR(EventStatusText(EventFamily::Raid, true, true, ""), "진행 중 (이름을 읽지 못함)");
		CHECK_STR(EventStatusText(EventFamily::Raid, false, false, ""), "읽지 못함");
		CHECK_STR(EventStatusText(EventFamily::Rebellion, false, false, ""), "모름 (읽을 자리를 아직 모른다)");
		// 결과의 글(일으키기는 WorldPlan 에서 옮겨 왔다)
		CHECK_STR(ForceEventReport("u_guest_bard", 'n', ""), "u_guest_bard: 게임에 그 이름의 이벤트가 없습니다");
		CHECK_STR(ForceEventReport("u_guest_bard", 'w', "why"), "u_guest_bard: 강제 이벤트에 쓰지 못했습니다 (why)");
		CHECK_STR(ForceEventReport("u_guest_bard", 'd', ""), "u_guest_bard: 강제 이벤트로 써 두었습니다. 게임의 감독이 다음에 이벤트를 뽑을 때(하루 한 번, 오후) 이것을 고릅니다");
		CHECK_STR(CancelEventReport('n', ""), "예약된 이벤트가 없습니다");
		CHECK_STR(CancelEventReport('f', "no method"), "예약을 지우는 함수를 부르지 못했습니다 (no method)");
		CHECK_STR(CancelEventReport('u', "u_guest_joker"), "함수를 불렀지만 예약이 남아 있습니다: u_guest_joker");
		CHECK_STR(CancelEventReport('d', "u_guest_joker"), "예약을 지웠습니다: u_guest_joker");
		CHECK_STR(EndEventReport(EventFamily::Raid, 'x', ""), "습격 끝내기는 확인 전이라 부르지 않습니다 (research/29 의 절차로 게임에서 본 뒤에 켭니다)");
		CHECK_STR(EndEventReport(EventFamily::Prophecy, 'n', ""), "진행 중인 예언이 없습니다");
		CHECK_STR(EndEventReport(EventFamily::Guest, 'f', "why"), "손님 끝내기의 함수를 부르지 못했습니다 (why)");
		CHECK_STR(EndEventReport(EventFamily::Conspiracy, 'u', ""), "함수를 불렀지만 음모가 그대로입니다");
		CHECK_STR(EndEventReport(EventFamily::Unrest, 'd', "x"), "소요를 끝냈습니다: x");
		// WorldAct 셋과 원격의 낱말
		WorldAct act = WorldAct::CooldownsClear;
		CHECK(ParseWorldAct("events", act) && act == WorldAct::EventList && !WorldActChanges(WorldAct::EventList));
		CHECK(ParseWorldAct("event_cancel", act) && act == WorldAct::EventCancel && WorldActChanges(WorldAct::EventCancel));
		CHECK(ParseWorldAct("event_end", act) && act == WorldAct::EventEnd && WorldActChanges(WorldAct::EventEnd));
		CHECK(IsEventAct(WorldAct::EventForce) && IsEventAct(WorldAct::EventCancel) && IsEventAct(WorldAct::EventEnd) && IsEventAct(WorldAct::EventList) && IsEventAct(WorldAct::CooldownsClear));
		CHECK(!IsEventAct(WorldAct::BishopSend) && !IsEventAct(WorldAct::SaveNow) && !IsEventAct(WorldAct::SeasonShow));
		CHECK(WorldActNeedsKind(WorldAct::EventEnd) && !WorldActNeedsKind(WorldAct::EventForce) && !WorldActNeedsName(WorldAct::EventEnd));
		CHECK(WorldActWords() == "cooldowns_clear, bishop, season, season_delay, season_end, save, event, events, event_cancel, event_end");
		const RemoteCommand list = ParseRemoteLine("world events group=GUEST find=bard");
		CHECK(list.Error.empty() && list.Target == "events" && list.Options.at("group") == "GUEST" && list.Options.at("find") == "bard");
		CHECK(ParseRemoteLine("world events").Error.empty());
		CHECK(ParseRemoteLine("world event_cancel").Error.empty() && !ParseRemoteLine("world event_cancel name=x").Error.empty());
		const RemoteCommand end = ParseRemoteLine("world event_end kind=raid");
		CHECK(end.Error.empty() && end.Target == "event_end" && end.Options.at("kind") == "raid");
		CHECK(!ParseRemoteLine("world event_end").Error.empty() && !ParseRemoteLine("world event_end kind=x").Error.empty() && !ParseRemoteLine("world event_end kind=rebellion").Error.empty());
	});
```

`tests/native/test_world.cpp` 에서 `ForceEventReport` 의 `CHECK_STR` 세 줄(과 그 위의 주석 한 줄)을 지우고(EventPlan 의 시험으로 옮겼다), `WorldActWords()` 의 기대 글을 `"cooldowns_clear, bishop, season, season_delay, season_end, save, event, events, event_cancel, event_end"` 로 바꾼다.

- [ ] **Step 2: 빌드해 실패를 본다**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1`
Expected: `EventEndRow`, `ChooseCancelStep`, `WorldAct::EventList` 들을 찾지 못한다는 컴파일 오류. `build ok` 없음.

- [ ] **Step 3: EventPlan 의 헤더에 더한다**(`namespace NlCore` 안, `MergeEventNames` 뒤):

```cpp
	// 끝내기의 후보(research/29 에서 이름만 봤다). StatusPath: 진행 중의 자리(undefined·-4 면 없음, 구조체면 진행 중). EndPath: 끝내는 함수. ArgShape: 게임이 부르는 꼴(확인 전에는 빈 글).
	// Verified: 게임에서 인자와 효과를 본 뒤에만 참(스펙 §7 의 절차). 그때까지 부르지 않는다.
	struct EventEndRow
	{
		EventFamily Family;
		const char* StatusPath;
		const char* EndPath;
		const char* ArgShape;
		bool Verified;
	};
	const std::vector<EventEndRow>& EventEndTable();			// 다섯: 습격, 예언, 음모, 손님, 소요
	const EventEndRow* FindEventEnd(EventFamily Family);		// 없으면 nullptr(None, Rebellion)
	bool EventEndAllowed(const EventEndRow& Row);				// Verified 일 때만
	bool ParseEventFamily(const std::string& Word, EventFamily& Out);	// 끝내기의 가족 다섯의 열쇠(raid …)만. rebellion 은 받지 않는다

	// 예약 취소: 예약이 없으면 함수를 부르지 않는다.
	enum class CancelStep { Nothing, Call };
	CancelStep ChooseCancelStep(bool HasForced);

	// 진행 중의 글. Read: 자리를 읽었다. Present: 구조체가 있다. Name: 그 구조체의 이름(없으면 빈 글). Rebellion 은 언제나 "모름".
	std::string EventStatusText(EventFamily Family, bool Read, bool Present, const std::string& Name);
	// 일으키기의 결과. Outcome: 'n' 그 이름의 이벤트가 없다, 'w' 쓰지 못했다(Why), 'd' 써 두었다(감독이 다음에 뽑을 때 고른다. research/28).
	std::string ForceEventReport(const std::string& Name, char Outcome, const std::string& Why);
	// 취소의 결과. 'n' 예약이 없었다, 'f' 함수를 부르지 못했다(Detail 에 까닭), 'u' 부른 뒤에도 남아 있다(Detail 에 이름), 'd' 지웠다(Detail 에 이름).
	std::string CancelEventReport(char Outcome, const std::string& Detail);
	// 끝내기의 결과. 'x' 확인 전, 'n' 진행 중이 아니다, 'f' 부르지 못했다(Detail), 'u' 부른 뒤에도 그대로다, 'd' 끝냈다(Detail 에 이름).
	std::string EndEventReport(EventFamily Family, char Outcome, const std::string& Detail);
```

- [ ] **Step 4: EventPlan 의 구현에 더한다**(`namespace NlCore` 안, 끝에):

```cpp
	namespace
	{
		// research/29 의 표. 확인 전(Verified false): 인자의 꼴을 모르고 효과도 못 봤다.
		const std::vector<EventEndRow> k_Ends = {
			{ EventFamily::Raid, "inst:o_game_map_controller.__raids_manager.__current_raid", "inst:o_game_map_controller.__raids_manager.try_to_remove_raid", "", false },
			{ EventFamily::Prophecy, "inst:o_game_map_controller.__province.__prophecy_manager.__current_prophecy", "inst:o_game_map_controller.__province.__prophecy_manager.__reset_prophecy", "", false },
			{ EventFamily::Conspiracy, "inst:o_game_map_controller.__province.__conspiracy.__current_conspiracy", "inst:o_game_map_controller.__province.__conspiracy.__reset_conspiracy", "", false },
			{ EventFamily::Guest, "inst:o_game_map_controller.__province.__unique_guests.__current_guest", "inst:o_game_map_controller.__province.__unique_guests.remove_guest_result", "", false },
			{ EventFamily::Unrest, "inst:o_game_map_controller.__province.__politics_manager.__is_unrest_active_struct", "inst:o_game_map_controller.__province.__politics_manager.__all_unrest_finish", "", false },
		};
	}

	const std::vector<EventEndRow>& EventEndTable()
	{
		return k_Ends;
	}

	const EventEndRow* FindEventEnd(EventFamily Family)
	{
		for (const EventEndRow& row : k_Ends)
			if (row.Family == Family)
				return &row;
		return nullptr;
	}

	bool EventEndAllowed(const EventEndRow& Row)
	{
		return Row.Verified;
	}

	bool ParseEventFamily(const std::string& Word, EventFamily& Out)
	{
		for (const EventEndRow& row : k_Ends)
			if (Word == EventFamilyKey(row.Family))
			{
				Out = row.Family;
				return true;
			}
		return false;
	}

	CancelStep ChooseCancelStep(bool HasForced)
	{
		return HasForced ? CancelStep::Call : CancelStep::Nothing;
	}

	std::string EventStatusText(EventFamily Family, bool Read, bool Present, const std::string& Name)
	{
		if (Family == EventFamily::Rebellion)
			return "모름 (읽을 자리를 아직 모른다)";
		if (!Read)
			return "읽지 못함";
		if (!Present)
			return "없음";
		return Name.empty() ? "진행 중 (이름을 읽지 못함)" : "진행 중: " + Name;
	}

	std::string ForceEventReport(const std::string& Name, char Outcome, const std::string& Why)
	{
		switch (Outcome)
		{
		case 'n': return Name + ": 게임에 그 이름의 이벤트가 없습니다";
		case 'w': return Name + ": 강제 이벤트에 쓰지 못했습니다 (" + Why + ")";
		default: return Name + ": 강제 이벤트로 써 두었습니다. 게임의 감독이 다음에 이벤트를 뽑을 때(하루 한 번, 오후) 이것을 고릅니다";
		}
	}

	std::string CancelEventReport(char Outcome, const std::string& Detail)
	{
		switch (Outcome)
		{
		case 'n': return "예약된 이벤트가 없습니다";
		case 'f': return "예약을 지우는 함수를 부르지 못했습니다 (" + Detail + ")";
		case 'u': return "함수를 불렀지만 예약이 남아 있습니다: " + Detail;
		default: return "예약을 지웠습니다: " + Detail;
		}
	}

	std::string EndEventReport(EventFamily Family, char Outcome, const std::string& Detail)
	{
		const std::string word = EventFamilyWord(Family);
		switch (Outcome)
		{
		case 'x': return word + " 끝내기는 확인 전이라 부르지 않습니다 (research/29 의 절차로 게임에서 본 뒤에 켭니다)";
		case 'n': return "진행 중인 " + word + "이 없습니다";
		case 'f': return word + " 끝내기의 함수를 부르지 못했습니다 (" + Detail + ")";
		case 'u': return "함수를 불렀지만 " + word + "가 그대로입니다";
		default: return word + "를 끝냈습니다: " + Detail;
		}
	}
```

- [ ] **Step 5: `WorldPlan`** — `src/core/WorldPlan.hpp`: `enum class WorldAct { CooldownsClear, BishopSend, SeasonShow, SeasonDelay, SeasonEnd, SaveNow, EventForce, EventList, EventCancel, EventEnd };` 로 바꾸고, `ForceEventReport` 의 선언과 주석 두 줄을 지우고(EventPlan 으로 갔다), `IsSeasonAct` 뒤에 더한다:

```cpp
	// 이벤트의 일인가(일으키기, 목록, 예약 취소, 끝내기, 쿨다운 지우기). src/Events 가 하고 제 자리에 결과를 둔다(계절과 같다).
	bool IsEventAct(WorldAct Act);
	// 가족(kind=)을 받는 일인가(끝내기만).
	bool WorldActNeedsKind(WorldAct Act);
```

`src/core/WorldPlan.cpp`: `k_Acts` 에 `{ WorldAct::EventList, "events", false }, { WorldAct::EventCancel, "event_cancel", true }, { WorldAct::EventEnd, "event_end", true },` 를 `EventForce` 줄 뒤에 더하고, `ForceEventReport` 의 정의를 지우고, `IsSeasonAct` 뒤에 더한다:

```cpp
	bool IsEventAct(WorldAct Act)
	{
		return Act == WorldAct::EventForce || Act == WorldAct::EventList || Act == WorldAct::EventCancel || Act == WorldAct::EventEnd || Act == WorldAct::CooldownsClear;
	}

	bool WorldActNeedsKind(WorldAct Act)
	{
		return Act == WorldAct::EventEnd;
	}
```

- [ ] **Step 6: `ParseWorldLine`** — `src/core/RemoteCommand.cpp` 의 함수를 이것으로 바꾼다(파일 머리의 포함에 `#include "EventPlan.hpp"` 을 더한다):

```cpp
		void ParseWorldLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			// world <cooldowns_clear|bishop|season|season_delay|season_end>      한 번 하는 일(core/WorldPlan): 이벤트 쿨다운 지우기, 주교 부르기,
			// 계절 보기·가혹한 계절 하루 미루기·지금 단계 끝내기
			// world save 는 지금 저장, world event name=<이벤트의 시스템 이름> 은 그 이벤트를 감독의 강제 이벤트로 써 둔다(2026-10-07. research/28).
			// world events [group=<묶음>] [find=<글>] 은 이벤트의 표, world event_cancel 은 예약 취소, world event_end kind=<raid|prophecy|conspiracy|guest|unrest> 는 끝내기(research/29).
			WorldAct act = WorldAct::CooldownsClear;
			if (count < 2 || !ParseWorldAct(tokens[1], act))
				return Fail(command, "world needs one of: " + WorldActWords());
			command.Target = tokens[1];
			if (!Options(tokens, 2, command))
				return;
			const auto name = command.Options.find("name");
			const auto kind = command.Options.find("kind");
			if (WorldActNeedsName(act))
			{
				if (name == command.Options.end() || !GoodEventName(name->second))
					return Fail(command, "world event needs name=<the event's system name: letters, digits, underscores>");
			}
			else if (WorldActNeedsKind(act))
			{
				EventFamily family = EventFamily::None;
				if (kind == command.Options.end() || !ParseEventFamily(kind->second, family))
					return Fail(command, "world event_end needs kind=<raid|prophecy|conspiracy|guest|unrest>");
			}
			else if (act == WorldAct::EventList)
			{
				for (const auto& [key, value] : command.Options)
					if (key != "group" && key != "find")
						return Fail(command, "world events takes only group= and find=");
			}
			else if (count != 2)
				return Fail(command, std::string("world ") + tokens[1] + " takes nothing");
		}
```

`src/core/RemoteCommand.hpp` 의 주석 줄 `//   world <cooldowns_clear|bishop>  …` 뒤에 한 줄: `//   world events [group=<묶음>] [find=<글>],  world event_cancel,  world event_end kind=<raid|prophecy|conspiracy|guest|unrest>   이벤트의 표, 예약 취소, 확인된 가족의 끝내기(research/29)`.

- [ ] **Step 7: 빌드와 시험**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1` 뒤 `pwsh -NoProfile -File E:\NlToyBox\tools\test-native.ps1`
Expected: `build ok`, 경고 0, `core tests: 120 passed`. (`src/World.cpp` 는 `NlCore::ForceEventReport` 를 쓰므로 `#include "core/EventPlan.hpp"` 을 더해야 빌드된다 — Task 3 에서 World 의 이벤트 코드가 Events 로 가지만, 이 Task 에서는 World.cpp 의 포함에 그 한 줄만 더한다.)

- [ ] **Step 8: 커밋**

```bash
git -C E:\NlToyBox add src/core/EventPlan.hpp src/core/EventPlan.cpp src/core/WorldPlan.hpp src/core/WorldPlan.cpp src/core/RemoteCommand.hpp src/core/RemoteCommand.cpp src/World.cpp tests/native/test_events.cpp tests/native/test_world.cpp
git -C E:\NlToyBox commit -m "feat(core): 끝내기의 표(확인 전 다섯), 취소·상태·결과의 글, WorldAct 셋과 원격 world events|event_cancel|event_end (이벤트 탭, Task 2)"
```

---

### Task 3: `src/Events` — World 의 이벤트 부분을 그대로 옮긴다(동작 불변)

**Files:**
- Create: `src/Events.hpp`, `src/Events.cpp`
- Modify: `src/World.hpp`, `src/World.cpp`, `src/Menu.cpp`(`Area::Events` 의 그리기와 틱의 차례), `src/ModuleMain.cpp`(`NlEvents::Init`), `src/Remote.cpp:441-454`(`DoWorld`), `CMakeLists.txt`(`src/Events.cpp` 를 `src/World.cpp` 뒤에)

**Interfaces:**
- Consumes: Task 2 의 `IsEventAct`, `ForceEventReport`(EventPlan).
- Produces(`namespace NlEvents`): `using LogFn = std::function<void(const std::string&)>;`, `void Init(LogFn Log, const std::filesystem::path& GameDir);`, `void Tick(double Now, bool Visible);`, `void Draw();`, `std::string Do(NlCore::WorldAct Act);`, `std::string ForceEvent(const std::string& Name);`.
  이 Task 에서는 `Init` 이 `GameDir` 를 받지만 쓰지 않는다(`(void)GameDir;`. Task 4 가 화면 이름을 읽는다). `Tick` 의 `Visible` 도 받기만 한다.

- [ ] **Step 1: 헤더** — `src/Events.hpp`:

```cpp
#pragma once
// 이벤트(research/28, 29): 게임의 이벤트 61개의 표와 줄마다 일으키기(감독의 __debug_forced_event 에 구조체를 쓴다), 예약 취소(reset_debug_forced_event()),
// 가족 다섯(습격·예언·음모·손님·소요)의 진행 중 상태와 확인된 가족만의 끝내기, 이벤트 쿨다운 지우기. 2026-10-07 에 src/World.cpp 의 이벤트 부분을 옮기고 더했다.
// 그리는 쪽은 청만 쌓고 틱이 러너를 건드린다. 스냅샷은 글과 수다.

#include "core/EventPlan.hpp"
#include "core/WorldPlan.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace NlEvents
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번(NlWorld::Init 뒤). GameDir: 게임 폴더(화면 이름을 localization\main.csv 에서 읽는다. 러너를 부르지 않는다).
	void Init(LogFn Log, const std::filesystem::path& GameDir);
	// 게임 스레드의 틱(NlWorld::GameTick 뒤). 쌓인 청을 하나 하고, 이름을 한 번 읽고, 패널이 보이면 1초마다 상태를 읽는다. Visible: 이벤트 패널이 보이는가.
	void Tick(double Now, bool Visible);
	// 이벤트 패널(치트 표의 항목 아래).
	void Draw();
	// 이벤트의 일(EventForce 는 ForceEvent 로, EventList 는 List 로. 그 밖: EventCancel, EventEnd(가족은 SetEndFamily 로 먼저), CooldownsClear)을 지금 한다(원격). 돌려주는 것: 한 일.
	std::string Do(NlCore::WorldAct Act);
	// 이벤트 골라 일으키기(원격 world event name=…). 돌려주는 것: 한 일.
	std::string ForceEvent(const std::string& Name);
}
```

- [ ] **Step 2: 구현** — `src/Events.cpp`(World.cpp 의 `k_Director`·`k_EventsData`, `g_PendingEvent`·`g_EventNames`·`g_NextNames`·`g_EventPick`, `ClearNumbers`, `ForceEventNow`, `ReadEventNames`, `DoNow` 의 `EventForce`·`CooldownsClear` 갈래, `DrawEvents` 를 **그대로** 옮긴 것이다. 로그의 글은 그대로 `world: …`):

```cpp
#include "Events.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/Guard.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <deque>
#include <mutex>
#include <vector>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::WorldAct;

namespace
{
	// 이벤트를 고르는 감독(research/14). 쿨다운은 이벤트의 이름 → 남은 날, 묶음의 이름 → 남은 날.
	constexpr const char* k_Director = "inst:o_game_map_controller.__game_director";
	constexpr const char* k_EventsData = "inst:o_data.__game_director_events_data";

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlEvents::LogFn g_Log;
	std::deque<WorldAct> g_Queue;		// 창이 쌓고 틱이 한다
	std::string g_Last;					// 마지막으로 한 일
	std::string g_PendingEvent;			// 일으킬 이벤트의 이름(창이 적고 틱이 쓴다)
	std::vector<std::string> g_EventNames;	// 게임의 이벤트 이름들(감독의 자료의 ds_map 열쇠. 게임 화면에서 한 번 읽는다)
	double g_NextNames = 0;				// 이름을 다시 읽어 볼 시각
	int g_EventPick = 0;				// 창의 선택
	bool g_Busy = false;				// 하는 중이다(여기서 부른 게임의 함수가 틱을 다시 부르면 안쪽은 아무것도 하지 않는다)

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 구조체의 칸 가운데 0 보다 큰 수에 0 을 쓴다. 불리언인 칸은 수로 치지 않는다(true 를 false 로 덮어쓰지 않게).
	NlCore::ClearResult ClearNumbers(const std::string& Path)
	{
		NlCore::ClearResult out;
		RValue box;		// 이 함수 안에서만 든다
		Holder kind = Holder::None;
		std::string why;
		if (!NlAccess::Open(NlCore::ParseAskPath(Path), box, kind, why) || kind != Holder::Struct)
		{
			Log("world: cannot open " + Path + ": " + (why.empty() ? "not a struct" : why));
			return out;
		}
		std::vector<NlCore::PathStep> steps;		// 도는 동안에는 쓰지 않는다
		const double children = NlAccess::ForEachChild(box, kind, [&](const NlCore::PathStep& step, const RValue& child) {
			const bool number = NlGame::IsRealNumber(child);
			if (NlCore::ShouldClearCooldown(number, number ? child.ToDouble() : 0))
				steps.push_back(step);
			return true;
		});
		if (children < 0)
		{
			Log("world: cannot list " + Path);
			return out;
		}
		out.Opened = true;
		for (const NlCore::PathStep& step : steps)
		{
			if (NlAccess::SetNumber(box, step, 0, why))		// 쓴 뒤 다시 읽어 확인한다
				out.Cleared++;
			else
			{
				out.Failed++;
				Log("world: cannot clear " + Path + "." + step.Name + ": " + why);
			}
		}
		return out;
	}

	// 이벤트 골라 일으키기: 감독의 자료의 __events_by_name(ds_map: 이름 -> 이벤트 구조체)에서 그 구조체를 얻어 __debug_forced_event 에 쓴다.
	// 게임의 감독은 하루 한 번 이벤트를 뽑을 때 get_debug_forced_event() 를 읽고 reset_debug_forced_event() 로 지운다(research/28 의 기록).
	// 구조체를 쓰는 것이 맞다(research/28: 감독의 결정 함수가 그 구조체를 돌려주고 그 이벤트가 쿨다운에 올랐다).
	std::string ForceEventNow(const std::string& Name)
	{
		double id = 0;
		if (!NlAccess::ReadNumber(std::string(k_EventsData) + ".__events_by_name", id))
			return NlCore::ForceEventReport(Name, 'w', "이벤트 목록(ds_map)의 번호를 읽지 못했습니다");
		RValue event;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath("map:" + NlCore::Shortest(id) + "@" + Name), event, why) || !event.IsStruct())
			return NlCore::ForceEventReport(Name, 'n', std::string());
		Log("world: forced event: writing the struct of " + Name + " to " + std::string(k_EventsData) + ".__debug_forced_event");		// 쓰기 전에 남긴다
		if (!NlAccess::Write(NlCore::ParseAskPath(std::string(k_EventsData) + ".__debug_forced_event"), event, why))
			return NlCore::ForceEventReport(Name, 'w', why);
		return NlCore::ForceEventReport(Name, 'd', std::string());
	}

	// 이벤트의 이름들(ds_map 의 열쇠)을 한 번 읽는다. 못 읽으면 다음 틱에 다시.
	void ReadEventNames()
	{
		double id = 0;
		RValue keys;		// 이 함수 안에서만 든다
		if (!NlAccess::ReadNumber(std::string(k_EventsData) + ".__events_by_name", id) || !NlGame::Call("ds_map_keys_to_array", { RValue(id) }, keys) || !keys.IsArray())
			return;
		std::vector<std::string> names;
		NlAccess::ForEachChild(keys, Holder::Array, [&](const NlCore::PathStep&, const RValue& key) {
			if (key.IsString())
				names.push_back(key.ToString());
			return true;
		});
		std::sort(names.begin(), names.end());
		g_EventNames = std::move(names);
		Log("world: event names: " + std::to_string(g_EventNames.size()));
	}

	std::string DoNow(WorldAct Act)
	{
		if (!NlAccess::InGame())
			return "게임 화면이 아닙니다";
		switch (Act)
		{
		case WorldAct::EventForce:
			return ForceEventNow(g_PendingEvent);
		case WorldAct::CooldownsClear:
		{
			const NlCore::ClearResult events = ClearNumbers(std::string(k_Director) + ".__events_cooldowns");
			const NlCore::ClearResult groups = ClearNumbers(std::string(k_Director) + ".__events_groups_cooldowns");
			if (NlCore::CooldownTouched(events, groups))		// 한쪽만 됐어도 건드린 것은 남긴다
				Log("world: event cooldowns: events cleared " + std::to_string(events.Cleared) + " failed " + std::to_string(events.Failed)
					+ ", groups cleared " + std::to_string(groups.Cleared) + " failed " + std::to_string(groups.Failed));
			return NlCore::CooldownReport(events, groups);
		}
		default:
			return std::string();
		}
	}

	std::string Remember(WorldAct Act, std::string Text)
	{
		if (NlCore::WorldActChanges(Act))
			g_Last = Text;
		return Text;
	}

	constexpr size_t k_MaxQueue = 4;		// 창이 쌓아 둘 청의 수. 넘치면 받지 않고 결과 줄에 적는다

	void Push(WorldAct Act)
	{
		if (g_Queue.size() >= k_MaxQueue)
		{
			g_Last = NlCore::QueueFullText(k_MaxQueue);
			return;
		}
		g_Queue.push_back(Act);
	}
}

void NlEvents::Init(LogFn Log_, const std::filesystem::path& GameDir)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	(void)GameDir;		// Task 4 가 화면 이름을 읽는다
}

void NlEvents::Tick(double Now, bool Visible)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return;
	(void)Visible;		// Task 4 가 패널이 보일 때 상태를 읽는다
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

void NlEvents::Draw()
{
	std::lock_guard lock(g_Mutex);
	// 이벤트 골라 일으키기(2026-10-07. research/28): 이름을 고르고 누르면 틱이 그 이벤트의 구조체를 감독의 강제 이벤트에 쓴다.
	ImGui::SeparatorText("이벤트 골라 일으키기");
	if (g_EventNames.empty())
		NlUi::Hint("게임 화면에서 이벤트의 이름을 읽습니다.");
	else
	{
		if (g_EventPick < 0 || static_cast<size_t>(g_EventPick) >= g_EventNames.size())
			g_EventPick = 0;
		ImGui::SetNextItemWidth(320);
		if (ImGui::BeginCombo("##event", g_EventNames[g_EventPick].c_str()))
		{
			for (size_t i = 0; i < g_EventNames.size(); i++)
				if (ImGui::Selectable(g_EventNames[i].c_str(), static_cast<int>(i) == g_EventPick))
					g_EventPick = static_cast<int>(i);
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button("일으키기"))
		{
			g_PendingEvent = g_EventNames[g_EventPick];
			Push(WorldAct::EventForce);
		}
	}
	NlUi::Hint("게임의 감독이 보는 '강제 이벤트' 자리에 고른 이벤트의 구조체를 써 둡니다. 감독은 하루 한 번(오후) 이벤트를 뽑을 때 그 자리를 읽어 그것을 고르고 지웁니다"
		"(research/28: 써 둔 u_guest_bard 가 그날 뽑혀 쿨다운에 올랐다). 쿨다운 중인 이벤트도 오는지는 재지 않았습니다. 습격·반란·예언 이벤트도 그대로 옵니다.");
	ImGui::SeparatorText("쿨다운");
	if (ImGui::Button("이벤트 쿨다운 지우기"))
		Push(WorldAct::CooldownsClear);
	NlUi::Hint("게임이 이벤트를 고를 때 보는 '남은 날'(이벤트마다, 묶음마다)을 0 으로 씁니다. 써지는 것까지 봤고, 이벤트가 더 일찍 오는지는 확인 전입니다. "
		"쿨다운은 세이브에 들어가는 자료입니다(세이브 파일에 그 열쇠가 있습니다). 쓴 채 저장하면 남습니다.");
	if (!g_Last.empty())
		NlUi::Hint(g_Last.c_str());
}

std::string NlEvents::Do(NlCore::WorldAct Act)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return "busy";
	const NlCore::ScopedFlag busy(g_Busy);
	return Remember(Act, DoNow(Act));
}

std::string NlEvents::ForceEvent(const std::string& Name)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return "busy";
	const NlCore::ScopedFlag busy(g_Busy);
	g_PendingEvent = Name;
	return Remember(WorldAct::EventForce, DoNow(WorldAct::EventForce));
}
```

- [ ] **Step 3: World 에서 뺀다** — `src/World.cpp`:
  - 익명 네임스페이스에서 `k_Director`, `g_PendingEvent`, `g_EventNames`, `g_NextNames`, `g_EventPick`, `k_EventsData` 의 줄과 주석을 지운다. `ClearNumbers`, `ForceEventNow`, `ReadEventNames` 함수(각각의 위 주석까지)를 지운다.
  - `DoNow` 에서 `case WorldAct::EventForce:` 와 `case WorldAct::CooldownsClear: { … }` 갈래를 지우고 그 자리에 주석 한 줄: `// 이벤트의 일(EventForce·EventList·EventCancel·EventEnd·CooldownsClear)은 여기 오지 않는다: NlWorld::Do 가 NlEvents::Do 로 바로 돌려준다.`
  - `LastOf` 의 주석을 `// 한 일의 글을 그 패널의 자리에 둔다(지금 저장은 유틸 패널에, 주교는 종교 패널에. 계절과 이벤트는 제 모듈이 제 자리에 둔다).` 로.
  - `GameTick` 에서 `names` 관련 네 줄(`const bool names = …`, 조건의 `&& !names`, `if (names) { … }`)을 지워 이렇게 둔다:
    ```cpp
    void NlWorld::GameTick(double Now)
    {
    	std::lock_guard lock(g_Mutex);
    	if (g_Busy)
    		return;
    	if (g_Queue.empty())		// 시각부터 본다(이 틱은 오브젝트 이벤트마다 불린다). 계절·광산·이벤트는 제 모듈의 틱이 한다
    		return;
    	(void)Now;
    	const NlCore::ScopedFlag busy(g_Busy);
    	const WorldAct act = g_Queue.front();
    	g_Queue.pop_front();
    	Remember(act, DoNow(act));
    }
    ```
  - `NlWorld::DrawEvents` 함수 전체와 `NlWorld::ForceEvent` 함수 전체를 지운다. `NlWorld::Do` 의 첫 분기 뒤에 더한다: `if (NlCore::IsEventAct(Act)) return NlEvents::Do(Act);		// 이벤트는 src/Events 가 하고 제 자리(이벤트 패널)에 적는다`. 포함에 `#include "Events.hpp"` 을 더하고(`Season.hpp` 뒤), Task 2 에서 더한 `#include "core/EventPlan.hpp"` 은 지운다(World 는 더 쓰지 않는다).
  - `src/World.hpp`: 머리 주석을 `// 종교·유틸 영역에서 한 번 하는 일(research/14, 28): 주교 부르기, 지금 저장. 이벤트는 src/Events, 계절은 src/Season, 광산 붙들기는 src/Mines (2026-10-07).` 로, `void DrawEvents();` 줄과 `ForceEvent` 의 선언(주석 포함)을 지운다. `GameTick` 의 주석을 `// 게임 스레드의 틱. 쌓인 청을 하나 한다. Now: 모듈이 뜬 뒤의 초.` 로.
  - 빌드해서 `World.cpp` 에 남은 안 쓰는 포함(`<algorithm>` 은 `SaveNow` 의 `std::find` 가 쓴다 — 둔다)을 본다. 경고가 없으면 그대로.

- [ ] **Step 4: 잇는다** — `src/Menu.cpp`: `case Area::Events:` 의 `NlWorld::DrawEvents();` 를 `NlEvents::DrawEvents` 가 아니라 `NlEvents::Draw();` 로. 틱의 차례에서 `NlWorld::GameTick(now);` 바로 뒤에 `NlEvents::Tick(now, visible && page == Area::Events);`. 포함에 `#include "Events.hpp"`.
  `src/ModuleMain.cpp`: `NlWorld::Init(…)` 바로 뒤에 `NlEvents::Init([](const std::string& Line) { LogLine(Line); }, module_dir.parent_path().parent_path());		// 이벤트의 화면 이름을 게임의 localization\main.csv 에서 읽는다(Task 4)`. 포함에 `#include "Events.hpp"`.
  `src/Remote.cpp` 의 `DoWorld` 를 이것으로(포함에 `#include "Events.hpp"`):

```cpp
	// 종교·유틸·이벤트 패널의 단추와 같은 길(NlWorld::Do, NlEvents). 줄의 꼴은 ParseRemoteLine 이 이미 봤다.
	void DoWorld(const RemoteCommand& C)
	{
		NlCore::WorldAct act = NlCore::WorldAct::CooldownsClear;
		if (!NlCore::ParseWorldAct(C.Target, act))
		{
			Say("  : world needs one of: " + NlCore::WorldActWords());
			return;
		}
		Say("  running world " + C.Target);		// 죽으면 여기까지 남는다
		if (act == NlCore::WorldAct::EventForce)
			Say("  " + NlEvents::ForceEvent(C.Options.count("name") ? C.Options.at("name") : std::string()));
		else
			Say("  " + NlWorld::Do(act));		// 이벤트의 다른 일은 NlWorld::Do 가 NlEvents 로 넘긴다(Task 4 가 events·event_end 를 더 가른다)
	}
```

  `CMakeLists.txt`: `src/World.cpp` 뒤에 `  src/Events.cpp`.

- [ ] **Step 5: 빌드와 시험**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1` 뒤 `pwsh -NoProfile -File E:\NlToyBox\tools\test-native.ps1`
Expected: `build ok`, 경고 0, `core tests: 120 passed`. `grep -n "DrawEvents\|ForceEventNow\|ReadEventNames\|k_EventsData" E:\NlToyBox\src\World.cpp` 가 비어야 한다.

- [ ] **Step 6: 커밋**

```bash
git -C E:\NlToyBox add src/Events.hpp src/Events.cpp src/World.hpp src/World.cpp src/Menu.cpp src/ModuleMain.cpp src/Remote.cpp CMakeLists.txt
git -C E:\NlToyBox commit -m "refactor(world): 이벤트(이름 읽기, 일으키기, 쿨다운 지우기, 그리기)를 src/Events 로 그대로 옮긴다 (이벤트 탭, Task 3)"
```

---

### Task 4: `src/Events` — 스냅샷(쿨다운·예약·진행 중), 화면 이름, 예약 취소, 끝내기, 원격 `events`·`event_end`

**Files:**
- Modify: `src/Events.hpp`, `src/Events.cpp`, `src/Remote.cpp`(`DoWorld`)

**Interfaces:**
- Consumes: Task 1·2 의 `EventTable`, `EventLabel`, `MergeEventNames`, `EventEndTable`, `FindEventEnd`, `EventEndAllowed`, `ChooseCancelStep`, `EventStatusText`, `CancelEventReport`, `EndEventReport`, `ParseEventFamily`, `CooldownText`, `EventRowShown`.
- Produces(`NlEvents`): `std::vector<std::string> List(const std::string& Group, const std::string& Find);`(원격 `world events`. 지금 읽어서 줄로), `std::string End(NlCore::EventFamily Family);`(원격 `world event_end`), 그리고 창이 쓰는 안쪽의 스냅샷(`Snapshot`). `Do(WorldAct::EventCancel)` 이 취소를 한다.

- [ ] **Step 1: 헤더에 더한다**(`ForceEvent` 뒤):

```cpp
	// 이벤트의 표(원격 world events [group=] [find=]). 지금 읽어서: 첫 줄 예약, 가족마다 한 줄 상태, 그 뒤 거른 줄들, 끝에 "(N of M)". 게임 화면이 아니면 그 한 줄.
	std::vector<std::string> List(const std::string& Group, const std::string& Find);
	// 끝내기(원격 world event_end kind=…). 확인 전인 가족은 부르지 않는다. 돌려주는 것: 한 일.
	std::string End(NlCore::EventFamily Family);
```

- [ ] **Step 2: 구현** — `src/Events.cpp` 를 아래처럼 고친다(Task 3 의 코드에 더하고 바꾼다. 포함에 `#include "core/Localization.hpp"`, `<array>`, `<fstream>`, `<iterator>`, `<unordered_map>` 를 더한다).

익명 네임스페이스의 전역에 더한다(`g_Busy` 뒤):

```cpp
	constexpr size_t k_Families = 7;			// EventFamily 의 수(None 포함). 가족을 자리로 쓴다
	struct Status			// 가족 하나의 진행 중(틱이 읽는다)
	{
		bool Read = false, Present = false;
		std::string Name;		// 그 구조체의 __system_name 이나 __name(글일 때)
	};
	struct Snapshot			// 틱이 채우고 Draw 가 읽는다. 글과 수만
	{
		bool Ready = false;
		std::vector<std::string> Names;					// 보일 이름(표의 차례, 그 뒤 표에 없는 것)
		std::unordered_map<std::string, double> Cooldowns, GroupCooldowns;		// 이름 → 남은 날(없으면 칸이 없다)
		std::string Forced;								// 예약된 이벤트의 이름(없으면 빈 글)
		int Delayed = -1;								// 지연 생성의 수(-1: 읽지 못함)
		size_t MissingFromGame = 0;						// 표에만 있는 이름의 수
		std::array<Status, k_Families> Families;		// 가족을 자리로(EventFamily 의 수)
	};
	Snapshot g_Now;
	std::unordered_map<std::string, std::string> g_Captions;	// 열쇠 → 화면 이름(main.csv 에서. 표의 열쇠만)
	double g_NextRead = 0;				// 상태를 다시 읽을 시각(패널이 보일 때 1초)
	char g_Filter[48] = "";				// 창의 찾기 칸
	int g_GroupPick = 0;				// 창의 묶음(0 전체, 1.. EventGroups 의 차례)
	NlCore::EventFamily g_EndFamily = NlCore::EventFamily::None;		// 끝낼 가족(창이 적고 틱이 쓴다)

	std::string LabelOf(const std::string& Name)
	{
		const NlCore::EventRow* row = NlCore::FindEvent(Name);
		return row ? NlCore::EventLabel(*row, g_Captions) : Name;
	}

	// 구조체 안의 칸 가운데 글인 것 하나(__system_name 이 먼저, 그 다음 __name)를 이름으로.
	std::string NameInside(const std::string& Path)
	{
		std::string text;
		if (NlAccess::ReadText(Path + ".__system_name", text) && !text.empty())
			return text;
		if (NlAccess::ReadText(Path + ".__name", text) && !text.empty())
			return text;
		return std::string();
	}

	// 가족 하나의 진행 중을 읽는다. 자리를 읽지 못하면 Read false. undefined 나 -4 는 없음, 구조체는 진행 중.
	// 소요(Unrest)의 자리는 불리언들의 구조체다: 참인 칸이 있으면 진행 중이고 그 칸의 이름이 이름(뜻은 추정. research/29).
	Status ReadStatus(const NlCore::EventEndRow& Row)
	{
		Status out;
		RValue value;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath(Row.StatusPath), value, why))
			return out;
		out.Read = true;
		if (!value.IsStruct())
			return out;		// undefined, -4, 수 → 없음
		if (Row.Family == NlCore::EventFamily::Unrest)
		{
			NlAccess::ForEachChild(value, Holder::Struct, [&](const NlCore::PathStep& step, const RValue& child) {
				if (NlGame::IsNumber(child) && child.ToDouble() != 0 && out.Name.empty())
				{
					out.Present = true;
					out.Name = step.Name;
				}
				return true;
			});
			return out;
		}
		out.Present = true;
		out.Name = NameInside(Row.StatusPath);
		return out;
	}

	// 쿨다운의 구조체(이름 → 남은 날)를 수의 칸만 받는다.
	void ReadCooldowns(const std::string& Path, std::unordered_map<std::string, double>& Out)
	{
		Out.clear();
		RValue box;		// 이 함수 안에서만 든다
		Holder kind = Holder::None;
		std::string why;
		if (!NlAccess::Open(NlCore::ParseAskPath(Path), box, kind, why) || kind != Holder::Struct)
			return;
		NlAccess::ForEachChild(box, kind, [&](const NlCore::PathStep& step, const RValue& child) {
			if (NlGame::IsRealNumber(child))
				Out[step.Name] = child.ToDouble();
			return true;
		});
	}

	std::string ReadForced()
	{
		std::string name;
		return NlAccess::ReadText(std::string(k_EventsData) + ".__debug_forced_event.__system_name", name) ? name : std::string();
	}

	int ReadDelayed()
	{
		RValue list;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath(std::string(k_Director) + ".__delayed_events"), list, why) || !list.IsArray())
			return -1;
		const double length = NlGame::ArrayLength(list);
		return length < 0 ? -1 : static_cast<int>(length);
	}

	// 패널의 스냅샷을 새로 읽는다(게임 화면에서만).
	void ReadSnapshot()
	{
		Snapshot next;
		const NlCore::EventListing merged = NlCore::MergeEventNames(g_EventNames);
		next.Names = merged.Known;
		next.Names.insert(next.Names.end(), merged.Extra.begin(), merged.Extra.end());
		next.MissingFromGame = merged.MissingFromGame;
		ReadCooldowns(std::string(k_Director) + ".__events_cooldowns", next.Cooldowns);
		ReadCooldowns(std::string(k_Director) + ".__events_groups_cooldowns", next.GroupCooldowns);
		next.Forced = ReadForced();
		next.Delayed = ReadDelayed();
		for (const NlCore::EventEndRow& row : NlCore::EventEndTable())
			next.Families[static_cast<size_t>(row.Family)] = ReadStatus(row);
		next.Ready = true;
		g_Now = std::move(next);
	}

	double CooldownOf(const std::unordered_map<std::string, double>& Map, const std::string& Key)
	{
		const auto found = Map.find(Key);
		return found == Map.end() ? -1 : found->second;
	}

	// 예약 취소: 예약을 읽고, 있으면 게임의 reset_debug_forced_event()(인자 없음. 게임이 그 꼴로 부른다. research/28·29)를 부르고, 다시 읽어 판정한다.
	std::string CancelNow()
	{
		const std::string before = ReadForced();
		if (NlCore::ChooseCancelStep(!before.empty()) == NlCore::CancelStep::Nothing)
			return NlCore::CancelEventReport('n', std::string());
		RValue result;		// 이 함수 안에서만 든다
		std::string why;
		Log("world call reset_debug_forced_event() (forced: " + before + ")");		// 부르기 전에 남긴다
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(std::string(k_EventsData) + ".reset_debug_forced_event"), {}, result, why))
			return NlCore::CancelEventReport('f', why);
		const std::string after = ReadForced();
		Log("world: forced event after reset: " + (after.empty() ? std::string("(none)") : after));
		return NlCore::CancelEventReport(after.empty() ? 'd' : 'u', before);
	}

	// 끝내기: 확인된 가족만. 진행 중을 읽고, 표의 꼴(지금은 인자 없음뿐)로 부르고, 다시 읽어 판정한다.
	std::string EndNow(NlCore::EventFamily Family)
	{
		const NlCore::EventEndRow* row = NlCore::FindEventEnd(Family);
		if (!row || !NlCore::EventEndAllowed(*row))
			return NlCore::EndEventReport(Family, 'x', std::string());
		const Status before = ReadStatus(*row);
		if (!before.Read || !before.Present)
			return NlCore::EndEventReport(Family, 'n', std::string());
		if (row->ArgShape[0])		// 본 적 없는 꼴로는 부르지 않는다
			return NlCore::EndEventReport(Family, 'f', std::string("인자의 꼴(") + row->ArgShape + ")을 아직 부르지 못합니다");
		RValue result;		// 이 함수 안에서만 든다
		std::string why;
		Log(std::string("world call ") + row->EndPath + "() (" + NlCore::EventFamilyKey(Family) + ": " + before.Name + ")");		// 부르기 전에 남긴다
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(row->EndPath), {}, result, why))
			return NlCore::EndEventReport(Family, 'f', why);
		const Status after = ReadStatus(*row);
		return NlCore::EndEventReport(Family, after.Read && !after.Present ? 'd' : 'u', before.Name);
	}
```

`DoNow` 의 `switch` 에 갈래 둘을 더한다(`CooldownsClear` 뒤):

```cpp
		case WorldAct::EventCancel:
			return CancelNow();
		case WorldAct::EventEnd:
			return EndNow(g_EndFamily);
```

`NlEvents::Init` 을 이것으로(화면 이름 읽기. 표의 열쇠만 받는다):

```cpp
void NlEvents::Init(LogFn Log_, const std::filesystem::path& GameDir)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	// 이벤트의 화면 이름. 게임의 글은 레포에 싣지 않는다: 게임 폴더의 파일에서 표의 열쇠만 읽는다(한 줄짜리 짧은 글만 받는다. 계절과 같다).
	std::ifstream in(GameDir / "localization" / "main.csv", std::ios::binary);
	if (in)
	{
		const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		std::unordered_map<std::string, std::string> rows;
		std::string why;
		if (NlCore::ReadLocalization(text, "", { "Korean", "English" }, rows, why))
			for (const NlCore::EventRow& row : NlCore::EventTable())
			{
				if (!row.CaptionKey[0])
					continue;
				const auto found = rows.find(row.CaptionKey);
				if (found != rows.end() && !found->second.empty() && found->second.size() <= 48 && found->second.find_first_of("\r\n<{") == std::string::npos)
					g_Captions.emplace(row.CaptionKey, found->second);
			}
	}
	Log("world: " + std::to_string(g_Captions.size()) + " event caption(s) from the game's localization file");
}
```

`NlEvents::Tick` 을 이것으로:

```cpp
void NlEvents::Tick(double Now, bool Visible)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return;
	const bool names = g_EventNames.empty() && Now >= g_NextNames;
	const bool read = Visible && Now >= g_NextRead;
	if (g_Queue.empty() && !names && !read)		// 시각부터 본다(이 틱은 오브젝트 이벤트마다 불린다)
	{
		if (!Visible && g_Now.Ready)
			g_Now = Snapshot{};		// 패널을 다시 열면 새로 읽은 것을 보인다
		return;
	}
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
	if (read)
	{
		g_NextRead = Now + 1;
		if (NlAccess::InGame())
			ReadSnapshot();
		else
			g_Now = Snapshot{};
	}
}
```

공개 함수 둘을 더한다(파일 끝):

```cpp
std::vector<std::string> NlEvents::List(const std::string& Group, const std::string& Find)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const NlCore::ScopedFlag busy(g_Busy);
	if (!NlAccess::InGame())
		return { "게임 화면이 아닙니다" };
	if (g_EventNames.empty())
		ReadEventNames();
	ReadSnapshot();
	std::vector<std::string> lines;
	lines.push_back("예약: " + (g_Now.Forced.empty() ? std::string("없음") : g_Now.Forced) + " (지연 " + (g_Now.Delayed < 0 ? std::string("?") : std::to_string(g_Now.Delayed)) + ")");
	for (const NlCore::EventEndRow& row : NlCore::EventEndTable())
	{
		const Status& status = g_Now.Families[static_cast<size_t>(row.Family)];
		lines.push_back(std::string(NlCore::EventFamilyWord(row.Family)) + ": " + NlCore::EventStatusText(row.Family, status.Read, status.Present, status.Name));
	}
	size_t shown = 0;
	for (const std::string& name : g_Now.Names)
	{
		const NlCore::EventRow* row = NlCore::FindEvent(name);
		const std::string group = row ? row->Group : "?";
		const std::string label = LabelOf(name);
		if (!NlCore::EventRowShown(Group, Find, group, name, label))
			continue;
		shown++;
		lines.push_back(group + "  " + name + "  " + label + "  " + (row ? NlCore::EventTypeWord(row->Type) : "(표에 없음)") + "  cd " + NlCore::CooldownText(CooldownOf(g_Now.Cooldowns, name)));
	}
	lines.push_back("(" + std::to_string(shown) + " of " + std::to_string(g_Now.Names.size()) + ")");
	return lines;
}

std::string NlEvents::End(NlCore::EventFamily Family)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return "busy";
	const NlCore::ScopedFlag busy(g_Busy);
	g_EndFamily = Family;
	return Remember(WorldAct::EventEnd, DoNow(WorldAct::EventEnd));
}
```

- [ ] **Step 3: 원격** — `src/Remote.cpp` 의 `DoWorld` 에서 `if (act == NlCore::WorldAct::EventForce) … else …` 를 이것으로:

```cpp
		if (act == NlCore::WorldAct::EventForce)
			Say("  " + NlEvents::ForceEvent(C.Options.count("name") ? C.Options.at("name") : std::string()));
		else if (act == NlCore::WorldAct::EventList)
		{
			for (const std::string& line : NlEvents::List(C.Options.count("group") ? C.Options.at("group") : std::string(), C.Options.count("find") ? C.Options.at("find") : std::string()))
				Say("  " + line);
		}
		else if (act == NlCore::WorldAct::EventEnd)
		{
			NlCore::EventFamily family = NlCore::EventFamily::None;
			NlCore::ParseEventFamily(C.Options.count("kind") ? C.Options.at("kind") : std::string(), family);		// 줄의 꼴은 ParseRemoteLine 이 이미 봤다
			Say("  " + NlEvents::End(family));
		}
		else
			Say("  " + NlWorld::Do(act));		// 취소·쿨다운은 NlWorld::Do 가 NlEvents::Do 로 넘긴다
```

- [ ] **Step 4: 빌드와 시험**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1` 뒤 `pwsh -NoProfile -File E:\NlToyBox\tools\test-native.ps1`
Expected: `build ok`, 경고 0, `core tests: 120 passed`.

- [ ] **Step 5: 커밋**

```bash
git -C E:\NlToyBox add src/Events.hpp src/Events.cpp src/Remote.cpp
git -C E:\NlToyBox commit -m "feat(events): 스냅샷(쿨다운·예약·진행 중), 화면 이름, 예약 취소, 확인된 가족만의 끝내기, 원격 events·event_end (이벤트 탭, Task 4)"
```

---

### Task 5: 이벤트 패널 — 예약, 표(묶음·찾기·일으키기), 진행 중, 쿨다운

**Files:**
- Modify: `src/Events.cpp`(`NlEvents::Draw`)

**Interfaces:**
- Consumes: Task 4 의 `g_Now`, `g_Captions`, `g_Filter`, `g_GroupPick`, `g_EndFamily`, `Push`, `LabelOf`, `CooldownOf`.

- [ ] **Step 1: `NlEvents::Draw` 를 이것으로 바꾼다**(스펙 §5):

```cpp
void NlEvents::Draw()
{
	std::lock_guard lock(g_Mutex);
	// 예약(강제 이벤트)과 취소. research/28·29.
	ImGui::SeparatorText("예약");
	if (!g_Now.Ready)
		NlUi::Hint("게임 화면에서 이벤트의 상태를 읽습니다.");
	else
	{
		ImGui::Text("예약된 이벤트: %s", g_Now.Forced.empty() ? "없음" : (LabelOf(g_Now.Forced) + "  (" + g_Now.Forced + ")").c_str());
		if (!g_Now.Forced.empty())
		{
			ImGui::SameLine();
			if (ImGui::Button("예약 취소"))
				Push(WorldAct::EventCancel);
		}
		if (g_Now.Delayed > 0)
			ImGui::Text("지연 생성 %d건 (뽑혔고 그 시각에 생긴다)", g_Now.Delayed);
	}
	NlUi::Hint("일으키기는 게임의 감독이 보는 '강제 이벤트' 자리에 그 이벤트의 구조체를 써 둡니다. 감독은 하루 한 번(오후) 이벤트를 뽑을 때 그 자리를 읽어 그것을 고르고 지웁니다"
		"(research/28: 써 둔 u_guest_bard 가 그날 뽑혀 쿨다운에 올랐다). 예약 취소는 게임의 함수 reset_debug_forced_event() 로 그 자리를 비웁니다(research/29 에서 확인). "
		"쿨다운 중인 이벤트도 오는지는 재지 않았습니다. 습격·반란·예언 이벤트도 그대로 옵니다.");

	// 표
	const std::vector<std::string> groups = NlCore::EventGroups();
	ImGui::SeparatorText(("이벤트 (" + std::to_string(g_Now.Names.size()) + ")").c_str());
	if (g_GroupPick < 0 || static_cast<size_t>(g_GroupPick) > groups.size())
		g_GroupPick = 0;
	ImGui::SetNextItemWidth(150);
	if (ImGui::BeginCombo("묶음", g_GroupPick == 0 ? "전체" : groups[static_cast<size_t>(g_GroupPick) - 1].c_str()))
	{
		if (ImGui::Selectable("전체", g_GroupPick == 0))
			g_GroupPick = 0;
		for (size_t i = 0; i < groups.size(); i++)
			if (ImGui::Selectable(groups[i].c_str(), static_cast<int>(i) + 1 == g_GroupPick))
				g_GroupPick = static_cast<int>(i) + 1;
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(160);
	ImGui::InputText("찾기 (이름, 화면 이름)", g_Filter, sizeof(g_Filter));
	const std::string group = g_GroupPick == 0 ? std::string() : groups[static_cast<size_t>(g_GroupPick) - 1];
	if (g_Now.Names.empty())
		NlUi::Hint("게임 화면에서 이벤트의 이름을 읽습니다.");
	else if (ImGui::BeginChild("event_rows", ImVec2(0, 300), ImGuiChildFlags_Borders))
	{
		if (ImGui::BeginTable("events", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("묶음");
			ImGui::TableSetupColumn("이름");
			ImGui::TableSetupColumn("갈래");
			ImGui::TableSetupColumn("쿨다운");
			ImGui::TableSetupColumn("묶음 쿨다운");
			ImGui::TableSetupColumn("");
			ImGui::TableHeadersRow();
			for (const std::string& name : g_Now.Names)
			{
				const NlCore::EventRow* row = NlCore::FindEvent(name);
				const std::string rowGroup = row ? row->Group : "?";
				const std::string label = LabelOf(name);
				if (!NlCore::EventRowShown(group, g_Filter, rowGroup, name, label))
					continue;
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(rowGroup.c_str());
				ImGui::TableNextColumn();
				if (row)
				{
					ImGui::TextUnformatted(label.c_str());
					if (label != name)
					{
						ImGui::SameLine();
						ImGui::TextDisabled("%s", name.c_str());
					}
				}
				else
				{
					ImGui::TextUnformatted(name.c_str());
					ImGui::SameLine();
					ImGui::TextDisabled("(표에 없음)");
				}
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(row ? NlCore::EventTypeWord(row->Type) : "-");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(NlCore::CooldownText(CooldownOf(g_Now.Cooldowns, name)).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(NlCore::CooldownText(CooldownOf(g_Now.GroupCooldowns, rowGroup)).c_str());
				ImGui::TableNextColumn();
				if (ImGui::SmallButton(("일으키기##" + name).c_str()))
				{
					g_PendingEvent = name;
					Push(WorldAct::EventForce);
				}
			}
			ImGui::EndTable();
		}
		ImGui::EndChild();
	}
	else
		ImGui::EndChild();
	if (g_Now.MissingFromGame > 0)
		NlUi::Hint("표의 " + std::to_string(g_Now.MissingFromGame) + "줄이 이 게임에 없습니다 (게임이 갱신됐을 수 있습니다).");
	NlUi::Hint("이름·묶음·갈래는 게임의 director_params.json 과 런타임에서 본 것입니다. 화면 이름은 게임의 localization\\main.csv 에서 읽고, 열쇠가 없는 이벤트의 이름은 모드가 지은 것(추정)입니다. "
		"쿨다운은 게임의 감독이 보는 '남은 날'입니다.");

	// 진행 중과 끝내기(확인된 가족만)
	ImGui::SeparatorText("진행 중");
	for (const NlCore::EventEndRow& end : NlCore::EventEndTable())
	{
		const Status& status = g_Now.Families[static_cast<size_t>(end.Family)];
		ImGui::Text("%s: %s", NlCore::EventFamilyWord(end.Family), g_Now.Ready ? NlCore::EventStatusText(end.Family, status.Read, status.Present, status.Name).c_str() : "-");
		ImGui::SameLine();
		const bool allowed = NlCore::EventEndAllowed(end);
		ImGui::BeginDisabled(!allowed || !status.Present);
		if (ImGui::SmallButton((std::string(allowed ? "끝내기" : "끝내기 (확인 전)") + "##" + NlCore::EventFamilyKey(end.Family)).c_str()))
		{
			g_EndFamily = end.Family;
			Push(WorldAct::EventEnd);
		}
		ImGui::EndDisabled();
	}
	ImGui::Text("반란: %s", NlCore::EventStatusText(NlCore::EventFamily::Rebellion, false, false, std::string()).c_str());
	NlUi::Hint("끝내기는 게임의 관리자 함수(research/29 의 후보)를 부릅니다. 가족마다 진행 중인 세이브에서 인자와 효과를 본 뒤에만 켭니다. 확인 전인 가족은 단추가 꺼져 있습니다.");

	ImGui::SeparatorText("쿨다운");
	if (ImGui::Button("이벤트 쿨다운 지우기"))
		Push(WorldAct::CooldownsClear);
	NlUi::Hint("게임이 이벤트를 고를 때 보는 '남은 날'(이벤트마다, 묶음마다)을 0 으로 씁니다. 써지는 것까지 봤고, 이벤트가 더 일찍 오는지는 확인 전입니다. "
		"쿨다운은 세이브에 들어가는 자료입니다(세이브 파일에 그 열쇠가 있습니다). 쓴 채 저장하면 남습니다.");
	if (!g_Last.empty())
		NlUi::Hint(g_Last.c_str());
}
```

`g_EventPick` 과 옛 콤보는 더 쓰지 않으므로 전역 `g_EventPick` 을 지운다. `NlUi::Hint` 는 `std::string` 도 받는다(`src/Ui.hpp`).

- [ ] **Step 2: 빌드와 시험**

Run: `pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1` 뒤 `pwsh -NoProfile -File E:\NlToyBox\tools\test-native.ps1`
Expected: `build ok`, 경고 0(`ImGui::BeginChild` 의 반환값을 쓰지 않는 갈래에서도 `EndChild` 를 부른다 — Dear ImGui 1.91 의 규칙), `core tests: 120 passed`.

- [ ] **Step 3: 커밋**

```bash
git -C E:\NlToyBox add src/Events.cpp
git -C E:\NlToyBox commit -m "feat(events): 이벤트 패널 — 예약과 취소, 묶음·찾기로 거르는 표와 줄마다 일으키기, 가족 다섯의 진행 중과 끝내기(확인 전은 꺼짐) (이벤트 탭, Task 5)"
```

---

### Task 6: 문서, 버전, 모든 시험

**Files:**
- Modify: `src/ModuleMain.cpp:37`(`k_Version`), `CLAUDE.md`, `research/29-events.md`

- [ ] **Step 1: 버전** — `constexpr const char* k_Version = "0.29.1";` → `"0.30.0"`.

- [ ] **Step 2: CLAUDE.md** — "종교·이벤트 패널(`src/World.cpp`, `core/WorldPlan`. `research/14`)" 항목을 둘로 가른다. 그 항목의 첫 줄을 `- 종교 패널(`src/World.cpp`, `core/WorldPlan`. `research/14`): 주교 부르기(…). 원격 `world bishop`.` 꼴로 두고(이벤트 쿨다운 지우기와 `world cooldowns_clear` 는 아래 항목으로), 그 아래의 "**이벤트 골라 일으키기**" 줄과 "**지금 저장**" 줄은 그대로 두되 이벤트 줄의 앞에 새 항목을 더한다:

```markdown
- **이벤트 탭**(`src/Events.cpp`, `core/EventPlan`. `research/29`. 2026-10-07): 게임의 이벤트 61개의 표(이름·묶음·갈래·화면 이름의 열쇠·모드가 지은 이름·가족)가 `core/EventPlan` 에 있다(값은 옮기지 않는다).
  화면 이름은 모듈이 시작할 때 `main.csv` 에서 표의 열쇠만 읽는다(`guest.`, `raid.`, `prophecy.`, `map.reward_*`, `map.conspiracy`. 열쇠가 없는 것의 이름은 모드가 지은 추정이다).
  패널: 예약된 강제 이벤트와 **취소**(`reset_debug_forced_event()`. 인자 없음. 확인됨), 묶음·찾기로 거르는 표와 줄마다 일으키기, 가족 다섯(습격·예언·음모·손님·소요)의 진행 중 상태(`__current_raid` 들을 읽기만)와 **끝내기**.
  **끝내기는 표(`EventEndTable`)의 `Verified` 가족만 부른다. 처음에는 다섯 모두 확인 전이다**: 그 이벤트가 진행 중인 세이브에서 후보 함수를 `record` 로 보고, 실행의 맨 마지막에 한 번 불러 상태가 바뀌는 것을 본 뒤에만 `Verified` 를 참으로 바꾸고 `research/29` 에 적는다.
  원격 `world events [group=] [find=]`, `world event_cancel`, `world event_end kind=<raid|prophecy|conspiracy|guest|unrest>`. 이벤트의 일은 `NlWorld::Do` 가 `NlEvents::Do` 로 넘긴다(`NlCore::IsEventAct`. 계절과 같다).
```

파일 지도의 줄에 `이벤트는 \`Events.cpp\`` 를 더한다(월드의 괄호 뒤). 원격 명령의 줄 `world cooldowns_clear|bishop|season|…|save` 에 `events`, `event_cancel`, `event_end kind=` 를 더한다.

- [ ] **Step 3: research/29** — "## 남은 것" 앞에 절을 더한다:

```markdown
## 구현(0.30.0. 이벤트 탭)

- `src/Events.cpp` + `core/EventPlan`. 표 61줄(이름·묶음·갈래·열쇠·지은 이름·가족), 예약과 취소, 묶음·찾기의 표, 가족 다섯의 진행 중, 끝내기(확인 전 다섯은 꺼져 있다), 쿨다운 지우기.
- 확인(Task 7 의 켜기): (여기에 적는다 — 창의 화면 `refs/ui/events-*.png`, 원격 `world events` 의 답, 표에 없는 이름의 수, `world event_cancel` 의 답, `world event_end kind=raid` 의 'x' 답)
```

- [ ] **Step 4: 모든 시험**

```bash
pwsh -NoProfile -File E:\NlToyBox\tools\build.ps1          # build ok, 경고 0
pwsh -NoProfile -File E:\NlToyBox\tools\test-native.ps1    # core tests: 120 passed
pwsh -NoProfile -File E:\NlToyBox\tools\tests\safety.tests.ps1      # safety tests: 29 passed (게임이 꺼져 있을 때)
py -3.14 -m unittest discover -s E:\NlToyBox\tools\re\tests         # OK
py -3.14 -m unittest discover -s E:\NlToyBox\tools\overlay\tests    # OK
```

- [ ] **Step 5: 커밋**

```bash
git -C E:\NlToyBox add src/ModuleMain.cpp CLAUDE.md research/29-events.md
git -C E:\NlToyBox commit -m "docs: 이벤트 탭의 규칙(CLAUDE.md)과 구현의 기록(research/29), 0.30.0 (이벤트 탭, Task 6)"
```

---

### Task 7: 게임에서 확인(승인 1번), PR

**Files:**
- Modify: `research/29-events.md`(확인의 줄)

**사용자에게 먼저 알린다**: 게임을 한 번 켠다. 게임 창을 누르지 말 것.

- [ ] **Step 1: 배포와 실행 묶음**

```bash
pwsh -NoProfile -File E:\NlToyBox\tools\deploy.ps1                       # 게임이 꺼져 있을 때
pwsh -NoProfile -File E:\NlToyBox\tools\session.ps1 -Action start
pwsh -NoProfile -File E:\NlToyBox\tools\load-save.ps1 -Name 'Evening_day_4'   # 아덴 4일차 저녁. "게임의 저장: 끔 (no_autosave)" 과 PASS 를 본다(적재 판정)
```

- [ ] **Step 2: 원격과 창** — `tools/ask.ps1` 로 차례로(PowerShell 에서 `& E:\NlToyBox\tools\ask.ps1 -Lines '<줄>','<줄>'`). 기대를 함께 적는다:

| 명령 | 기대 |
|---|---|
| `state` | `in_game 1` |
| `world events` | 첫 줄 "예약: 없음 (지연 0)", 가족 다섯 줄("습격: 없음" …), 그 뒤 61줄(묶음, 이름, 화면 이름, 갈래, cd), 끝 "(61 of 61)". 손님 줄에 한국어 화면 이름(`guest.bard` 의 글) |
| `world events group=GUEST find=bard` | "(1 of 61)" 과 `u_guest_bard` 한 줄 |
| `world event name=u_guest_joker`, 2초 뒤 `world events` | "강제 이벤트로 써 두었습니다", 첫 줄 "예약: u_guest_joker (지연 0)" |
| `window open`, `page events`, 2초, `shot events-forced` | 창에 "예약된 이벤트: 광대 (u_guest_joker)"(열쇠 `guest.joker` 의 글)와 [예약 취소], 표, 진행 중 다섯 줄과 꺼진 단추, 반란 "모름" |
| `world event_cancel`, 2초 뒤 `world events` | "예약을 지웠습니다: u_guest_joker", 첫 줄 "예약: 없음" |
| `world event_cancel` | "예약된 이벤트가 없습니다"(함수를 부르지 않는다: 모듈 로그에 두 번째 `reset_debug_forced_event` 줄이 없다) |
| `world event_end kind=raid` | "습격 끝내기는 확인 전이라 부르지 않습니다 …" |
| `world event_end kind=rebellion` | 오류(`world event_end needs kind=…`) |
| `ui click` 로 표의 "일으키기" 하나(자리는 shot 으로), 2초, `world events` | 예약에 그 이름 |
| `world cooldowns_clear` | 지금과 같은 글("이벤트 쿨다운 N개와 묶음 쿨다운 M개를 0 으로 썼습니다" 꼴) |
| `page world`, `shot events-world` | 월드 패널에 이벤트의 글이 섞이지 않았다 |
| `window close` | — |

끝나면:

```bash
pwsh -NoProfile -File E:\NlToyBox\tools\session.ps1 -Action stop -Name events-tab-run1
```

모듈 로그(`refs\runtime\events-tab-run1.log`)에 `world: N event caption(s) from the game's localization file`(N 은 33 안팎: 손님 11, 습격 7, 예언 6, 보상 4, 음모 1 가운데 파일에 줄이 있는 것) 과 `error` 가 없는지, 게임의 오류 파일 끝에 새 `ERROR` 가 없는지 본다(읽기만).

- [ ] **Step 3: research/29 의 확인 줄을 채우고 커밋**

```bash
git -C E:\NlToyBox add research/29-events.md
git -C E:\NlToyBox commit -m "docs(research): 이벤트 탭의 확인 결과 (Task 7)"
```

- [ ] **Step 4: 푸시와 PR**

```bash
git -C E:\NlToyBox ls-files | Select-String "^(refs|backups|downloads|build)/"    # 비어야 한다
git -C E:\NlToyBox push -u origin feat/events-tab
gh pr create --repo game-mod-project/NlToybox --base develop --title "feat: 이벤트 탭 — 표 61줄과 화면 이름, 강제 발동, 예약 취소, 확인된 가족만의 끝내기 (0.30.0)" --body-file E:\NlToyBox\build\pr-body.md
```

`build\pr-body.md`(추적되지 않는 폴더에 잠깐 쓴다):

```markdown
이벤트 탭(0.30.0). 스펙 `docs/superpowers/specs/2026-10-07-events-tab-design.md`, 조사 `research/29-events.md`.

- `core/EventPlan`: 이벤트 61개의 표(이름·묶음·갈래·화면 이름의 열쇠·모드가 지은 이름·가족), 끝내기의 표(확인 전 다섯), 글, 원격의 낱말(시험 2묶음).
- `src/Events`: World 의 이벤트 부분을 옮기고 스냅샷(쿨다운·예약·진행 중), 화면 이름(main.csv), 예약 취소(reset_debug_forced_event. 확인됨), 끝내기(확인된 가족만), 표의 패널.
- 원격 `world events [group=] [find=]`, `world event_cancel`, `world event_end kind=`.
- 코어 시험 120, 안전 29, 파이썬 16+99, 실행 묶음에서 창과 원격 확인(research/29).

🤖 Generated with [Claude Code](https://claude.com/claude-code)
```

머지는 `gh pr merge <번호> --merge --delete-branch`, 그 뒤 `git checkout develop && git pull`, `git branch -d feat/events-tab`.
