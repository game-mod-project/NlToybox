#include "WorldPlan.hpp"

#include <cmath>

namespace NlCore
{
	namespace
	{
		struct ActInfo
		{
			WorldAct Act;
			const char* Word;
		};
		constexpr ActInfo k_Acts[] = {
			{ WorldAct::CooldownsClear, "cooldowns_clear" },
			{ WorldAct::BishopSend, "bishop" },
		};
	}

	bool ParseWorldAct(const std::string& Word, WorldAct& Out)
	{
		for (const ActInfo& act : k_Acts)
			if (Word == act.Word)
			{
				Out = act.Act;
				return true;
			}
		return false;
	}

	const char* WorldActWord(WorldAct Act)
	{
		for (const ActInfo& act : k_Acts)
			if (act.Act == Act)
				return act.Word;
		return "";
	}

	bool ShouldClearCooldown(bool IsNumber, double Value)
	{
		return IsNumber && std::isfinite(Value) && Value > 0;
	}

	BishopStep ChooseBishopStep(bool Read, bool Has)
	{
		if (!Read)
			return BishopStep::Unknown;
		return Has ? BishopStep::AlreadyHere : BishopStep::Call;
	}
}
