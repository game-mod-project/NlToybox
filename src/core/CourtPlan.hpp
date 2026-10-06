#pragma once
// 영주의 호감·충성 패널(src/Court.cpp)의 판단 가운데 러너에 기대지 않는 것. 잰 것은 research/20.
// 영주가 다른 영주를 보는 평판은 그 영주의 평판 구조체(OpinionMinds)에 든 평판들의 합이다. 수를 바로 쓰지 않고, 게임의 함수로 디버그 평판을 하나씩 붙이고 뗀다.
// 왕에 대한 충성은 왕을 보는 평판이다(SoulBasic.get_loyalty_to_king 이 그 수를 돌려준다).

#include <string>
#include <vector>

namespace NlCore
{
	// 충성 상태(SoulBasic.get_loyalty_state() 의 수)를 창에 보일 이름으로: 0 낮음, 1 보통, 2 높음. 모르는 수는 "?".
	// 왕에 대한 평판이 -19 일 때 0, -14 ~ 49 일 때 1, 55 일 때 2 였다(평판이 오르면 수도 오른다는 것까지 쟀다. 게임이 화면에 쓰는 이름은 보지 않았다).
	const char* LoyaltyLabel(double State);

	// 할 일. Raise: 평판이 목표에 닿을 때까지 올린다. Opinion: 정한 개수만큼 움직인다. Clear: 붙여 둔 디버그 평판을 모두 뗀다.
	// Release: 그 영주를 따르는 사람들의 충성 대상을 지운다(평판이 아니다).
	enum class CourtGoal { Raise, Opinion, Clear, Release };
	// 원격 명령의 낱말: loyal(왕을 보는 평판을 올린다. AboutKing 이 참), like, opinion, clear, release.
	bool ParseCourtGoal(const std::string& Word, CourtGoal& Out, bool& AboutKing);
	const char* CourtGoalWord(CourtGoal Goal);

	constexpr double k_CourtRaiseTo = 100;		// 올리기의 기본 목표
	constexpr double k_CourtRaiseMax = 200;		// 올리기의 목표의 한도(좋은 평판 50겹이면 +250 까지다)
	constexpr int k_CourtClearMax = 120;		// 떼기의 걸음 한도(겹침 한도 50 이 둘, 그리고 여유)

	struct CourtCommand
	{
		std::string Who;					// "lords"(플레이어의 영주 모두) 또는 영주의 uuid. 평판을 갖는 쪽이다(Release 에서는 주군)
		CourtGoal Goal = CourtGoal::Raise;
		std::string About;					// 누구를 보는 평판인가: "king", "lords", 영주의 uuid. Release 에서는 빈 글
		double Amount = 0;					// Opinion: 개수(부호가 방향, 절댓값 1 ~ 40). Raise: 목표(0 이면 k_CourtRaiseTo, 아니면 1 ~ 200 의 정수)
	};
	bool GoodCourtWho(const std::string& Who);
	bool GoodCourtAbout(const std::string& About);
	// 명령이 말이 되는가. 아니면 거짓이고 Why 에 까닭.
	bool CheckCourt(const CourtCommand& Command, std::string& Why);

	struct CourtLord
	{
		std::string Uuid;
		bool King = false;		// 플레이어 세력의 왕이다
	};
	struct CourtJob
	{
		std::string Holder, About;		// 평판을 갖는 영주와 대상의 uuid. Release 는 About 이 빈 글(Holder 가 주군)
		CourtGoal Goal = CourtGoal::Raise;
		int Left = 0;					// 더 해도 되는 걸음의 수(걸음마다 부른 쪽이 하나 줄인다)
		int Sign = 0;					// Opinion 의 방향
		double Target = 0;				// Raise 의 목표
	};
	// 명령을 일들로 푼다: 평판을 갖는 쪽과 대상의 짝마다 하나(자기 자신은 뺀다), Release 는 영주마다 하나.
	// 말이 안 되는 명령, 없는 영주, 왕이 없는 "king"은 일을 내지 않는다.
	std::vector<CourtJob> PlanCourtJobs(const CourtCommand& Command, const std::vector<CourtLord>& Lords);

	// 일의 한 걸음의 판단. Outcome 이 0 이면 Action 을 한다: 'A' 좋은 평판을 붙인다, 'D' 좋은 평판을 뗀다, 'a' 나쁜 평판을 붙인다, 'd' 나쁜 평판을 뗀다.
	// 아니면 끝났다: 'd' 됐다, 'a' 할 것이 없었다(이미 목표 이상, 붙여 둔 것이 없다), 'l' 이 명령의 한도까지 했지만 되지 않았다,
	//   's' 같은 평판의 겹침 한도라 더 붙일 수 없다, 'f' 걸을 수 없다(읽지 못한 수, 방향이 없는 일, 평판의 일이 아니다).
	// Opinion: 지금의 평판(get_opinion). Good·Bad: 그 대상에게 붙어 있는 좋은·나쁜 디버그 평판의 수(게임이 세어 준 수). Done: 이 일이 한 걸음의 수.
	// StackLimit: 그 평판의 자료에 적힌 겹침 한도. 좋은 것과 나쁜 것을 함께 두지 않는다: 올릴 때 나쁜 것이 있으면 그것부터 뗀다(내릴 때도 같다).
	struct CourtStep
	{
		char Outcome = 0;
		char Action = 0;
	};
	CourtStep PlanCourtStep(const CourtJob& Job, double Opinion, double Good, double Bad, int Done, double StackLimit);
	// 그 걸음이 됐는가: 한 뒤에 센 수가 바라는 대로 하나만 바뀌었다(붙이는 함수의 반환값으로 판정하지 않는다. research/19).
	bool CourtStepDone(char Action, double GoodBefore, double BadBefore, double GoodAfter, double BadAfter);

	// 이 일이 한 것.
	struct CourtDone
	{
		int GoodOn = 0, BadOn = 0, GoodOff = 0, BadOff = 0;
		int Total() const { return GoodOn + BadOn + GoodOff + BadOff; }
	};
	void CountCourtStep(CourtDone& Done, char Action);

	// 한 짝의 결과를 글로. Holder·About: 화면의 이름. Before·After: 평판. Outcome: PlanCourtStep 의 것('f' 는 Why 에 까닭).
	std::string CourtReport(const std::string& Holder, const std::string& About, const CourtJob& Job, double Before, double After, const CourtDone& Done,
		char Outcome, const std::string& Why);

	// 충성 대상 지우기의 결과: 'a' 따르는 사람이 없다, 'd' 모두 지웠다, 'f' 다 지우지 못했다.
	char ReleaseOutcome(int Followers, int Released);
	std::string ReleaseReport(const std::string& Lord, int Followers, int Released, const std::string& Why);
}
