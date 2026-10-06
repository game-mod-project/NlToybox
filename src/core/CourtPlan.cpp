#include "CourtPlan.hpp"

#include "DiplomacyPlan.hpp"
#include "Text.hpp"

#include <cmath>

namespace NlCore
{
	namespace
	{
		bool IsUuid(const std::string& Text)
		{
			if (Text.size() != 16)
				return false;
			for (const char c : Text)
				if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
					return false;
			return true;
		}

		// 한 것을 글로: "나쁜 평판 2개 뗌, 좋은 평판 13개 붙임". 뗀 것을 먼저 적는다(걸음의 차례가 그렇다). 한 것이 없으면 빈 글.
		std::string DoneText(const CourtDone& Done)
		{
			std::string text;
			const auto part = [&](int Count, const char* What, const char* Verb) {
				if (Count <= 0)
					return;
				if (!text.empty())
					text += ", ";
				text += std::string(What) + " " + std::to_string(Count) + "개 " + Verb;
			};
			part(Done.GoodOff, "좋은 평판", "뗌");
			part(Done.BadOff, "나쁜 평판", "뗌");
			part(Done.GoodOn, "좋은 평판", "붙임");
			part(Done.BadOn, "나쁜 평판", "붙임");
			return text;
		}

		std::string ChangeText(double Before, double After)
		{
			if (!std::isfinite(Before) || Before == After)
				return "평판 " + Shortest(After);
			return "평판 " + Shortest(Before) + " -> " + Shortest(After);
		}
	}

	const char* LoyaltyLabel(double State)
	{
		if (State == 0)
			return "낮음";
		if (State == 1)
			return "보통";
		if (State == 2)
			return "높음";
		return "?";
	}

	bool ParseCourtGoal(const std::string& Word, CourtGoal& Out, bool& AboutKing)
	{
		AboutKing = false;
		if (Word == "loyal")
		{
			Out = CourtGoal::Raise;
			AboutKing = true;
		}
		else if (Word == "like")
			Out = CourtGoal::Raise;
		else if (Word == "opinion")
			Out = CourtGoal::Opinion;
		else if (Word == "clear")
			Out = CourtGoal::Clear;
		else if (Word == "release")
			Out = CourtGoal::Release;
		else
			return false;
		return true;
	}

	const char* CourtGoalWord(CourtGoal Goal)
	{
		switch (Goal)
		{
		case CourtGoal::Raise: return "raise";
		case CourtGoal::Opinion: return "opinion";
		case CourtGoal::Clear: return "clear";
		case CourtGoal::Release: return "release";
		}
		return "?";
	}

	bool GoodCourtWho(const std::string& Who)
	{
		return Who == "lords" || IsUuid(Who);
	}

	bool GoodCourtAbout(const std::string& About)
	{
		return About == "king" || About == "lords" || IsUuid(About);
	}

	bool CheckCourt(const CourtCommand& Command, std::string& Why)
	{
		Why.clear();
		if (!GoodCourtWho(Command.Who))
		{
			Why = "누구의 평판인지 모르겠습니다 (lords 또는 영주의 uuid)";
			return false;
		}
		if (Command.Goal == CourtGoal::Release)
		{
			if (!Command.About.empty() || Command.Amount != 0)
				Why = "충성 대상 지우기는 대상도 수도 받지 않습니다";
			return Why.empty();
		}
		if (!GoodCourtAbout(Command.About))
		{
			Why = "누구를 보는 평판인지 모르겠습니다 (king, lords 또는 영주의 uuid)";
			return false;
		}
		if (Command.Who == Command.About && Command.Who != "lords")
		{
			Why = "자기 자신을 보는 평판은 없습니다";
			return false;
		}
		if (Command.OnlyLoyal && (Command.Goal != CourtGoal::Raise || Command.About != "king"))
		{
			Why = "충성 올리기는 왕을 보는 평판을 올리는 일에만 씁니다";
			return false;
		}
		switch (Command.Goal)
		{
		case CourtGoal::Raise:
			if (Command.Amount != 0 && (!std::isfinite(Command.Amount) || Command.Amount != std::floor(Command.Amount) || Command.Amount < 1
				|| Command.Amount > k_CourtRaiseMax))
				Why = "목표는 1 에서 200 사이의 정수입니다";
			break;
		case CourtGoal::Opinion:
			// 한 짝씩만 한다: 여럿을 한꺼번에 내리는 명령을 받지 않는다(여럿에게는 올리기와 떼기만. 창에도 짝 하나의 단추만 있다).
			if (Command.Who == "lords" || Command.About == "lords")
				Why = "평판의 개수는 한 짝씩만 움직입니다 (영주의 uuid 와, 대상의 uuid 또는 king)";
			else if (OpinionSteps(Command.Amount) == 0)
				Why = "평판의 개수는 0 이 아닌 -40 에서 40 사이의 정수입니다";
			break;
		case CourtGoal::Clear:
			if (Command.Amount != 0)
				Why = "떼기는 수를 받지 않습니다";
			break;
		default:
			break;
		}
		return Why.empty();
	}

	std::vector<CourtJob> PlanCourtJobs(const CourtCommand& Command, const std::vector<CourtLord>& Lords)
	{
		std::vector<CourtJob> jobs;
		std::string why;
		if (!CheckCourt(Command, why))
			return jobs;

		std::vector<std::string> holders, abouts;
		for (const CourtLord& lord : Lords)
		{
			// 충성 올리기는 게임이 충성을 따지는 영주에게만 한다(아이와 왕은 뺀다).
			if ((Command.Who == "lords" || Command.Who == lord.Uuid) && (!Command.OnlyLoyal || lord.HasLoyalty))
				holders.push_back(lord.Uuid);
			if (Command.About == "lords" || Command.About == lord.Uuid || (Command.About == "king" && lord.King))
				abouts.push_back(lord.Uuid);
		}

		CourtJob job;
		job.Goal = Command.Goal;
		switch (Command.Goal)
		{
		case CourtGoal::Raise:
			job.Left = k_OpinionStepsMax;
			job.Sign = 1;
			job.Target = Command.Amount == 0 ? k_CourtRaiseTo : Command.Amount;
			break;
		case CourtGoal::Opinion:
		{
			const int steps = OpinionSteps(Command.Amount);
			job.Left = steps < 0 ? -steps : steps;
			job.Sign = steps < 0 ? -1 : 1;
			break;
		}
		case CourtGoal::Clear:
			job.Left = k_CourtClearMax;
			break;
		case CourtGoal::Release:
			for (const std::string& holder : holders)
			{
				job.Holder = holder;
				jobs.push_back(job);
			}
			return jobs;
		}
		for (const std::string& holder : holders)
			for (const std::string& about : abouts)
			{
				if (holder == about)
					continue;
				job.Holder = holder;
				job.About = about;
				jobs.push_back(job);
			}
		return jobs;
	}

	CourtStep PlanCourtStep(const CourtJob& Job, double Opinion, double Good, double Bad, int Done, double StackLimit)
	{
		// 읽지 못한 수로는 걷지 않는다(NaN 은 어느 견줌에서도 거짓이다).
		if (!std::isfinite(Opinion) || !(Good >= 0) || !(Bad >= 0) || !(StackLimit >= 1) || !std::isfinite(Good) || !std::isfinite(Bad))
			return { 'f', 0 };
		switch (Job.Goal)
		{
		case CourtGoal::Raise:
			if (Opinion >= Job.Target)
				return { Done == 0 ? 'a' : 'd', 0 };
			if (Job.Left <= 0)
				return { 'l', 0 };
			if (Bad > 0)
				return { 0, 'd' };
			if (Good >= StackLimit)
				return { 's', 0 };
			return { 0, 'A' };
		case CourtGoal::Opinion:
			if (Job.Sign == 0)
				return { 'f', 0 };
			if (Job.Left <= 0)
				return { 'd', 0 };
			if (Job.Sign > 0)
			{
				if (Bad > 0)
					return { 0, 'd' };
				return Good >= StackLimit ? CourtStep{ 's', 0 } : CourtStep{ 0, 'A' };
			}
			if (Good > 0)
				return { 0, 'D' };
			return Bad >= StackLimit ? CourtStep{ 's', 0 } : CourtStep{ 0, 'a' };
		case CourtGoal::Clear:
			if (Good <= 0 && Bad <= 0)
				return { Done == 0 ? 'a' : 'd', 0 };
			if (Job.Left <= 0)
				return { 'l', 0 };
			return { 0, Good > 0 ? 'D' : 'd' };
		case CourtGoal::Release:
			break;
		}
		return { 'f', 0 };
	}

	bool CourtStepDone(char Action, double GoodBefore, double BadBefore, double GoodAfter, double BadAfter)
	{
		if (!(GoodBefore >= 0) || !(BadBefore >= 0) || !(GoodAfter >= 0) || !(BadAfter >= 0))
			return false;
		switch (Action)
		{
		case 'A': return GoodAfter == GoodBefore + 1 && BadAfter == BadBefore;
		case 'D': return GoodAfter == GoodBefore - 1 && BadAfter == BadBefore;
		case 'a': return BadAfter == BadBefore + 1 && GoodAfter == GoodBefore;
		case 'd': return BadAfter == BadBefore - 1 && GoodAfter == GoodBefore;
		}
		return false;
	}

	void CountCourtStep(CourtDone& Done, char Action)
	{
		switch (Action)
		{
		case 'A': Done.GoodOn++; break;
		case 'D': Done.GoodOff++; break;
		case 'a': Done.BadOn++; break;
		case 'd': Done.BadOff++; break;
		}
	}

	std::string CourtReport(const std::string& Holder, const std::string& About, const CourtJob& Job, double Before, double After, const CourtDone& Done,
		char Outcome, const std::string& Why)
	{
		const std::string head = Holder + " -> " + About + ": ";
		const std::string done = DoneText(Done);
		const std::string moved = ChangeText(Before, After) + (done.empty() ? "" : " (" + done + ")");
		switch (Outcome)
		{
		case 'd':
			return head + moved;
		case 'a':
			if (Job.Goal == CourtGoal::Raise)
				return head + "평판 " + Shortest(After) + ". 이미 목표(" + Shortest(Job.Target) + ") 이상입니다";
			if (Job.Goal == CourtGoal::Clear)
				return head + "붙여 둔 디버그 평판이 없습니다 (평판 " + Shortest(After) + ")";
			return head + "할 것이 없습니다";
		case 'l':
			return head + moved + ". 한 번의 한도까지 했지만 "
				+ (Job.Goal == CourtGoal::Raise ? "목표(" + Shortest(Job.Target) + ")에 닿지 않았습니다" : std::string("다 하지 못했습니다"));
		case 's':
			return head + moved + ". 같은 평판의 겹침 한도에 닿아 더 붙일 수 없습니다";
		default:
			return head + "하지 못했습니다" + (Why.empty() ? "" : " (" + Why + ")") + (Done.Total() > 0 ? ". 그 전까지: " + moved : "");
		}
	}

	char ReleaseOutcome(int Followers, int Released)
	{
		if (Followers <= 0)
			return 'a';
		return Released >= Followers ? 'd' : 'f';
	}

	std::string ReleaseReport(const std::string& Lord, int Followers, int Released, const std::string& Why)
	{
		switch (ReleaseOutcome(Followers, Released))
		{
		case 'a':
			return Lord + ": 따르는 사람이 없습니다";
		case 'd':
			return Lord + ": 따르던 " + std::to_string(Followers) + "명의 충성 대상을 지웠습니다";
		default:
			return Lord + ": 따르던 " + std::to_string(Followers) + "명 가운데 " + std::to_string(Released < 0 ? 0 : Released) + "명의 충성 대상만 지웠습니다"
				+ (Why.empty() ? "" : " (" + Why + ")");
		}
	}
}
