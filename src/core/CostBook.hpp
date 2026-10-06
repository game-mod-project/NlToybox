#pragma once
// 건물 종류의 건설비를 0 으로 쓰기 전에 본 값의 장부. 끌 때 이 값으로 되돌린다. 러너에 기대지 않는다.
// 자리: 건물 종류의 이름, 등급(…__construction_cost.levels[등급]), 칸(자원 번호. -1 이면 금화). research/09.

#include <string>
#include <vector>

namespace NlCore
{
	struct CostEntry
	{
		std::string Building;
		int Level = 0;
		int Slot = 0;			// 자원 번호. -1 이면 금화(money)
		double Value = 0;		// 처음 본 값
	};

	class CostBook
	{
	public:
		// 처음 본 값을 기억한다. 0 이거나 유한한 수가 아니면 기억하지 않는다(바꿀 것이 없거나 되돌릴 수 없다).
		// 이미 기억한 자리는 바꾸지 않는다: 0 으로 쓴 뒤에 다시 보면 0 이 보이고, 그것은 원래 값이 아니다.
		// 돌려주는 값: 이 자리에 되돌릴 값이 있는가(방금 기억했거나 이미 기억했다). 거짓이면 그 자리에 0 을 쓰지 않는다.
		bool Remember(const std::string& Building, int Level, int Slot, double Value);

		// 그 자리의 처음 본 값(바탕). 기억한 적이 없으면 거짓이고 Value 는 그대로다. Index 를 주면 Entries() 에서의 번호도 돌려준다.
		bool Find(const std::string& Building, int Level, int Slot, double& Value, size_t* Index = nullptr) const;

		// 되돌린 자리를 잊는다. Done[i] 가 0 이 아니면 Entries()[i] 를 지운다. 못 되돌린 자리는 남는다(다음에 다시 되돌린다).
		// Done 의 수가 장부의 수와 다르면 아무것도 지우지 않는다.
		void Forget(const std::vector<char>& Done);

		const std::vector<CostEntry>& Entries() const { return m_Entries; }
		size_t Size() const { return m_Entries.size(); }
		bool Empty() const { return m_Entries.empty(); }
		void Clear() { m_Entries.clear(); }

	private:
		std::vector<CostEntry> m_Entries;		// 기억한 차례대로
	};

	// 자료의 한 자리에 쓸 값을 정한다(자료를 돌며 배율을 쓰는 치트. src/Production.cpp).
	// Target: 바라는 배율. 1 이면 처음 본 값으로 되돌린다. Zero: 배율을 곱하는 대신 0 을 쓴다(Target 이 1 이 아닐 때).
	// 처음 보는 자리는 지금 값을 바탕으로 장부에 적는다. 배율은 언제나 바탕에 곱한다(써 둔 값을 다시 봐도 두 번 곱하지 않는다).
	// 돌려주는 값: 이 자리를 다루는가. 거짓이면 건드리지 않는다(되돌리는 중인데 건드린 적이 없는 자리, 0 이거나 유한하지 않아 장부가 받지 않는 자리).
	// Wanted: 그 자리에 있어야 할 값(지금 값과 같으면 쓸 것이 없다). Index: 장부에서의 번호(되돌린 자리를 Forget 에 넘길 때 쓴다).
	bool PlanValue(CostBook& Book, const std::string& Key, int Level, int Slot, double Current, double Target, bool Zero, double& Wanted, size_t& Index);
}
