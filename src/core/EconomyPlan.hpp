#pragma once
// 경제 패널의 러너에 기대지 않는 부분: 자원의 이름, 명령을 변화량으로 푸는 일. tests/native 가 직접 부른다.
// 스펙: 치트 메뉴 §8(경제). 자리와 함수는 research/07.

#include <string>
#include <vector>

namespace NlCore
{
	// "resource.wood" → "wood". 접두가 없으면 그대로.
	std::string ResourceKey(const std::string& Caption);
	// 창에 보일 이름: "나무 (wood)". 모르는 키는 키 그대로. 한글 이름은 이 레포가 붙인 것이다.
	std::string ResourceLabel(const std::string& Key);
	// 창고 갈래의 이름: "food" → "음식". 모르는 키는 키 그대로.
	std::string CategoryLabel(const std::string& Key);

	// FloorSet, GoldFloor: 최소값(바닥)을 정한다. 변화량을 내지 않는다(모자란 것은 틱이 PlanFloors 로 채운다).
	enum class EconomyAct { GoldAdd, GoldSet, ResourceAdd, ResourceSet, AllAdd, FloorSet, GoldFloor };

	struct EconomyCommand
	{
		EconomyAct Act = EconomyAct::GoldAdd;
		int Resource = -1;		// ResourceAdd, ResourceSet, FloorSet 의 자원 번호
		double Amount = 0;		// 더할 수, 맞출 수, 또는 바닥
	};

	// 원격 명령의 낱말: gold_add, gold_set, add, set, all, floor, gold_floor. 모르는 낱말이면 거짓.
	bool ParseEconomyAct(const std::string& Word, EconomyAct& Out);
	// 그 명령이 자원 번호를 받는가(add, set, floor).
	bool NeedsResource(EconomyAct Act);
	// 바닥을 정하는 명령인가(floor, gold_floor).
	bool IsFloorAct(EconomyAct Act);

	// 신성 반지의 자원 번호: 자원의 열쇠들(번호 순) 가운데 "rune"의 자리. 없으면 -1.
	// 반지는 창고의 어느 갈래에도 들지 않지만 영지 창고의 그 칸에 있고 같은 함수로 바뀐다(화면의 반지 수가 따라왔다. research/18).
	int RingResource(const std::vector<std::string>& Keys);
	// 그 명령이 건드려도 되는 자원 번호들. 하나씩 하는 것(add, set, floor)은 갈래의 자원과 신성 반지, 모두에게 하는 것(all)은 갈래의 자원만
	// (반지가 갈래에 들어 있어도 뺀다). Ring 이 음수면(반지의 자리를 모른다) 넣지도 빼지도 않는다.
	std::vector<int> EconomyTargets(EconomyAct Act, const std::vector<int>& Stocked, int Ring);

	struct EconomyChange
	{
		int Resource = -1;		// -1 이면 금화
		double Delta = 0;
	};

	// 지금의 수를 보고 명령을 변화량들로 푼다. 넘기는 변화량은 언제나 유한한 정수다(게임의 함수에 그대로 간다). 0 인 변화는 뺀다.
	// 줄일 때는 줄일 수 있는 양을 넘지 않는다: 금화는 지금 수, 자원은 Counts 와 Free 가운데 작은 쪽(예약된 몫은 줄이지 않는다).
	// 읽은 수가 수가 아니면 그것은 하지 않는다.
	// Counts: 자원 번호 → 지금 수(맞추기의 기준). Free: 자원 번호 → 예약되지 않은 수(없으면 Counts 를 쓴다).
	// Stocked: 창고의 갈래에 든 자원 번호들. 여기에 없는 자원은 하나씩으로도 건드리지 않는다.
	std::vector<EconomyChange> PlanEconomy(const EconomyCommand& Command, double Gold, const std::vector<double>& Counts,
		const std::vector<double>& Free, const std::vector<int>& Stocked);

	// 최소값 유지. 자원마다(금화는 -1) 바닥을 두고, 화면의 수가 그보다 적으면 모자란 만큼 더한다.
	struct EconomyFloor
	{
		int Resource = -1;		// -1 이면 금화
		double Min = 0;			// 0 이하면 유지하지 않는다
	};

	// 저장하고 쓸 바닥: 정수로. 0 이하, 수가 아닌 것, 한도(10억)를 넘는 것은 0(유지하지 않는다). 큰 수를 한도로 당기지 않는다(잘못 친 수로 10억이 채워지지 않게).
	double FloorValue(double Asked);
	// 바닥으로 받을 수 있는 수인가: 유한하고 한도(10억)를 넘지 않는다(0 이하는 "지운다"는 뜻이라 받는다). 아니면 창이 까닭을 말하고 아무것도 바꾸지 않는다.
	bool GoodFloorAmount(double Asked);
	// 상태 파일에 적을 수 있는 열쇠인가(자원의 열쇠나 "gold": 소문자·숫자·밑줄, 40자까지).
	bool GoodFloorKey(const std::string& Key);

	// 최소값 칸의 편집. 치는 동안의 수는 들고만 있다가 칸을 떠날 때(Enter, Tab, 다른 곳을 누름) 한 번 넣는다.
	// Dear ImGui 의 수 입력 칸(InputScalar)은 "Enter 를 눌렀을 때만 참"(EnterReturnsTrue)을 지원하지 않는다(그 함수의 단언. 편집을 마친 것은
	// IsItemDeactivatedAfterEdit 로 보라고 적혀 있다). 그 플래그에 기대던 칸은 Enter 말고는 수를 넣을 길이 없었고, 칸을 떠나면 친 수가 버려졌다.
	struct FloorEdit
	{
		bool Has = false;		// 치고 있는 수가 있다
		double Value = 0;
	};
	// 프레임마다 칸을 그린 뒤에 부른다. Typed: 이번 프레임에 칸의 수가 바뀌었다(그 수가 Value). Left: 편집한 뒤 칸을 떠났다. Active: 칸이 아직 잡혀 있다.
	// 참이면 Out 을 넣는다(한 번만). 치지 않고 떠났으면 넣지 않고, 잡혀 있지도 떠나지도 않았는데 남은 수는 버린다.
	bool StepFloorEdit(FloorEdit& Edit, bool Typed, double Value, bool Left, bool Active, double& Out);

	// 바닥에 못 미치는 것들의 변화량(언제나 양의 정수). 바닥과 같거나 많은 것은 건드리지 않는다(줄이지 않는다).
	// Gold: 지금 금화. Free: 자원 번호 → 예약되지 않은 수(게임의 화면이 보이는 수). Allowed: 건드려도 되는 자원 번호들.
	// 읽은 수가 수가 아니면 그것은 하지 않는다. 같은 자원이 두 번 있으면 앞의 것만 본다. 채울 양이 한도(10억)를 넘으면 하지 않는다.
	std::vector<EconomyChange> PlanFloors(const std::vector<EconomyFloor>& Floors, double Gold, const std::vector<double>& Free,
		const std::vector<int>& Allowed);

	// 최소값 유지 한 바퀴의 결과. Ok 가 거짓이면 부른 쪽이 간격을 늘려 다시 한다(core/Retry).
	struct FloorRound
	{
		bool Ok = true;
		std::string Note;		// 치트 표의 항목 옆에 보일 글
	};
	// Kept: 지키는 바닥의 수(이 게임에 있는 것). Asked: 채우려던 것의 수. Called: 부른 것의 수.
	// Short: 불렀지만 앞뒤의 수가 청한 만큼 달라지지 않은 것의 수. Why: 호출이 안 된 까닭.
	FloorRound FloorReport(int Kept, int Asked, int Called, int Short, const std::string& Why);

	struct EconomyShort
	{
		int Resource = -1;		// -1 이면 금화
		double Asked = 0;		// 청한 변화량
		double Applied = 0;		// 앞뒤의 수로 본 변화량. 뒤의 수를 읽지 못했으면 0
	};

	// 한 변화들 가운데 앞뒤의 수가 청한 만큼 달라지지 않은 것들(용량에 걸렸다, 게임이 따르지 않았다).
	// 함수의 반환값에 기대지 않는다: 그것이 적용된 양인지는 모른다(research/07 의 반환은 둘 다 청한 수와 같았다).
	std::vector<EconomyShort> EconomyShortfall(const std::vector<EconomyChange>& Done, double GoldBefore, const std::vector<double>& Before,
		double GoldAfter, const std::vector<double>& After);
}
