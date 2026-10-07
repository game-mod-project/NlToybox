#include "Events.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/Guard.hpp"
#include "core/Localization.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <deque>
#include <fstream>
#include <iterator>
#include <mutex>
#include <unordered_map>
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
		case WorldAct::EventCancel:
			return CancelNow();
		case WorldAct::EventEnd:
			return EndNow(g_EndFamily);
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
