#include "BuildingPlan.hpp"

namespace NlCore
{
	bool IsBarracksName(const std::string& BuildingName)
	{
		return BuildingName.rfind("barrack_", 0) == 0;
	}
}
