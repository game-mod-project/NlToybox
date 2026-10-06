#pragma once
// 임신·출생·성장(research/24). 러너를 부르지 않는 판단만 둔다.
// 임신의 단계는 특성으로 보인다(pregnant_st1, st2, st3). 임신 구성요소(__soul.__pregnancy: ComponentPregnancy)의 debug_pregnancy_next_stage()(인자 없음)를
// 임신 1/3기의 영주에게 세 번 불러 2/3기, 3/3기, 출산(아이가 생기고 어머니에게 pregnant_forbid 가 붙는다)까지 가는 것을 봤다.
// 그 함수는 게임의 확률을 그대로 탄다: 다른 실행에서는 3/3기의 다음 호출이 유산으로 끝났다(아이가 생기지 않고 어머니에게 생각 pregnancy_miscarriage 가 붙었다).
// **begin_pregnant() 는 부르지 않는다**: 아버지의 uuid 를 적고 불렀는데 게임이 끝났다("I32 argument is undefined", 그 함수의 205번째 줄).

#include <string>
#include <vector>

namespace NlCore
{
	// 임신의 단계: 특성 pregnant_st1·st2·st3 이면 1·2·3, 아니면 0.
	int PregnancyStage(const std::vector<std::string>& Traits);
	// 그 단계의 특성 이름(1 ~ 3). 아니면 빈 글.
	const char* PregnancyTrait(int Stage);
	// 아이인가(특성 kid).
	bool IsKid(const std::vector<std::string>& Traits);
	// 어른으로 만들 때 맞추는 나이. 15, 16 에서는 아이 그대로였고 18 에서 게임이 kid 를 떼고 untitled_lord(소영주)를 붙였다(진영은 player_untitled 가 됐다).
	constexpr double k_GrownAge = 18;
	// 출산 뒤에 어머니에게 붙는 특성(이름으로 보아 다시 임신하지 못한다. 그 판정을 재지는 않았다).
	constexpr const char* k_PregnantForbid = "pregnant_forbid";
	// 다음 단계 함수를 한 사람에게 잇달아 부르는 한도(1/3기에서 출산까지 세 번이었다).
	constexpr int k_StageCallsMax = 3;

	// 다음 단계 함수를 한 번 부른 뒤, 앞뒤의 단계로 결과를 가른다. 본 것(1→2, 2→3, 3→0)만 된 것으로 친다.
	enum class StageOutcome { Advanced, Born, Stuck };
	StageOutcome AfterStageCall(int Before, int After);
	// "바로 출산"에서 한 번 더 부를 것인가: 임신 중이고 부른 횟수가 한도 아래일 때.
	bool BirthNeedsCall(int Stage, int Calls);
	// 결과의 글. Birth: "바로 출산"을 청했는가. First: 부르기 전의 단계, Last: 마지막에 읽은 단계(부른 뒤 읽지 못했으면 -1), Calls: 부른 횟수,
	// Why: 멈춘 까닭(없으면 빈 글), Children: 부르는 동안 새로 생긴 사람의 수.
	// 임신이 끝났는데 아이가 생기지 않았으면 출산이라고 말하지 않는다(유산일 수 있다). 출산을 청했는데 중간에 멈췄으면 그렇게 말한다.
	std::string StageReport(const std::string& Name, bool Birth, int First, int Last, int Calls, const std::string& Why, int Children);
	// "다음 단계"가 된 것인가: 한 번 불렀고 본 대로(1→2, 2→3, 3→0) 바뀌었고 멈춘 까닭이 없다.
	bool NextDone(int First, int Last, int Calls, const std::string& Why);
	// "바로 출산"이 된 것인가: 임신이 끝났고(Last 0) 아이가 생겼고 멈춘 까닭이 없다.
	bool BirthDone(int Last, int Children, const std::string& Why);

	// 성별: 게임의 SoulBasic.get_gender() -> 1 | 0. 임신한 영주가 1 이었고 그 아이의 아버지가 0 이었다.
	constexpr double k_Female = 1, k_Male = 0;
	// 임신을 시작할 수 있는가(누르기 전의 판정. 게임의 판정 is_can_pregant()는 틱이 따로 묻는다). 아니면 거짓이고 Why 에 까닭.
	bool CanConceive(double Gender, const std::vector<std::string>& Traits, std::string& Why);
	// 아버지로 삼을 수 있는가: 남성이고 아이가 아니다.
	bool CanFather(double Gender, const std::vector<std::string>& Traits);

	// 게임 변수(global.__gameplay_vars)의 열쇠들. 값은 게임에서 읽는다(레포에 옮기지 않는다). 게임이 그 값을 따르는지는 보지 못했다.
	const std::vector<const char*>& PregnancyChanceVars();		// 임신 확률(영주끼리, 주민과)
	const std::vector<const char*>& MiscarriageVars();			// 유산 확률
	const std::vector<const char*>& ChildbirthDeathVars();		// 출산 중 어머니가 죽을 확률(바탕과 난산 특성의 것)
}
