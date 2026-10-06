#include "DiplomacyPlan.hpp"

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
		if (!std::isfinite(Amount))
			return 0;
		const double steps = std::round(Amount / k_OpinionUnit);
		if (steps > k_OpinionStepsMax)
			return k_OpinionStepsMax;
		if (steps < -k_OpinionStepsMax)
			return -k_OpinionStepsMax;
		return static_cast<int>(steps);
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
			Why = "평판의 변화량이 없습니다 (5 의 배수로 움직입니다)";
		return Why.empty();
	}

	bool DiplomacyFailed(char Outcome)
	{
		return Outcome == 'l' || Outcome == 'f';
	}

	std::string DiplomacyReport(const std::string& Name, char Side, double Before, double After, int Steps, char Outcome, const std::string& Why)
	{
		const std::string who = Name + (Side == 't' ? ": 그쪽이 우리를 " : ": 우리가 그쪽을 ");
		const std::string moved = Steps == 0 ? std::string()
			: " (평판 " + std::string(Steps > 0 ? "+" : "-") + std::to_string(k_OpinionUnit) + " 를 " + std::to_string(Steps > 0 ? Steps : -Steps) + "번)";
		const std::string change = std::string(RelationLabel(Before)) + " -> " + RelationLabel(After) + moved;
		switch (Outcome)
		{
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
