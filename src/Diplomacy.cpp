#include "Diplomacy.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/JobTally.hpp"
#include "core/Guard.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <deque>
#include <iterator>
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
	// 협정의 칸: __matrix.<A 의 uuid>.<B 의 uuid> = 비트(평화 4, 교역 협정 8, 방어 동맹 192). 협정이 없으면 칸도 없다. set_agreement 는 양쪽에 쓴다.
	constexpr const char* k_Pacts = "inst:o_game_map_controller.__factions_manager.__agreement_matrix";
	// 게임의 디버그용 평판. __opinion_modify 가 +5 / -5, __duration 이 -4, 50겹까지다.
	constexpr const char* k_Good = "inst:o_data.opinion_mind_debug_positive";
	constexpr const char* k_Bad = "inst:o_data.opinion_mind_debug_negative";
	constexpr double k_StepEvery = 0.02;		// 쌓인 일을 한 묶음씩 하는 간격(초)
	constexpr int k_StepsPerTick = 8;			// 한 묶음에 붙이는 평판의 수(한 틱이 길어지지 않게)
	constexpr size_t k_ShownLines = 8;			// 창에 보이는 결과 줄의 수(실패한 줄이 앞에 온다)

	struct Kingdom
	{
		std::string Uuid, Name;
		int Index = -1;				// __array_of_factions 의 자리. 쓰기 전에 그 자리의 uuid 를 다시 본다
		double Theirs = -1;			// 그쪽이 우리를 보는 관계. 읽지 못하면 -1
		double Ours = -1;			// 우리가 그쪽을 보는 관계
		double Pacts = -1;			// 우리와 맺은 협정의 비트. 칸이 없으면(협정이 없다) -1
	};

	struct Job
	{
		NlCore::DiplomacyJob Plan;		// 누구의 어느 쪽, 붙여도 되는 수, 방향(core 의 PlanJobs 가 정한다)
		DiplomacyGoal Goal = DiplomacyGoal::Neutral;
		NlCore::DiplomacyPact Pact = NlCore::DiplomacyPact::Peace;		// Goal 이 Pact 일 때
		int Done = 0;				// 붙인 평판의 수(부호가 방향). 떼기(Clear)에서는 뗀 수
		int Good = 0, Bad = 0;		// 떼기: 뗀 좋은·나쁜 평판의 수
		int Phase = 0;				// 떼기: 0 좋은 자료를 떼는 중, 1 나쁜 자료를 떼는 중(좋은 것이 더 떨어지지 않으면 넘어간다)
		double Before = -1;			// 시작할 때의 관계
		bool Started = false;
		bool Checked = false;		// 그 왕국이 살아 있고 양쪽에 왕이 있는지 봤다
		// 붙었는지는 평판을 갖는 왕의 평판 목록(__opinion_minds)의 원소 수를 붙이기 바로 앞뒤로 세어 본다(core 의 AttachCheck·AfterAttach).
		bool Seen = false;			// 이 일에서 그 수가 느는 것을 본 적이 있다(세는 길이 이 왕에게 듣는다)
		bool Unsure = false;		// 확인하지 못하고 센 걸음이 있다(결과 줄에 그렇게 적는다)
		bool CountLogged = false;	// 세는 길이 듣는지(어느 자리인지, 왜 못 세는지)를 로그에 한 번 남겼다
		double MindsFirst = -1;		// 처음에 센 수와 마지막에 센 수(로그에 남긴다). 세지 못했으면 -1
		double MindsLast = -1;
	};

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlDiplomacy::LogFn g_Log;
	bool g_Ready = false;
	std::string g_Why;					// g_Ready 가 아닌 까닭
	std::string g_PlayerUuid;
	std::vector<Kingdom> g_Kingdoms;	// 틱이 채우고 Draw 가 읽는다. RValue 를 담지 않는다
	std::deque<Job> g_Jobs;				// 창이 쌓고 틱이 한다
	NlCore::JobTally g_Tally;		// 쌓인 일들의 결과(창이 보인다)
	std::string g_Refused;				// 창의 단추가 거부된 까닭
	double g_NextRead = 0, g_NextStep = 0;
	bool g_Busy = false;				// 하는 중이다(여기서 부른 게임의 함수가 틱을 다시 부르면 안쪽은 아무것도 하지 않는다)
	int g_SideChoice = 0;				// 창의 선택: 0 양쪽, 1 그쪽이 우리를, 2 우리가 그쪽을

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// ---- 게임 스레드 ----

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

	// 우리와 그 세력 사이의 협정의 비트. 칸이 없으면 -1.
	double PactCellOf(const std::string& Uuid)
	{
		double cell = -1;
		return NlAccess::ReadNumber(std::string(k_Pacts) + ".__matrix." + g_PlayerUuid + "." + Uuid, cell) ? cell : -1;
	}

	void ReadKinds()
	{
		for (Kingdom& kingdom : g_Kingdoms)
		{
			kingdom.Theirs = Relation(kingdom.Uuid, g_PlayerUuid);
			kingdom.Ours = Relation(g_PlayerUuid, kingdom.Uuid);
			kingdom.Pacts = PactCellOf(kingdom.Uuid);
		}
	}

	// 쌓인 일을 버린다(하지 못했다). 버린 수를 실패로 세고 까닭을 줄로 남긴다.
	void DropJobs(const std::string& Why)
	{
		if (g_Jobs.empty())
			return;
		Log("diplomacy: dropped " + std::to_string(g_Jobs.size()) + " job(s): " + Why);
		g_Tally.Drop(static_cast<int>(g_Jobs.size()), Why);
		g_Jobs.clear();
	}

	// 왕국들을 다시 모은다(이름은 앞서 읽은 것을 쓴다). 못 하면 거짓이고 g_Why 에 까닭. 쌓인 일은 건드리지 않는다(부른 쪽이 버린다).
	bool Scan()
	{
		g_Ready = false;
		if (!NlAccess::InGame())
		{
			g_Why = "게임 화면이 아닙니다";
			g_Kingdoms.clear();
			return false;
		}
		std::string player;
		RValue list;		// 이 함수 안에서만 든다
		Holder kind = Holder::None;
		std::string why;
		if (!NlAccess::ReadText(std::string(k_Player) + ".__uuid", player) || !NlCore::GoodFactionWho(player) || player == "all")
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

	// 왕의 구조체 안에서 평판의 목록(__opinion_minds)이 있을 자리들. 우리 왕의 것은 …__soul.__character_soul.__opinions.__opinion_minds 에서 봤다
	// (붙일 때마다 원소가 하나 늘었다: 33 → 73 → 83. research/19). get_king_character_soul() 이 돌려주는 구조체가 그 가운데 어느 층인지는
	// 재지 않았으므로 세 자리를 차례로 본다(__soul 바로 아래에는 __opinions 가 없어 얕은 자리가 잘못 맞지 않는다).
	const std::vector<std::vector<NlCore::PathStep>>& MindPlaces()
	{
		static const std::vector<std::vector<NlCore::PathStep>> places = {
			{ { '.', "__opinions", 0 }, { '.', "__opinion_minds", 0 } },
			{ { '.', "__character_soul", 0 }, { '.', "__opinions", 0 }, { '.', "__opinion_minds", 0 } },
			{ { '.', "__soul", 0 }, { '.', "__character_soul", 0 }, { '.', "__opinions", 0 }, { '.', "__opinion_minds", 0 } },
		};
		return places;
	}

	// 그 자리의 평판의 수(__opinion_minds 의 원소 수). 읽기만 한다. 못 읽으면 -1.
	double MindsAt(const RValue& King, int Place)
	{
		RValue minds;		// 이 함수 안에서만 든다
		std::string why;
		if (Place < 0 || static_cast<size_t>(Place) >= MindPlaces().size() || !NlAccess::Follow(King, MindPlaces()[Place], minds, why) || !minds.IsArray())
			return -1;
		return NlGame::ArrayLength(minds);
	}

	// 그 세력의 왕의 구조체와, 그 안에서 평판의 목록이 있는 자리를 얻는다. Faction.get_king_character_soul() -> 구조체(게임이 그 꼴로 부른다).
	// 못 얻으면 거짓이고 Why 에 까닭(그때는 붙었는지를 함수의 반환값으로만 본다).
	bool KingMinds(const std::string& Faction, RValue& King, int& Place, std::string& Why)
	{
		Place = -1;
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(Faction + ".get_king_character_soul"), {}, King, Why))
			return false;
		if (!King.IsStruct())
		{
			Why = "get_king_character_soul did not return a struct";
			return false;
		}
		for (size_t i = 0; i < MindPlaces().size(); i++)
			if (MindsAt(King, static_cast<int>(i)) >= 0)
			{
				Place = static_cast<int>(i);
				return true;
			}
		Why = "no __opinion_minds array at any of the three places";
		return false;
	}

	// 그 왕국을 건드려도 되는가. 게임이 부르는 꼴 그대로 묻는다: is_destroyed() -> 불리언(네 시간에 19,785번),
	// get_king_character_soul() -> 구조체(34번). 망했거나 왕이 없는 세력에는 평판도 협정도 걸지 않는다(그런 세력에 불러 본 적이 없다).
	// 돌려주는 값: core 의 AliveOutcome(0 된다, 'x' 망했거나 왕이 없다, 'f' 묻지 못했다. Why 에 까닭).
	char CheckAlive(const std::string& Faction, std::string& Why)
	{
		RValue destroyed, king, ours;		// 이 함수 안에서만 든다
		std::string why_destroyed, why_king, why_ours;
		Log("diplomacy call is_destroyed(), get_king_character_soul() on " + Faction + " and get_king_character_soul() on the player faction");		// 부르기 전에 남긴다
		const bool asked = NlAccess::CallMethod(NlCore::ParseAskPath(Faction + ".is_destroyed"), {}, destroyed, why_destroyed) && NlGame::IsNumber(destroyed);
		const bool gone = asked && destroyed.ToDouble() != 0;
		// 망한 세력에는 더 묻지 않는다.
		const bool king_asked = asked && !gone && NlAccess::CallMethod(NlCore::ParseAskPath(Faction + ".get_king_character_soul"), {}, king, why_king);
		const bool has_king = king_asked && king.IsStruct();
		const bool ours_asked = has_king && NlAccess::CallMethod(NlCore::ParseAskPath(std::string(k_Player) + ".get_king_character_soul"), {}, ours, why_ours);
		const bool ours_king = ours_asked && ours.IsStruct();
		const char outcome = NlCore::AliveOutcome(asked, gone, king_asked, has_king, ours_asked, ours_king);
		if (outcome == 'f')
			Why = !asked ? "그 왕국이 망했는지 묻지 못했습니다 (" + (why_destroyed.empty() ? std::string("불리언이 아닌 답") : why_destroyed) + ")"
				: !king_asked ? "그 왕국의 왕을 묻지 못했습니다 (" + why_king + ")"
				: !ours_asked ? "우리 왕을 묻지 못했습니다 (" + why_ours + ")" : "우리 쪽에 왕이 없습니다";
		Log(std::string("diplomacy: alive check ") + (outcome == 0 ? "ok" : std::string(1, outcome)) + ": destroyed " + (asked ? (gone ? "true" : "false") : "unread")
			+ ", king " + (king_asked ? (has_king ? "struct" : "none") : "unasked") + ", ours " + (ours_asked ? (ours_king ? "struct" : "none") : "unasked"));
		return outcome;
	}

	// 협정을 맺는다(한 번에 끝난다). 돌려주는 값: core 의 PactReport 의 Outcome.
	char MakePact(const Job& It, const std::string& Faction, std::string& Why)
	{
		const double before = PactCellOf(It.Plan.Uuid);
		if (NlCore::HasPact(before, It.Pact))
			return 'a';
		RValue us, them, result;		// 이 함수 안에서만 든다
		if (!NlAccess::Read(NlCore::ParseAskPath(k_Player), us, Why) || !us.IsStruct()
			|| !NlAccess::Read(NlCore::ParseAskPath(Faction), them, Why) || !them.IsStruct())
		{
			Why = "세력의 구조체를 읽지 못했습니다";
			return 'f';
		}
		// FactionsAgreementMatrix.set_agreement(세력, 세력, 비트) -> undefined. (플레이어, 왕국, 4)로 불러 양쪽 칸이 4 가 되고
		// 양쪽의 is_declared_peace_with 가 참이 되는 것을 봤다(research/19). 이미 든 협정을 지우지 않게 지금의 비트에 더한 수를 넘긴다.
		const double cell = NlCore::PactCell(before, It.Pact);
		Log("diplomacy call set_agreement(" + std::string(k_Player) + ", " + Faction + ", " + std::to_string(static_cast<int>(cell)) + ")");
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(std::string(k_Pacts) + ".set_agreement"), { us, them, RValue(cell) }, result, Why))
			return 'f';
		// 쓴 뒤 다시 읽는다: 우리 쪽 칸이 넘긴 수가 됐는가.
		const double after = PactCellOf(It.Plan.Uuid);
		Log("diplomacy: pact cell " + NlCore::Shortest(before) + " -> " + NlCore::Shortest(after) + " (asked " + NlCore::Shortest(cell) + ")");
		if (after != cell)
		{
			Why = "협정의 칸이 넘긴 수가 되지 않았습니다 (앞 " + NlCore::Shortest(before) + ", 넘긴 수 " + NlCore::Shortest(cell) + ", 뒤 " + NlCore::Shortest(after) + ")";
			return 'f';
		}
		return 'd';
	}

	// 떼기(Clear)의 한 걸음: 붙여 둔 디버그 평판을 하나 뗀다. Faction.detach_opinion_about_faction(대상 세력, 평판 자료)(research/19 에서 이름과 인자를 봤다.
	// 하나를 뗀다). 좋은 자료부터 떼고, 뗀 뒤 그 왕의 평판의 수가 줄지 않으면 나쁜 자료로, 그것도 줄지 않으면 끝(core 의 DetachCheck·AfterDetach).
	// 평판의 수를 세지 못하는 왕에게는 떼지 않는다(언제 멈출지 모른다). 붙은 것이 없을 때 그 함수를 불러도 탈이 없었다(research/28: 수가 그대로라 그것으로 끝을 안다).
	// 돌려주는 값: 0 이면 더 한다, 아니면 끝났다(core 의 ClearOpinionReport 의 Outcome). Why 에 실패의 까닭.
	char ClearStep(Job& It, const std::string& Faction, std::string& Why)
	{
		const double kind = It.Plan.Side == 't' ? Relation(It.Plan.Uuid, g_PlayerUuid) : Relation(g_PlayerUuid, It.Plan.Uuid);
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
		if (It.Plan.Left <= 0)
			return 'l';
		const std::string holder = It.Plan.Side == 't' ? Faction : k_Player;
		const std::string about_path = It.Plan.Side == 't' ? k_Player : Faction;
		const std::string generic_path = It.Phase == 0 ? k_Good : k_Bad;
		RValue about, generic, result, updated, king;		// 이 함수 안에서만 든다
		if (!NlAccess::Read(NlCore::ParseAskPath(generic_path), generic, Why) || !generic.IsStruct())
		{
			Why = "게임의 디버그 평판 자료를 읽지 못했습니다";
			return 'f';
		}
		if (!NlAccess::Read(NlCore::ParseAskPath(about_path), about, Why) || !about.IsStruct())
		{
			Why = "세력의 구조체를 읽지 못했습니다";
			return 'f';
		}
		int place = -1;
		std::string count_why;
		if (!KingMinds(holder, king, place, count_why))
		{
			Why = "평판의 수를 세지 못해 떼지 않습니다 (" + count_why + ")";
			return 'f';
		}
		const double minds_before = MindsAt(king, place);
		if (!It.CountLogged)
		{
			Log("diplomacy: counting the opinion minds of " + holder + " at place " + std::to_string(place) + ": " + NlCore::Shortest(minds_before));
			It.CountLogged = true;
			It.MindsFirst = minds_before;
		}
		Log("diplomacy call " + holder + ".detach_opinion_about_faction(" + about_path + ", " + (It.Phase == 0 ? "debug_positive" : "debug_negative") + ")");		// 부르기 전에 남긴다
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(holder + ".detach_opinion_about_faction"), { about, generic }, result, Why))
			return 'f';
		const double minds_after = MindsAt(king, place);
		if (minds_after >= 0)
			It.MindsLast = minds_after;
		switch (NlCore::AfterDetach(NlCore::DetachCheck(minds_before, minds_after)))
		{
		case 'c':
			(It.Phase == 0 ? It.Good : It.Bad)++;
			It.Done++;
			It.Plan.Left--;
			break;
		case 'n':
			if (It.Phase == 0)
			{
				It.Phase = 1;		// 좋은 것은 더 없다: 나쁜 것으로
				return 0;
			}
			return It.Good + It.Bad == 0 ? 'a' : 'd';
		default:
			Why = "뗀 뒤의 평판의 수가 하나 줄지 않았습니다 (앞 " + NlCore::Shortest(minds_before) + ", 뒤 " + NlCore::Shortest(minds_after) + ")";
			return 'f';
		}
		// Faction.__update_relations(세력) -> undefined. 관계의 종류를 평판에서 다시 셈해 행렬에 쓴다(붙일 때와 같다).
		Log("diplomacy call " + holder + ".__update_relations(" + about_path + ")");
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(holder + ".__update_relations"), { about }, updated, Why))
		{
			Why = "평판은 뗐지만 관계를 다시 셈하게 하지 못했습니다 (" + Why + ")";
			return 'f';
		}
		return 0;
	}

	// 일의 한 걸음. 돌려주는 값: 0 이면 더 한다, 아니면 끝났다(core 의 DiplomacyReport 의 Outcome). Why 에 실패의 까닭.
	char StepJob(Job& It, std::string& Why)
	{
		const Kingdom* kingdom = FindKingdom(It.Plan.Uuid);
		std::string uuid;
		if (!kingdom || !NlAccess::ReadText(FactionPath(kingdom->Index) + ".__uuid", uuid) || uuid != It.Plan.Uuid)
		{
			Why = "그 왕국을 찾지 못했습니다";
			g_Ready = false;		// 자리가 밀렸다. 다음 틱이 다시 모은다(패널이 닫혀 있어도)
			return 'f';
		}
		const std::string faction = FactionPath(kingdom->Index);
		if (!It.Checked)
		{
			const char alive = CheckAlive(faction, Why);
			if (alive != 0)
				return alive;
			It.Checked = true;
		}
		if (It.Goal == DiplomacyGoal::Pact)
			return MakePact(It, faction, Why);
		if (It.Goal == DiplomacyGoal::Clear)
			return ClearStep(It, faction, Why);

		const double kind = It.Plan.Side == 't' ? Relation(It.Plan.Uuid, g_PlayerUuid) : Relation(g_PlayerUuid, It.Plan.Uuid);
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
		// 무엇을 할지는 core 가 정한다(바라는 관계, 이 명령의 한도).
		const NlCore::DiplomacyStep step = NlCore::PlanStep(kind, It.Goal, It.Plan.Sign, It.Plan.Left, It.Done);
		if (step.Outcome != 0)
			return step.Outcome;
		const int direction = step.Direction;

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
		const std::string holder = It.Plan.Side == 't' ? faction : k_Player;
		const std::string about_path = It.Plan.Side == 't' ? k_Player : faction;
		if (!NlAccess::Read(NlCore::ParseAskPath(about_path), about, Why) || !about.IsStruct())
		{
			Why = "세력의 구조체를 읽지 못했습니다";
			return 'f';
		}
		// 부르기 전에 남긴다: 이 걸음이 부르는 것은 그 왕의 구조체를 얻는 호출과 붙이는 호출이다.
		Log("diplomacy call " + holder + ".get_king_character_soul(), then attach_opinion_about_faction(" + about_path + ", "
			+ (direction > 0 ? "debug_positive" : "debug_negative") + ")");
		// 붙었는지를 가리려고 그 왕의 평판의 수를 붙이기 바로 앞뒤로 센다(같은 구조체에서 읽기만 한다. 사이에는 붙이는 호출 하나뿐이다).
		RValue king;		// 이 함수 안에서만 든다
		int place = -1;
		std::string count_why;
		const bool countable = KingMinds(holder, king, place, count_why);
		const double minds_before = countable ? MindsAt(king, place) : -1;
		if (!It.CountLogged)
		{
			Log(countable ? "diplomacy: counting the opinion minds of " + holder + " at place " + std::to_string(place) + ": " + NlCore::Shortest(minds_before)
				: "diplomacy: cannot count the opinion minds of " + holder + " (" + count_why + "); trusting what attach returns");
			It.CountLogged = true;
			It.MindsFirst = minds_before;
		}
		// Faction.attach_opinion_about_faction(대상 세력, 평판의 자료) -> 구조체. 게임이 (구조체, 구조체)로 부르는 것을 기록했고,
		// 그 꼴로 불러 평판이 -7 → -2, 관계가 opponent → neutrals 가 되는 것을 봤다(research/19).
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(holder + ".attach_opinion_about_faction"), { about, generic }, attached, Why))
			return 'f';
		if (!attached.IsStruct())
		{
			Why = "평판을 붙이는 함수가 구조체를 돌려주지 않았습니다 (붙지 않은 것으로 봅니다)";
			return 'f';
		}
		// 쓴 뒤 다시 읽는다: 그 왕의 평판이 하나 늘었는가. 우리 왕에게서는 같은 평판이 50개에 닿자 구조체가 돌아와도 늘지 않았다(research/19).
		// 무엇으로 볼지는 core 가 정한다: 늘었으면 확인, 이 일에서 느는 것을 본 뒤에 그대로면 붙지 않은 것(세지 않고 멈춘다), 그 밖에는 반환값을 믿는다.
		const double minds_after = countable ? MindsAt(king, place) : -1;
		if (minds_after >= 0)
			It.MindsLast = minds_after;
		const char verdict = NlCore::AfterAttach(NlCore::AttachCheck(minds_before, minds_after), It.Seen);
		if (verdict == 's')
			return 's';
		It.Seen = It.Seen || verdict == 'c';
		It.Unsure = It.Unsure || verdict == 't';
		It.Done += direction;
		It.Plan.Left--;
		// Faction.__update_relations(세력) -> undefined. 게임이 그 꼴로 955번 불렀다. 관계의 종류를 평판에서 다시 셈해 행렬에 쓴다.
		Log("diplomacy call " + holder + ".__update_relations(" + about_path + ")");
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(holder + ".__update_relations"), { about }, updated, Why))
		{
			Why = "평판은 붙었지만 관계를 다시 셈하게 하지 못했습니다 (" + Why + ")";
			return 'f';
		}
		return 0;
	}

	// 일 하나를 Budget 걸음까지 한다. 끝났으면 결과를 Tally 에 적고 참을 돌려준다.
	bool RunJob(Job& It, int Budget, NlCore::JobTally& Tally)
	{
		std::string why;
		char outcome = 0;
		for (int i = 0; i < Budget && outcome == 0; i++)
			outcome = StepJob(It, why);
		if (outcome == 0)
			return false;

		const Kingdom* kingdom = FindKingdom(It.Plan.Uuid);
		const double after = It.Plan.Side == 't' ? Relation(It.Plan.Uuid, g_PlayerUuid) : Relation(g_PlayerUuid, It.Plan.Uuid);
		const std::string name = kingdom ? kingdom->Name : It.Plan.Uuid;
		const double before = It.Started ? It.Before : after;
		// 망한 왕국('x')과 묻지 못한 것('f', 관계를 읽기 전)은 어느 일이든 같은 글이다. 협정은 제 글, 평판의 수를 다 붙인 것은 붙인 개수와 관계를 따로 적는다.
		std::string line = It.Goal == DiplomacyGoal::Pact && outcome != 'x' ? NlCore::PactReport(name, It.Pact, outcome, why)
			: It.Goal == DiplomacyGoal::Clear ? NlCore::ClearOpinionReport(name, It.Plan.Side, before, after, It.Good, It.Bad, outcome, why)
			: It.Goal == DiplomacyGoal::Opinion && ((outcome == 'd' && It.Done != 0) || outcome == 's')
				? NlCore::OpinionReport(name, It.Plan.Side, before, after, It.Done, outcome == 's')
			: NlCore::DiplomacyReport(name, It.Plan.Side, before, after, It.Done, outcome, why);
		// 붙은 것을 확인하지 못하고 센 걸음이 있으면 그렇게 적는다(세지 못한 왕, 느는 것을 본 적이 없는 왕).
		if (It.Unsure && It.Done != 0)
			line += NlCore::UnsureNote();
		Log("diplomacy: " + It.Plan.Uuid + " side " + std::string(1, It.Plan.Side) + " goal " + NlCore::DiplomacyGoalWord(It.Goal)
			+ (It.Goal == DiplomacyGoal::Pact ? std::string(" ") + NlCore::DiplomacyPactWord(It.Pact) : std::string()) + ": outcome "
			+ std::string(1, outcome) + ", attached " + std::to_string(It.Done)
			+ (It.CountLogged ? ", minds " + NlCore::Shortest(It.MindsFirst) + " -> " + NlCore::Shortest(It.MindsLast) + (It.Seen ? " (growth seen)" : " (no growth seen)")
				+ (It.Unsure ? ", some steps unconfirmed" : "") : std::string())
			+ (why.empty() ? "" : ", " + why));
		Tally.Add(outcome, line);
		return true;
	}

	// 명령을 일들로 푼다(core 의 PlanJobs). g_Kingdoms 는 틱이 채운 사본이다(러너를 부르지 않는다).
	std::vector<Job> MakeJobs(const DiplomacyCommand& Command)
	{
		std::vector<std::string> uuids;
		for (const Kingdom& kingdom : g_Kingdoms)
			uuids.push_back(kingdom.Uuid);
		std::vector<Job> jobs;
		for (NlCore::DiplomacyJob& plan : NlCore::PlanJobs(Command, uuids))
		{
			Job job;
			job.Plan = std::move(plan);
			job.Goal = Command.Goal;
			job.Pact = Command.Pact;
			jobs.push_back(std::move(job));
		}
		return jobs;
	}

	// ---- 그리는 쪽 (러너를 부르지 않는다) ----

	// 흐린 글. 창의 너비에서 줄을 바꾼다.
	char SideChoice()
	{
		return g_SideChoice == 1 ? 't' : g_SideChoice == 2 ? 'u' : 'b';
	}

	// 명령을 일로 풀어 쌓는다. 돌려주는 값: 쌓은 일의 수. 거부하면 -1 이고 Why 에 까닭.
	int PushCommand(const DiplomacyCommand& Command, std::string& Why)
	{
		if (!NlCore::CheckDiplomacy(Command, Why))
			return -1;
		std::vector<Job> jobs = MakeJobs(Command);
		if (jobs.empty())
			return 0;				// 그 왕국이 없다. 앞의 결과를 지우지 않는다
		if (g_Jobs.empty())
			g_Tally.Reset();		// 앞의 명령이 다 끝났다. 새로 센다
		for (Job& job : jobs)
			g_Jobs.push_back(std::move(job));
		g_Tally.Expect(static_cast<int>(jobs.size()));
		return static_cast<int>(jobs.size());
	}

	// 창의 단추.
	void Push(const std::string& Who, DiplomacyGoal Goal, double Amount, NlCore::DiplomacyPact Pact = NlCore::DiplomacyPact::Peace)
	{
		DiplomacyCommand command{ Who, Goal, SideChoice(), Amount };
		command.Pact = Pact;
		g_Refused.clear();
		const int count = PushCommand(command, g_Refused);
		if (count == 0)
			g_Refused = "그 왕국이 없습니다";
		else if (count > 0)
			g_Refused.clear();
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
	const NlCore::ScopedFlag busy(g_Busy);
	if (read || !g_Ready)
	{
		g_NextRead = Now + 1;
		if (!Scan())
		{
			DropJobs(g_Why);		// 게임 화면을 떠났거나 읽지 못했다. 버린 일을 "됐다"에 남기지 않는다
			return;
		}
	}
	if (work)
	{
		g_NextStep = Now + k_StepEvery;
		// 패널이 닫혀 있으면 위의 읽기가 돌지 않는다. 게임의 함수를 부르기 전에 게임 화면인지부터 본다(메뉴에서 부르지 않는다).
		if (!NlAccess::InGame())
		{
			g_Ready = false;
			g_Why = "게임 화면이 아닙니다";
			DropJobs(g_Why);
			return;
		}
		if (RunJob(g_Jobs.front(), k_StepsPerTick, g_Tally))
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
	const NlCore::ScopedFlag busy(g_Busy);
	if (!Scan())
		return { g_Why };

	// 제 결과는 따로 센다(창이 쌓아 둔 일들의 셈과 섞지 않는다).
	NlCore::JobTally tally;
	std::vector<Job> jobs = MakeJobs(Command);
	if (jobs.empty())
		return { "그 왕국이 없습니다" };
	tally.Expect(static_cast<int>(jobs.size()));
	for (Job& job : jobs)
		RunJob(job, NlCore::k_ClearStepsMax + 3, tally);		// 끝까지: 한도만큼 붙이거나 떼고 한두 걸음 더 보면 끝난다(떼기의 한도가 더 크다)
	ReadKinds();
	std::vector<std::string> lines = tally.Lines();
	lines.push_back(tally.Summary());
	return lines;
}

std::string NlDiplomacy::Queue(const DiplomacyCommand& Command)
{
	std::lock_guard lock(g_Mutex);
	if (!g_Ready)
		return g_Why.empty() ? "왕국을 아직 읽지 않았습니다 (외교 패널을 열거나 diplomacy list 를 먼저)" : g_Why;
	std::string why;
	const int count = PushCommand(Command, why);
	if (count < 0)
		return why;
	return count == 0 ? "그 왕국이 없습니다" : "일 " + std::to_string(count) + "개를 쌓았습니다 (틱이 합니다)";
}

std::vector<std::string> NlDiplomacy::List()
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const NlCore::ScopedFlag busy(g_Busy);
	if (!Scan())
		return { g_Why };
	std::vector<std::string> lines;
	for (const Kingdom& kingdom : g_Kingdoms)
		lines.push_back(kingdom.Uuid + "  " + kingdom.Name + "  them " + NlCore::RelationLabel(kingdom.Theirs) + " (" + NlCore::Shortest(kingdom.Theirs)
			+ ")  us " + NlCore::RelationLabel(kingdom.Ours) + " (" + NlCore::Shortest(kingdom.Ours) + ")  pacts " + NlCore::PactText(kingdom.Pacts)
			+ " (" + NlCore::Shortest(kingdom.Pacts) + ")");
	lines.push_back("(" + std::to_string(g_Kingdoms.size()) + " kingdoms, player " + g_PlayerUuid + ")");
	// 쌓인 일들의 결과(창의 단추와 queue=1). 실패한 줄이 앞에 온다.
	if (!g_Tally.Empty())
	{
		lines.push_back("queued: " + g_Tally.Summary());
		for (size_t i = 0; i < g_Tally.Lines().size() && i < k_ShownLines; i++)
			lines.push_back("  " + g_Tally.Lines()[i]);
		if (g_Tally.Lines().size() > k_ShownLines)
			lines.push_back("  (and " + std::to_string(g_Tally.Lines().size() - k_ShownLines) + " more lines; failures come first)");
	}
	return lines;
}

void NlDiplomacy::Draw()
{
	std::lock_guard lock(g_Mutex);
	if (!g_Ready)
	{
		ImGui::TextDisabled("%s", g_Why.empty() ? "게임을 시작하면 왕국들이 보입니다." : g_Why.c_str());
		if (!g_Tally.Empty())
			ImGui::TextDisabled("%s", g_Tally.Summary().c_str());
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
	ImGui::SameLine();
	if (ImGui::Button("모든 왕국의 평판 떼기"))
		Push("all", DiplomacyGoal::Clear, 0);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("여기서 붙인 디버그 평판을 하나씩 뗍니다 (고른 쪽마다. 좋은 것부터, 그다음 나쁜 것). 게임의 함수를 부르고 왕의 평판의 수가 줄었는지로 봅니다");
	NlUi::Hint("게임의 디버그용 평판을 왕에게 붙여 관계를 움직이고, 게임의 협정 함수로 협정을 맺습니다. '떼기'는 붙인 디버그 평판을 뗍니다(research/28: 붙인 3개와 2개가 그대로 떨어졌다. 뗀 뒤의 관계는 게임이 다시 셈한 것이라 붙이기 전과 다를 수 있다). 협정을 푸는 단추는 없습니다.");
	// 긴 설명은 접어 둔다(펼쳐 두면 왕국의 표가 창 아래로 밀린다).
	if (ImGui::CollapsingHeader("설명"))
		NlUi::Hint("왕국 사이의 관계는 게임이 왕끼리의 평판에서 셈합니다. 여기서는 게임의 디버그용 평판(좋은 것, 나쁜 것)을 게임의 함수로 하나씩 붙이고 관계를 다시 셈하게 합니다. "
			"'우호'·'중립'·'적대'는 그 관계가 될 때까지 붙입니다(한 번에 한쪽 40개까지. 적대는 철천지원수까지 내립니다). '+'·'-'는 고른 쪽마다 평판 하나를 붙입니다. "
			"우호가 되기까지 드는 개수는 왕국마다 달랐습니다(1개에서 15개). 붙을 때마다 그 왕의 평판의 수를 세어 붙었는지 봅니다. 우리 왕에게서는 같은 평판이 50개까지만 겹쳤습니다: "
			"그 뒤로 붙지 않으면 멈추고 그렇게 적습니다. 세어서 확인하지 못한 것은 함수가 돌려준 값으로만 봤다고 적습니다. "
			"동맹·봉신·주군 관계와 망한 왕국은 건드리지 않습니다. 붙인 평판을 떼는 단추는 없습니다(반대쪽을 붙여 상쇄합니다). 얼마나 오래 남는지, 세이브에 남는지는 확인 전입니다. "
			"'평화'·'방어'·'교역'은 게임의 협정 함수로 그 왕국과 협정(평화 협정, 방어 동맹, 교역 협정)을 바로 맺습니다. 이미 맺은 협정은 단추가 아니라 글자로 보입니다. "
			"방어 동맹을 맺어도 관계가 '동맹'이 되지는 않습니다. 푸는 단추는 없습니다. 게임이 그 협정을 어떻게 따르는지(기한, 침공)는 확인 전입니다.");
	if (!g_Refused.empty())
		NlUi::Hint(g_Refused.c_str());
	if (!g_Tally.Empty())
	{
		ImGui::TextDisabled("%s", g_Tally.Summary().c_str());
		// 실패한 줄이 앞에 온다(core 의 JobTally). 창은 앞의 몇 줄만 보인다.
		const std::vector<std::string>& lines = g_Tally.Lines();
		for (size_t i = 0; i < lines.size() && i < k_ShownLines; i++)
			NlUi::Hint(lines[i].c_str());
		if (lines.size() > k_ShownLines)
			ImGui::TextDisabled("(그 밖에 %d줄. 안 된 것은 위에 먼저 적혀 있습니다)", static_cast<int>(lines.size() - k_ShownLines));
	}

	if (ImGui::BeginTable("kingdoms", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("왕국");
		ImGui::TableSetupColumn("그쪽이 우리를");
		ImGui::TableSetupColumn("우리가 그쪽을");
		ImGui::TableSetupColumn("관계·평판");
		ImGui::TableSetupColumn("협정");		// 맨 끝에 둔다: 글이 길어져도 앞의 단추들을 밀어내지 않게
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
			if (ImGui::SmallButton("+"))
				Push(kingdom.Uuid, DiplomacyGoal::Opinion, 1);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("좋은 평판 하나를 붙입니다 (위에서 고른 쪽마다)");
			ImGui::SameLine();
			if (ImGui::SmallButton("-"))
				Push(kingdom.Uuid, DiplomacyGoal::Opinion, -1);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("나쁜 평판 하나를 붙입니다 (위에서 고른 쪽마다)");
			ImGui::SameLine();
			if (ImGui::SmallButton("떼기"))
				Push(kingdom.Uuid, DiplomacyGoal::Clear, 0);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("여기서 붙인 디버그 평판을 모두 뗍니다 (위에서 고른 쪽마다)");
			ImGui::TableNextColumn();
			// 협정: 이미 맺은 것은 글자로, 아직 없는 것은 단추로. "방어"는 방어 동맹(협정)이다. 관계의 종류 "동맹"과 다른 것이라 낱말을 가른다.
			static const struct
			{
				NlCore::DiplomacyPact Pact;
				const char* Label;
			} pacts[] = { { NlCore::DiplomacyPact::Peace, "평화" }, { NlCore::DiplomacyPact::Defence, "방어" }, { NlCore::DiplomacyPact::Trade, "교역" } };
			for (size_t p = 0; p < std::size(pacts); p++)
			{
				if (p > 0)
					ImGui::SameLine();
				if (NlCore::HasPact(kingdom.Pacts, pacts[p].Pact))
				{
					ImGui::TextUnformatted(pacts[p].Label);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s: 맺었습니다", NlCore::DiplomacyPactLabel(pacts[p].Pact));
				}
				else
				{
					if (ImGui::SmallButton(pacts[p].Label))
						Push(kingdom.Uuid, DiplomacyGoal::Pact, 0, pacts[p].Pact);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s을 맺습니다", NlCore::DiplomacyPactLabel(pacts[p].Pact));
				}
			}
			// 아는 협정으로 설명되지 않는 것이 칸에 있으면 숨기지 않는다.
			if (NlCore::PactUnknown(kingdom.Pacts))
			{
				ImGui::SameLine();
				ImGui::TextDisabled("?");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("이 칸에 모르는 협정의 비트가 있습니다 (%s)", NlCore::Shortest(kingdom.Pacts).c_str());
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
}
