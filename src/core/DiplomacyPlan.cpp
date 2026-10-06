#include "DiplomacyPlan.hpp"

#include <algorithm>
#include <cmath>

namespace NlCore
{
	namespace
	{
		const struct
		{
			DiplomacyGoal Goal;
			const char* Word;
			const char* Label;
		} k_Goals[] = {
			{ DiplomacyGoal::Friends, "friends", "우호" }, { DiplomacyGoal::Neutral, "neutral", "중립" },
			{ DiplomacyGoal::Hostile, "hostile", "적대" }, { DiplomacyGoal::Opinion, "opinion", "평판" },
			{ DiplomacyGoal::Pact, "pact", "협정" },
		};

		const struct
		{
			DiplomacyPact Pact;
			const char* Word;
			const char* Label;
			const char* Short;
			int Bits;
		} k_Pacts[] = {
			{ DiplomacyPact::Peace, "peace", "평화 협정", "평화", 4 }, { DiplomacyPact::Trade, "trade", "교역 협정", "교역", 8 },
			{ DiplomacyPact::Defence, "defence", "방어 동맹", "방어 동맹", 192 },
		};

		// 협정의 칸: 0 이상의 정수여야 비트로 읽는다. 아니면 -1.
		int WholeCell(double Cell)
		{
			return std::isfinite(Cell) && Cell == std::floor(Cell) && Cell >= 0 && Cell < 2147483648.0 ? static_cast<int>(Cell) : -1;
		}

		// 정수인 종류만 본다. 아니면 -1.
		int WholeKind(double Kind)
		{
			return std::isfinite(Kind) && Kind == std::floor(Kind) && Kind >= 0 && Kind <= 100 ? static_cast<int>(Kind) : -1;
		}
	}

	const char* RelationLabel(double Kind)
	{
		switch (WholeKind(Kind))
		{
		case k_RelationAllies: return "동맹";
		case k_RelationEnemies: return "적";
		case k_RelationDeadly: return "철천지원수";
		case k_RelationNeutrals: return "중립";
		case k_RelationFriends: return "우호";
		case k_RelationVassal: return "봉신";
		case k_RelationSir: return "주군";
		case k_RelationOpponent: return "대립";
		default: return "?";
		}
	}

	bool IsKingdom(const std::string& SystemName)
	{
		static const std::string prefix = "faction.new.name.";
		if (SystemName.size() <= prefix.size() || SystemName.compare(0, prefix.size(), prefix) != 0)
			return false;
		for (size_t i = prefix.size(); i < SystemName.size(); i++)
			if (SystemName[i] < '0' || SystemName[i] > '9')
				return false;
		return true;
	}

	bool ParseDiplomacyGoal(const std::string& Word, DiplomacyGoal& Out)
	{
		for (const auto& goal : k_Goals)
			if (Word == goal.Word)
			{
				Out = goal.Goal;
				return true;
			}
		return false;
	}

	const char* DiplomacyGoalWord(DiplomacyGoal Goal)
	{
		for (const auto& goal : k_Goals)
			if (goal.Goal == Goal)
				return goal.Word;
		return "";
	}

	const char* DiplomacyGoalLabel(DiplomacyGoal Goal)
	{
		for (const auto& goal : k_Goals)
			if (goal.Goal == Goal)
				return goal.Label;
		return "";
	}

	bool ParseDiplomacyPact(const std::string& Word, DiplomacyPact& Out)
	{
		for (const auto& pact : k_Pacts)
			if (Word == pact.Word)
			{
				Out = pact.Pact;
				return true;
			}
		return false;
	}

	const char* DiplomacyPactWord(DiplomacyPact Pact)
	{
		for (const auto& pact : k_Pacts)
			if (pact.Pact == Pact)
				return pact.Word;
		return "";
	}

	const char* DiplomacyPactLabel(DiplomacyPact Pact)
	{
		for (const auto& pact : k_Pacts)
			if (pact.Pact == Pact)
				return pact.Label;
		return "";
	}

	int PactBits(DiplomacyPact Pact)
	{
		for (const auto& pact : k_Pacts)
			if (pact.Pact == Pact)
				return pact.Bits;
		return 0;
	}

	bool HasPact(double Cell, DiplomacyPact Pact)
	{
		const int cell = WholeCell(Cell), bits = PactBits(Pact);
		return cell >= 0 && bits != 0 && (cell & bits) == bits;
	}

	double PactCell(double Cell, DiplomacyPact Pact)
	{
		const int cell = WholeCell(Cell);
		return static_cast<double>((cell < 0 ? 0 : cell) | PactBits(Pact));
	}

	std::string PactText(double Cell)
	{
		const int cell = WholeCell(Cell);
		if (cell <= 0)
			return "-";
		std::string text;
		for (const auto& pact : k_Pacts)
			if ((cell & pact.Bits) == pact.Bits)
				text += (text.empty() ? "" : ", ") + std::string(pact.Short);
		return text.empty() ? "?" : text;
	}

	std::string PactReport(const std::string& Name, DiplomacyPact Pact, char Outcome, const std::string& Why)
	{
		const std::string label = DiplomacyPactLabel(Pact);		// 셋 모두 받침으로 끝난다("협정을", "동맹을")
		switch (Outcome)
		{
		case 'd':
			return Name + ": " + label + "을 맺었습니다";
		case 'a':
			return Name + ": 이미 " + label + "이 있습니다";
		default:
			return Name + ": " + label + "을 맺지 못했습니다 (" + (Why.empty() ? "모릅니다" : Why) + ")";
		}
	}

	bool ParseDiplomacySide(const std::string& Word, char& Out)
	{
		if (Word == "them")
			Out = 't';
		else if (Word == "us")
			Out = 'u';
		else if (Word == "both")
			Out = 'b';
		else
			return false;
		return true;
	}

	int StepToward(double Kind, DiplomacyGoal Goal)
	{
		const int kind = WholeKind(Kind);
		const bool bad = kind == k_RelationEnemies || kind == k_RelationDeadly || kind == k_RelationOpponent;
		if (!bad && kind != k_RelationNeutrals && kind != k_RelationFriends)
			return 2;			// 동맹, 봉신, 주군, 모르는 수
		switch (Goal)
		{
		case DiplomacyGoal::Friends:
			return kind == k_RelationFriends ? 0 : 1;
		case DiplomacyGoal::Neutral:
			return kind == k_RelationNeutrals ? 0 : bad ? 1 : -1;
		case DiplomacyGoal::Hostile:
			return kind == k_RelationDeadly ? 0 : -1;
		default:
			return 2;			// 평판의 수는 관계를 보지 않는다
		}
	}

	int OpinionSteps(double Amount)
	{
		if (!std::isfinite(Amount) || Amount != std::floor(Amount) || Amount == 0 || std::fabs(Amount) > k_OpinionStepsMax)
			return 0;
		return static_cast<int>(Amount);
	}

	DiplomacyStep PlanStep(double Kind, DiplomacyGoal Goal, int Sign, int Left, int Done, int Good, int Bad)
	{
		DiplomacyStep step;
		const int kind = WholeKind(Kind);
		const bool movable = kind == k_RelationEnemies || kind == k_RelationDeadly || kind == k_RelationOpponent || kind == k_RelationNeutrals
			|| kind == k_RelationFriends;
		if (!movable || Goal == DiplomacyGoal::Pact)
		{
			step.Outcome = 'k';
			return step;
		}
		int direction = 0;
		if (Goal == DiplomacyGoal::Opinion)
		{
			if (Sign == 0)
			{
				step.Outcome = 'k';
				return step;
			}
			if (Left <= 0)
			{
				step.Outcome = 'd';
				return step;
			}
			direction = Sign > 0 ? 1 : -1;
		}
		else
		{
			direction = StepToward(Kind, Goal);
			if (direction == 0)
			{
				step.Outcome = Done == 0 ? 'a' : 'd';
				return step;
			}
			if (direction != 1 && direction != -1)
			{
				step.Outcome = 'k';
				return step;
			}
			if (Left <= 0)
			{
				step.Outcome = 'l';
				return step;
			}
		}
		// 같은 평판의 겹침 한도까지 가지 않는다.
		if ((direction > 0 ? Good : Bad) >= k_OpinionStackLimit)
		{
			step.Outcome = 'l';
			return step;
		}
		step.Direction = direction;
		return step;
	}

	bool GoodFactionWho(const std::string& Who)
	{
		if (Who == "all")
			return true;
		if (Who.size() != 16)
			return false;
		for (const char c : Who)
			if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
				return false;
		return true;
	}

	bool CheckDiplomacy(const DiplomacyCommand& Command, std::string& Why)
	{
		Why.clear();
		if (!GoodFactionWho(Command.Who))
			Why = "어느 왕국인지 없습니다";
		else if (Command.Side != 't' && Command.Side != 'u' && Command.Side != 'b')
			Why = "누구의 평판인지 모릅니다";
		else if (Command.Who == "all" && Command.Goal == DiplomacyGoal::Pact)
			Why = "협정은 한 왕국씩 맺습니다";
		else if (Command.Who == "all" && (Command.Goal == DiplomacyGoal::Hostile || Command.Goal == DiplomacyGoal::Opinion))
			Why = "모든 왕국에게는 우호와 중립만 합니다 (적대와 평판의 수는 한 왕국씩)";
		else if (Command.Goal == DiplomacyGoal::Opinion && OpinionSteps(Command.Amount) == 0)
			Why = "평판의 수는 -40 에서 40 사이의 0 이 아닌 정수입니다 (붙일 평판의 개수)";
		return Why.empty();
	}

	std::vector<DiplomacyJob> PlanJobs(const DiplomacyCommand& Command, const std::vector<std::string>& Kingdoms)
	{
		std::vector<DiplomacyJob> jobs;
		std::string why;
		if (!CheckDiplomacy(Command, why))
			return jobs;
		const int steps = Command.Goal == DiplomacyGoal::Opinion ? OpinionSteps(Command.Amount) : 0;
		for (const std::string& uuid : Kingdoms)
		{
			if (Command.Who != "all" && Command.Who != uuid)
				continue;
			for (const char side : { 't', 'u' })
			{
				if (Command.Goal == DiplomacyGoal::Pact ? side != 't' : (Command.Side != 'b' && Command.Side != side))
					continue;
				DiplomacyJob job;
				job.Uuid = uuid;
				job.Side = side;
				job.Left = Command.Goal == DiplomacyGoal::Opinion ? (steps > 0 ? steps : -steps) : k_OpinionStepsMax;
				job.Sign = steps > 0 ? 1 : steps < 0 ? -1 : 0;
				jobs.push_back(std::move(job));
			}
		}
		return jobs;
	}

	void DiplomacyTally::Add(char Outcome, const std::string& Line)
	{
		if (DiplomacyFailed(Outcome))
		{
			m_Failed++;
			m_Lines.insert(m_Lines.begin() + static_cast<std::ptrdiff_t>(m_FailedLines), Line);
			m_FailedLines++;
			return;
		}
		(Outcome == 'd' ? m_Changed : m_Same)++;
		m_Lines.push_back(Line);
	}

	void DiplomacyTally::Drop(int Jobs, const std::string& Why)
	{
		if (Jobs <= 0)
			return;
		m_Failed += Jobs;
		m_Lines.insert(m_Lines.begin() + static_cast<std::ptrdiff_t>(m_FailedLines), "하지 못하고 버린 일 " + std::to_string(Jobs) + "개: " + Why);
		m_FailedLines++;
	}

	std::string DiplomacyTally::Summary() const
	{
		std::string text = std::to_string(m_Asked) + "개 가운데 바꾼 것 " + std::to_string(m_Changed) + "개, 그대로 둔 것 " + std::to_string(m_Same)
			+ "개, 안 된 것 " + std::to_string(m_Failed) + "개";
		if (Pending() > 0)
			text += " (남은 일 " + std::to_string(Pending()) + "개)";
		return text;
	}

	bool DiplomacyFailed(char Outcome)
	{
		return Outcome == 'l' || Outcome == 'f';
	}

	std::string DiplomacyReport(const std::string& Name, char Side, double Before, double After, int Steps, char Outcome, const std::string& Why)
	{
		const std::string who = Name + (Side == 't' ? ": 그쪽이 우리를 " : ": 우리가 그쪽을 ");
		// 붙인 것은 개수로 적는다: 하나가 관계를 얼마나 움직이는지는 왕마다 달랐다.
		const std::string moved = Steps == 0 ? std::string()
			: " (" + std::string(Steps > 0 ? "좋은" : "나쁜") + " 평판 " + std::to_string(Steps > 0 ? Steps : -Steps) + "개)";
		const std::string change = std::string(RelationLabel(Before)) + " -> " + RelationLabel(After) + moved;
		switch (Outcome)
		{
		case 'x':
			return Name + ": 망했거나 왕이 없는 왕국입니다. 건드리지 않습니다";
		case 'd':
			return who + change;
		case 'a':
			return who + "이미 " + RelationLabel(Before) + "입니다";
		case 'k':
			return who + RelationLabel(Before) + " 관계는 건드리지 않습니다";
		case 'l':
			return who + change + ". 한도까지 붙였지만 바라는 관계가 되지 않았습니다";
		default:
			return who + change + ". 실패: " + (Why.empty() ? "모릅니다" : Why);
		}
	}
}
