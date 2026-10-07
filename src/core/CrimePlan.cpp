#include "CrimePlan.hpp"

#include "Text.hpp"

#include "FamilyPlan.hpp"

#include <cmath>

namespace NlCore
{
	namespace
	{
		constexpr double k_Day = 86400;

		struct ActInfo
		{
			CrimeAct Act;
			const char* Word;
			char Who;		// 대상: 'n' 없음, 'a' 범죄자(uuid 나 all), 'l' 영주(uuid 나 lords)
		};
		constexpr ActInfo k_Acts[] = {
			{ CrimeAct::List, "list", 'n' },
			{ CrimeAct::Clear, "clear", 'a' },
			{ CrimeAct::ReturnStolen, "return_stolen", 'a' },
			{ CrimeAct::Absolve, "absolve", 'l' },
			{ CrimeAct::Acquit, "acquit", 'l' },
		};

		std::string People(int Count)
		{
			return std::to_string(Count) + "명";
		}
	}

	bool IsSinTrait(const std::string& Name)
	{
		return Name.size() > 4 && Name.compare(0, 4, "sin_") == 0;
	}

	bool IsAccusationTrait(const std::string& Name)
	{
		return Name == "character_crime" || Name == "character_crime_blamed_by_bishop" || Name == "character_crime_blamed_by_fanatics";
	}

	std::vector<std::string> CrimeTraits(const std::vector<std::string>& Traits, bool Sins)
	{
		std::vector<std::string> out;
		for (const std::string& name : Traits)
			if (Sins ? IsSinTrait(name) : IsAccusationTrait(name))
				out.push_back(name);
		return out;
	}

	bool IsVagabondFlag(bool Read, double Raw)
	{
		return Read && std::isfinite(Raw) && Raw > 0;
	}

	std::string VagabondLine(const Vagabond& Who, double Now)
	{
		std::string out = Who.Name;
		if (Who.Begin > 0 && std::isfinite(Now) && Now >= Who.Begin)
		{
			const long long days = static_cast<long long>((Now - Who.Begin) / k_Day);
			out += days == 0 ? std::string("  오늘부터") : "  " + std::to_string(days) + "일째";
		}
		if (Who.Thug)
			out += "  깡패";
		if (std::isfinite(Who.StolenGold) && Who.StolenGold > 0)
			out += "  훔친 금화 " + std::to_string(std::llround(Who.StolenGold));
		return out;
	}

	std::string LordsLine(int Lords, int WithCrimes, int Unread)
	{
		if (Lords <= 0)
			return "플레이어의 영주가 없습니다";
		std::string out = "플레이어의 영주 " + People(Lords) + " 가운데 죄나 범죄 혐의가 있는 사람 " + People(WithCrimes);
		if (Unread > 0)
			out += " (" + People(Unread) + "은 읽지 못했습니다)";
		return out;
	}

	ClearOutcome AfterClear(bool SamePerson, bool FlagRead, double Raw, bool ThugRead, double ThugRaw)
	{
		if (!SamePerson || !FlagRead || !std::isfinite(Raw) || !ThugRead || !std::isfinite(ThugRaw))
			return ClearOutcome::Unknown;
		return Raw > 0 || ThugRaw > 0 ? ClearOutcome::Still : ClearOutcome::Cleared;
	}

	std::string StolenReport(int Called, double GoldBefore, double GoldAfter, const std::string& Why)
	{
		if (Called <= 0)
			return Why.empty() ? std::string("부를 부랑자가 없습니다") : "게임의 '훔친 것 되돌리기'를 부르지 못했습니다: " + Why;
		return "부랑자 " + People(Called) + "에게 게임의 '훔친 것 되돌리기'를 불렀습니다: 훔친 금화의 합 " + std::to_string(std::llround(GoldBefore)) + " 에서 "
			+ std::to_string(std::llround(GoldAfter)) + " (확인 전의 기능입니다" + (Why.empty() ? std::string() : ". 못 부른 사람이 있습니다: " + Why) + ")";
	}

	std::string CrimeSummary(int Vagabonds, int Thugs, int Unread)
	{
		std::string out = Vagabonds > 0 ? "부랑자 " + People(Vagabonds) : std::string("부랑자 없음");
		if (Vagabonds > 0 && Thugs > 0)
			out += " (깡패 " + People(Thugs) + ")";
		if (Unread > 0)
			out += " (주민 " + People(Unread) + "은 읽지 못했습니다)";
		return out;
	}

	bool ParseCrimeCommand(const std::vector<std::string>& Words, CrimeCommand& Out, std::string& Why)
	{
		static const char* k_Usage = "crime needs list, clear <uuid|all>, return_stolen <uuid|all>, absolve <uuid|lords> or acquit <uuid|lords>";
		if (Words.empty())
		{
			Why = k_Usage;
			return false;
		}
		for (const ActInfo& act : k_Acts)
		{
			if (Words[0] != act.Word)
				continue;
			if (act.Who == 'n')
			{
				if (Words.size() != 1)
				{
					Why = std::string("crime ") + act.Word + " takes nothing";
					return false;
				}
				Out = CrimeCommand{ act.Act, std::string() };
				return true;
			}
			const char* everyone = act.Who == 'a' ? "all" : "lords";
			if (Words.size() != 2 || (Words[1] != everyone && !IsUuid(Words[1])))
			{
				Why = std::string("crime ") + act.Word + " needs a uuid or " + everyone;
				return false;
			}
			Out = CrimeCommand{ act.Act, Words[1] };
			return true;
		}
		Why = k_Usage;
		return false;
	}

	const char* CrimeActWord(CrimeAct Act)
	{
		for (const ActInfo& act : k_Acts)
			if (act.Act == Act)
				return act.Word;
		return "";
	}

	std::string ClearReport(int Asked, int Done, int Skipped, int Unsure, const std::string& Why, int ThugsDone)
	{
		if (Asked <= 0)
			return "되돌릴 부랑자가 없습니다";
		// 깡패의 되돌리기는 확인 전이다(게임이 만든 깡패에게 해 본 적이 없다): 그 수를 따로 적는다.
		const std::string thugs = ThugsDone > 0 ? "그 가운데 깡패 " + People(ThugsDone) + ". 깡패의 되돌리기는 확인 전입니다" : std::string();
		if (Done == Asked)
			return "부랑자 " + People(Asked) + "을 주민으로 되돌렸습니다" + (thugs.empty() ? "" : " (" + thugs + ")");
		std::string notes;
		const auto note = [&notes](const std::string& text) { notes += std::string(notes.empty() ? "" : ", ") + text; };
		const int failed = Asked - Done - Skipped - Unsure;
		if (Skipped > 0)
			note("그사이 범죄자가 아니게 됐거나 자리가 바뀐 " + People(Skipped) + "은 건너뜀");
		if (Unsure > 0)
			note(People(Unsure) + "은 부른 뒤 확인하지 못함");
		if (failed > 0)
			note(People(failed) + "은 못 함" + (Why.empty() ? "" : ": " + Why));
		return "부랑자 " + People(Asked) + " 가운데 " + People(Done) + "을 주민으로 되돌렸습니다 (" + notes + (thugs.empty() ? "" : ". " + thugs) + ")";
	}

	std::string TraitClearReport(bool Sins, int Lords, int Removed, int Failed, int Unread, const std::string& Why)
	{
		const char* what = Sins ? "죄" : "범죄 혐의";
		if (Lords <= 0)
			return "플레이어의 영주가 없습니다";
		if (Removed == 0 && Failed == 0)
		{
			if (Unread <= 0)
				return std::string("지울 ") + what + "가 없습니다 (영주 " + People(Lords) + ")";
			if (Unread >= Lords)
				return "영주 " + People(Lords) + "의 특성을 읽지 못했습니다";
			return "영주 " + People(Lords) + " 가운데 " + People(Unread) + "의 특성을 읽지 못했습니다 (나머지에게는 지울 " + what + "가 없습니다)";
		}
		std::string notes;
		const auto note = [&notes](const std::string& text) { notes += std::string(notes.empty() ? "" : ", ") + text; };
		if (Failed > 0)
			note(std::to_string(Failed) + "개는 못 뗌" + (Why.empty() ? "" : ": " + Why));
		if (Unread > 0)
			note(People(Unread) + "은 읽지 못함");
		return "영주 " + People(Lords) + "에게서 " + what + " " + std::to_string(Removed) + "개를 지웠습니다" + (notes.empty() ? "" : " (" + notes + ")");
	}

	const std::vector<const char*>& BanditTurnVars()
	{
		static const std::vector<const char*> keys = { "dummy_turn_to_bandit_chance", "dummy_turn_to_bandit_chance_peaceful" };
		return keys;
	}

	const std::vector<const char*>& CrimeMindVars()
	{
		static const std::vector<const char*> keys = { "mind_crime_not_punished_modify", "mind_crime_victim_modify" };
		return keys;
	}

	const std::vector<const char*>& ThugDaysVars()
	{
		static const std::vector<const char*> keys = { "dummy_criminal_days_to_thug" };
		return keys;
	}

	const std::vector<const char*>& TheftAmountVars()
	{
		static const std::vector<const char*> keys = { "dummy_storage_steal_minimal_val", "dummy_storage_steal_maximal_val" };
		return keys;
	}
}
