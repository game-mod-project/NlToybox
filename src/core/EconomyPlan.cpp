#include "EconomyPlan.hpp"

#include <algorithm>
#include <cmath>

namespace NlCore
{
	namespace
	{
		struct Named
		{
			const char* Key;
			const char* Label;
		};

		// 키는 global.__resource_caption 에서 봤다(research/07). 한글 이름은 이 레포가 붙였다.
		constexpr Named k_Resources[] = {
			{ "rune", "룬" }, { "wood", "나무" }, { "food", "음식" }, { "beer", "맥주" }, { "iron", "철" }, { "instruments", "도구" },
			{ "light_armor", "경갑" }, { "heavy_armor", "중갑" }, { "bow", "활" }, { "crossbow", "석궁" }, { "wooden_hammer", "나무 망치" },
			{ "wooden_spear", "나무 창" }, { "sword", "검" }, { "battle_axe", "전투 도끼" }, { "knife", "단검" }, { "shield", "방패" },
			{ "medicine", "약" }, { "coal", "석탄" }, { "nectar", "넥타" }, { "paper", "종이" }, { "hop", "홉" }, { "rye", "호밀" },
			{ "steel", "강철" }, { "meat", "고기" }, { "herb", "약초" }, { "sweden", "순무" }, { "moonshine", "밀주" }, { "ale", "에일" },
			{ "carrot", "당근" }, { "stone", "돌" }, { "instruments_steel", "강철 도구" }, { "firewood", "장작" }, { "wood_blanks", "목재" },
			{ "berry", "열매" }, { "herb_drink", "약초 음료" }, { "clay_tile", "점토 타일" }, { "clay_pot", "점토 항아리" },
			{ "clay_roof_tile", "점토 기와" }, { "stone_tile", "석재 타일" },
		};

		// 키는 o_data.__resource_categories_data.__categories 에서 봤다(research/07).
		constexpr Named k_Categories[] = {
			{ "food", "음식" }, { "liquid", "액체" }, { "resources", "물자" }, { "armory", "전쟁 물자" }, { "herbs", "약초·작물" }, { "raw", "원자재" },
		};

		constexpr double k_MaxAmount = 1e9;		// 이보다 큰 수는 잘못 친 것으로 본다

		// 정수로 맞춘 변화량. 줄일 때 지금 수가 0 아래로 내려가지 않게 한다(이미 음수면 더 내리지 않는다).
		double Bounded(double Current, double Delta)
		{
			Delta = std::round(Delta);
			if (Delta < 0 && Current + Delta < 0)
				return std::min(0.0, -Current);
			return Delta;
		}
	}

	std::string ResourceKey(const std::string& Caption)
	{
		static const std::string prefix = "resource.";
		return Caption.compare(0, prefix.size(), prefix) == 0 ? Caption.substr(prefix.size()) : Caption;
	}

	std::string ResourceLabel(const std::string& Key)
	{
		for (const Named& named : k_Resources)
			if (Key == named.Key)
				return std::string(named.Label) + " (" + Key + ")";
		return Key;
	}

	std::string CategoryLabel(const std::string& Key)
	{
		for (const Named& named : k_Categories)
			if (Key == named.Key)
				return named.Label;
		return Key;
	}

	bool ParseEconomyAct(const std::string& Word, EconomyAct& Out)
	{
		static const struct
		{
			const char* Word;
			EconomyAct Act;
		} acts[] = {
			{ "gold_add", EconomyAct::GoldAdd }, { "gold_set", EconomyAct::GoldSet }, { "add", EconomyAct::ResourceAdd },
			{ "set", EconomyAct::ResourceSet }, { "all", EconomyAct::AllAdd },
		};
		for (const auto& act : acts)
			if (Word == act.Word)
			{
				Out = act.Act;
				return true;
			}
		return false;
	}

	bool NeedsResource(EconomyAct Act)
	{
		return Act == EconomyAct::ResourceAdd || Act == EconomyAct::ResourceSet;
	}

	std::vector<EconomyChange> PlanEconomy(const EconomyCommand& Command, double Gold, const std::vector<double>& Counts,
		const std::vector<int>& Stocked)
	{
		std::vector<EconomyChange> changes;
		if (!std::isfinite(Command.Amount) || std::fabs(Command.Amount) > k_MaxAmount)
			return changes;

		const auto add = [&](int resource, double delta) {
			if (delta != 0)
				changes.push_back({ resource, delta });
		};
		const auto valid = [&](int resource) { return resource >= 0 && static_cast<size_t>(resource) < Counts.size(); };
		const double target = std::max(0.0, std::round(Command.Amount));

		switch (Command.Act)
		{
		case EconomyAct::GoldAdd:
			add(-1, Bounded(Gold, Command.Amount));
			break;
		case EconomyAct::GoldSet:
			add(-1, target - Gold);
			break;
		case EconomyAct::ResourceAdd:
			if (valid(Command.Resource))
				add(Command.Resource, Bounded(Counts[Command.Resource], Command.Amount));
			break;
		case EconomyAct::ResourceSet:
			if (valid(Command.Resource))
				add(Command.Resource, target - Counts[Command.Resource]);
			break;
		case EconomyAct::AllAdd:
			for (const int resource : Stocked)
				if (valid(resource))
					add(resource, Bounded(Counts[resource], Command.Amount));
			break;
		}
		return changes;
	}
}
