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
	// 그 명령이 건드려도 되는 자원 번호들. 하나씩 하는 것(add, set, floor)은 갈래의 자원과 신성 반지, 모두에게 하는 것(all)은 갈래의 자원만.
	// Ring 이 음수면(반지의 자리를 모른다) 넣지 않는다.
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

	// 저장하고 쓸 바닥: 정수로, 0 이하와 수가 아닌 것은 0(유지하지 않는다), 한도(10억)까지.
	double FloorValue(double Asked);
	// 상태 파일에 적을 수 있는 열쇠인가(자원의 열쇠나 "gold": 소문자·숫자·밑줄, 40자까지).
	bool GoodFloorKey(const std::string& Key);

	// 바닥에 못 미치는 것들의 변화량(언제나 양의 정수). 바닥과 같거나 많은 것은 건드리지 않는다(줄이지 않는다).
	// Gold: 지금 금화. Free: 자원 번호 → 예약되지 않은 수(게임의 화면이 보이는 수). Allowed: 건드려도 되는 자원 번호들.
	// 읽은 수가 수가 아니면 그것은 하지 않는다. 같은 자원이 두 번 있으면 앞의 것만.
	std::vector<EconomyChange> PlanFloors(const std::vector<EconomyFloor>& Floors, double Gold, const std::vector<double>& Free,
		const std::vector<int>& Allowed);

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
