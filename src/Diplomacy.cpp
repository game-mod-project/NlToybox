#include "Diplomacy.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"

#include <imgui.h>

#include <algorithm>
#include <deque>
#include <mutex>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::DiplomacyCommand;
using NlCore::DiplomacyGoal;

namespace
{
	// 자리와 함수는 research/19 에서 잰 것이다(0.5588.9777.0).
	constexpr const char* k_Factions = "inst:o_game_map_controller.__factions_manager.__array_of_factions";
	constexpr const char* k_Player = "inst:o_game_map_controller.__factions_manager.__player_faction";
	// __matrix.<A 의 uuid>.<B 의 uuid> = A 가 B 를 보는 관계의 종류(0 allies … 7 opponent). 방향이 있다.
	constexpr const char* k_Matrix = "inst:o_game_map_controller.__factions_manager.__allies_matrix.__matrix";
	// 게임의 디버그용 평판. __opinion_modify 가 +5 / -5, 끝이 없고(__duration -4), 50겹까지다.
	constexpr const char* k_Good = "inst:o_data.opinion_mind_debug_positive";
	constexpr const char* k_Bad = "inst:o_data.opinion_mind_debug_negative";
	constexpr double k_StepEvery = 0.02;		// 쌓인 일을 한 묶음씩 하는 간격(초)
	constexpr int k_StepsPerTick = 8;			// 한 묶음에 붙이는 평판의 수(한 틱이 길어지지 않게)
	constexpr size_t k_ResultLines = 60;

	struct Kingdom
	{
		std::string Uuid, Name;
		int Index = -1;				// __array_of_factions 의 자리. 쓰기 전에 그 자리의 uuid 를 다시 본다
		double Theirs = -1;			// 그쪽이 우리를 보는 관계. 읽지 못하면 -1
		double Ours = -1;			// 우리가 그쪽을 보는 관계
	};

	struct Job
	{
		std::string Uuid;
		char Side = 't';			// 't' 그쪽이 우리를 보는 평판, 'u' 우리가 그쪽을 보는 평판
		DiplomacyGoal Goal = DiplomacyGoal::Neutral;
		int Left = 0;				// 더 붙여도 되는 평판의 수
		int Sign = 0;				// Opinion: 방향(+1, -1)
		int Done = 0;				// 붙인 평판의 수(부호가 방향)
		double Before = -1;			// 시작할 때의 관계
		bool Started = false;
	};

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlDiplomacy::LogFn g_Log;
	bool g_Ready = false;
	std::string g_Why;					// g_Ready 가 아닌 까닭
	std::string g_PlayerUuid;
	std::vector<Kingdom> g_Kingdoms;	// 틱이 채우고 Draw 가 읽는다. RValue 를 담지 않는다
	std::deque<Job> g_Jobs;				// 창이 쌓고 틱이 한다
	std::vector<std::string> g_Results;	// 마지막 명령의 결과(줄마다)
	int g_Asked = 0, g_Failed = 0;		// 마지막 명령의 일의 수와 실패한 수
	double g_NextRead = 0, g_NextStep = 0;
	bool g_Busy = false;				// 하는 중이다(여기서 부른 게임의 함수가 틱을 다시 부르면 안쪽은 아무것도 하지 않는다)
	int g_SideChoice = 0;				// 창의 선택: 0 양쪽, 1 그쪽이 우리를, 2 우리가 그쪽을

	struct Busy
	{
		Busy() { g_Busy = true; }
		~Busy() { g_Busy = false; }
	};

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// ---- 게임 스레드 ----

	bool ReadText(const std::string& Path, std::string& Out)
	{
		RValue value;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath(Path), value, why) || !value.IsString())
			return false;
		Out = value.ToString();
		return true;
	}

	std::string FactionPath(int Index)
	{
		return std::string(k_Factions) + "[" + std::to_string(Index) + "]";
	}

	// A 가 B 를 보는 관계의 종류. 읽지 못하면 -1.
	double Relation(const std::string& From, const std::string& To)
	{
		double kind = -1;
		return NlAccess::ReadNumber(std::string(k_Matrix) + "." + From + "." + To, kind) ? kind : -1;
	}

	void ReadKinds()
	{
		for (Kingdom& kingdom : g_Kingdoms)
		{
			kingdom.Theirs = Relation(kingdom.Uuid, g_PlayerUuid);
			kingdom.Ours = Relation(g_PlayerUuid, kingdom.Uuid);
		}
	}

	// 왕국들을 다시 모은다(이름은 앞서 읽은 것을 쓴다). 못 하면 거짓이고 g_Why 에 까닭.
	bool Scan()
	{
		g_Ready = false;
		if (!NlAccess::InGame())
		{
			g_Why = "게임 화면이 아닙니다";
			g_Kingdoms.clear();
			g_Jobs.clear();
			return false;
		}
		std::string player;
		RValue list;		// 이 함수 안에서만 든다
		Holder kind = Holder::None;
		std::string why;
		if (!ReadText(std::string(k_Player) + ".__uuid", player) || !NlCore::GoodFactionWho(player) || player == "all")
		{
			g_Why = "플레이어의 세력을 읽지 못했습니다";
			return false;
		}
		if (!NlAccess::Open(NlCore::ParseAskPath(k_Factions), list, kind, why) || kind != Holder::Array)
		{
			g_Why = "세력의 목록을 읽지 못했습니다";
			return false;
		}

		std::vector<Kingdom> found;
		int index = 0;
		NlAccess::ForEachChild(list, Holder::Array, [&](const NlCore::PathStep&, const RValue& faction) {
			const int at = index++;
			RValue name, uuid;
			std::string unused;
			if (!faction.IsStruct() || !NlAccess::Follow(faction, { { '.', "__system_name", 0 } }, name, unused) || !name.IsString()
				|| !NlCore::IsKingdom(name.ToString()))
				return true;
			if (!NlAccess::Follow(faction, { { '.', "__uuid", 0 } }, uuid, unused) || !uuid.IsString())
				return true;
			Kingdom kingdom;
			kingdom.Uuid = uuid.ToString();
			kingdom.Index = at;
			kingdom.Name = name.ToString();
			if (kingdom.Uuid != player && NlCore::GoodFactionWho(kingdom.Uuid) && kingdom.Uuid != "all")
				found.push_back(std::move(kingdom));
			return true;
		});

		// 이름: Faction.get_caption()(인자 없음. 불러서 현지화된 이름이 오는 것을 봤다). 왕국마다 한 번만 부른다.
		bool logged = false;
		for (Kingdom& kingdom : found)
		{
			const auto old = std::find_if(g_Kingdoms.begin(), g_Kingdoms.end(), [&](const Kingdom& k) { return k.Uuid == kingdom.Uuid; });
			if (g_PlayerUuid == player && old != g_Kingdoms.end())
			{
				kingdom.Name = old->Name;
				continue;
			}
			RValue caption;
			if (!logged)
			{
				Log("diplomacy call get_caption() on the kingdoms");		// 부르기 전에 남긴다(왕국마다 같은 호출이다)
				logged = true;
			}
			if (NlAccess::CallMethod(NlCore::ParseAskPath(FactionPath(kingdom.Index) + ".get_caption"), {}, caption, why) && caption.IsString()
				&& !caption.ToString().empty())
				kingdom.Name = caption.ToString();
		}
		if (g_PlayerUuid != player || found.size() != g_Kingdoms.size())
			Log("diplomacy: " + std::to_string(found.size()) + " kingdoms, player " + player);
		g_PlayerUuid = player;
		g_Kingdoms = std::move(found);
		ReadKinds();
		g_Ready = true;
		g_Why.clear();
		return true;
	}

	const Kingdom* FindKingdom(const std::string& Uuid)
	{
		for (const Kingdom& kingdom : g_Kingdoms)
			if (kingdom.Uuid == Uuid)
				return &kingdom;
		return nullptr;
	}

	// 일의 한 걸음. 돌려주는 값: 0 이면 더 한다, 아니면 끝났다(core 의 DiplomacyReport 의 Outcome). Why 에 실패의 까닭.
	char StepJob(Job& It, std::string& Why)
	{
		const Kingdom* kingdom = FindKingdom(It.Uuid);
		std::string uuid;
		if (!kingdom || !ReadText(FactionPath(kingdom->Index) + ".__uuid", uuid) || uuid != It.Uuid)
		{
			Why = "그 왕국을 찾지 못했습니다";		// 자리가 밀렸다. 다음 읽기가 다시 모은다
			return 'f';
		}
		const std::string faction = FactionPath(kingdom->Index);
		const double kind = It.Side == 't' ? Relation(It.Uuid, g_PlayerUuid) : Relation(g_PlayerUuid, It.Uuid);
		if (kind < 0)
		{
			Why = "관계를 읽지 못했습니다";
			return 'f';
		}
		if (!It.Started)
		{
			It.Before = kind;
			It.Started = true;
		}

		int direction = It.Sign;
		if (It.Goal != DiplomacyGoal::Opinion)
		{
			direction = NlCore::StepToward(kind, It.Goal);
			if (direction == 0)
				return It.Done == 0 ? 'a' : 'd';
			if (direction == 2)
				return 'k';
		}
		if (It.Left <= 0)
			return It.Goal == DiplomacyGoal::Opinion ? 'd' : 'l';

		// 붙일 평판의 자료가 잰 것과 같은지 본다(±5). 아니면 부르지 않는다.
		const std::string generic_path = direction > 0 ? k_Good : k_Bad;
		double modify = 0;
		RValue about, generic, attached, updated;		// 이 함수 안에서만 든다
		if (!NlAccess::ReadNumber(generic_path + ".__opinion_modify", modify) || modify != direction * NlCore::k_OpinionUnit
			|| !NlAccess::Read(NlCore::ParseAskPath(generic_path), generic, Why) || !generic.IsStruct())
		{
			Why = "게임의 디버그 평판 자료가 잰 것과 다릅니다";
			return 'f';
		}
		// 평판을 갖는 쪽(holder)의 왕이 대상(about)의 왕에게 갖는 평판에 붙인다.
		const std::string holder = It.Side == 't' ? faction : k_Player;
		const std::string about_path = It.Side == 't' ? k_Player : faction;
		if (!NlAccess::Read(NlCore::ParseAskPath(about_path), about, Why) || !about.IsStruct())
		{
			Why = "세력의 구조체를 읽지 못했습니다";
			return 'f';
		}
		// Faction.attach_opinion_about_faction(대상 세력, 평판의 자료) -> 구조체. 게임이 (구조체, 구조체)로 부르는 것을 기록했고,
		// 그 꼴로 불러 평판이 -7 → -2, 관계가 opponent → neutrals 가 되는 것을 봤다(research/19). 부르기 전에 남긴다.
		Log("diplomacy call " + holder + ".attach_opinion_about_faction(" + about_path + ", " + (direction > 0 ? "debug_positive" : "debug_negative") + ")");
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(holder + ".attach_opinion_about_faction"), { about, generic }, attached, Why))
			return 'f';
		if (!attached.IsStruct())
		{
			Why = "평판이 붙지 않았습니다 (겹침의 한도일 수 있습니다)";
			return 'f';
		}
		It.Done += direction;
		It.Left--;
		// Faction.__update_relations(세력) -> undefined. 게임이 그 꼴로 955번 불렀다. 관계의 종류를 평판에서 다시 셈해 행렬에 쓴다.
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(holder + ".__update_relations"), { about }, updated, Why))
			return 'f';
		return 0;
	}

	// 일 하나를 Budget 걸음까지 한다. 끝났으면 결과를 적고 참을 돌려준다.
	bool RunJob(Job& It, int Budget)
	{
		std::string why;
		char outcome = 0;
		for (int i = 0; i < Budget && outcome == 0; i++)
			outcome = StepJob(It, why);
		if (outcome == 0)
			return false;

		const Kingdom* kingdom = FindKingdom(It.Uuid);
		const double after = It.Side == 't' ? Relation(It.Uuid, g_PlayerUuid) : Relation(g_PlayerUuid, It.Uuid);
		const std::string line = NlCore::DiplomacyReport(kingdom ? kingdom->Name : It.Uuid, It.Side, It.Started ? It.Before : after, after, It.Done, outcome, why);
		Log("diplomacy: " + It.Uuid + " side " + std::string(1, It.Side) + " goal " + NlCore::DiplomacyGoalWord(It.Goal) + ": outcome "
			+ std::string(1, outcome) + ", steps " + std::to_string(It.Done) + (why.empty() ? "" : ", " + why));
		if (NlCore::DiplomacyFailed(outcome))
			g_Failed++;
		if (g_Results.size() < k_ResultLines)
			g_Results.push_back(line);
		return true;
	}

	// 명령을 일들로 푼다. Scan 이 참을 돌려준 뒤에 부른다. 돌려주는 값: 쌓은 일의 수.
	int Enqueue(const DiplomacyCommand& Command, std::deque<Job>& Out)
	{
		const int steps = Command.Goal == DiplomacyGoal::Opinion ? NlCore::OpinionSteps(Command.Amount) : 0;
		int count = 0;
		for (const Kingdom& kingdom : g_Kingdoms)
		{
			if (Command.Who != "all" && Command.Who != kingdom.Uuid)
				continue;
			for (const char side : { 't', 'u' })
			{
				if (Command.Side != 'b' && Command.Side != side)
					continue;
				Job job;
				job.Uuid = kingdom.Uuid;
				job.Side = side;
				job.Goal = Command.Goal;
				job.Left = Command.Goal == DiplomacyGoal::Opinion ? (steps > 0 ? steps : -steps) : NlCore::k_OpinionStepsMax;
				job.Sign = steps > 0 ? 1 : steps < 0 ? -1 : 0;
				Out.push_back(std::move(job));
				count++;
			}
		}
		return count;
	}

	std::string Summary()
	{
		return std::to_string(g_Asked - g_Failed) + "/" + std::to_string(g_Asked) + " 개가 됐습니다" + (g_Failed > 0 ? " (안 된 것은 아래에 적혀 있습니다)" : "");
	}

	// ---- 그리는 쪽 (러너를 부르지 않는다) ----

	// 흐린 글. 창의 너비에서 줄을 바꾼다.
	void Hint(const char* Text)
	{
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextDisabled("%s", Text);
		ImGui::PopTextWrapPos();
	}

	char SideChoice()
	{
		return g_SideChoice == 1 ? 't' : g_SideChoice == 2 ? 'u' : 'b';
	}

	// 창의 단추: 명령을 일로 풀어 쌓는다(g_Kingdoms 는 틱이 채운 사본이다. 러너를 부르지 않는다).
	void Push(const std::string& Who, DiplomacyGoal Goal, double Amount)
	{
		const DiplomacyCommand command{ Who, Goal, SideChoice(), Amount };
		std::string why;
		if (!NlCore::CheckDiplomacy(command, why))
		{
			g_Results = { why };
			return;
		}
		if (g_Jobs.empty())
		{
			g_Results.clear();
			g_Asked = g_Failed = 0;
		}
		g_Asked += Enqueue(command, g_Jobs);
	}
}

void NlDiplomacy::Init(LogFn Log_)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
}

void NlDiplomacy::GameTick(double Now, bool Active)
{
	std::lock_guard lock(g_Mutex);
	// 오브젝트 이벤트마다 불린다: 시각부터 본다.
	const bool work = !g_Jobs.empty() && Now >= g_NextStep;
	const bool read = Active && Now >= g_NextRead;
	if (g_Busy || (!work && !read))
		return;
	const Busy busy;
	if (read || !g_Ready)
	{
		g_NextRead = Now + 1;
		if (!Scan())
		{
			if (!g_Jobs.empty())
			{
				Log("diplomacy: cannot read, dropped " + std::to_string(g_Jobs.size()) + " job(s)");
				g_Jobs.clear();
				g_Results.push_back(g_Why);
			}
			return;
		}
	}
	if (work)
	{
		g_NextStep = Now + k_StepEvery;
		if (RunJob(g_Jobs.front(), k_StepsPerTick))
		{
			g_Jobs.pop_front();
			ReadKinds();
		}
	}
}

std::vector<std::string> NlDiplomacy::Do(const DiplomacyCommand& Command)
{
	std::lock_guard lock(g_Mutex);
	std::string why;
	if (!NlCore::CheckDiplomacy(Command, why))
		return { why };
	if (g_Busy)
		return { "busy" };
	const Busy busy;
	if (!Scan())
		return { g_Why };

	std::deque<Job> jobs;
	g_Results.clear();
	g_Failed = 0;
	g_Asked = Enqueue(Command, jobs);
	if (g_Asked == 0)
		return { "그 왕국이 없습니다" };
	for (Job& job : jobs)
		RunJob(job, NlCore::k_OpinionStepsMax + 2);		// 끝까지: 한도만큼 붙이고 한 걸음 더 보면 끝난다
	ReadKinds();
	std::vector<std::string> lines = g_Results;
	lines.push_back(Summary());
	return lines;
}

std::vector<std::string> NlDiplomacy::List()
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const Busy busy;
	if (!Scan())
		return { g_Why };
	std::vector<std::string> lines;
	for (const Kingdom& kingdom : g_Kingdoms)
		lines.push_back(kingdom.Uuid + "  " + kingdom.Name + "  them " + NlCore::RelationLabel(kingdom.Theirs) + " (" + std::to_string(static_cast<int>(kingdom.Theirs))
			+ ")  us " + NlCore::RelationLabel(kingdom.Ours) + " (" + std::to_string(static_cast<int>(kingdom.Ours)) + ")");
	lines.push_back("(" + std::to_string(g_Kingdoms.size()) + " kingdoms, player " + g_PlayerUuid + ")");
	return lines;
}

void NlDiplomacy::Draw()
{
	std::lock_guard lock(g_Mutex);
	if (!g_Ready)
	{
		ImGui::TextDisabled("%s", g_Why.empty() ? "게임을 시작하면 왕국들이 보입니다." : g_Why.c_str());
		return;
	}

	ImGui::TextUnformatted("왕국과의 관계");
	ImGui::SameLine();
	ImGui::RadioButton("양쪽", &g_SideChoice, 0);
	ImGui::SameLine();
	ImGui::RadioButton("그쪽이 우리를 보는 평판만", &g_SideChoice, 1);
	ImGui::SameLine();
	ImGui::RadioButton("우리가 그쪽을 보는 평판만", &g_SideChoice, 2);
	if (ImGui::Button("모든 왕국과 우호로"))
		Push("all", DiplomacyGoal::Friends, 0);
	ImGui::SameLine();
	if (ImGui::Button("모든 왕국과 중립으로"))
		Push("all", DiplomacyGoal::Neutral, 0);
	Hint("왕국 사이의 관계는 게임이 왕끼리의 평판에서 셈합니다. 여기서는 게임의 디버그용 평판(+5 또는 -5)을 게임의 함수로 붙이고 관계를 다시 셈하게 합니다. "
		"'우호'·'중립'·'적대'는 그 관계가 될 때까지 붙입니다(한쪽에 40개까지. 적대는 철천지원수까지 내립니다). 동맹·봉신·주군 관계는 건드리지 않습니다. "
		"붙인 평판을 떼는 단추는 없습니다(반대쪽을 붙여 상쇄합니다). 얼마나 오래 남는지, 세이브에 남는지는 확인 전입니다.");
	if (!g_Jobs.empty())
		ImGui::TextDisabled("하는 중: 남은 일 %d개", static_cast<int>(g_Jobs.size()));
	else if (g_Asked > 0)
		ImGui::TextDisabled("%s", Summary().c_str());
	for (size_t i = 0; i < g_Results.size() && i < 6; i++)
		Hint(g_Results[i].c_str());
	if (g_Results.size() > 6)
		ImGui::TextDisabled("(그 밖에 %d줄)", static_cast<int>(g_Results.size() - 6));

	if (ImGui::BeginTable("kingdoms", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("왕국");
		ImGui::TableSetupColumn("그쪽이 우리를");
		ImGui::TableSetupColumn("우리가 그쪽을");
		ImGui::TableSetupColumn("바꾸기");
		ImGui::TableHeadersRow();
		for (size_t i = 0; i < g_Kingdoms.size(); i++)
		{
			const Kingdom& kingdom = g_Kingdoms[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(kingdom.Name.c_str());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(NlCore::RelationLabel(kingdom.Theirs));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(NlCore::RelationLabel(kingdom.Ours));
			ImGui::TableNextColumn();
			ImGui::PushID(static_cast<int>(i));
			for (const DiplomacyGoal goal : { DiplomacyGoal::Friends, DiplomacyGoal::Neutral, DiplomacyGoal::Hostile })
			{
				if (ImGui::SmallButton(NlCore::DiplomacyGoalLabel(goal)))
					Push(kingdom.Uuid, goal, 0);
				ImGui::SameLine();
			}
			if (ImGui::SmallButton("+5"))
				Push(kingdom.Uuid, DiplomacyGoal::Opinion, 5);
			ImGui::SameLine();
			if (ImGui::SmallButton("-5"))
				Push(kingdom.Uuid, DiplomacyGoal::Opinion, -5);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
}
