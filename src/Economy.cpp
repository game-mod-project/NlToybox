#include "Economy.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"
#include "core/NumberEdit.hpp"
#include "core/Retry.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
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
	// 자리와 함수는 research/07, 08 에서 잰 것이다(0.5588.9777.0. 새 게임과 불러온 세이브 둘 다).
	constexpr const char* k_Gold = "inst:o_game_map_controller.__province.__budget.__budget.__no_reserve__";
	constexpr const char* k_GoldChange = "gml_Script_budget_money_change";		// (변화량). 더하기와 줄이기 모두 화면까지 확인했다
	constexpr const char* k_Counts = "inst:o_game_map_controller.__province.__warehouse.__warehouse.__total__";
	// 예약되지 않은 수(= __total__ - 예약). 게임의 화면이 보여 주는 수가 이것이다(당근: total 120, 화면 112).
	constexpr const char* k_Free = "inst:o_game_map_controller.__province.__warehouse.__warehouse.__no_reserve__";
	// (자원 번호, 변화량). 정적 메서드라 CallMethod 가 창고(주소의 부모)에 묶어 부른다. (28, 5) -> 5, (28, -5) -> -5 로 화면까지 확인했다.
	// 용량을 넘겨도 들어간다(나무 400/300). 신성 반지(0번 rune)도 이 함수로 바뀐다: (0, 5) -> 5 로 화면의 반지가 7 → 12 가 됐다(research/18).
	constexpr const char* k_Change = "inst:o_game_map_controller.__province.__warehouse.change";
	constexpr const char* k_Captions = "global.__resource_caption";
	constexpr const char* k_Categories = "inst:o_data.__resource_categories_data.__categories";
	constexpr const char* k_CategoryNames = "inst:o_data.__resource_categories_data.__categories_names";	// 갈래의 차례(게임이 둔 배열)
	constexpr const char* k_Capacity = "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type";
	constexpr const char* k_FloorCheat = "resource_floor";		// 치트 표의 항목: 최소값 유지를 켜고 끈다
	constexpr const char* k_GoldKey = "gold";					// 최소값의 열쇠 가운데 금화(자원의 열쇠가 아니다)
	constexpr double k_FloorEvery = 1.0;						// 최소값을 보는 간격(초)
	constexpr double k_FloorLogEvery = 30.0;					// 같은 것을 채우는 호출을 로그에 다시 적는 간격(초)

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
		std::vector<double> Counts;			// 자원 번호 → 영지 창고의 수(예약된 몫 포함). 청한 만큼 바뀌었는지 보는 데 쓴다
		std::vector<double> Free;			// 자원 번호 → 예약되지 않은 수. 창에 보이고, 맞추기와 줄이기의 기준이다(게임의 화면과 같은 수)
		std::vector<std::string> Keys;		// 자원 번호 → 열쇠("wood"). 최소값의 열쇠다
		std::vector<std::string> Labels;	// 자원 번호 → 창에 보일 이름
		std::vector<Group> Groups;
		std::vector<int> Stocked;			// 갈래에 든 자원 번호들
		int Ring = -1;						// 신성 반지의 자원 번호(갈래에 들지 않는다). 못 찾으면 음수
		std::string Last;					// 마지막으로 한 일
	};

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlEconomy::LogFn g_Log;
	Snapshot g_Now;
	std::deque<EconomyCommand> g_Queue;	// 창이 쌓고 틱이 한다
	double g_NextRead = 0;
	double g_GoldInput = 0;				// 창의 입력 칸
	bool g_GoldInputSet = false;		// 입력 칸을 지금 금화로 채웠는가(0 으로 맞추는 실수를 막는다)
	double g_RingInput = 0;				// 신성 반지의 입력 칸
	bool g_RingInputSet = false;
	double g_AllFloorInput = 0;			// "모든 자원의 최소값" 입력 칸

	// 최소값(바닥). 열쇠 → 0 보다 큰 정수(core 의 FloorValue 를 거친 것). 창과 원격 명령이 바꾸고 틱이 읽는다.
	std::map<std::string, double> g_Floors;
	std::map<std::string, NlCore::NumberEdit> g_NumberEdits;		// 최소값 칸마다의 치고 있는 수(열쇠 → 편집). 그리는 쪽만 쓴다
	bool g_FloorsChanged = false;		// 상태 파일에 적을 것이 있다
	double g_NextFloor = 0;
	std::map<int, double> g_FloorLogged;	// 자원 번호(-1 금화) → 그것을 채우는 호출을 로그에 마지막으로 적은 때
	std::string g_FloorNote;			// 치트 표의 항목 옆에 보일 글
	NlCore::Retry g_FloorRetry(5, 300);	// 채우지 못했을 때 다시 해 보는 간격(5초부터 두 배씩, 5분까지)

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
		g_Now.Keys.assign(Count, "");
		g_Now.Labels.assign(Count, "");
		for (size_t i = 0; i < Count; i++)
		{
			if (i < rows.size() && rows[i].IsString)
				g_Now.Keys[i] = NlCore::ResourceKey(rows[i].Raw);
			g_Now.Labels[i] = g_Now.Keys[i].empty() ? "#" + std::to_string(i) : NlCore::ResourceLabel(g_Now.Keys[i]);
		}
		g_Now.Ring = NlCore::RingResource(g_Now.Keys);

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
			+ std::to_string(g_Now.Stocked.size()) + " stocked, ring " + std::to_string(g_Now.Ring));
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

	// 변화량들을 게임의 함수로 부른다. 하나가 안 되면 나머지도 하지 않는다. 돌려주는 것: 부른 것들. Why 에 안 된 까닭.
	// Quiet: 최소값 유지가 1초마다 같은 것을 채울 때 로그를 되풀이하지 않는다(k_FloorLogEvery 마다 한 번은 부르기 전에 적는다).
	std::vector<NlCore::EconomyChange> Apply(const std::vector<NlCore::EconomyChange>& Changes, std::string& Why, bool Quiet, double Now)
	{
		const NlCore::AskPath change_method = NlCore::ParseAskPath(k_Change);
		std::vector<NlCore::EconomyChange> done;
		for (const NlCore::EconomyChange& change : Changes)
		{
			RValue result;		// 이 함수 안에서만 든다
			bool ok = false;
			bool log = true;
			if (Quiet)
			{
				double& logged = g_FloorLogged[change.Resource];
				log = logged == 0 || Now >= logged + k_FloorLogEvery;
				if (log)
					logged = Now > 0 ? Now : 1;
			}
			// 부르기 전에 남긴다(틀린 호출이 게임을 끝내면 어디였는지 남는다).
			if (change.Resource < 0)
			{
				if (log)
					Log(std::string("economy call budget_money_change(") + Signed(change.Delta) + ")" + (Quiet ? " [floor]" : ""));
				ok = NlGame::CallScript(k_GoldChange, { RValue(change.Delta) }, result);
				if (!ok)
					Why = "no such script";
			}
			else
			{
				if (log)
					Log("economy call warehouse.change(" + std::to_string(change.Resource) + ", " + Signed(change.Delta) + ")" + (Quiet ? " [floor]" : ""));
				ok = NlAccess::CallMethod(change_method, { RValue(static_cast<double>(change.Resource)), RValue(change.Delta) }, result, Why);
			}
			if (!ok)
				break;
			done.push_back(change);
		}
		return done;
	}

	// 최소값의 열쇠가 가리키는 자원 번호. 금화는 -1, 모르는 열쇠는 -2.
	int FloorResource(const std::string& Key)
	{
		if (Key == k_GoldKey)
			return -1;
		for (size_t i = 0; i < g_Now.Keys.size(); i++)
			if (g_Now.Keys[i] == Key)
				return static_cast<int>(i);
		return -2;
	}

	void SetFloorNote(const std::string& Note)
	{
		if (Note == g_FloorNote)
			return;
		g_FloorNote = Note;
		NlCheats::SetNote(k_FloorCheat, Note);
	}

	// 최소값 하나를 정한다(0 이하면 지운다). 러너를 부르지 않는다. 돌려주는 글: 한 일.
	std::string SetFloor(const std::string& Key, const std::string& Label, double Asked)
	{
		if (!NlCore::GoodFloorKey(Key))
			return "그 자원에는 최소값을 둘 수 없습니다";
		if (!NlCore::GoodFloorAmount(Asked))		// 큰 수를 한도로 당겨 채우지 않는다(잘못 친 수다)
			return "최소값으로 쓸 수 없는 수입니다 (10억까지): " + Label;
		const double value = NlCore::FloorValue(Asked);
		const auto it = g_Floors.find(Key);
		if (value > 0)
		{
			if (it != g_Floors.end() && it->second == value)
				return "최소값 " + Label + " " + Thousands(value) + " (그대로입니다)";
			g_Floors[Key] = value;
		}
		else
		{
			if (it == g_Floors.end())
				return "최소값이 없습니다: " + Label;
			g_Floors.erase(it);
		}
		g_FloorsChanged = true;
		g_NextFloor = 0;		// 다음 틱에 바로 본다
		g_FloorRetry.Succeeded();
		Log("economy floor " + Key + " = " + Fixed(value, 0));
		return value > 0 ? "최소값 " + Label + " " + Thousands(value) : "최소값을 지웠습니다: " + Label;
	}

	// 최소값을 정하는 명령(floor, gold_floor). Refresh 가 참을 돌려준 뒤에 부른다(자원의 열쇠가 있어야 한다).
	std::string FloorCommand(const EconomyCommand& Command)
	{
		std::string text;
		if (Command.Act == EconomyAct::GoldFloor)
			text = SetFloor(k_GoldKey, "금화", Command.Amount);
		else
		{
			const std::vector<int> targets = NlCore::EconomyTargets(Command.Act, g_Now.Stocked, g_Now.Ring);
			if (Command.Resource < 0 || static_cast<size_t>(Command.Resource) >= g_Now.Keys.size()
				|| std::find(targets.begin(), targets.end(), Command.Resource) == targets.end())
				return "그 자원에는 최소값을 둘 수 없습니다";
			text = SetFloor(g_Now.Keys[Command.Resource], g_Now.Labels[Command.Resource], Command.Amount);
		}
		if (!NlCheats::IsOn(k_FloorCheat))
			text += " ('최소값 유지'가 꺼져 있어 채우지 않습니다)";
		return text;
	}

	// 명령 하나를 하고 다시 읽는다. Refresh 가 참을 돌려준 바로 뒤에 부른다. 돌려주는 글: 한 일.
	// 다시 읽지 못하면 g_Now.Ready 가 거짓이 된다(부른 쪽이 본다).
	std::string Execute(const EconomyCommand& Command)
	{
		if (NlCore::IsFloorAct(Command.Act))
			return FloorCommand(Command);

		// 기준은 예약되지 않은 수다: "120 으로 맞추기"는 게임의 화면이 120 을 보이게 한다.
		// 하나씩 하는 것은 신성 반지에도 한다. "모든 자원"은 갈래의 자원에만 한다(core 의 EconomyTargets).
		const std::vector<NlCore::EconomyChange> changes = NlCore::PlanEconomy(Command, g_Now.Gold, g_Now.Free, g_Now.Free,
			NlCore::EconomyTargets(Command.Act, g_Now.Stocked, g_Now.Ring));
		if (changes.empty())
			return "바꿀 것이 없습니다";

		const double gold = g_Now.Gold;
		const std::vector<double> counts = g_Now.Counts;
		std::string why;
		const std::vector<NlCore::EconomyChange> done = Apply(changes, why, false, 0);
		Log("economy: called " + std::to_string(done.size()) + "/" + std::to_string(changes.size()) + (why.empty() ? "" : ": " + why));

		std::string text = std::to_string(done.size()) + "/" + std::to_string(changes.size()) + " 개를 불렀습니다" + (why.empty() ? "" : " (" + why + ")");
		if (!Refresh())
			return text + "; 다시 읽지 못했습니다";

		// 청한 만큼 바뀌었는지는 앞뒤의 수(__total__)로 본다. 함수의 반환값에 기대지 않는다. 예약되지 않은 수는 예약이 함께 바뀔 수 있어 쓰지 않는다.
		const std::vector<NlCore::EconomyShort> shorts = NlCore::EconomyShortfall(done, gold, counts, g_Now.Gold, g_Now.Counts);
		for (const NlCore::EconomyShort& one : shorts)
			Log("economy: " + (one.Resource < 0 ? std::string("gold") : "resource " + std::to_string(one.Resource)) + " asked "
				+ Signed(one.Asked) + ", changed " + Signed(one.Applied));
		if (!shorts.empty())
			text += "; " + std::to_string(shorts.size()) + " 개는 청한 만큼 바뀌지 않았습니다";
		return text;
	}

	// 최소값 유지: 바닥에 못 미치는 것을 모자란 만큼 채운다. Refresh 가 참을 돌려준 바로 뒤에 부른다.
	// 채운 뒤에는 앞뒤의 수를 견준다(단추의 길과 같다). 부르지 못했거나 수가 바뀌지 않았으면 그렇게 적고 간격을 늘려 다시 한다.
	void HoldFloors(double Now)
	{
		const std::vector<int> targets = NlCore::EconomyTargets(EconomyAct::FloorSet, g_Now.Stocked, g_Now.Ring);
		std::vector<NlCore::EconomyFloor> floors;
		for (const auto& [key, min] : g_Floors)
		{
			const int resource = FloorResource(key);
			// 이 게임에 없는 열쇠와 건드리지 않는 자원은 건너뛴다(파일에는 남긴다). 지키는 수에도 세지 않는다.
			if (resource == -1 || (resource >= 0 && std::find(targets.begin(), targets.end(), resource) != targets.end()))
				floors.push_back({ resource, min });
		}
		const std::vector<NlCore::EconomyChange> changes = NlCore::PlanFloors(floors, g_Now.Gold, g_Now.Free, targets);
		int called = 0, shorts = 0;
		std::string why;
		if (!changes.empty())
		{
			const double gold = g_Now.Gold;
			const std::vector<double> counts = g_Now.Counts;
			// 금화와 창고를 따로 부른다: 한쪽의 실패가 다른 쪽을 굶기지 않게.
			std::vector<NlCore::EconomyChange> done;
			for (const bool money : { true, false })
			{
				std::vector<NlCore::EconomyChange> part;
				for (const NlCore::EconomyChange& change : changes)
					if ((change.Resource < 0) == money)
						part.push_back(change);
				std::string part_why;
				const std::vector<NlCore::EconomyChange> part_done = Apply(part, part_why, true, Now);
				if (part_done.size() < part.size())
				{
					const NlCore::EconomyChange& failed = part[part_done.size()];
					Log("economy floor: cannot call " + (failed.Resource < 0 ? std::string("budget_money_change(") : "warehouse.change(" + std::to_string(failed.Resource) + ", ")
						+ Signed(failed.Delta) + "): " + part_why);
					why = part_why;
				}
				done.insert(done.end(), part_done.begin(), part_done.end());
			}
			called = static_cast<int>(done.size());
			if (!Refresh())
			{
				SetFloorNote("최소값: 채운 뒤 다시 읽지 못했습니다");
				g_FloorRetry.Failed(Now);
				return;
			}
			// 청한 만큼 바뀌었는지는 앞뒤의 수(__total__)로 본다. 함수의 반환값에 기대지 않는다.
			const std::vector<NlCore::EconomyShort> missing = NlCore::EconomyShortfall(done, gold, counts, g_Now.Gold, g_Now.Counts);
			for (const NlCore::EconomyShort& one : missing)
				Log("economy floor: " + (one.Resource < 0 ? std::string("gold") : "resource " + std::to_string(one.Resource)) + " asked "
					+ Signed(one.Asked) + ", changed " + Signed(one.Applied));
			shorts = static_cast<int>(missing.size());
		}
		const NlCore::FloorRound round = NlCore::FloorReport(static_cast<int>(floors.size()), static_cast<int>(changes.size()), called, shorts, why);
		if (round.Ok)
			g_FloorRetry.Succeeded();
		else
			g_FloorRetry.Failed(Now);		// 안 되는 호출을 1초마다 되풀이하지 않는다
		SetFloorNote(round.Note);
	}

	// ---- 그리는 쪽 (러너를 부르지 않는다) ----

	constexpr size_t k_MaxQueue = 16;		// 창이 쌓아 둘 명령의 수(한도가 없었다. 2026-10-07 리뷰 R3). 넘치면 받지 않고 결과 줄에 적는다

	void Push(EconomyAct Act, int Resource, double Amount)
	{
		if (g_Queue.size() >= k_MaxQueue)
		{
			g_Now.Last = NlCore::QueueFullText(k_MaxQueue);
			return;
		}
		g_Queue.push_back({ Act, Resource, Amount });
	}

	// 최소값의 입력 칸. 치는 동안의 수는 들고만 있다가(치는 도중의 수로 채우지 않게) 칸을 떠날 때 넣는다: Enter, Tab, 다른 곳을 누름(core 의 StepNumberEdit).
	// 0 을 넣으면 지운다. PushID 된 자리에서 부른다.
	// 0.24.1 까지는 InputDouble 에 EnterReturnsTrue 를 줬는데 Dear ImGui 의 수 입력 칸은 그것을 지원하지 않는다(InputScalar 의 단언).
	// Enter 말고는 수를 넣을 길이 없었고 칸을 떠나면 친 수가 버려졌다(사용자 보고 2026-10-06).
	void DrawFloor(const std::string& Key, const std::string& Label)
	{
		const auto it = g_Floors.find(Key);
		NlCore::NumberEdit& edit = g_NumberEdits[Key];
		double value = edit.Has ? edit.Value : (it == g_Floors.end() ? 0 : it->second);
		ImGui::SetNextItemWidth(80);
		const bool typed = ImGui::InputDouble("##min", &value, 0, 0, "%.0f");
		double apply = 0;
		if (NlCore::StepNumberEdit(edit, typed, value, ImGui::IsItemDeactivatedAfterEdit(), ImGui::IsItemActive(), apply))
			g_Now.Last = SetFloor(Key, Label, apply);
	}
}

void NlEconomy::Init(LogFn Log_, const std::map<std::string, double>& Floors)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	g_Floors.clear();
	for (const auto& [key, value] : Floors)
		if (NlCore::GoodFloorKey(key) && NlCore::FloorValue(value) > 0)
			g_Floors[key] = NlCore::FloorValue(value);
	if (!g_Floors.empty())
		Log("economy floors loaded: " + std::to_string(g_Floors.size()));
}

std::string NlEconomy::Do(const EconomyCommand& Command)
{
	std::lock_guard lock(g_Mutex);
	if (!Refresh())
		return g_Now.Why;

	const bool one = NlCore::NeedsResource(Command.Act) && Command.Resource >= 0 && static_cast<size_t>(Command.Resource) < g_Now.Free.size();
	const double gold = g_Now.Gold, count = one ? g_Now.Free[Command.Resource] : 0;
	g_Now.Last = Execute(Command);
	if (!g_Now.Ready)
		return g_Now.Last;

	std::string line = g_Now.Last + "; gold " + Fixed(gold, 0) + " -> " + Fixed(g_Now.Gold, 0);
	if (one && static_cast<size_t>(Command.Resource) < g_Now.Free.size())
		line += "; resource " + std::to_string(Command.Resource) + " " + Fixed(count, 0) + " -> " + Fixed(g_Now.Free[Command.Resource], 0)
			+ " (total " + Fixed(g_Now.Counts[Command.Resource], 0) + ")";
	return line;
}

void NlEconomy::GameTick(double Now, bool Active)
{
	std::lock_guard lock(g_Mutex);
	// 오브젝트 이벤트마다 불린다: 시각부터 보고, 치트 표는 그 뒤에 읽는다.
	bool floors = Now >= g_NextFloor;
	if (floors)
	{
		g_NextFloor = Now + k_FloorEvery;
		floors = !g_Floors.empty() && NlCheats::IsOn(k_FloorCheat);
		if (!floors)
		{
			SetFloorNote("");
			g_FloorRetry.Succeeded();
		}
		else if (!g_FloorRetry.Due(Now))		// 채우지 못한 뒤다. 간격이 지날 때까지 쉰다(글은 그대로 둔다)
			floors = false;
	}
	if (g_Queue.empty() && !floors && (!Active || Now < g_NextRead))
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
		if (floors)
			SetFloorNote("최소값: " + g_Now.Why);		// 지키고 있지 않다(게임 화면이 아니다, 자리를 읽지 못했다)
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
			return;
		}
	}
	if (floors)
		HoldFloors(Now);
}

bool NlEconomy::TakeChanges(std::map<std::string, double>& Floors)
{
	std::lock_guard lock(g_Mutex);
	if (!g_FloorsChanged)
		return false;
	g_FloorsChanged = false;
	Floors = g_Floors;
	return true;
}

void NlEconomy::Draw()
{
	std::lock_guard lock(g_Mutex);
	if (!g_Now.Ready)
	{
		g_GoldInputSet = g_RingInputSet = false;
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
	const bool ring = g_Now.Ring >= 0 && static_cast<size_t>(g_Now.Ring) < g_Now.Free.size();
	if (ring && !g_RingInputSet)
	{
		g_RingInput = g_Now.Free[g_Now.Ring];
		g_RingInputSet = true;
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
	ImGui::SameLine();
	ImGui::TextUnformatted("최소");
	ImGui::SameLine();
	ImGui::PushID("gold_floor");
	DrawFloor(k_GoldKey, "금화");
	ImGui::PopID();

	if (ring)
	{
		// 신성 반지: 영지 창고의 0번 칸(rune). 게임의 화면에서 금화 옆에 보이는 수다(research/18).
		ImGui::PushID("ring");
		ImGui::Text("신성 반지 %s", Thousands(g_Now.Free[g_Now.Ring]).c_str());
		ImGui::SameLine();
		if (ImGui::Button("+1"))
			Push(EconomyAct::ResourceAdd, g_Now.Ring, 1);
		ImGui::SameLine();
		if (ImGui::Button("+10"))
			Push(EconomyAct::ResourceAdd, g_Now.Ring, 10);
		ImGui::SameLine();
		if (ImGui::Button("+100"))
			Push(EconomyAct::ResourceAdd, g_Now.Ring, 100);
		ImGui::SetNextItemWidth(130);
		ImGui::InputDouble("##ring", &g_RingInput, 0, 0, "%.0f");
		ImGui::SameLine();
		if (ImGui::Button("이 수로 맞추기"))
			Push(EconomyAct::ResourceSet, g_Now.Ring, g_RingInput);
		ImGui::SameLine();
		ImGui::TextUnformatted("최소");
		ImGui::SameLine();
		DrawFloor(g_Now.Keys[g_Now.Ring], g_Now.Labels[g_Now.Ring]);
		ImGui::PopID();
	}

	ImGui::Separator();
	ImGui::TextUnformatted("영지 창고의 자원");
	ImGui::SameLine();
	if (ImGui::Button("모든 자원 +100"))
		Push(EconomyAct::AllAdd, -1, 100);
	ImGui::SameLine();
	if (ImGui::Button("모든 자원 +1,000"))
		Push(EconomyAct::AllAdd, -1, 1000);
	ImGui::TextDisabled("게임의 함수로 바꿉니다. 수는 게임의 화면과 같습니다(예약된 몫은 뺀 수). 용량을 넘겨도 들어갑니다.");

	ImGui::TextUnformatted("모든 자원의 최소값");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80);
	ImGui::InputDouble("##allmin", &g_AllFloorInput, 0, 0, "%.0f");
	ImGui::SameLine();
	if (ImGui::Button("표의 자원 모두에 넣기"))
	{
		int count = 0;
		for (const int resource : g_Now.Stocked)
			if (static_cast<size_t>(resource) < g_Now.Keys.size() && !g_Now.Keys[resource].empty())
			{
				SetFloor(g_Now.Keys[resource], g_Now.Labels[resource], g_AllFloorInput);
				count++;
			}
		g_Now.Last = NlCore::FloorValue(g_AllFloorInput) > 0
			? "자원 " + std::to_string(count) + "개의 최소값을 " + Thousands(NlCore::FloorValue(g_AllFloorInput)) + " 으로 정했습니다"
			: "자원의 최소값을 지웠습니다";
	}
	ImGui::SameLine();
	if (ImGui::Button("최소값 모두 지우기") && !g_Floors.empty())
	{
		g_Floors.clear();
		g_FloorsChanged = true;
		Log("economy floors cleared");
		g_Now.Last = "최소값을 모두 지웠습니다";
	}
	ImGui::PushTextWrapPos(0.0f);		// 긴 글이 창의 오른쪽에서 잘렸다(0.24.2 의 화면). 창의 너비에서 줄을 바꾼다
	ImGui::TextDisabled("'최소' 칸에 수를 적고 Enter 를 누르거나 칸을 떠나면(Tab, 다른 곳을 누름) 들어갑니다. 위의 '최소값 유지'가 켜져 있는 동안 그 수보다 적어질 때마다 그 수까지 채웁니다. 0 은 유지하지 않습니다.");
	ImGui::PopTextWrapPos();
	if (!g_Now.Last.empty())
		ImGui::TextDisabled("%s", g_Now.Last.c_str());

	if (ImGui::BeginTable("resources", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
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
			ImGui::TableNextColumn();
			ImGui::TextDisabled("최소");

			for (const int resource : group.Resources)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("  %s", g_Now.Labels[resource].c_str());
				ImGui::TableNextColumn();
				const double reserved = g_Now.Counts[resource] - g_Now.Free[resource];
				if (reserved > 0)
					ImGui::Text("%s (예약 %s)", Thousands(g_Now.Free[resource]).c_str(), Thousands(reserved).c_str());
				else
					ImGui::TextUnformatted(Thousands(g_Now.Free[resource]).c_str());
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
				ImGui::TableNextColumn();
				if (!g_Now.Keys[resource].empty())
					DrawFloor(g_Now.Keys[resource], g_Now.Labels[resource]);
				ImGui::PopID();
			}
		}
		ImGui::EndTable();
	}
}
