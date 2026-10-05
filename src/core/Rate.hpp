#pragma once
// 흐르는 값(게임 시간)이 실제 1초에 얼마나 느는지 잰다. 러너에 기대지 않는다. 스펙: 치트 메뉴 §9.

#include <deque>
#include <utility>

namespace NlCore
{
	class Rate
	{
	public:
		explicit Rate(double WindowSeconds = 1.0) : m_Window(WindowSeconds) {}

		// Seconds: 실제 시각(늘기만 한다). Value 가 줄면(새 게임, 불러오기) 처음부터 다시 잰다.
		void Add(double Seconds, double Value);
		void Reset() { m_Samples.clear(); }

		// 창의 절반 이상을 덮는 표본이 있는가.
		bool Ready() const;
		// 실제 1초에 느는 양. Ready 가 아니면 0.
		double PerSecond() const;

	private:
		double m_Window;
		std::deque<std::pair<double, double>> m_Samples;	// (시각, 값)
	};

	// 배율을 건 뒤의 흐름이 기대(Before × Factor)에서 Tolerance 비율 안인가. Before 나 Factor 가 0 이하이면 거짓.
	bool RateMatches(double Before, double After, double Factor, double Tolerance = 0.25);
}
