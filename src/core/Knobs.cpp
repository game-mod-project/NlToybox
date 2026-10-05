#include "Knobs.hpp"

#include "Text.hpp"

#include <cmath>

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
