#include "Rate.hpp"

#include <cmath>

namespace NlCore
{
	void Rate::Add(double Seconds, double Value)
	{
		if (!m_Samples.empty() && (Value < m_Samples.back().second || Seconds <= m_Samples.back().first))
			m_Samples.clear();
		m_Samples.push_back({ Seconds, Value });

		// 창보다 오래된 표본을 버린다. 창을 덮는 가장 오래된 표본 하나는 남긴다.
		while (m_Samples.size() > 2 && Seconds - m_Samples[1].first >= m_Window)
			m_Samples.pop_front();
	}

	bool Rate::Ready() const
	{
		return m_Samples.size() >= 2 && m_Samples.back().first - m_Samples.front().first >= m_Window * 0.5;
	}

	double Rate::PerSecond() const
	{
		if (!Ready())
			return 0;
		return (m_Samples.back().second - m_Samples.front().second) / (m_Samples.back().first - m_Samples.front().first);
	}

	bool RateMatches(double Before, double After, double Factor, double Tolerance)
	{
		if (Before <= 0 || Factor <= 0)
			return false;
		const double expected = Before * Factor;
		return std::abs(After - expected) <= expected * Tolerance;
	}
}
