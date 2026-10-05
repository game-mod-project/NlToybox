#pragma once
// 덤프의 일정. 첫 덤프는 delay 뒤의 "menu" 이고, 되풀이하면 그 뒤로 "late0", "late1" … 를 돌려 쓴다.
// 스펙: 데이터 오버레이 §3.7.

#include <string>

namespace NlCore
{
	class Schedule
	{
	public:
		Schedule() = default;
		Schedule(double DelaySeconds, double RepeatSeconds, int KeepLast);

		// 지금 덤프할 때인가. 때가 됐으면 그 덤프의 이름을, 아니면 빈 글을 돌려준다.
		std::string Due(double Now) const;

		// 덤프 하나가 끝났다고 알린다. 다음 덤프는 이 시각에서 RepeatSeconds 뒤다
		// (덤프가 오래 걸려도 다음 덤프가 바로 겹치지 않는다).
		void Finished(double Now);

		bool Repeats() const { return m_Repeat > 0; }

		// 더 할 덤프가 없는가(되풀이하지 않는 요청에서 첫 덤프가 끝났다).
		bool Exhausted() const { return m_Count > 0 && m_Repeat <= 0; }

		// 지금까지 끝난 덤프의 수.
		int Count() const { return m_Count; }

	private:
		double m_Repeat = 0;
		int m_KeepLast = 1;
		int m_Count = 0;
		double m_NextAt = 0;
	};
}
