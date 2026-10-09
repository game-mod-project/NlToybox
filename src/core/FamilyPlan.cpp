#include "FamilyPlan.hpp"

#include "Text.hpp"

#include <algorithm>

namespace NlCore
{
	const char* PregnancyTrait(int Stage)
	{
		return Stage == 1 ? "pregnant_st1" : Stage == 2 ? "pregnant_st2" : Stage == 3 ? "pregnant_st3" : "";
	}

	int PregnancyStage(const std::vector<std::string>& Traits)
	{
		for (int stage = 3; stage >= 1; stage--)
			if (Has(Traits, PregnancyTrait(stage)))
				return stage;
		return 0;
	}

	bool IsKid(const std::vector<std::string>& Traits)
	{
		return Has(Traits, "kid");
	}

	StageOutcome AfterStageCall(int Before, int After)
	{
		if (Before == 3 && After == 0)
			return StageOutcome::Born;
		if ((Before == 1 || Before == 2) && After == Before + 1)
			return StageOutcome::Advanced;
		return StageOutcome::Stuck;
	}

	bool BirthNeedsCall(int Stage, int Calls)
	{
		return Stage >= 1 && Stage <= 3 && Calls >= 0 && Calls < k_StageCallsMax;
	}

	bool BirthDone(int Last, int Children, const std::string& Why)
	{
		return Last == 0 && Children > 0 && Why.empty();
	}

	bool NextDone(int First, int Last, int Calls, const std::string& Why)
	{
		return First > 0 && Calls == 1 && Why.empty() && AfterStageCall(First, Last) != StageOutcome::Stuck;
	}

	std::string StageReport(const std::string& Name, bool Birth, int First, int Last, int Calls, const std::string& Why, int Children)
	{
		const auto stage = [](int value) { return "임신 " + std::to_string(value) + "/3기"; };
		const std::string called = "다음 단계 함수를 " + std::to_string(Calls) + "번 불렀";
		if (First <= 0)
			return Name + ": 임신 중이 아닙니다";
		if (Last < 0)		// 부른 뒤 그 사람을 다시 읽지 못했다. 뒤의 단계를 모른다
			return Name + ": " + called + "지만 그 뒤를 읽지 못했습니다" + (Why.empty() ? "" : " (" + Why + ")");
		if (!Why.empty())
			return Name + ": " + (Last > 0 ? stage(Last) + "에서 멈췄습니다" : std::string("임신이 끝났습니다")) + " (" + Why + ")";
		if (Last == 0)
			return Name + (Children > 0 ? ": 출산했습니다 (" : ": 임신이 끝났지만 아이가 생기지 않았습니다 (유산으로 보입니다. ") + called + "습니다)";
		if (Last == First)
			return Name + ": 단계가 바뀌지 않았습니다 (" + stage(Last) + " 그대로)";
		if (Birth)
			return Name + ": 출산까지 가지 못했습니다 (" + stage(Last) + "에서 단계가 더 바뀌지 않았습니다)";
		return Name + ": " + stage(Last) + "가 됐습니다";
	}

	bool CanConceive(double Gender, const std::vector<std::string>& Traits, std::string& Why)
	{
		Why.clear();
		if (Gender != k_Female)
			Why = Gender == k_Male ? "남성입니다" : "성별을 읽지 못했습니다";
		else if (IsKid(Traits))
			Why = "아이입니다";
		else if (PregnancyStage(Traits) > 0)
			Why = "이미 임신 중입니다";
		else if (Has(Traits, k_PregnantForbid))
			Why = "출산 뒤의 임신 금지가 붙어 있습니다";
		return Why.empty();
	}

	bool CanFather(double Gender, const std::vector<std::string>& Traits)
	{
		return Gender == k_Male && !IsKid(Traits);
	}

	const std::vector<const char*>& PregnancyChanceVars()
	{
		static const std::vector<const char*> keys = { "pregnancy_chance", "pregnancy_from_dummy_chance" };
		return keys;
	}

	const std::vector<const char*>& MiscarriageVars()
	{
		static const std::vector<const char*> keys = { "pregnancy_miscarriage_chance" };
		return keys;
	}

	const std::vector<const char*>& ChildbirthDeathVars()
	{
		static const std::vector<const char*> keys = { "pregnancy_mother_die", "trait_death_in_childbirth" };
		return keys;
	}
}
