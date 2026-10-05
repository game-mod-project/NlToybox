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

	enum class EconomyAct { GoldAdd, GoldSet, ResourceAdd, ResourceSet, AllAdd };

	struct EconomyCommand
	{
		EconomyAct Act = EconomyAct::GoldAdd;
		int Resource = -1;		// ResourceAdd, ResourceSet 의 자원 번호
		double Amount = 0;		// 더할 수, 또는 맞출 수
	};

	// 원격 명령의 낱말: gold_add, gold_set, add, set, all. 모르는 낱말이면 거짓.
	bool ParseEconomyAct(const std::string& Word, EconomyAct& Out);
	// 그 명령이 자원 번호를 받는가(add, set).
	bool NeedsResource(EconomyAct Act);

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
