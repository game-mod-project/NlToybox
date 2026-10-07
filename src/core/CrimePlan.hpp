#pragma once
// 범죄 패널(src/Crime.cpp)의 판단 가운데 러너에 기대지 않는 것. 잰 것은 research/26.
// 게임에서 범죄자("부랑자")는 주민(o_dummy)의 범죄 구성요소(c_criminal)의 깃발이고, 죄와 영주의 범죄 혐의는 특성이다.

#include <string>
#include <vector>

namespace NlCore
{
	// 죄는 특성이다: 게임의 이름이 sin_ 으로 시작한다(열두 가지를 봤다). 영주의 범죄 혐의도 특성이다(character_crime 과 그 둘째·셋째 꼴).
	bool IsSinTrait(const std::string& Name);
	bool IsAccusationTrait(const std::string& Name);
	// 가진 특성 가운데 죄(Sins 가 참)나 혐의인 것들. 차례는 받은 그대로.
	std::vector<std::string> CrimeTraits(const std::vector<std::string>& Traits, bool Sins);

	// 범죄자로 읽는가: 깃발(c_criminal.__is_dummy_criminal)을 읽었고 0 보다 크다(처음에는 수 0, 지정된 뒤로는 불리언이다. 음수는 게임의 "없음"으로 본다).
	bool IsVagabondFlag(bool Read, double Raw);

	// 범죄자가 된 주민 한 사람. 틱이 읽어 글로 둔다.
	struct Vagabond
	{
		std::string Uuid, Name;
		int Index = 0;			// o_dummy 의 몇 번째였는가. 누가 떠나면 바뀐다: 부르기 전에 uuid 를 다시 본다
		bool Thug = false;		// 깡패가 됐는가(__is_dummy_thug)
		double Begin = -4;		// 범죄자가 된 게임 시각(__criminal_begin_time. 모르면 -4)
		double StolenGold = 0;	// __stolen_gold
	};
	// 한 줄: 이름, 며칠째인지, 깡패인지, 훔친 금화. Now 는 지금의 게임 시각. 모르는 것은 적지 않는다.
	std::string VagabondLine(const Vagabond& Who, double Now);
	// 요약의 한 줄. Unread: 범죄의 깃발을 읽지 못한 주민의 수("없음"과 "못 읽음"을 가른다).
	std::string CrimeSummary(int Vagabonds, int Thugs, int Unread);
	// 영주의 줄. Unread: 특성을 읽지 못한 영주의 수.
	std::string LordsLine(int Lords, int WithCrimes, int Unread);

	// 지정 풀기를 부른 뒤의 판정. SamePerson: 그 자리에 아직 그 사람이 있다. FlagRead: 깃발을 읽었다.
	// 깡패도 되돌린다(2026-10-07. research/27): 지정을 푼 뒤 깡패의 깃발(__is_dummy_thug)에 0 을 쓰고 둘을 다시 읽는다. ThugRead·ThugRaw 는 그 깃발을 읽은 것
	// (깡패가 아니었던 사람에게는 기본값: 보지 않는다). 범죄자의 깃발이 거짓이어도 깡패의 깃발이 남아 있으면 Still, 못 읽었으면 Unknown.
	enum class ClearOutcome { Cleared, Still, Unknown };
	ClearOutcome AfterClear(bool SamePerson, bool FlagRead, double Raw, bool ThugRead = true, double ThugRaw = 0);

	// 창의 단추와 원격 명령이 같은 길을 탄다.
	//   crime list                     범죄자와, 죄·혐의가 있는 영주
	//   crime clear <uuid|all>         범죄자 지정을 푼다(주민으로 되돌린다)
	//   crime return_stolen <uuid|all> 그 범죄자에게 훔친 것을 창고로 되돌리는 게임의 함수를 부른다(확인 전)
	//   crime absolve <uuid|lords>     영주의 죄(sin_*)를 지운다
	//   crime acquit <uuid|lords>      영주의 범죄 혐의(character_crime…)를 지운다
	enum class CrimeAct { List, Clear, ReturnStolen, Absolve, Acquit };
	struct CrimeCommand
	{
		CrimeAct Act = CrimeAct::List;
		std::string Who;		// uuid, "all"(범죄자 전원), "lords"(플레이어의 영주 전원). List 는 빈 글
	};
	// crime 뒤의 낱말들을 읽는다. 틀리면 거짓이고 Why 에 까닭.
	bool ParseCrimeCommand(const std::vector<std::string>& Words, CrimeCommand& Out, std::string& Why);
	const char* CrimeActWord(CrimeAct Act);

	// 지정 풀기의 결과. Asked: 풀려던 사람, Done: 풀린 사람(깃발이 거짓이 됐다), Skipped: 그사이 범죄자가 아니게 됐거나 자리가 바뀌어 부르지 않은 사람.
	// Unsure: 부른 뒤 깃발을 읽지 못했거나 그 자리의 사람이 바뀌어 확인하지 못한 사람. 나머지(Asked - Done - Skipped - Unsure)는 부르고도 풀리지 않았거나 부르지 못한 사람이다.
	// Why: 그 까닭(마지막 것). ThugsDone: 되돌린 사람 가운데 깡패였던 사람의 수(깡패의 되돌리기는 확인 전이라 따로 적는다).
	std::string ClearReport(int Asked, int Done, int Skipped, int Unsure, const std::string& Why, int ThugsDone = 0);
	// 죄·혐의 지우기의 결과. Lords: 살펴본 영주, Removed: 뗀 특성, Failed: 떼지 못한 특성, Unread: 특성을 읽지 못한 영주.
	std::string TraitClearReport(bool Sins, int Lords, int Removed, int Failed, int Unread, const std::string& Why);
	// 훔친 것 되돌리기의 결과: 부른 사람의 수와 그들의 훔친 금화의 합(부르기 전, 부른 뒤).
	std::string StolenReport(int Called, double GoldBefore, double GoldAfter, const std::string& Why);

	// 게임 변수(global.__gameplay_vars)의 열쇠들. 뜻은 이름에서 읽은 것이고 게임이 그 값을 따르는지는 재지 않았다(research/26).
	const std::vector<const char*>& BanditTurnVars();		// 주민이 도적으로 넘어가는 확률
	const std::vector<const char*>& CrimeMindVars();		// 범죄 때문에 생기는 생각의 크기
	const std::vector<const char*>& ThugDaysVars();			// 범죄자가 깡패가 되기까지의 날
	const std::vector<const char*>& TheftAmountVars();		// 창고 도둑이 가져가는 양
}
