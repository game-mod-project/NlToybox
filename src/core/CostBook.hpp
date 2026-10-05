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
		// 처음 본 값을 기억한다. 0 이거나 수가 아니면 기억하지 않는다(바꿀 것이 없다).
		// 이미 기억한 자리는 바꾸지 않는다: 0 으로 쓴 뒤에 다시 보면 0 이 보이고, 그것은 원래 값이 아니다.
		void Remember(const std::string& Building, int Level, int Slot, double Value);

		const std::vector<CostEntry>& Entries() const { return m_Entries; }
		size_t Size() const { return m_Entries.size(); }
		bool Empty() const { return m_Entries.empty(); }
		void Clear() { m_Entries.clear(); }

	private:
		std::vector<CostEntry> m_Entries;		// 기억한 차례대로
	};
}
