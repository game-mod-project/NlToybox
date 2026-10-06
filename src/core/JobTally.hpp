#pragma once
// 쌓인 일들(외교·궁정의 걸음)의 결과를 세고 줄을 모은다. 외교가 만든 것을 궁정도 써서 이름을 일에 맞게 바꿨다(DiplomacyTally → JobTally. 2026-10-07 리뷰 R9).
// 러너에 기대지 않는다. tests/native 가 시험한다.

#include <string>
#include <vector>

namespace NlCore
{
	// 일 하나의 결과 글자: 'd' 바뀌었다, 'l'·'f'·'s' 실패(한도, 실패, 멈춤), 그 밖은 그대로였다. 그 결과가 실패인가.
	bool JobFailed(char Outcome);

	// 명령 하나(또는 쌓인 일들)의 결과를 세고 줄을 모은다. 실패한 줄을 앞에 둔다(창은 앞의 몇 줄만 보인다).
	// "이미 그 관계였다"와 "건드리지 않았다"는 한 것에 넣지 않는다(그대로 둔 것).
	class JobTally
	{
	public:
		void Reset() { *this = JobTally(); }
		// 일이 그만큼 쌓였다.
		void Expect(int Jobs) { m_Asked += Jobs > 0 ? Jobs : 0; }
		// 일 하나가 끝났다.
		void Add(char Outcome, const std::string& Line);
		// 하지 못하고 버린 일들(게임 화면을 떠났다, 다시 읽지 못했다). 실패로 센다.
		void Drop(int Jobs, const std::string& Why);
		bool Empty() const { return m_Asked == 0 && m_Lines.empty(); }
		int Asked() const { return m_Asked; }
		int Changed() const { return m_Changed; }
		int Same() const { return m_Same; }
		int Failed() const { return m_Failed; }
		int Pending() const { return m_Asked - m_Changed - m_Same - m_Failed; }
		const std::vector<std::string>& Lines() const { return m_Lines; }
		std::string Summary() const;

	private:
		int m_Asked = 0, m_Changed = 0, m_Same = 0, m_Failed = 0;
		size_t m_FailedLines = 0;			// 줄들의 앞쪽에 있는 실패한 줄의 수
		std::vector<std::string> m_Lines;
	};
}
