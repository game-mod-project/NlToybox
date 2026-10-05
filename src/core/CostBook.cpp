#include "CostBook.hpp"

#include "Knobs.hpp"

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

	bool CostBook::Find(const std::string& Building, int Level, int Slot, double& Value, size_t* Index) const
	{
		for (size_t i = 0; i < m_Entries.size(); i++)
		{
			const CostEntry& entry = m_Entries[i];
			if (entry.Level == Level && entry.Slot == Slot && entry.Building == Building)
			{
				Value = entry.Value;
				if (Index)
					*Index = i;
				return true;
			}
		}
		return false;
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

	bool PlanValue(CostBook& Book, const std::string& Key, int Level, int Slot, double Current, double Target, bool Zero, double& Wanted, size_t& Index)
	{
		const bool restoring = Target == 1;
		double base = Current;
		if (!Book.Find(Key, Level, Slot, base, &Index))
		{
			if (restoring || !Book.Remember(Key, Level, Slot, Current))
				return false;
			Index = Book.Size() - 1;
		}
		Wanted = restoring ? base : Zero ? 0 : Scale(base, Target);
		return true;
	}
}
