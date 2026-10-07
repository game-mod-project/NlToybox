#include "World.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Jobs.hpp"
#include "Season.hpp"
#include "Game.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/Guard.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::PathStep;
using NlCore::WorldAct;

namespace
{
	// 이벤트를 고르는 감독(research/14). 쿨다운은 이벤트의 이름 → 남은 날, 묶음의 이름 → 남은 날.
	constexpr const char* k_Director = "inst:o_game_map_controller.__game_director";
	// 주교를 다루는 관리자. is_has_bishop()·debug_force_send_bishop() 은 인자가 없다(기계어). 불러서 주교가 오는 것을 봤다.
	constexpr const char* k_Religion = "inst:o_game_map_controller.__province.__religiosity_manager";

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlWorld::LogFn g_Log;
	std::deque<WorldAct> g_Queue;		// 창이 쌓고 틱이 한다
	std::string g_Last;					// 마지막으로 한 일
	std::string g_UtilLast;				// 유틸(지금 저장)에 마지막으로 한 일
	std::string g_PendingEvent;			// 일으킬 이벤트의 이름(창이 적고 틱이 쓴다)
	std::vector<std::string> g_EventNames;	// 게임의 이벤트 이름들(감독의 자료의 ds_map 열쇠. 게임 화면에서 한 번 읽는다)
	double g_NextNames = 0;				// 이름을 다시 읽어 볼 시각
	int g_EventPick = 0;				// 창의 선택
	constexpr const char* k_EventsData = "inst:o_data.__game_director_events_data";
	bool g_Busy = false;				// 하는 중이다(여기서 부른 게임의 함수가 틱을 다시 부르면 안쪽은 아무것도 하지 않는다)
	bool g_BishopCalled = false;		// 이 게임에서 주교를 이미 불렀다(디버그 함수를 되풀이해 부르지 않는다). 게임 화면이 아니게 되면 푼다

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	bool CallNoArgs(const std::string& Path, RValue& Result, std::string& Why)
	{
		return NlAccess::CallMethod(NlCore::ParseAskPath(Path), {}, Result, Why);
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

	bool HasBishop(bool& Has, std::string& Why)
	{
		RValue answer;
		if (!CallNoArgs(std::string(k_Religion) + ".is_has_bishop", answer, Why) || !NlGame::IsNumber(answer))
			return false;
		Has = answer.ToDouble() != 0;
		return true;
	}

	// 세이브 폴더(%LOCALAPPDATA%\Strategy\saves). 읽기만 한다(새 파일의 이름을 보려고).
	std::filesystem::path SavesDir()
	{
		char* local = nullptr;
		size_t length = 0;
		if (_dupenv_s(&local, &length, "LOCALAPPDATA") != 0 || !local)
			return std::filesystem::path();
		const std::filesystem::path dir = std::filesystem::path(local) / "Strategy" / "saves";
		std::free(local);
		return dir;
	}

	std::vector<std::string> ListSaves()
	{
		std::vector<std::string> names;
		std::error_code ec;
		const std::filesystem::path dir = SavesDir();
		if (dir.empty() || !std::filesystem::is_directory(dir, ec))
			return names;
		for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
			if (entry.is_regular_file(ec) && entry.path().extension() == ".norland")
				names.push_back(entry.path().filename().string());
		return names;
	}

	// 지금 저장: 게임의 자동 저장 함수 save_game(0, 1)(아침의 자동 저장이 그 꼴로 불렀다. 저녁은 (0, 2). research/28).
	// 게임의 저장이 꺼져 있으면(유틸의 no_autosave) 부르지 않는다. 새 파일의 이름은 세이브 폴더를 앞뒤로 보고 적는다.
	std::string SaveNow()
	{
		double disabled = 0;
		if (!NlAccess::ReadNumber("inst:o_debug.is_save_disabled", disabled))
			return NlCore::SaveNowReport('u', std::string());
		if (disabled != 0)
			return NlCore::SaveNowReport('d', std::string());
		const std::vector<std::string> before = ListSaves();
		RValue result;		// 이 함수 안에서만 든다
		Log("world call gml_Script_save_game(0, 1)");		// 부르기 전에 남긴다
		if (!NlGame::CallScript("gml_Script_save_game", { RValue(0.0), RValue(1.0) }, result))
			return NlCore::SaveNowReport('f', "스크립트를 부르지 못했습니다");
		std::string fresh;
		for (const std::string& name : ListSaves())
			if (std::find(before.begin(), before.end(), name) == before.end())
				fresh = name;
		Log("world: save_game called" + (fresh.empty() ? std::string(" (no new file seen yet)") : ", new file " + fresh));
		return NlCore::SaveNowReport(fresh.empty() ? 'n' : 's', fresh);
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

	// 설교의 종류 열 가지(__name, __cost …). research/21. 종교의 게임 변수 일 셋은 src/Jobs 의 엔진에 등록한다(리팩토링 C 에서 Production 에서 옮겼다).
	// 설교의 종류 열 가지(__name, __cost …). research/21.
	constexpr const char* k_Preaches = "inst:o_data.__preach_data.__preach_list";
	bool g_PreachSkippedLogged = false;		// 같은 줄을 주기마다 적지 않는다

	// 종교 행동의 비용: 게임 변수 여섯과 설교 종류마다의 __cost(inst:o_data.__preach_data.__preach_list. 열쇠는 "preach.<설교의 이름>"). research/21.
	bool WalkReligionCosts(const NlJobs::Visit& V, std::string& Why)
	{
		if (!NlJobs::WalkVars(V, NlCore::ReligionCostVars(), Why))
			return false;
		RValue list;
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath(k_Preaches), list, why) || !list.IsArray())
		{
			Why = "the preach list is not available";		// 게임 변수만 쓰고 "됐다"고 하지 않는다
			return false;
		}
		// 장부의 열쇠는 설교의 이름이다. 이름이 비었거나 앞의 것과 같은 설교는 건너뛴다(한 칸을 둘이 함께 쓰면 끌 때 남의 값이 써진다).
		std::vector<std::string> seen;
		size_t skipped = 0;
		NlAccess::ForEachChild(list, Holder::Array, [&](const PathStep&, const RValue& item) {
			const PathStep step{ '.', "__cost", 0 };
			RValue name, cost;
			std::string ignored;
			if (!item.IsStruct() || !NlAccess::Follow(item, { { '.', "__name", 0 } }, name, ignored) || !name.IsString() || name.ToString().empty()
				|| std::find(seen.begin(), seen.end(), name.ToString()) != seen.end() || !NlAccess::Follow(item, { step }, cost, ignored))
			{
				skipped++;
				return true;
			}
			seen.push_back(name.ToString());
			V(item, step, "preach." + name.ToString(), 0, -1, cost);
			return true;
		});
		if (skipped && !g_PreachSkippedLogged)
		{
			g_PreachSkippedLogged = true;
			Log("religion costs: skipped " + std::to_string(skipped) + " preach(es) without a usable name or cost (" + std::to_string(seen.size()) + " used)");
		}
		return true;
	}

	// 설교 전환 계수(게임 변수 하나).
	bool WalkPreachFactor(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::PreachFactorVars(), Why); }

	// 기도와 예배가 신앙심을 되돌리는 양(게임 변수 넷).
	bool WalkPietyRestore(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::PietyRestoreVars(), Why); }

	std::string DoNow(WorldAct Act)
	{
		if (!NlAccess::InGame())
		{
			g_BishopCalled = false;
			return "게임 화면이 아닙니다";
		}
		switch (Act)
		{
		case WorldAct::SaveNow:
			return SaveNow();
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
		case WorldAct::BishopSend:
		{
			bool has = false;
			std::string why;
			const bool read = HasBishop(has, why);
			switch (NlCore::ChooseBishopStep(read, has))
			{
			case NlCore::BishopStep::Unknown:
				return "주교가 있는지 읽지 못했습니다: " + why;
			case NlCore::BishopStep::AlreadyHere:
				return "주교가 이미 있습니다";
			case NlCore::BishopStep::Call:
				break;
			}
			if (g_BishopCalled)		// 불렀는데 아직 없다고 읽힌다. 디버그 함수를 되풀이해 부르지 않는다
				return "주교를 이미 불렀습니다. 아직 보이지 않으면 세이브를 다시 불러온 뒤에 눌러 주세요";
			RValue result;
			Log("world call debug_force_send_bishop()");		// 부르기 전에 남긴다
			if (!CallNoArgs(std::string(k_Religion) + ".debug_force_send_bishop", result, why))
				return "주교를 부르지 못했습니다: " + why;
			g_BishopCalled = true;
			bool came = false;
			const bool reread = HasBishop(came, why);
			Log(std::string("world: bishop ") + (!reread ? "called (cannot read back)" : came ? "is here" : "called (not here yet)"));
			return reread && came ? "주교가 왔습니다" : "주교를 불렀습니다 (아직 왔다고 읽히지 않습니다)";
		}
		case WorldAct::SeasonShow:
		case WorldAct::SeasonDelay:
		case WorldAct::SeasonEnd:
			return NlSeason::Do(Act);		// 계절은 src/Season (리팩토링 C)
		}
		return std::string();
	}

	// 한 일의 글을 그 패널의 자리에 둔다(지금 저장은 유틸 패널에, 나머지는 이벤트·종교 패널에. 계절의 것은 src/Season 이 제 자리에 둔다).
	std::string& LastOf(WorldAct Act)
	{
		return Act == WorldAct::SaveNow ? g_UtilLast : g_Last;
	}

	std::string Remember(WorldAct Act, std::string Text)
	{
		if (NlCore::WorldActChanges(Act))
			LastOf(Act) = Text;
		return Text;
	}

	// 흐린 글. 창의 너비에서 줄을 바꾼다.
	void DrawLast()
	{
		if (!g_Last.empty())
			NlUi::Hint(g_Last.c_str());
	}

	constexpr size_t k_MaxQueue = 4;		// 창이 쌓아 둘 청의 수. 넘치면 받지 않고 결과 줄에 적는다(조용히 버리지 않는다. 2026-10-07 리뷰 R3)

	void Push(WorldAct Act)
	{
		if (g_Queue.size() >= k_MaxQueue)
		{
			LastOf(Act) = NlCore::QueueFullText(k_MaxQueue);
			return;
		}
		g_Queue.push_back(Act);
	}
}

void NlWorld::Init(LogFn Log_)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	NlJobs::Add({ "religion_free", "religion costs", true, &WalkReligionCosts, nullptr, 15 });
	NlJobs::Add({ "piety_restore", "piety restore", false, &WalkPietyRestore, nullptr, 15 });
	NlJobs::Add({ "preach_conversion", "preach conversion", false, &WalkPreachFactor, nullptr, 15 });
}

void NlWorld::GameTick(double Now)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return;
	const bool names = g_EventNames.empty() && Now >= g_NextNames;
	if (g_Queue.empty() && !names)		// 시각부터 본다(이 틱은 오브젝트 이벤트마다 불린다). 계절·광산은 src/Season·src/Mines 의 틱이 한다
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

void NlWorld::DrawEvents()
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
	DrawLast();
}

void NlWorld::DrawWorld()
{
	NlSeason::Draw();		// 계절(src/Season). 월드 영역의 표의 항목 아래에 그린다
}

void NlWorld::DrawReligion()
{
	std::lock_guard lock(g_Mutex);
	if (ImGui::Button("주교 부르기"))
		Push(WorldAct::BishopSend);
	NlUi::Hint("게임의 디버그 함수로 주교를 바로 오게 합니다(교단의 영주 하나가 영지에 나타납니다). 주교가 이미 있으면 부르지 않습니다. 되돌릴 수 없습니다.");
	DrawLast();
}

void NlWorld::DrawUtil()
{
	std::lock_guard lock(g_Mutex);
	if (ImGui::Button("지금 저장 (게임의 자동 저장 함수)"))
		Push(WorldAct::SaveNow);
	NlUi::Hint("게임의 자동 저장 함수를 아침의 꼴(save_game(0, 1))로 부릅니다: 세이브 폴더에 '…_Autosave_Morning_…' 파일이 하나 생깁니다. "
		"위의 '게임의 저장 끄기'가 켜져 있으면 부르지 않습니다.");
	if (!g_UtilLast.empty())
		NlUi::Hint(g_UtilLast.c_str());
}

std::string NlWorld::Do(NlCore::WorldAct Act)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return "busy";
	const NlCore::ScopedFlag busy(g_Busy);
	return Remember(Act, DoNow(Act));
}

std::string NlWorld::ForceEvent(const std::string& Name)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return "busy";
	const NlCore::ScopedFlag busy(g_Busy);
	g_PendingEvent = Name;
	return Remember(WorldAct::EventForce, DoNow(WorldAct::EventForce));
}
