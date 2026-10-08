#include "BuildingPlan.hpp"

#include <cmath>

namespace NlCore
{
	EffectSide EffectSideOf(double EffectValue)
	{
		if (!std::isfinite(EffectValue) || EffectValue == 0)
			return EffectSide::None;
		return EffectValue > 0 ? EffectSide::Good : EffectSide::Bad;
	}

	EffectSide EffectSides::Of(const std::string& BuildingName, double EffectValue)
	{
		const auto found = m_Seen.find(BuildingName);
		if (found != m_Seen.end())
			return found->second;
		const EffectSide side = EffectSideOf(EffectValue);
		if (side != EffectSide::None)
			m_Seen.emplace(BuildingName, side);
		return side;
	}

	bool IsBarracksName(const std::string& BuildingName)
	{
		return BuildingName.rfind("barrack_", 0) == 0;
	}

	bool JobMayRun(bool InGame, bool AnyScreen, bool DataReady)
	{
		return InGame || (AnyScreen && DataReady);
	}
}
