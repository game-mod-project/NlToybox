#include "SpeedTrial.hpp"

#include "Rate.hpp"

namespace NlCore
{
	SpeedTrial::SpeedTrial(int Candidates, double HoldSeconds, double SettleSeconds)
		: m_Count(Candidates), m_Hold(HoldSeconds), m_Settle(SettleSeconds)
	{
	}

	bool SpeedTrial::Start(double BaseRate, double BaseWarp)
	{
		if (Running() || BaseRate <= 0 || BaseWarp <= 0 || m_Count <= 0)
			return false;

		m_Phase = Phase::Holding;
		m_Candidate = 0;
		m_Began = false;
		m_Sticky = false;
		m_BaseRate = BaseRate;
		m_BaseWarp = BaseWarp;
		m_Probe = BaseWarp * 2;
		m_Attempts.clear();
		return true;
	}

	void SpeedTrial::Next()
	{
		m_Began = false;
		if (++m_Candidate >= m_Count)
		{
			m_Candidate = -1;
			m_Phase = Phase::Failed;
		}
	}

	SpeedStep SpeedTrial::Tick(double Now, bool Readable, double Current, double Rate)
	{
		if (m_Phase == Phase::Holding)
		{
			if (!m_Began)
			{
				if (!Readable)		// 읽을 수 없는 후보는 쓰지 않는다(없는 변수를 만들지 않는다)
				{
					m_Attempts.push_back({ m_Candidate, false, 0, Rate, false });
					Next();
					return {};
				}
				m_Began = true;
				m_Old = Current;
				m_Until = Now + m_Hold;
				return { SpeedStep::Kind::Write, m_Candidate, m_Probe };
			}
			if (Now < m_Until)
				return { SpeedStep::Kind::Write, m_Candidate, m_Probe };

			const SpeedStep restore{ SpeedStep::Kind::Restore, m_Candidate, m_Old };
			if (Rate <= 0)			// 시험 중에 멈췄다(일시정지). 판정할 수 없다
			{
				m_Phase = Phase::Aborted;
				return restore;
			}

			const bool matched = RateMatches(m_BaseRate, Rate, m_Probe / m_BaseWarp);
			m_Attempts.push_back({ m_Candidate, true, m_Old, Rate, matched });
			if (!matched)
			{
				Next();
				return restore;
			}
			m_Phase = Phase::Settling;
			m_Until = Now + m_Settle;
			return {};
		}

		if (m_Phase == Phase::Settling && Now >= m_Until)
		{
			m_Sticky = RateMatches(m_BaseRate, Rate, m_Probe / m_BaseWarp);
			m_Phase = Phase::Done;
		}
		return {};
	}
}
