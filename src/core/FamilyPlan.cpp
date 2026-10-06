#include "FamilyPlan.hpp"

#include <algorithm>

namespace NlCore
{
	namespace
	{
		bool Has(const std::vector<std::string>& List, const char* Name)
		{
			return std::find(List.begin(), List.end(), Name) != List.end();
		}
	}

	int PregnancyStage(const std::vector<std::string>& Traits)
	{
		if (Has(Traits, "pregnant_st3"))
			return 3;
		if (Has(Traits, "pregnant_st2"))
			return 2;
		return Has(Traits, "pregnant_st1") ? 1 : 0;
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

	std::string StageReport(const std::string& Name, int First, int Last, int Calls, const std::string& Why)
	{
		const auto stage = [](int value) { return "임신 " + std::to_string(value) + "/3기"; };
		if (First <= 0)
			return Name + ": 임신 중이 아닙니다";
		if (!Why.empty())
			return Name + ": " + (Last > 0 ? stage(Last) + "에서 멈췄습니다" : std::string("임신이 끝났습니다")) + " (" + Why + ")";
		if (Last == 0)
			return Name + ": 출산했습니다 (다음 단계 함수를 " + std::to_string(Calls) + "번 불렀습니다)";
		if (Last == First)
			return Name + ": 단계가 바뀌지 않았습니다 (" + stage(Last) + " 그대로)";
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

	bool GoodUuid(const std::string& Text)
	{
		return Text.size() == 16 && std::all_of(Text.begin(), Text.end(), [](unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
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
