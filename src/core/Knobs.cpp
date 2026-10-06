#include "Knobs.hpp"

#include "Text.hpp"

#include <cmath>

const std::vector<NlCore::KnobDef>& NlCore::KnobDefs()
{
	static const std::vector<KnobDef> defs = {
		{ "building_cost", "건물 건설 비용", "건물을 지을 때 드는 자원의 양", KnobTarget::BuildingCost, nullptr },
		{ "start_resources", "시작 자원", "새 게임을 시작할 때 주어지는 자원(나무, 당근, 약).\n새 게임을 시작하기 전에 정한다", KnobTarget::StartResources, nullptr },
		{ "book_exp", "교본의 능력치 경험", "교본을 읽어 얻는 능력치 경험", KnobTarget::BookExp, nullptr },
		{ "bribe_cost", "뇌물 비용", nullptr, KnobTarget::GameplayVar, "bribe_give_rings" },
		{ "free_lord_stay", "자유 영주 체류 기간", nullptr, KnobTarget::GameplayVar, "free_lord_stay_duration" },
		{ "church_capacity", "교회 수용 인원", nullptr, KnobTarget::GameplayVar, "church_max_capacity" },
		{ "tavern_capacity", "선술집 수용 인원", nullptr, KnobTarget::GameplayVar, "tavern_max_capacity" },
	};
	return defs;
}

double NlCore::Scale(double Base, double Factor)
{
	const double value = Base * Factor;
	if (Base != std::floor(Base))
		return value;

	const double whole = std::floor(value + 0.5);
	return Base > 0 && whole < 1 ? 1 : whole;
}

double NlCore::ClampFactor(double Factor)
{
	if (!std::isfinite(Factor))
		return 1;
	return Factor < k_MinFactor ? k_MinFactor : Factor > k_MaxFactor ? k_MaxFactor : Factor;
}

std::map<std::string, double> NlCore::ParseSettings(std::istream& In)
{
	std::map<std::string, double> values;
	std::string line;
	while (std::getline(In, line))
	{
		line = Trim(line);
		const size_t eq = line.find('=');
		if (line.empty() || line[0] == '#' || eq == std::string::npos)
			continue;

		const std::string name = Trim(line.substr(0, eq));
		double factor = 0;
		if (name.empty() || !ParseNumber(Trim(line.substr(eq + 1)), factor))
			continue;
		values[name] = ClampFactor(factor);
	}
	return values;
}

std::string NlCore::FormatSettings(const std::map<std::string, double>& Values)
{
	std::string text = "# NlToyBox 의 배율. 모드창(F8)에서 바꾸면 여기에 저장된다. 1 = 원래 값.\n";
	for (const auto& [name, factor] : Values)
		text += name + "=" + Fixed(factor, 2) + "\n";
	return text;
}
