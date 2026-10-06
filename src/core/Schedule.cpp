#include "Schedule.hpp"

namespace NlCore
{
	Schedule::Schedule(double DelaySeconds, double RepeatSeconds, int KeepLast)
		: m_Repeat(RepeatSeconds), m_KeepLast(KeepLast < 1 ? 1 : KeepLast), m_NextAt(DelaySeconds)
	{
	}

	std::string Schedule::Due(double Now) const
	{
		if (Exhausted() || Now < m_NextAt)
			return "";
		if (m_Count == 0)
			return "menu";
		return "late" + std::to_string((m_Count - 1) % m_KeepLast);
	}

	void Schedule::Finished(double Now)
	{
		m_Count++;
		m_NextAt = Now + m_Repeat;
	}
}
