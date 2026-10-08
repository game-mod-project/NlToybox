#include "WorldPlan.hpp"

#include <cctype>
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
			bool Changes;		// 게임의 자료를 바꾸는가
		};
		constexpr ActInfo k_Acts[] = {
			{ WorldAct::CooldownsClear, "cooldowns_clear", true },
			{ WorldAct::BishopSend, "bishop", true },
			{ WorldAct::SeasonShow, "season", false },
			{ WorldAct::SeasonDelay, "season_delay", true },
			{ WorldAct::SeasonEnd, "season_end", true },
			{ WorldAct::SaveNow, "save", true },
			{ WorldAct::EventForce, "event", true },
			{ WorldAct::EventList, "events", false },
			{ WorldAct::EventCancel, "event_cancel", true },
			{ WorldAct::EventEnd, "event_end", true },
			{ WorldAct::EventNow, "event_now", true },
		};
	}

	bool WorldActNeedsName(WorldAct Act)
	{
		return Act == WorldAct::EventForce || Act == WorldAct::EventNow;
	}

	bool GoodEventName(const std::string& Name)
	{
		if (Name.empty() || Name.size() > 64)
			return false;
		for (const unsigned char c : Name)
			if (!std::isalnum(c) && c != '_')
				return false;
		return true;
	}

	std::string SaveNowReport(char Outcome, const std::string& Detail)
	{
		switch (Outcome)
		{
		case 'd': return "게임의 저장이 꺼져 있어 저장하지 않았습니다 (유틸의 '게임의 저장 끄기'를 끈 뒤에)";
		case 'u': return "게임의 저장이 꺼져 있는지 읽지 못해 저장하지 않았습니다";
		case 'f': return "게임의 저장 함수를 부르지 못했습니다 (" + Detail + ")";
		case 's': return "저장했습니다: " + Detail;
		default: return "게임의 저장 함수를 불렀습니다 (새 파일은 아직 보이지 않습니다)";
		}
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

	bool KeepStock(std::map<std::string, double>& Kept, const std::string& Key, double Now, double& Write)
	{
		if (!std::isfinite(Now) || Now < 0)
			return false;
		const auto found = Kept.find(Key);
		if (found == Kept.end() || Now >= found->second)
		{
			Kept[Key] = Now;
			return false;
		}
		Write = found->second;
		return true;
	}

	void EnterStockPlace(StockBook& Book, const PlaceKey& Place, double Now)
	{
		if (!(Book.Place == Place) || Now < Book.Seen)
			Book.Kept.clear();
		Book.Place = Place;
		Book.Seen = Now;
	}

	std::string WorldActWords()
	{
		std::string out;
		for (const ActInfo& act : k_Acts)
			out += std::string(out.empty() ? "" : ", ") + act.Word;
		return out;
	}

	bool WorldActChanges(WorldAct Act)
	{
		for (const ActInfo& act : k_Acts)
			if (act.Act == Act)
				return act.Changes;
		return false;
	}

	bool IsSeasonAct(WorldAct Act)
	{
		return Act == WorldAct::SeasonShow || Act == WorldAct::SeasonDelay || Act == WorldAct::SeasonEnd;
	}

	bool IsEventAct(WorldAct Act)
	{
		return Act == WorldAct::EventForce || Act == WorldAct::EventNow || Act == WorldAct::EventList || Act == WorldAct::EventCancel || Act == WorldAct::EventEnd
			|| Act == WorldAct::CooldownsClear;
	}

	bool WorldActNeedsKind(WorldAct Act)
	{
		return Act == WorldAct::EventEnd;
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

	const std::vector<const char*>& PreachFactorVars()
	{
		static const std::vector<const char*> vars = { "church_preach_conversion_factor" };
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
