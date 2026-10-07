#include "common.hpp"

void RunCrimeTests()
{
	Test("범죄: 죄와 혐의의 특성, 범죄자의 줄, 명령, 결과의 글", [] {
		// 죄는 이름이 sin_ 으로 시작하는 특성이다(research/26). 그 밖의 특성은 죄가 아니다.
		CHECK(IsSinTrait("sin_murder") && IsSinTrait("sin_evil_joke") && IsSinTrait("sin_x"));
		CHECK(!IsSinTrait("sin_") && !IsSinTrait("sin") && !IsSinTrait("human") && !IsSinTrait("original_sin_x") && !IsSinTrait("") && !IsSinTrait("SIN_murder"));
		// 영주의 범죄 혐의: 세 가지 특성. 주민의 것(dummy_crime)과 겁먹은 범죄자의 표식은 혐의가 아니다.
		CHECK(IsAccusationTrait("character_crime") && IsAccusationTrait("character_crime_blamed_by_bishop") && IsAccusationTrait("character_crime_blamed_by_fanatics"));
		CHECK(!IsAccusationTrait("dummy_crime") && !IsAccusationTrait("criminal_intimidated") && !IsAccusationTrait("character_crime_x") && !IsAccusationTrait("sin_criminal")
			&& !IsAccusationTrait(""));
		const std::vector<std::string> traits = { "human", "sin_fight", "brave", "character_crime", "sin_seduce" };
		CHECK(CrimeTraits(traits, true) == (std::vector<std::string>{ "sin_fight", "sin_seduce" }));
		CHECK(CrimeTraits(traits, false) == (std::vector<std::string>{ "character_crime" }));
		CHECK(CrimeTraits({ "human" }, true).empty() && CrimeTraits({}, false).empty());

		// 범죄자의 깃발: 처음에는 수 0 이고 지정된 뒤로는 불리언 참·거짓이다. 읽지 못한 것은 범죄자가 아니다.
		CHECK(IsVagabondFlag(true, 1) && !IsVagabondFlag(true, 0) && !IsVagabondFlag(false, 1) && !IsVagabondFlag(true, std::nan("")));
		// 음수는 범죄자가 아니다: 같은 구성요소의 다른 칸은 "없음"이 -4 다(게임의 "아직 셈하지 않음"을 참으로 읽지 않는다).
		CHECK(!IsVagabondFlag(true, -4) && !IsVagabondFlag(true, -1));
		// 부른 뒤의 판정: 같은 사람이고 깃발을 읽었고 거짓이면 풀렸다. 깃발을 읽지 못했거나 그 자리의 사람이 바뀌었으면 "모른다"(풀렸다고 적지 않는다).
		CHECK(AfterClear(true, true, 0) == ClearOutcome::Cleared && AfterClear(true, true, 1) == ClearOutcome::Still);
		CHECK(AfterClear(true, false, 0) == ClearOutcome::Unknown && AfterClear(false, true, 0) == ClearOutcome::Unknown && AfterClear(true, true, std::nan("")) == ClearOutcome::Unknown);
		// 깡패도 되돌린다(2026-10-07. 게임이 만든 깡패에게서 확인했다. research/28): 지정을 푼 뒤 깡패의 깃발(__is_dummy_thug)에 0 을 쓰고 둘을 다시 읽는다.
		// 범죄자의 깃발이 거짓이어도 깡패의 깃발이 남아 있으면 그대로다. 깡패의 깃발을 읽지 못했으면 모른다.
		CHECK(AfterClear(true, true, 0, true, 0) == ClearOutcome::Cleared && AfterClear(true, true, 0, true, 1) == ClearOutcome::Still);
		CHECK(AfterClear(true, true, 0, false, 0) == ClearOutcome::Unknown && AfterClear(true, true, 1, true, 0) == ClearOutcome::Still);
		CHECK(AfterClear(true, true, 0, true, std::nan("")) == ClearOutcome::Unknown);

		// 한 줄. 이름은 지어낸 것이다.
		Vagabond who;
		who.Name = "가람";
		who.Begin = 669607;
		CHECK(VagabondLine(who, 669607 + 2.3 * 86400) == "가람  2일째");
		CHECK(VagabondLine(who, 669607 + 3600) == "가람  오늘부터");
		who.Thug = true;
		who.StolenGold = 12;
		CHECK(VagabondLine(who, 669607 + 86400) == "가람  1일째  깡패  훔친 금화 12");
		who.Begin = -4;		// 시작 시각을 모른다
		who.Thug = false;
		who.StolenGold = 0;
		CHECK(VagabondLine(who, 700000) == "가람");
		who.Begin = 800000;		// 시작이 지금보다 뒤로 읽혔다: 날을 적지 않는다
		CHECK(VagabondLine(who, 700000) == "가람");

		// 요약: 범죄자와 깡패의 수. 깃발을 읽지 못한 주민이 있으면 숨기지 않는다("없음"과 "못 읽음"을 가른다).
		// 관리자의 수(오늘의 사건, 지난 범죄)는 적지 않는다: 0 이 아닌 값을 범죄와 맞춰 본 적이 없다.
		CHECK(CrimeSummary(5, 1, 0) == "부랑자 5명 (깡패 1명)");
		CHECK(CrimeSummary(3, 0, 0) == "부랑자 3명" && CrimeSummary(0, 0, 0) == "부랑자 없음");
		CHECK(CrimeSummary(0, 0, 4) == "부랑자 없음 (주민 4명은 읽지 못했습니다)" && CrimeSummary(2, 0, 1) == "부랑자 2명 (주민 1명은 읽지 못했습니다)");
		// 영주의 줄: 특성을 읽지 못한 영주를 "죄 없음"으로 세지 않는다.
		CHECK(LordsLine(7, 2, 0) == "플레이어의 영주 7명 가운데 죄나 범죄 혐의가 있는 사람 2명");
		CHECK(LordsLine(6, 0, 2) == "플레이어의 영주 6명 가운데 죄나 범죄 혐의가 있는 사람 0명 (2명은 읽지 못했습니다)");
		CHECK(LordsLine(0, 0, 0) == "플레이어의 영주가 없습니다");

		// 명령.
		CrimeCommand c;
		std::string why;
		const std::string uuid = "0123456789abcdef";
		CHECK(ParseCrimeCommand({ "list" }, c, why) && c.Act == CrimeAct::List && c.Who.empty());
		CHECK(ParseCrimeCommand({ "clear", "all" }, c, why) && c.Act == CrimeAct::Clear && c.Who == "all");
		CHECK(ParseCrimeCommand({ "clear", uuid }, c, why) && c.Act == CrimeAct::Clear && c.Who == uuid);
		// 훔친 것 되돌리기는 대상을 받는다: 처음 해 보는 호출을 한 사람에게만 해 볼 수 있게.
		CHECK(ParseCrimeCommand({ "return_stolen", uuid }, c, why) && c.Act == CrimeAct::ReturnStolen && c.Who == uuid);
		CHECK(ParseCrimeCommand({ "return_stolen", "all" }, c, why) && c.Who == "all");
		CHECK(ParseCrimeCommand({ "absolve", "lords" }, c, why) && c.Act == CrimeAct::Absolve && c.Who == "lords");
		CHECK(ParseCrimeCommand({ "acquit", uuid }, c, why) && c.Act == CrimeAct::Acquit && c.Who == uuid);
		// 범죄자 전원은 all, 영주 전원은 lords 다: 뒤바꾸지 못한다. 대상이 빠졌거나 남는 낱말이 있으면 받지 않는다.
		for (const std::vector<std::string>& bad : std::vector<std::vector<std::string>>{ {}, { "clear" }, { "clear", "lords" }, { "clear", "people" }, { "absolve" },
				{ "absolve", "all" }, { "acquit", "all" }, { "list", "all" }, { "return_stolen" }, { "return_stolen", "lords" }, { "clear", "all", "now" }, { "clear", "0123" }, { "punish", uuid } })
		{
			why.clear();
			CHECK(!ParseCrimeCommand(bad, c, why) && !why.empty());
		}
		for (CrimeAct act : { CrimeAct::List, CrimeAct::Clear, CrimeAct::ReturnStolen, CrimeAct::Absolve, CrimeAct::Acquit })
		{
			CrimeCommand round;
			const std::vector<std::string> words = act == CrimeAct::List ? std::vector<std::string>{ CrimeActWord(act) }
				: std::vector<std::string>{ CrimeActWord(act), uuid };
			CHECK(ParseCrimeCommand(words, round, why) && round.Act == act);
		}
		// 원격의 줄.
		const auto who_of = [](const RemoteCommand& r) {
			const auto it = r.Options.find("who");
			return it == r.Options.end() ? std::string("<none>") : it->second;
		};
		CHECK(ParseRemoteLine("crime list").Error.empty() && ParseRemoteLine("crime list").Target == "list");
		CHECK(who_of(ParseRemoteLine("crime return_stolen " + uuid)) == uuid && !ParseRemoteLine("crime return_stolen").Error.empty());
		RemoteCommand remote = ParseRemoteLine("crime clear all");
		CHECK(remote.Error.empty() && remote.Verb == "crime" && remote.Target == "clear" && who_of(remote) == "all");
		CHECK(ParseRemoteLine("crime absolve lords").Error.empty() && who_of(ParseRemoteLine("crime acquit " + uuid)) == uuid);
		CHECK(!ParseRemoteLine("crime").Error.empty() && !ParseRemoteLine("crime clear").Error.empty() && !ParseRemoteLine("crime absolve all").Error.empty());

		// 지정 풀기의 결과: 몇 명 가운데 몇 명이 풀렸는가, 건너뛴 사람과 못 한 사람을 숨기지 않는다.
		CHECK(ClearReport(0, 0, 0, 0, "") == "되돌릴 부랑자가 없습니다");
		CHECK(ClearReport(5, 5, 0, 0, "") == "부랑자 5명을 주민으로 되돌렸습니다");
		CHECK(ClearReport(1, 1, 0, 0, "") == "부랑자 1명을 주민으로 되돌렸습니다");
		CHECK(ClearReport(5, 3, 2, 0, "") == "부랑자 5명 가운데 3명을 주민으로 되돌렸습니다 (그사이 범죄자가 아니게 됐거나 자리가 바뀐 2명은 건너뜀)");
		CHECK(ClearReport(5, 3, 1, 0, "did not stick") == "부랑자 5명 가운데 3명을 주민으로 되돌렸습니다 (그사이 범죄자가 아니게 됐거나 자리가 바뀐 1명은 건너뜀, 1명은 못 함: did not stick)");
		CHECK(ClearReport(2, 0, 0, 0, "busy") == "부랑자 2명 가운데 0명을 주민으로 되돌렸습니다 (2명은 못 함: busy)");
		// 부르고도 확인하지 못한 사람은 "되돌렸다"에 세지 않고 따로 적는다(검토: 깃발을 읽지 못해도 됐다고 적었다).
		CHECK(ClearReport(3, 1, 0, 2, "") == "부랑자 3명 가운데 1명을 주민으로 되돌렸습니다 (2명은 부른 뒤 확인하지 못함)");
		CHECK(ClearReport(4, 1, 1, 1, "x") == "부랑자 4명 가운데 1명을 주민으로 되돌렸습니다 (그사이 범죄자가 아니게 됐거나 자리가 바뀐 1명은 건너뜀, 1명은 부른 뒤 확인하지 못함, 1명은 못 함: x)");
		// 되돌린 사람 가운데 깡패가 있으면 따로 적는다(게임이 만든 깡패에게서 확인했다. research/28). 없으면 글이 그대로다.
		CHECK(ClearReport(3, 3, 0, 0, "", 1) == "부랑자 3명을 주민으로 되돌렸습니다 (그 가운데 깡패 1명)");
		CHECK(ClearReport(3, 3, 0, 0, "", 0) == "부랑자 3명을 주민으로 되돌렸습니다");
		CHECK(ClearReport(3, 2, 1, 0, "", 2) == "부랑자 3명 가운데 2명을 주민으로 되돌렸습니다 (그사이 범죄자가 아니게 됐거나 자리가 바뀐 1명은 건너뜀. 그 가운데 깡패 2명)");

		// 죄·혐의 지우기의 결과. 특성을 읽지 못한 영주를 숨기지 않는다(검토: 읽지 못한 영주가 "지울 죄가 없습니다"로 나왔다).
		CHECK(TraitClearReport(true, 7, 3, 0, 0, "") == "영주 7명에게서 죄 3개를 지웠습니다");
		CHECK(TraitClearReport(false, 1, 2, 0, 0, "") == "영주 1명에게서 범죄 혐의 2개를 지웠습니다");
		CHECK(TraitClearReport(true, 7, 0, 0, 0, "") == "지울 죄가 없습니다 (영주 7명)" && TraitClearReport(false, 7, 0, 0, 0, "") == "지울 범죄 혐의가 없습니다 (영주 7명)");
		CHECK(TraitClearReport(true, 2, 1, 2, 0, "protected") == "영주 2명에게서 죄 1개를 지웠습니다 (2개는 못 뗌: protected)");
		CHECK(TraitClearReport(true, 1, 0, 0, 1, "") == "영주 1명의 특성을 읽지 못했습니다");
		CHECK(TraitClearReport(true, 3, 0, 0, 1, "") == "영주 3명 가운데 1명의 특성을 읽지 못했습니다 (나머지에게는 지울 죄가 없습니다)");
		CHECK(TraitClearReport(true, 3, 2, 0, 1, "") == "영주 3명에게서 죄 2개를 지웠습니다 (1명은 읽지 못함)");
		CHECK(TraitClearReport(false, 3, 2, 1, 1, "busy") == "영주 3명에게서 범죄 혐의 2개를 지웠습니다 (1개는 못 뗌: busy, 1명은 읽지 못함)");
		CHECK(TraitClearReport(true, 0, 0, 0, 0, "") == "플레이어의 영주가 없습니다");

		// 훔친 것 되돌리기의 결과: 부른 사람과 훔친 금화의 합의 앞뒤. 효과를 본 적이 없으므로 본 수를 그대로 적는다.
		CHECK(StolenReport(0, 0, 0, "") == "부를 부랑자가 없습니다");
		CHECK(StolenReport(2, 30, 0, "") == "부랑자 2명에게 게임의 '훔친 것 되돌리기'를 불렀습니다: 훔친 금화의 합 30 에서 0 (확인 전의 기능입니다)");
		CHECK(StolenReport(1, 0, 0, "") == "부랑자 1명에게 게임의 '훔친 것 되돌리기'를 불렀습니다: 훔친 금화의 합 0 에서 0 (확인 전의 기능입니다)");
		CHECK(StolenReport(1, 12, 12, "no method") == "부랑자 1명에게 게임의 '훔친 것 되돌리기'를 불렀습니다: 훔친 금화의 합 12 에서 12 (확인 전의 기능입니다. 못 부른 사람이 있습니다: no method)");

		// 게임 변수의 열쇠들(이름만. 값과 뜻은 게임의 것이고 효과는 재지 않았다).
		CHECK(BanditTurnVars().size() == 2 && CrimeMindVars().size() == 2 && ThugDaysVars().size() == 1 && TheftAmountVars().size() == 2);
		CHECK(ThugDaysVars().size() == 1 && std::string(ThugDaysVars()[0]) == "dummy_criminal_days_to_thug");
	});
}
