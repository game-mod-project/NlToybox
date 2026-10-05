#include "CostBook.hpp"

#include <cmath>

namespace NlCore
{
	bool CostBook::Remember(const std::string& Building, int Level, int Slot, double Value)
	{
		for (const CostEntry& entry : m_Entries)
			if (entry.Level == Level && entry.Slot == Slot && entry.Building == Building)
				return true;
		if (!std::isfinite(Value) || Value == 0)
			return false;
		m_Entries.push_back({ Building, Level, Slot, Value });
		return true;
	}

	void CostBook::Forget(const std::vector<char>& Done)
	{
		if (Done.size() != m_Entries.size())
			return;
		std::vector<CostEntry> kept;
		for (size_t i = 0; i < m_Entries.size(); i++)
			if (!Done[i])
				kept.push_back(std::move(m_Entries[i]));
		m_Entries = std::move(kept);
	}
}
