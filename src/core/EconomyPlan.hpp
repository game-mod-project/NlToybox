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

	// 지금의 수를 보고 명령을 변화량들로 푼다. 수는 정수로 맞추고, 0 인 변화는 뺀다. 줄일 때 0 아래로 내려가지 않게 한다.
	// Counts: 자원 번호 → 지금 수. Stocked: "모든 자원"에 드는 자원 번호들(창고의 갈래에 든 것).
	std::vector<EconomyChange> PlanEconomy(const EconomyCommand& Command, double Gold, const std::vector<double>& Counts,
		const std::vector<int>& Stocked);
}
