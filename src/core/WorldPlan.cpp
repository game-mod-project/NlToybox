#include "WorldPlan.hpp"

#include <cmath>
#include <string>

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

	std::string CooldownReport(const ClearResult& Events, const ClearResult& Groups)
	{
		if (!Events.Opened && !Groups.Opened)
			return "이벤트 쿨다운을 읽지 못했습니다";
		const auto count = [](int Number) { return std::to_string(Number) + "개"; };
		std::string text;
		if (Events.Opened && Groups.Opened)
		{
			if (Events.Cleared == 0 && Groups.Cleared == 0 && Events.Failed == 0 && Groups.Failed == 0)
				return "지울 쿨다운이 없습니다 (0 보다 큰 칸이 없습니다)";
			text = "이벤트 쿨다운 " + count(Events.Cleared) + "와 묶음 쿨다운 " + count(Groups.Cleared) + "를 0 으로 썼습니다";
		}
		else if (Events.Opened)
			text = "이벤트 쿨다운 " + count(Events.Cleared) + "를 0 으로 썼습니다. 묶음 쿨다운은 읽지 못했습니다";
		else
			text = "묶음 쿨다운 " + count(Groups.Cleared) + "를 0 으로 썼습니다. 이벤트 쿨다운은 읽지 못했습니다";
		if (Events.Failed > 0 || Groups.Failed > 0)
			text += ". 쓰지 못한 칸: 이벤트 " + std::to_string(Events.Failed) + ", 묶음 " + std::to_string(Groups.Failed);
		return text;
	}

	bool CooldownTouched(const ClearResult& Events, const ClearResult& Groups)
	{
		return Events.Cleared > 0 || Events.Failed > 0 || Groups.Cleared > 0 || Groups.Failed > 0;
	}

	BishopStep ChooseBishopStep(bool Read, bool Has)
	{
		if (!Read)
			return BishopStep::Unknown;
		return Has ? BishopStep::AlreadyHere : BishopStep::Call;
	}

	const std::vector<const char*>& ReligionCostVars()
	{
		// 이름은 게임의 gameplay_variables 의 것이다(실행 중인 게임의 global.__gameplay_vars 에서 읽었다: 3, 5, 250, 300, 200, 50).
		static const std::vector<const char*> vars = {
			"religiosity_confession_cost", "religiosity_divorce_cost", "religiosity_begging_cost",
			"religiosity_canonization_cost_gold", "religiosity_canonization_cost_per_province", "religiosity_sacrificer_cost_gold",
		};
		return vars;
	}

	const std::vector<const char*>& PietyRestoreVars()
	{
		// 15, 15, 30, 20 이었다.
		static const std::vector<const char*> vars = {
			"church_pray_piety_restore", "altar_pray_piety_restore", "church_pray_morning_service_restore", "trait_saint_piety_talk_restore",
		};
		return vars;
	}
}
