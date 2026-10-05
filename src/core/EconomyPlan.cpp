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

		// 키는 o_data.__resource_categories_data.__categories 에서 봤다(research/07). 이름은 게임의 화면이 쓰는 말에 맞췄다(research/08 의 화면).
		constexpr Named k_Categories[] = {
			{ "food", "음식" }, { "liquid", "액체" }, { "resources", "자원" }, { "armory", "전쟁 물자" }, { "herbs", "식물" }, { "raw", "원자재" },
		};

		constexpr double k_MaxAmount = 1e9;		// 이보다 큰 수는 잘못 친 것으로 본다

		// 게임의 함수에 넘길 변화량: 유한한 정수. 줄일 때는 줄일 수 있는 양(Free 의 정수 부분)을 넘지 않는다(Free 가 음수면 줄이지 않는다).
		// 읽은 수가 수가 아니거나 변화량이 터무니없이 크면 0(하지 않는다).
		double Settle(double Free, double Delta)
		{
			if (!std::isfinite(Free) || !std::isfinite(Delta))
				return 0;
			Delta = std::round(Delta);
			if (Delta < 0)
				Delta = std::max(Delta, -std::floor(std::max(Free, 0.0)));
			return std::fabs(Delta) > k_MaxAmount ? 0 : Delta;
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
		const std::vector<double>& Free, const std::vector<int>& Stocked)
	{
		std::vector<EconomyChange> changes;
		if (!std::isfinite(Command.Amount) || std::fabs(Command.Amount) > k_MaxAmount)
			return changes;

		const auto add = [&](int resource, double delta) {
			if (delta == 0)
				return;
			for (const EconomyChange& change : changes)
				if (change.Resource == resource)
					return;			// 같은 자원을 두 번 하지 않는다
			changes.push_back({ resource, delta });
		};
		const auto valid = [&](int resource) { return resource >= 0 && static_cast<size_t>(resource) < Counts.size(); };
		const auto stocked = [&](int resource) {
			return valid(resource) && std::find(Stocked.begin(), Stocked.end(), resource) != Stocked.end();
		};
		// 줄일 수 있는 양: 예약되지 않은 수와 지금 수 가운데 작은 쪽.
		const auto free_of = [&](int resource) {
			const double count = Counts[resource];
			return static_cast<size_t>(resource) < Free.size() ? std::min(Free[resource], count) : count;
		};
		const double target = std::max(0.0, std::round(Command.Amount));

		switch (Command.Act)
		{
		case EconomyAct::GoldAdd:
			add(-1, Settle(Gold, Command.Amount));
			break;
		case EconomyAct::GoldSet:
			add(-1, Settle(Gold, target - Gold));
			break;
		case EconomyAct::ResourceAdd:
			if (stocked(Command.Resource))
				add(Command.Resource, Settle(free_of(Command.Resource), Command.Amount));
			break;
		case EconomyAct::ResourceSet:
			if (stocked(Command.Resource))
				add(Command.Resource, Settle(free_of(Command.Resource), target - Counts[Command.Resource]));
			break;
		case EconomyAct::AllAdd:
			for (const int resource : Stocked)
				if (valid(resource))
					add(resource, Settle(free_of(resource), Command.Amount));
			break;
		}
		return changes;
	}

	std::vector<EconomyShort> EconomyShortfall(const std::vector<EconomyChange>& Done, double GoldBefore, const std::vector<double>& Before,
		double GoldAfter, const std::vector<double>& After)
	{
		std::vector<EconomyShort> shorts;
		for (const EconomyChange& change : Done)
		{
			double applied = 0;
			if (change.Resource < 0)
				applied = GoldAfter - GoldBefore;
			else if (static_cast<size_t>(change.Resource) < Before.size() && static_cast<size_t>(change.Resource) < After.size())
				applied = After[change.Resource] - Before[change.Resource];
			if (!(applied == change.Delta))
				shorts.push_back({ change.Resource, change.Delta, applied });
		}
		return shorts;
	}
}
