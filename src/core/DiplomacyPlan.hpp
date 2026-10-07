#pragma once
// 외교 패널(src/Diplomacy.cpp)의 판단 가운데 러너에 기대지 않는 것. 잰 것은 research/19.
// 왕국 사이의 관계(종류)는 게임이 왕의 평판에서 다시 셈한다. 그래서 관계를 바로 쓰지 않고, 게임의 함수로 디버그 평판을 하나씩 붙여 움직인다.

#include <string>
#include <vector>

namespace NlCore
{
	// 관계의 종류(FactionsAlliesMatrix 의 수. relationship_to_string 이 돌려준 이름): 0 allies, 1 enemies, 2 deadly enemies, 3 neutrals, 4 friends, 5 vassal, 6 sir, 7 opponent.
	constexpr int k_RelationAllies = 0, k_RelationEnemies = 1, k_RelationDeadly = 2, k_RelationNeutrals = 3, k_RelationFriends = 4,
		k_RelationVassal = 5, k_RelationSir = 6, k_RelationOpponent = 7;
	// 창에 보일 이름. 모르는 수(읽지 못한 것 포함)는 "?".
	const char* RelationLabel(double Kind);

	// 왕국인가: 세력의 __system_name 이 "faction.new.name.<수>"다(도적, 상인, 교단 같은 게임의 꾸러미가 아니다).
	bool IsKingdom(const std::string& SystemName);

	// 바라는 관계. Opinion 은 관계를 보지 않고 평판을 정한 만큼만 움직인다. Pact 는 평판이 아니라 협정을 맺는다(아래).
	// Clear 는 붙여 둔 디버그 평판을 뗀다(2026-10-07. 좋은 자료부터, 더 떨어지지 않으면 나쁜 자료로. 아래의 DetachCheck·AfterDetach).
	enum class DiplomacyGoal { Friends, Neutral, Hostile, Opinion, Pact, Clear };
	// 원격 명령의 낱말: friends, neutral, hostile, opinion, pact, clear.
	bool ParseDiplomacyGoal(const std::string& Word, DiplomacyGoal& Out);
	const char* DiplomacyGoalWord(DiplomacyGoal Goal);
	// 창에 보일 이름: 우호, 중립, 적대.
	const char* DiplomacyGoalLabel(DiplomacyGoal Goal);

	// 누구의 평판을 움직일지: 't' 그쪽이 우리를 보는 평판, 'u' 우리가 그쪽을 보는 평판, 'b' 양쪽. 낱말: them, us, both.
	bool ParseDiplomacySide(const std::string& Word, char& Out);

	// 지금의 관계에서 바라는 관계로 가는 한 걸음: +1 좋은 평판 하나를 붙인다, -1 나쁜 평판 하나, 0 이미 그 관계다,
	// 2 건드리지 않는다(동맹·봉신·주군, 모르는 수). Opinion 과 Pact 에는 쓰지 않는다(2).
	// 우호: 우호가 될 때까지 올린다. 중립: 적·철천지원수·대립이면 올리고 우호면 내린다. 적대: 철천지원수가 될 때까지 내린다
	// (is_enemy_with 가 참이 되는 것은 그 관계에서였다).
	int StepToward(double Kind, DiplomacyGoal Goal);

	constexpr int k_OpinionUnit = 5;			// 디버그 평판의 자료에 적힌 크기(opinion_mind_debug_positive·negative 의 __opinion_modify 의 절댓값). 붙이기 전에 견준다
	constexpr int k_OpinionStepsMax = 40;		// 한 번의 명령이 한쪽에 붙이는 평판의 한도(그 평판의 겹침 한도보다 작게)
	constexpr int k_OpinionStackLimit = 50;		// 그 평판의 자료에 적힌 겹침 한도(__stack_limit). 모듈은 세지 않는다(세이브를 다시 불러오면 센 수가 게임과 어긋난다)
	constexpr int k_ClearStepsMax = 120;		// 떼기의 걸음 한도(겹침 한도 50 이 둘, 그리고 여유. 영주의 호감 떼기와 같다)
	// 평판의 수(Opinion 의 Amount): 붙일 디버그 평판의 개수다(부호가 방향). 0 이 아닌 정수이고 절댓값이 한도 안이어야 한다. 아니면 0(받지 않는다).
	// 중립에서 우호가 되기까지 드는 개수는 왕국마다 달랐다(1개 ~ 15개. research/19). 그래서 수치를 약속하지 않고 개수로 다룬다.
	int OpinionSteps(double Amount);

	// 일의 한 걸음의 판단. Outcome 이 0 이면 Direction(+1 좋은 평판, -1 나쁜 평판)을 하나 붙인다. 아니면 끝났다:
	//   'd' 됐다(바라는 관계가 됐다, 정한 만큼 붙였다), 'a' 처음부터 그 관계였다, 'k' 건드리지 않는 관계다(동맹·봉신·주군·모르는 수. 방향이 없는 평판, 협정도),
	//   'l' 이 명령의 한도까지 붙였지만 되지 않았다.
	// Kind: 지금의 관계. Sign: Opinion 의 방향. Left: 이 일이 더 붙여도 되는 수. Done: 이 일이 붙인 수(부호가 방향).
	// 0 을 돌려준 걸음마다 부른 쪽이 Left 를 하나 줄인다. 그래서 관계가 어떻게 뛰든 한도 안에 끝난다(시험이 가짜 관계표로 돌려 본다).
	struct DiplomacyStep
	{
		char Outcome = 0;
		int Direction = 0;
	};
	DiplomacyStep PlanStep(double Kind, DiplomacyGoal Goal, int Sign, int Left, int Done);

	// 그 왕국을 건드려도 되는가: 망했는지(is_destroyed)와 양쪽에 왕이 있는지(get_king_character_soul)를 물은 결과로 정한다.
	// Asked: is_destroyed 를 불러 불리언을 받았다. Destroyed: 그 답. KingAsked·HasKing: 그 왕국의 왕을 물어 답을 받았다, 구조체다.
	// OursAsked·OursHasKing: 우리 왕을 물어 답을 받았다, 구조체다.
	// 돌려주는 값: 0 건드려도 된다, 'x' 망했거나 그 왕국에 왕이 없다(건드리지 않는다), 'f' 묻지 못했거나 우리 쪽에 왕이 없다(실패).
	char AliveOutcome(bool Asked, bool Destroyed, bool KingAsked, bool HasKing, bool OursAsked, bool OursHasKing);

	// 협정(FactionsAgreementMatrix 의 칸의 비트. 게임의 판정 함수가 is_has_agreement 에 넘기는 수): 평화 4, 교역 협정 8, 방어 동맹 192.
	enum class DiplomacyPact { Peace, Trade, Defence };
	// 원격 명령의 낱말: peace, trade, defence.
	bool ParseDiplomacyPact(const std::string& Word, DiplomacyPact& Out);
	const char* DiplomacyPactWord(DiplomacyPact Pact);
	// 창에 보일 이름: 평화 협정, 교역 협정, 방어 동맹.
	const char* DiplomacyPactLabel(DiplomacyPact Pact);
	int PactBits(DiplomacyPact Pact);
	// 칸의 수에 그 협정이 들어 있는가(그 비트가 모두 켜져 있다). 칸이 없거나(음수) 정수가 아니면 거짓.
	bool HasPact(double Cell, DiplomacyPact Pact);
	// 그 협정을 맺을 때 쓸 수: 지금 칸의 비트에 더한다(이미 든 협정을 지우지 않게). 칸이 없으면 그 협정의 비트만.
	double PactCell(double Cell, DiplomacyPact Pact);
	// 칸의 수를 창에 보일 글로: "평화, 교역". 든 것이 없으면 "-", 아는 비트가 하나도 없는데 0 이 아니면 "?".
	std::string PactText(double Cell);
	// 칸에 아는 협정으로 설명되지 않는 것이 있는가(모르는 비트, 방어 동맹의 반쪽, 정수가 아닌 수). 칸이 없으면(음수) 거짓.
	bool PactUnknown(double Cell);
	// 협정을 맺은 결과의 글. Outcome: 'd' 맺었다, 'a' 이미 있다, 'f' 못 했다(Why 에 까닭).
	std::string PactReport(const std::string& Name, DiplomacyPact Pact, char Outcome, const std::string& Why);

	// 대상: "all"(모든 왕국) 또는 세력의 uuid(16자리 16진수).
	bool GoodFactionWho(const std::string& Who);

	struct DiplomacyCommand
	{
		std::string Who;							// "all" 또는 세력의 uuid
		DiplomacyGoal Goal = DiplomacyGoal::Neutral;
		char Side = 'b';
		double Amount = 0;							// Opinion 의 변화량
		DiplomacyPact Pact = DiplomacyPact::Peace;	// Pact 의 협정
	};
	// 명령이 말이 되는가. 아니면 거짓이고 Why 에 까닭. 모든 왕국에게는 우호와 중립만 한다(적대, 평판의 수, 협정은 한 왕국씩).
	bool CheckDiplomacy(const DiplomacyCommand& Command, std::string& Why);

	// 명령을 일들로 푼다. Kingdoms: 왕국들의 uuid(패널의 차례). 말이 안 되는 명령과 없는 왕국은 일을 내지 않는다.
	// 양쪽이면 왕국마다 일 둘('t' 먼저), 협정은 일 하나('t'. 양쪽 칸에 한 번에 쓰인다).
	struct DiplomacyJob
	{
		std::string Uuid;
		char Side = 't';
		int Left = 0;			// 붙여도 되는 평판의 수(Opinion 은 정한 수, 그 밖은 k_OpinionStepsMax)
		int Sign = 0;			// Opinion 의 방향
	};
	std::vector<DiplomacyJob> PlanJobs(const DiplomacyCommand& Command, const std::vector<std::string>& Kingdoms);

	// 한 왕국의 한쪽 평판을 움직인 결과를 글로. Side: 't' 또는 'u'. Before·After: 관계의 종류. Steps: 붙인 평판의 수(부호가 방향).
	// Outcome: PlanStep 의 것과, 'f' 게임의 함수가 실패했다(Why 에 까닭), 'x' 망했거나 왕이 없는 왕국이라 건드리지 않았다,
	// 's' 같은 평판의 겹침 한도에 닿아 더 붙지 않았다(붙은 수는 Steps 에. AttachCheck 가 가린다).
	std::string DiplomacyReport(const std::string& Name, char Side, double Before, double After, int Steps, char Outcome, const std::string& Why);
	// 평판 하나가 붙었는가: 붙이기 바로 앞뒤로(같은 걸음 안에서) 그 평판을 갖는 왕의 평판 목록(__opinion_minds)의 원소 수를 견준다.
	// 우리 왕에게서는 붙을 때마다 원소가 하나 늘었고, 같은 평판이 50개에 닿자 함수가 구조체를 돌려줘도 늘지 않았다(research/19).
	// 돌려주는 값: 'y' 하나 늘었다(붙었다), 'n' 그대로다(붙지 않았다), 'u' 모른다(세지 못했다, 줄었다, 둘 이상 늘었다).
	char AttachCheck(double Before, double After);
	// 붙인 뒤의 판단. Check: AttachCheck 의 답. Seen: 이 일에서 평판의 수가 느는 것을 본 적이 있다(세는 길이 이 왕에게 듣는다는 증거).
	// 돌려주는 값: 'c' 붙은 것을 확인했다(센다), 't' 확인하지 못했다(함수의 반환값을 믿고 센다. 결과 줄에 UnsureNote 를 덧붙인다),
	// 's' 붙지 않았다(세지 않고 일을 멈춘다). 느는 것을 본 적이 없는 왕의 "그대로다"는 한도인지 세는 길이 듣지 않는 것인지 가릴 수 없어 't'다.
	char AfterAttach(char Check, bool Seen);
	// 확인하지 못하고 센 걸음이 있는 일의 결과 줄에 덧붙이는 글.
	std::string UnsureNote();

	// 평판 떼기(Clear). 게임의 Faction.detach_opinion_about_faction(대상 세력, 평판 자료)가 하나를 뗀다(붙은 것이 없을 때는 불러 본 적이 없다: 세지 못하면 떼지 않는다).
	// 뗐는가: 떼기 바로 앞뒤의 그 왕의 평판의 수. 'y' 하나 줄었다, 'n' 그대로다(그 자료는 더 없다), 'u' 모른다(세지 못했다, 둘 이상 줄었다, 늘었다).
	char DetachCheck(double Before, double After);
	// 걸음 뒤의 판단. 'c' 뗐다(센다. 같은 자료로 계속), 'n' 그 자료는 더 없다(다음 자료로. 나쁜 것까지 끝났으면 끝), 'u' 모른다(멈춘다: 실패).
	char AfterDetach(char Check);
	// 뗀 결과의 글. Good·Bad: 뗀 좋은·나쁜 평판의 수. Outcome: 'd' 다 뗐다, 'a' 뗄 것이 없었다, 'l' 한도에 닿았다, 'f' 실패(Why 에 까닭), 'x' 망했거나 왕이 없는 왕국.
	std::string ClearOpinionReport(const std::string& Name, char Side, double Before, double After, int Good, int Bad, char Outcome, const std::string& Why);
	// 평판의 수(목표가 없는 일)를 정한 만큼 붙인 뒤의 글. 붙인 개수와 관계를 따로 적는다(붙인 것을 "바꿨다"고 적지 않는다).
	// Stopped: 정한 만큼 붙이기 전에 겹침의 한도에 닿아 멈췄다('s'). Steps 는 실제로 붙은 수다(0 일 수 있다).
	std::string OpinionReport(const std::string& Name, char Side, double Before, double After, int Steps, bool Stopped = false);

}
