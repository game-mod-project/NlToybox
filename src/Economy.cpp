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
	// 자리와 함수는 research/07 에서 잰 것이다(새 게임, 0.5588.9777.0).
	constexpr const char* k_Gold = "inst:o_game_map_controller.__province.__budget.__budget.__no_reserve__";
	constexpr const char* k_GoldChange = "gml_Script_budget_money_change";		// (변화량). 화면에서 확인했다
	constexpr const char* k_Counts = "inst:o_game_map_controller.__province.__warehouse.__warehouse.__total__";
	// (자원 번호, 변화량) → 적용된 변화량. 건물 창고에서 같은 함수가 (1, 5) -> 5 였다. 영지 창고에서는 아직 부르지 않았다.
	// 묶인 곳이 없으면 CallMethod 가 창고(주소의 부모)에 묶어 부른다.
	constexpr const char* k_Change = "inst:o_game_map_controller.__province.__warehouse.change";
	constexpr const char* k_Captions = "global.__resource_caption";
	constexpr const char* k_Categories = "inst:o_data.__resource_categories_data.__categories";
	constexpr const char* k_CategoryNames = "inst:o_data.__resource_categories_data.__categories_names";	// 게임이 보여 주는 차례
	constexpr const char* k_Capacity = "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type";

	struct Group
	{
		std::string Key, Label;
		double Capacity = -1;				// 갈래의 용량. 못 읽으면 음수
		std::vector<int> Resources;
	};

	struct Snapshot			// 틱이 채우고 Draw 가 읽는다. RValue 를 담지 않는다
	{
		bool Ready = false;
		double Gold = 0;
		std::vector<double> Counts;			// 자원 번호 → 영지 창고의 수
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

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 로그에 쓰는 정수("+1000", "-5").
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

	// 지금 값을 읽는다. 게임 화면이 아니거나 자리가 없으면 거짓.
	bool Refresh()
	{
		double gold = 0;
		std::vector<NlAccess::Row> rows;
		if (!NlAccess::InGame() || !NlAccess::ReadNumber(k_Gold, gold) || !ListRows(NlCore::ParseAskPath(k_Counts), rows) || rows.empty())
		{
			g_Now.Ready = false;
			return false;
		}

		std::vector<double> counts;
		for (const NlAccess::Row& row : rows)
			counts.push_back(row.IsNumber ? row.Number : 0);
		if (!g_Now.Ready || g_Now.Labels.size() != counts.size())
			LoadTables(counts.size());

		g_Now.Gold = gold;
		g_Now.Counts = std::move(counts);
		for (Group& group : g_Now.Groups)
			if (!NlAccess::ReadNumber(std::string(k_Capacity) + "." + group.Key, group.Capacity))
				group.Capacity = -1;
		g_Now.Ready = true;
		return true;
	}

	// 명령 하나를 한다. Refresh 가 참을 돌려준 바로 뒤에 부른다. 돌려주는 글: 한 일.
	std::string Run(const EconomyCommand& Command)
	{
		const std::vector<NlCore::EconomyChange> changes = NlCore::PlanEconomy(Command, g_Now.Gold, g_Now.Counts, g_Now.Stocked);
		if (changes.empty())
			return "바꿀 것이 없습니다";

		const NlCore::AskPath change_method = NlCore::ParseAskPath(k_Change);
		size_t done = 0, short_of = 0;
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
				// change 는 적용된 변화량을 돌려준다(건물 창고에서 (1, 5) -> 5, (1, -10) -> -10). 다르면 다 들어가지 않은 것이다.
				if (ok && NlGame::IsNumber(result) && result.ToDouble() != change.Delta)
				{
					short_of++;
					Log("economy: resource " + std::to_string(change.Resource) + " asked " + Signed(change.Delta) + ", applied "
						+ Signed(result.ToDouble()));
				}
			}
			if (!ok)
				break;			// 하나가 안 되면 나머지도 하지 않는다
			done++;
		}
		Log("economy: done " + std::to_string(done) + "/" + std::to_string(changes.size()) + (why.empty() ? "" : ": " + why));
		return std::to_string(done) + "/" + std::to_string(changes.size()) + " 개를 바꿨습니다"
			+ (short_of ? " (" + std::to_string(short_of) + " 개는 다 들어가지 않았습니다)" : "") + (why.empty() ? "" : " (" + why + ")");
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
		return "게임 화면이 아닙니다";

	const bool one = NlCore::NeedsResource(Command.Act) && Command.Resource >= 0 && static_cast<size_t>(Command.Resource) < g_Now.Counts.size();
	const double gold = g_Now.Gold, count = one ? g_Now.Counts[Command.Resource] : 0;
	g_Now.Last = Run(Command);
	if (!Refresh())
		return g_Now.Last + "; 다시 읽지 못했습니다";

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
			Log("economy: not in game, dropped " + std::to_string(g_Queue.size()) + " command(s)");
			g_Queue.clear();
			g_Now.Last = "게임 화면이 아닙니다";
		}
		return;
	}

	while (!g_Queue.empty())
	{
		const EconomyCommand command = g_Queue.front();
		g_Queue.pop_front();
		g_Now.Last = Run(command);
		if (!Refresh())		// 다음 명령은 바뀐 수를 보고 푼다. 읽지 못하면 낡은 수로 하지 않는다
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
		ImGui::TextDisabled("게임을 시작하면 금화와 자원이 보입니다.");
		if (!g_Now.Last.empty())
			ImGui::TextDisabled("%s", g_Now.Last.c_str());
		return;
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
	ImGui::TextDisabled("게임의 함수로 더합니다. 용량을 넘기면 다 들어가지 않을 수 있습니다.");
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
