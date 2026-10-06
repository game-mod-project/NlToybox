#pragma once
// 외교 패널(src/Diplomacy.cpp)의 판단 가운데 러너에 기대지 않는 것. 잰 것은 research/19.
// 왕국 사이의 관계(종류)는 게임이 왕의 평판에서 다시 셈한다. 그래서 관계를 바로 쓰지 않고, 게임의 함수로 디버그 평판(±5)을 붙여 움직인다.

#include <string>

namespace NlCore
{
	// 관계의 종류(FactionsAlliesMatrix 의 수. relationship_to_string 이 돌려준 이름): 0 allies, 1 enemies, 2 deadly enemies, 3 neutrals, 4 friends, 5 vassal, 6 sir, 7 opponent.
	constexpr int k_RelationAllies = 0, k_RelationEnemies = 1, k_RelationDeadly = 2, k_RelationNeutrals = 3, k_RelationFriends = 4,
		k_RelationVassal = 5, k_RelationSir = 6, k_RelationOpponent = 7;
	// 창에 보일 이름. 모르는 수(읽지 못한 것 포함)는 "?".
	const char* RelationLabel(double Kind);

	// 왕국인가: 세력의 __system_name 이 "faction.new.name.<수>"다(도적, 상인, 교단 같은 게임의 꾸러미가 아니다).
	bool IsKingdom(const std::string& SystemName);

	// 바라는 관계. Opinion 은 관계를 보지 않고 평판을 정한 만큼만 움직인다.
	enum class DiplomacyGoal { Friends, Neutral, Hostile, Opinion };
	// 원격 명령의 낱말: friends, neutral, hostile, opinion.
	bool ParseDiplomacyGoal(const std::string& Word, DiplomacyGoal& Out);
	const char* DiplomacyGoalWord(DiplomacyGoal Goal);
	// 창에 보일 이름: 우호, 중립, 적대.
	const char* DiplomacyGoalLabel(DiplomacyGoal Goal);

	// 누구의 평판을 움직일지: 't' 그쪽이 우리를 보는 평판, 'u' 우리가 그쪽을 보는 평판, 'b' 양쪽. 낱말: them, us, both.
	bool ParseDiplomacySide(const std::string& Word, char& Out);

	// 지금의 관계에서 바라는 관계로 가는 한 걸음: +1 좋은 평판 하나를 붙인다, -1 나쁜 평판 하나, 0 이미 그 관계다,
	// 2 건드리지 않는다(동맹·봉신·주군은 평판이 아닌 것으로 맺어진다. 모르는 수도). Opinion 에는 쓰지 않는다(2).
	// 우호: 우호가 될 때까지 올린다. 중립: 적·철천지원수·대립이면 올리고 우호면 내린다. 적대: 철천지원수가 될 때까지 내린다
	// (is_enemy_with 가 참이 되는 것은 그 관계에서였다).
	int StepToward(double Kind, DiplomacyGoal Goal);

	constexpr int k_OpinionUnit = 5;			// 디버그 평판 하나의 크기(opinion_mind_debug_positive·negative 의 __opinion_modify 의 절댓값)
	constexpr int k_OpinionStepsMax = 40;		// 한 번의 명령이 한쪽에 붙이는 평판의 한도(그 평판의 겹침 한도 50 보다 작게)
	// 평판의 변화량을 걸음 수로: 5 의 배수로 반올림하고 한도 안으로 당긴다. 부호가 방향이다. 수가 아니면 0.
	int OpinionSteps(double Amount);

	// 대상: "all"(모든 왕국) 또는 세력의 uuid(16자리 16진수).
	bool GoodFactionWho(const std::string& Who);

	struct DiplomacyCommand
	{
		std::string Who;							// "all" 또는 세력의 uuid
		DiplomacyGoal Goal = DiplomacyGoal::Neutral;
		char Side = 'b';
		double Amount = 0;							// Opinion 의 변화량
	};
	// 명령이 말이 되는가. 아니면 거짓이고 Why 에 까닭. 모든 왕국에게는 우호와 중립만 한다(적대와 평판의 수는 한 왕국씩).
	bool CheckDiplomacy(const DiplomacyCommand& Command, std::string& Why);

	// 한 왕국의 한쪽 평판을 움직인 결과를 글로. Side: 't' 또는 'u'. Before·After: 관계의 종류. Steps: 붙인 평판의 수(부호가 방향).
	// Outcome: 'd' 됐다(바라는 관계가 됐거나 정한 만큼 붙였다), 'a' 이미 그 관계였다, 'k' 건드리지 않는 관계다, 'l' 한도까지 붙였지만 되지 않았다,
	// 'f' 게임의 함수가 실패했다(Why 에 까닭).
	std::string DiplomacyReport(const std::string& Name, char Side, double Before, double After, int Steps, char Outcome, const std::string& Why);
	// 그 결과가 실패인가(여럿에게 할 때 "N/N"에 섞이지 않게).
	bool DiplomacyFailed(char Outcome);
}
