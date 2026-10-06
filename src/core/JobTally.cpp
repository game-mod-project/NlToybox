#include "JobTally.hpp"

namespace NlCore
{
	bool JobFailed(char Outcome)
	{
		return Outcome == 'l' || Outcome == 'f' || Outcome == 's';
	}

	void JobTally::Add(char Outcome, const std::string& Line)
	{
		if (JobFailed(Outcome))
		{
			m_Failed++;
			m_Lines.insert(m_Lines.begin() + static_cast<std::ptrdiff_t>(m_FailedLines), Line);
			m_FailedLines++;
			return;
		}
		(Outcome == 'd' ? m_Changed : m_Same)++;
		m_Lines.push_back(Line);
	}

	void JobTally::Drop(int Jobs, const std::string& Why)
	{
		if (Jobs <= 0)
			return;
		m_Failed += Jobs;
		m_Lines.insert(m_Lines.begin() + static_cast<std::ptrdiff_t>(m_FailedLines), "하지 못하고 버린 일 " + std::to_string(Jobs) + "개: " + Why);
		m_FailedLines++;
	}

	std::string JobTally::Summary() const
	{
		std::string text = std::to_string(m_Asked) + "개 가운데 한 것 " + std::to_string(m_Changed) + "개, 그대로 둔 것 " + std::to_string(m_Same)
			+ "개, 안 된 것 " + std::to_string(m_Failed) + "개";
		if (Pending() > 0)
			text += " (남은 일 " + std::to_string(Pending()) + "개)";
		return text;
	}
}
