#pragma once
// 실패한 일을 언제 다시 해 볼지. 실패가 이어지면 간격을 두 배씩 늘린다(게임 스레드와 로그를 실패한 일로 채우지 않는다).

namespace NlCore
{
	class Retry
	{
	public:
		// First: 첫 실패 뒤의 간격(초). Cap: 가장 긴 간격.
		Retry(double First, double Cap) : m_First(First), m_Cap(Cap), m_Delay(First) {}

		// 지금 해 봐도 되는가. 실패한 적이 없으면 언제나 참.
		bool Due(double Now) const { return Now >= m_Next; }
		int Failures() const { return m_Failures; }

		void Failed(double Now)
		{
			m_Next = Now + m_Delay;
			m_Delay = m_Delay * 2 < m_Cap ? m_Delay * 2 : m_Cap;		// std::min 은 쓰지 않는다: 모듈 쪽에서는 windows.h 의 min 매크로와 부딪친다
			m_Failures++;
		}

		void Succeeded()
		{
			m_Next = 0;
			m_Delay = m_First;
			m_Failures = 0;
		}

	private:
		double m_First, m_Cap, m_Delay;
		double m_Next = 0;
		int m_Failures = 0;
	};
}
