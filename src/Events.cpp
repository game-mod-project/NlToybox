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
