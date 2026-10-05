#include "CostBook.hpp"

#include <cmath>

namespace NlCore
{
	void CostBook::Remember(const std::string& Building, int Level, int Slot, double Value)
	{
		if (!std::isfinite(Value) || Value == 0)
			return;
		for (const CostEntry& entry : m_Entries)
			if (entry.Level == Level && entry.Slot == Slot && entry.Building == Building)
				return;
		m_Entries.push_back({ Building, Level, Slot, Value });
	}
}
