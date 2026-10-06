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
		};

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
