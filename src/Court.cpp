#include "Court.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "People.hpp"
#include "core/AskPath.hpp"
#include "core/DiplomacyPlan.hpp"
#include "core/PeoplePlan.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <mutex>

using namespace YYTK;
using NlCore::CourtCommand;
using NlCore::CourtGoal;

namespace
{
	// 자리와 함수는 research/20 에서 잰 것이다(0.5588.9777.0).
	constexpr const char* k_Player = "inst:o_game_map_controller.__factions_manager.__player_faction";
	// 게임의 디버그용 평판. __opinion_modify 가 +5 / -5, __stack_limit 이 50 이다(외교 패널이 쓰는 것과 같다).
	constexpr const char* k_Good = "inst:o_data.opinion_mind_debug_positive";
	constexpr const char* k_Bad = "inst:o_data.opinion_mind_debug_negative";
	constexpr double k_StepEvery = 0.02;		// 쌓인 일을 한 묶음씩 하는 간격(초)
	constexpr int k_StepsPerTick = 6;			// 한 묶음의 걸음 수(한 틱이 길어지지 않게)
	constexpr size_t k_ShownLines = 8;			// 원격의 답에 적는 결과 줄의 수(실패한 줄이 앞에 온다)
	constexpr size_t k_PanelLines = 2;			// 창에 바로 보이는 결과 줄의 수(더 두면 영주의 표가 창 아래로 밀린다. 나머지는 접어 둔다)
	constexpr size_t k_PanelMore = 40;			// 펼쳤을 때 더 보이는 줄의 한도
	const double k_Unread = std::numeric_limits<double>::quiet_NaN();

	struct Lord
	{
		std::string Uuid, Name;
		int Index = -1;					// o_character 의 몇 번째였는가. 인물이 드나들면 바뀐다: 쓰기 전에 그 자리의 uuid 를 다시 본다
		bool King = false;				// 플레이어 세력의 왕이다
		int HasLoyalty = -1;			// is_has_loyalty(): 게임이 이 사람에게 충성을 따지는가(왕과 아이는 거짓이었다). 1 참, 0 거짓, -1 읽지 못했다
		double Loyalty = k_Unread;		// get_loyalty_to_king()
		double State = -1;				// get_loyalty_state(): 0, 1, 2
		int Followers = 0;				// 이 영주를 충성 대상으로 삼은 사람의 수
		std::vector<double> Opinions;	// 이 영주가 g_Lords[i] 를 보는 평판(get_opinion). 자기 자신과 읽지 못한 것은 NaN
	};

	struct Follower
	{
		std::string Uuid, Lord;			// 그 사람과 충성 대상의 uuid
		bool Character = false;
		int Index = -1;
	};

	struct Job
	{
		NlCore::CourtJob Plan;
		NlCore::CourtDone Done;
		double Before = k_Unread, After = k_Unread;		// 평판
		bool Started = false;
		bool Logged = false;		// 이 일이 부르는 것을 로그에 남겼다
	};

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlCourt::LogFn g_Log;
	bool g_Ready = false;
	std::string g_Why;					// g_Ready 가 아닌 까닭
	std::vector<Lord> g_Lords;			// 틱이 채우고 Draw 가 읽는다. RValue 를 담지 않는다
	std::vector<Follower> g_Followers;
	std::deque<Job> g_Jobs;				// 창이 쌓고 틱이 한다
	NlCore::DiplomacyTally g_Tally;		// 쌓인 일들의 결과(창이 보인다)
	std::string g_Refused;				// 창의 단추가 거부된 까닭
	double g_NextRead = 0, g_NextStep = 0;
	bool g_Busy = false;				// 하는 중이다(여기서 부른 게임의 함수가 틱을 다시 부르면 안쪽은 아무것도 하지 않는다)
	bool g_ReadLogged = false;			// 훑을 때 부르는 것을 로그에 한 번 남겼다
	bool g_ScanSkipped = false;			// 마지막 훑기가 잠깐 못 한 것이다(인물 쪽이 하는 중). 실패가 아니다: 값과 쌓인 일을 그대로 둔다
	std::string g_PickHolder, g_PickAbout;		// 창에서 고른 짝

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

	std::string Base(bool Character, int Index)
	{
		return std::string("inst:") + (Character ? "o_character" : "o_dummy") + ":" + std::to_string(Index);
	}

	std::string Base(const Lord& It)
	{
		return Base(true, It.Index);
	}

	bool ReadText(const std::string& Path, std::string& Out)
	{
		RValue value;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath(Path), value, why) || !value.IsString())
			return false;
		Out = value.ToString();
		return true;
	}

	// 수(불리언 포함)를 돌려주는 메서드를 부른다. 못 부르거나 수가 아니면 거짓.
	bool CallNumber(const std::string& Path, const std::vector<RValue>& Args, double& Out)
	{
		RValue result;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(Path), Args, result, why) || !NlGame::IsNumber(result))
			return false;
		Out = result.ToDouble();
		return true;
	}

	bool ReadStruct(const std::string& Path, RValue& Out)
	{
		std::string why;
		return NlAccess::Read(NlCore::ParseAskPath(Path), Out, why) && Out.IsStruct();
	}

	const Lord* FindLord(const std::string& Uuid)
	{
		for (const Lord& lord : g_Lords)
			if (lord.Uuid == Uuid)
				return &lord;
		return nullptr;
	}

	// 그 자리의 인스턴스가 아직 그 영주인가.
	bool StillThere(const Lord& It)
	{
		std::string uuid;
		return ReadText(Base(It) + ".__soul.__uuid", uuid) && uuid == It.Uuid;
	}

	// 쌓인 일을 버린다(하지 못했다). 버린 수를 실패로 세고 까닭을 줄로 남긴다.
	void DropJobs(const std::string& Why)
	{
		if (g_Jobs.empty())
			return;
		Log("court: dropped " + std::to_string(g_Jobs.size()) + " job(s): " + Why);
		g_Tally.Drop(static_cast<int>(g_Jobs.size()), Why);
		g_Jobs.clear();
	}

	// 영주들과 값을 다시 읽는다. 못 하면 거짓이고 g_Why 에 까닭.
	// 인물 쪽이 게임의 함수를 부르는 중에 다시 들어온 틱이면 아무것도 건드리지 않고 거짓을 돌려준다(g_ScanSkipped 가 참. 다음 틱에 다시 한다).
	bool Scan()
	{
		g_ScanSkipped = false;
		std::vector<NlCore::PersonRow> rows;
		std::string why;
		const NlPeople::RowsResult read = NlAccess::InGame() ? NlPeople::Rows(rows, why) : NlPeople::RowsResult::Failed;
		if (read == NlPeople::RowsResult::Busy)
		{
			g_ScanSkipped = true;
			return false;
		}
		g_Ready = false;
		if (!NlAccess::InGame())
		{
			g_Why = "게임 화면이 아닙니다";
			g_Lords.clear();
			g_Followers.clear();
			return false;
		}
		if (read != NlPeople::RowsResult::Ok)
		{
			g_Why = why.empty() ? "사람들을 읽지 못했습니다" : why;
			return false;
		}
		if (!g_ReadLogged)
		{
			// 부르기 전에 남긴다(훑을 때마다 같은 호출이다): 모두 게임이 그 꼴로 부르는 것을 기록한 것이다(research/20).
			Log("court reads call get_king_character_soul() on the player faction, get_loyalty_to_king(), get_loyalty_state(), is_has_loyalty() on each lord "
				"and get_opinion(the other lord) on each lord's opinions");
			g_ReadLogged = true;
		}

		std::vector<Lord> lords;
		std::vector<RValue> souls;		// 영주마다의 __soul.__character_soul. 이 함수 안에서만 든다
		for (const NlCore::PersonRow& row : rows)
		{
			if (!row.Character || !NlCore::IsPlayers(row))
				continue;
			Lord lord;
			lord.Uuid = row.Uuid;
			lord.Name = row.Name;
			lord.Index = row.Index;
			RValue soul;
			if (!ReadStruct(Base(lord) + ".__soul.__character_soul", soul))
				continue;		// 평판을 갖지 않는 사람이다
			lords.push_back(std::move(lord));
			souls.push_back(soul);
		}

		// 왕: Faction.get_king_character_soul() -> 구조체. 영주의 __character_soul 과 같은 구조체인지를 주소로 견준다(우리 왕의 것이 그랬다).
		RValue king;
		const bool has_king = NlAccess::CallMethod(NlCore::ParseAskPath(std::string(k_Player) + ".get_king_character_soul"), {}, king, why) && king.IsStruct();
		for (size_t i = 0; i < lords.size(); i++)
		{
			Lord& lord = lords[i];
			lord.King = has_king && souls[i].m_Object == king.m_Object;
			const std::string soul = Base(lord) + ".__soul";
			// 충성의 함수들은 왕이 있는 세이브에서만 쟀다. 왕을 찾지 못했으면 부르지 않는다(읽지 못한 것으로 둔다).
			if (has_king)
			{
				double has = 0;
				lord.HasLoyalty = CallNumber(soul + ".is_has_loyalty", {}, has) ? (has != 0 ? 1 : 0) : -1;
				if (!CallNumber(soul + ".get_loyalty_to_king", {}, lord.Loyalty))
					lord.Loyalty = k_Unread;
				if (!CallNumber(soul + ".get_loyalty_state", {}, lord.State))
					lord.State = -1;
			}
			// OpinionMinds.get_opinion(상대의 __character_soul) -> 수.
			lord.Opinions.assign(lords.size(), k_Unread);
			for (size_t k = 0; k < lords.size(); k++)
				if (k != i)
					CallNumber(soul + ".__character_soul.__opinions.get_opinion", { souls[k] }, lord.Opinions[k]);
		}

		// 따르는 사람: __soul.__fealty.__loyaled_to_uuid 가 그 영주의 uuid 다(없으면 빈 글). 읽기만 한다.
		// 주민·병사(o_dummy)만 센다: 충성 대상을 지우는 함수는 주민에게만 불러 봤다(영주에게는 부르지 않는다).
		std::vector<Follower> followers;
		for (const NlCore::PersonRow& row : rows)
		{
			if (row.Character || !NlCore::IsPlayers(row))
				continue;
			std::string to;
			if (!ReadText(Base(row.Character, row.Index) + ".__soul.__fealty.__loyaled_to_uuid", to) || to.empty())
				continue;
			for (Lord& lord : lords)
				if (lord.Uuid == to)
				{
					lord.Followers++;
					followers.push_back({ row.Uuid, to, row.Character, row.Index });
				}
		}

		if (lords.size() != g_Lords.size())
			Log("court: " + std::to_string(lords.size()) + " lords, king " + (has_king ? "found" : "not found") + ", " + std::to_string(followers.size()) + " followers");
		g_Lords = std::move(lords);
		g_Followers = std::move(followers);
		g_Ready = true;
		g_Why.clear();
		return true;
	}

	// 따르는 사람들의 충성 대상을 지운다(한 번에 끝난다). 돌려주는 값: core 의 ReleaseOutcome.
	char Release(const Lord& It, std::string& Line)
	{
		int followers = 0, released = 0;
		std::string why;
		for (const Follower& one : g_Followers)
		{
			if (one.Lord != It.Uuid)
				continue;
			followers++;
			const std::string soul = Base(one.Character, one.Index) + ".__soul";
			std::string uuid, to;
			// 그 자리가 아직 그 사람이고 아직 이 영주를 따르는가.
			if (!ReadText(soul + ".__uuid", uuid) || uuid != one.Uuid || !ReadText(soul + ".__fealty.__loyaled_to_uuid", to) || to != It.Uuid)
			{
				why = "그 사람의 자리가 바뀌었습니다";
				g_Ready = false;
				continue;
			}
			// ComponentFealty.reset_loyaled_to()(인자 없음. 본문이 인자를 읽지 않는다): 한 주민에게 불러 충성 대상이 빈 글이 되는 것을 봤다(research/20).
			Log("court call reset_loyaled_to() on " + one.Uuid + " (was loyal to " + It.Uuid + ")");
			RValue result;		// 이 함수 안에서만 든다
			std::string call_why;
			if (!NlAccess::CallMethod(NlCore::ParseAskPath(soul + ".__fealty.reset_loyaled_to"), {}, result, call_why))
			{
				why = call_why;
				continue;
			}
			// 쓴 뒤 다시 읽는다.
			if (ReadText(soul + ".__fealty.__loyaled_to_uuid", to) && to.empty())
				released++;
			else
				why = "충성 대상이 지워지지 않았습니다";
		}
		Line = NlCore::ReleaseReport(It.Name, followers, released, why);
		Log("court: release " + It.Uuid + ": " + std::to_string(released) + " of " + std::to_string(followers) + (why.empty() ? "" : ", " + why));
		return NlCore::ReleaseOutcome(followers, released);
	}

	// 일의 한 걸음. 돌려주는 값: 0 이면 더 한다, 아니면 끝났다(core 의 PlanCourtStep 의 Outcome). Why 에 실패의 까닭.
	char StepJob(Job& It, std::string& Why)
	{
		const Lord* holder = FindLord(It.Plan.Holder);
		const Lord* about = FindLord(It.Plan.About);
		if (!holder || !about || !StillThere(*holder) || !StillThere(*about))
		{
			Why = "그 영주를 찾지 못했습니다 (자리가 바뀌었으면 다시 눌러 주세요)";
			g_Ready = false;		// 다음 틱이 다시 모은다
			return 'f';
		}
		// 붙일 평판의 자료가 잰 것과 같은지 본다(±5, 겹침 한도). 아니면 부르지 않는다.
		double good_modify = 0, bad_modify = 0, good_limit = 0, bad_limit = 0;
		RValue target, good, bad;		// 이 함수 안에서만 든다
		if (!NlAccess::ReadNumber(std::string(k_Good) + ".__opinion_modify", good_modify) || good_modify != NlCore::k_OpinionUnit
			|| !NlAccess::ReadNumber(std::string(k_Bad) + ".__opinion_modify", bad_modify) || bad_modify != -NlCore::k_OpinionUnit
			|| !NlAccess::ReadNumber(std::string(k_Good) + ".__stack_limit", good_limit) || !NlAccess::ReadNumber(std::string(k_Bad) + ".__stack_limit", bad_limit)
			|| !ReadStruct(k_Good, good) || !ReadStruct(k_Bad, bad))
		{
			Why = "게임의 디버그 평판 자료가 잰 것과 다릅니다";
			return 'f';
		}
		if (!ReadStruct(Base(*about) + ".__soul.__character_soul", target))
		{
			Why = "대상의 인물 영혼을 읽지 못했습니다";
			return 'f';
		}
		const std::string minds = Base(*holder) + ".__soul.__character_soul.__opinions";
		if (!It.Logged)
		{
			// 부르기 전에 남긴다: 이 일이 부르는 것은 아래의 넷이다(모두 게임이 그 꼴로 부르는 것을 기록했다. research/20).
			Log("court call on " + minds + " about " + It.Plan.About + ": get_opinion(target), get_number_of_attached_opinion_minds_for_character(target, generic), "
				"opinion_attach(target, generic, 1, undefined), detach_generic_opinion_mind(target, generic)");
			It.Logged = true;
		}
		// 지금의 평판과, 그 대상에게 붙어 있는 디버그 평판의 수(게임이 세어 준다).
		const std::string count = minds + ".get_number_of_attached_opinion_minds_for_character";
		double opinion = k_Unread, good_before = -1, bad_before = -1;
		if (!CallNumber(minds + ".get_opinion", { target }, opinion) || !CallNumber(count, { target, good }, good_before) || !CallNumber(count, { target, bad }, bad_before))
		{
			Why = "평판을 읽지 못했습니다";
			return 'f';
		}
		if (!It.Started)
		{
			It.Before = opinion;
			It.Started = true;
		}
		It.After = opinion;
		// 무엇을 할지는 core 가 정한다.
		const NlCore::CourtStep step = NlCore::PlanCourtStep(It.Plan, opinion, good_before, bad_before, It.Done.Total(), good_limit < bad_limit ? good_limit : bad_limit);
		if (step.Outcome != 0)
		{
			if (step.Outcome == 'f')
				Why = "읽은 수로는 걸을 수 없습니다";
			return step.Outcome;
		}

		RValue result;		// 이 함수 안에서만 든다
		const bool with_good = step.Action == 'A' || step.Action == 'D';
		const bool attach = step.Action == 'A' || step.Action == 'a';
		// opinion_attach(대상, 자료, 1, undefined) -> 평판. detach_generic_opinion_mind(대상, 자료) -> undefined(하나를 뗀다).
		const bool called = attach
			? NlAccess::CallMethod(NlCore::ParseAskPath(minds + ".opinion_attach"), { target, with_good ? good : bad, RValue(1.0), RValue() }, result, Why)
			: NlAccess::CallMethod(NlCore::ParseAskPath(minds + ".detach_generic_opinion_mind"), { target, with_good ? good : bad }, result, Why);
		if (!called)
			return 'f';
		// 쓴 뒤 다시 읽는다: 센 수가 바라는 대로 하나만 바뀌었는가(함수의 반환값으로 판정하지 않는다).
		double good_after = -1, bad_after = -1;
		if (!CallNumber(count, { target, good }, good_after) || !CallNumber(count, { target, bad }, bad_after))
		{
			// 세지 못했다: 됐는지 모른다(안 됐다고 적지 않는다). 세지 않고 멈춘다.
			Why = std::string(attach ? "붙이는" : "떼는") + " 함수는 불렀지만 그 뒤의 수를 읽지 못해 확인하지 못했습니다";
			return 'f';
		}
		if (!NlCore::CourtStepDone(step.Action, good_before, bad_before, good_after, bad_after))
		{
			Why = std::string(attach ? "붙지" : "떼어지지") + " 않았습니다 (좋은 평판 " + NlCore::Shortest(good_before) + " -> " + NlCore::Shortest(good_after)
				+ ", 나쁜 평판 " + NlCore::Shortest(bad_before) + " -> " + NlCore::Shortest(bad_after) + ")";
			return 'f';
		}
		NlCore::CountCourtStep(It.Done, step.Action);
		It.Plan.Left--;
		return 0;
	}

	// 일 하나를 Budget 걸음까지 한다. 끝났으면 결과를 Tally 에 적고 참을 돌려준다.
	bool RunJob(Job& It, int Budget, NlCore::DiplomacyTally& Tally)
	{
		const Lord* holder = FindLord(It.Plan.Holder);
		const std::string holder_name = holder ? holder->Name : It.Plan.Holder;
		if (It.Plan.Goal == CourtGoal::Release)
		{
			std::string line;
			const char outcome = holder && StillThere(*holder) ? Release(*holder, line) : 'f';
			if (line.empty())
			{
				line = holder_name + ": 그 영주를 찾지 못했습니다 (자리가 바뀌었으면 다시 눌러 주세요)";
				g_Ready = false;		// 다음 틱이 다시 모은다
			}
			Tally.Add(outcome, line);
			return true;
		}

		std::string why;
		char outcome = 0;
		for (int i = 0; i < Budget && outcome == 0; i++)
			outcome = StepJob(It, why);
		if (outcome == 0)
			return false;

		const Lord* about = FindLord(It.Plan.About);
		const std::string line = NlCore::CourtReport(holder_name, about ? about->Name : It.Plan.About, It.Plan, It.Before, It.After, It.Done, outcome, why);
		Log("court: " + It.Plan.Holder + " about " + It.Plan.About + " goal " + NlCore::CourtGoalWord(It.Plan.Goal) + ": outcome " + std::string(1, outcome)
			+ ", opinion " + NlCore::Shortest(It.Before) + " -> " + NlCore::Shortest(It.After) + ", good +" + std::to_string(It.Done.GoodOn) + " -"
			+ std::to_string(It.Done.GoodOff) + ", bad +" + std::to_string(It.Done.BadOn) + " -" + std::to_string(It.Done.BadOff) + (why.empty() ? "" : ", " + why));
		Tally.Add(outcome, line);
		return true;
	}

	// 명령을 일들로 푼다(core 의 PlanCourtJobs). g_Lords 는 틱이 채운 사본이다(러너를 부르지 않는다).
	std::vector<Job> MakeJobs(const CourtCommand& Command)
	{
		std::vector<NlCore::CourtLord> lords;
		for (const Lord& lord : g_Lords)
			lords.push_back({ lord.Uuid, lord.King, lord.HasLoyalty == 1 });
		std::vector<Job> jobs;
		for (NlCore::CourtJob& plan : NlCore::PlanCourtJobs(Command, lords))
		{
			Job job;
			job.Plan = std::move(plan);
			jobs.push_back(std::move(job));
		}
		return jobs;
	}

	// 일이 하나도 나오지 않은 까닭.
	std::string NoJobs(const CourtCommand& Command)
	{
		const bool king = std::any_of(g_Lords.begin(), g_Lords.end(), [](const Lord& lord) { return lord.King; });
		if (Command.About == "king" && !king)
			return "플레이어의 영주 가운데 왕을 찾지 못했습니다";
		if (Command.OnlyLoyal)
			return Command.Who == "lords" ? "게임이 충성을 따지는 영주가 없습니다" : "게임이 그 영주에게는 충성을 따지지 않습니다 (또는 그런 영주가 없습니다)";
		return "그런 영주(또는 짝)가 없습니다";
	}

	// ---- 그리는 쪽 (러너를 부르지 않는다) ----

	// 흐린 글. 창의 너비에서 줄을 바꾼다.
	void Hint(const char* Text)
	{
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextDisabled("%s", Text);
		ImGui::PopTextWrapPos();
	}

	// 명령을 일로 풀어 쌓는다. 돌려주는 값: 쌓은 일의 수. 거부하면 -1 이고 Why 에 까닭.
	int PushCommand(const CourtCommand& Command, std::string& Why)
	{
		if (!NlCore::CheckCourt(Command, Why))
			return -1;
		std::vector<Job> jobs = MakeJobs(Command);
		if (jobs.empty())
			return 0;
		if (g_Jobs.empty())
			g_Tally.Reset();		// 앞의 명령이 다 끝났다. 새로 센다
		for (Job& job : jobs)
			g_Jobs.push_back(std::move(job));
		g_Tally.Expect(static_cast<int>(jobs.size()));
		return static_cast<int>(jobs.size());
	}

	// 창의 단추. Loyal: 충성 올리기(게임이 충성을 따지는 영주에게만).
	void Push(const std::string& Who, CourtGoal Goal, const std::string& About, double Amount = 0, bool Loyal = false)
	{
		CourtCommand command{ Who, Goal, About, Amount };
		command.OnlyLoyal = Loyal;
		g_Refused.clear();
		if (PushCommand(command, g_Refused) == 0)
			g_Refused = NoJobs(command);
	}

	std::string NumberText(double Value)
	{
		return std::isfinite(Value) ? NlCore::Shortest(std::round(Value * 10) / 10) : std::string("?");
	}
}

void NlCourt::Init(LogFn Log_)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
}

void NlCourt::GameTick(double Now, bool Active)
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
			if (g_ScanSkipped)
			{
				g_NextRead = Now;		// 인물 쪽이 하는 중에 다시 들어온 틱이다. 값도 일도 그대로 두고 다음 틱에 다시 읽는다
				return;
			}
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
			// 쌓인 일이 다 끝났으면 바뀐 값을 바로 다시 읽는다(일마다 다시 읽지 않는다: 훑기가 영주 수의 제곱만큼 게임을 부른다).
			if (g_Jobs.empty())
				g_NextRead = 0;
		}
	}
}

std::vector<std::string> NlCourt::Do(const CourtCommand& Command)
{
	std::lock_guard lock(g_Mutex);
	std::string why;
	if (!NlCore::CheckCourt(Command, why))
		return { why };
	if (g_Busy)
		return { "busy" };
	const Busy busy;
	if (!Scan())
		return { g_ScanSkipped ? std::string("busy") : g_Why };

	// 제 결과는 따로 센다(창이 쌓아 둔 일들의 셈과 섞지 않는다).
	NlCore::DiplomacyTally tally;
	std::vector<Job> jobs = MakeJobs(Command);
	if (jobs.empty())
		return { NoJobs(Command) };
	tally.Expect(static_cast<int>(jobs.size()));
	for (Job& job : jobs)
		RunJob(job, NlCore::k_CourtClearMax + 2, tally);		// 끝까지: 한도만큼 걷고 한 걸음 더 보면 끝난다
	Scan();
	std::vector<std::string> lines = tally.Lines();
	lines.push_back(tally.Summary());
	return lines;
}

std::string NlCourt::Queue(const CourtCommand& Command)
{
	std::lock_guard lock(g_Mutex);
	if (!g_Ready)
		return (g_Why.empty() ? std::string("영주들을 아직 읽지 않았습니다") : "마지막으로 읽지 못한 까닭: " + g_Why) + " (영주 패널을 열거나 court list 를 먼저)";
	std::string why;
	const int count = PushCommand(Command, why);
	if (count < 0)
		return why;
	return count == 0 ? NoJobs(Command) : "일 " + std::to_string(count) + "개를 쌓았습니다 (틱이 합니다)";
}

std::vector<std::string> NlCourt::List()
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const Busy busy;
	if (!Scan())
		return { g_ScanSkipped ? std::string("busy") : g_Why };
	std::vector<std::string> lines;
	for (const Lord& lord : g_Lords)
		lines.push_back(lord.Uuid + "  " + lord.Name + (lord.King ? "  king" : "") + "  loyalty " + NumberText(lord.Loyalty) + "  state " + NlCore::Shortest(lord.State)
			+ " (" + NlCore::LoyaltyLabel(lord.State) + ")  has_loyalty " + (lord.HasLoyalty < 0 ? "?" : lord.HasLoyalty ? "1" : "0") + "  followers "
			+ std::to_string(lord.Followers));
	for (const Lord& lord : g_Lords)
	{
		std::string line = "  " + lord.Name + " ->";
		for (size_t i = 0; i < g_Lords.size() && i < lord.Opinions.size(); i++)
			if (g_Lords[i].Uuid != lord.Uuid)
				line += "  " + g_Lords[i].Name + " " + NumberText(lord.Opinions[i]);
		lines.push_back(line);
	}
	lines.push_back("(" + std::to_string(g_Lords.size()) + " lords, " + std::to_string(g_Followers.size()) + " followers)");
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

void NlCourt::Draw()
{
	std::lock_guard lock(g_Mutex);
	ImGui::SeparatorText("호감과 충성");
	if (!g_Ready)
	{
		ImGui::TextDisabled("%s", g_Why.empty() ? "게임을 시작하면 영주들이 보입니다." : g_Why.c_str());
		if (!g_Tally.Empty())
			ImGui::TextDisabled("%s", g_Tally.Summary().c_str());
		return;
	}

	if (ImGui::Button("모든 영주의 충성을 100으로"))
		Push("lords", CourtGoal::Raise, "king", 0, true);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("게임이 충성을 따지는 영주만 (왕과, 충성 칸이 '-'인 사람은 빼고)");
	ImGui::SameLine();
	if (ImGui::Button("모든 영주가 서로를 100까지 좋아하게"))
		Push("lords", CourtGoal::Raise, "lords");
	if (ImGui::Button("붙인 평판 모두 떼기"))
		Push("lords", CourtGoal::Clear, "lords");
	ImGui::SameLine();
	if (ImGui::Button("따르는 사람들의 충성 대상 모두 지우기"))
		Push("lords", CourtGoal::Release, "");
	// 긴 설명은 접어 둔다(펼쳐 두면 영주의 표가 창 아래로 밀린다).
	if (ImGui::CollapsingHeader("설명##court"))
		Hint("게임의 디버그용 평판을 영주에게 붙이거나 떼어 서로를 보는 평판을 움직입니다. 왕에 대한 충성은 왕을 보는 평판입니다. "
			"영주가 다른 영주를 보는 평판은 게임이 여러 평판의 합으로 셈합니다. 여기서는 게임의 디버그용 평판(좋은 것 +5, 나쁜 것 -5. 사람에 따라 +6 인 것도 봤습니다)을 "
			"게임의 함수로 하나씩 붙이고 뗍니다. 올릴 때 나쁜 것이 붙어 있으면 그것부터 뗍니다(좋은 것과 나쁜 것을 함께 두지 않습니다). 걸음마다 게임이 세어 준 수로 붙었는지 확인합니다. "
			"올리고 내리는 일은 한 번에 한 짝에 40걸음까지 하고(떼기는 다 뗄 때까지), 같은 평판은 50개까지만 겹칩니다. "
			"충성 칸의 '낮음·보통·높음'은 게임의 충성 상태의 수(0, 1, 2)에 이 모드가 붙인 이름입니다: 평판이 오르면 수도 올랐습니다(55 에서 2, -19 에서 0 을 봤습니다). "
			"'-'는 게임이 그 사람에게 충성을 따지지 않는다는 뜻이고(왕과 아이가 그랬습니다), '?'는 읽지 못했다는 뜻입니다. "
			"'붙인 평판 모두 떼기'는 여기서 붙인 디버그 평판만 뗍니다(게임이 붙인 평판은 건드리지 않습니다). 붙인 평판이 세이브에 남는지는 확인 전입니다. "
			"'충성 대상 지우기'는 그 영주를 충성 대상으로 삼은 주민·병사의 충성 대상을 게임의 함수로 비웁니다(한 명에게 불러 비워지는 것을 봤습니다. 반란에 어떻게 먹는지는 확인 전입니다).");
	if (!g_Refused.empty())
		Hint(g_Refused.c_str());
	if (!g_Tally.Empty())
	{
		ImGui::TextDisabled("%s", g_Tally.Summary().c_str());
		// 실패한 줄이 앞에 온다(core 의 DiplomacyTally). 창은 앞의 몇 줄만 보인다.
		const std::vector<std::string>& lines = g_Tally.Lines();
		for (size_t i = 0; i < lines.size() && i < k_PanelLines; i++)
			Hint(lines[i].c_str());
		if (lines.size() > k_PanelLines && ImGui::CollapsingHeader(("결과 " + std::to_string(lines.size() - k_PanelLines) + "줄 더 (안 된 것이 먼저)###court_more").c_str()))
		{
			for (size_t i = k_PanelLines; i < lines.size() && i < k_PanelLines + k_PanelMore; i++)
				Hint(lines[i].c_str());
			if (lines.size() > k_PanelLines + k_PanelMore)
				ImGui::TextDisabled("(그 밖에 %d줄은 적지 않았습니다)", static_cast<int>(lines.size() - k_PanelLines - k_PanelMore));
		}
	}

	if (ImGui::BeginTable("court_lords", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("영주");
		ImGui::TableSetupColumn("왕에 대한 충성");
		ImGui::TableSetupColumn("따르는 사람");
		ImGui::TableSetupColumn("");
		ImGui::TableHeadersRow();
		for (size_t i = 0; i < g_Lords.size(); i++)
		{
			const Lord& lord = g_Lords[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted((lord.Name + (lord.King ? " (왕)" : "")).c_str());
			ImGui::TableNextColumn();
			if (lord.HasLoyalty == 1)
				ImGui::Text("%s (%s)", NumberText(lord.Loyalty).c_str(), NlCore::LoyaltyLabel(lord.State));
			else
			{
				ImGui::TextDisabled(lord.HasLoyalty == 0 ? "-" : "?");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", lord.HasLoyalty == 0 ? "게임이 이 사람에게는 충성을 따지지 않습니다" : "충성을 읽지 못했습니다");
			}
			ImGui::TableNextColumn();
			ImGui::Text("%d", lord.Followers);
			ImGui::TableNextColumn();
			ImGui::PushID(static_cast<int>(i));
			ImGui::BeginDisabled(lord.HasLoyalty != 1);		// 왕과, 게임이 충성을 따지지 않는 사람에게는 끈다
			if (ImGui::SmallButton("충성 100"))
				Push(lord.Uuid, CourtGoal::Raise, "king", 0, true);
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(lord.Followers == 0);
			if (ImGui::SmallButton("충성 대상 지우기"))
				Push(lord.Uuid, CourtGoal::Release, "");
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("이 영주를 충성 대상으로 삼은 사람들의 충성 대상을 비웁니다");
			ImGui::EndDisabled();
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	// 서로를 보는 평판: 줄의 영주가 칸의 영주를 보는 평판. 칸을 누르면 그 짝을 고른다.
	ImGui::TextUnformatted("서로를 보는 평판 (줄의 영주가 칸의 영주를)");
	const int columns = static_cast<int>(g_Lords.size()) + 1;
	if (columns > 1 && columns <= 64 && ImGui::BeginTable("court_opinions", columns, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("");
		for (const Lord& lord : g_Lords)
			ImGui::TableSetupColumn(lord.Name.c_str());
		ImGui::TableHeadersRow();
		for (size_t i = 0; i < g_Lords.size(); i++)
		{
			const Lord& holder = g_Lords[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(holder.Name.c_str());
			for (size_t k = 0; k < g_Lords.size(); k++)
			{
				ImGui::TableNextColumn();
				if (k == i || k >= holder.Opinions.size())
				{
					ImGui::TextDisabled("-");
					continue;
				}
				ImGui::PushID(static_cast<int>(i * 64 + k));
				const bool picked = g_PickHolder == holder.Uuid && g_PickAbout == g_Lords[k].Uuid;
				if (ImGui::Selectable(NumberText(holder.Opinions[k]).c_str(), picked))
				{
					g_PickHolder = holder.Uuid;
					g_PickAbout = g_Lords[k].Uuid;
				}
				ImGui::PopID();
			}
		}
		ImGui::EndTable();
	}

	const Lord* holder = FindLord(g_PickHolder);
	const Lord* about = FindLord(g_PickAbout);
	if (!holder || !about)
	{
		Hint("표의 칸을 누르면 그 짝의 평판을 올리고 내릴 수 있습니다.");
		return;
	}
	size_t at = 0;
	while (at < g_Lords.size() && g_Lords[at].Uuid != about->Uuid)
		at++;
	ImGui::Text("%s -> %s: 평판 %s", holder->Name.c_str(), about->Name.c_str(), at < holder->Opinions.size() ? NumberText(holder->Opinions[at]).c_str() : "?");
	if (ImGui::SmallButton("+1개"))
		Push(holder->Uuid, CourtGoal::Opinion, about->Uuid, 1);
	ImGui::SameLine();
	if (ImGui::SmallButton("+5개"))
		Push(holder->Uuid, CourtGoal::Opinion, about->Uuid, 5);
	ImGui::SameLine();
	if (ImGui::SmallButton("-1개"))
		Push(holder->Uuid, CourtGoal::Opinion, about->Uuid, -1);
	ImGui::SameLine();
	if (ImGui::SmallButton("-5개"))
		Push(holder->Uuid, CourtGoal::Opinion, about->Uuid, -5);
	ImGui::SameLine();
	if (ImGui::SmallButton("100까지"))
		Push(holder->Uuid, CourtGoal::Raise, about->Uuid);
	ImGui::SameLine();
	if (ImGui::SmallButton("붙인 것 떼기"))
		Push(holder->Uuid, CourtGoal::Clear, about->Uuid);
	Hint("'+1개'는 좋은 평판 하나를 붙이고(나쁜 것이 붙어 있으면 그것 하나를 떼고), '-1개'는 그 반대입니다.");
}
