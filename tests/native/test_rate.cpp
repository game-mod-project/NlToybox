#include "common.hpp"

void RunRateTests()
{
	Test("흐름: 일정하게 느는 값의 빠르기를 잰다", [] {
		Rate rate(1.0);
		CHECK(!rate.Ready() && rate.PerSecond() == 0);
		for (int i = 0; i <= 4; i++)
			rate.Add(i * 0.1, 100 + i * 5.74);			// 실제 1초에 57.4
		CHECK(!rate.Ready());								// 0.4초: 창의 절반이 안 된다
		for (int i = 5; i <= 30; i++)
			rate.Add(i * 0.1, 100 + i * 5.74);
		CHECK(rate.Ready() && std::abs(rate.PerSecond() - 57.4) < 1e-6);
	});

	Test("흐름: 창보다 오래된 표본은 잊는다", [] {
		Rate rate(1.0);
		double value = 0;
		for (int i = 0; i <= 20; i++)						// 2초 동안 1초에 10
			rate.Add(i * 0.1, value = i * 1.0);
		for (int i = 21; i <= 35; i++)						// 그 뒤 1.5초 동안 1초에 50
			rate.Add(i * 0.1, value += 5.0);
		CHECK(std::abs(rate.PerSecond() - 50) < 1e-6);
	});

	Test("흐름: 값이 줄면 처음부터 다시 잰다", [] {
		Rate rate(1.0);
		for (int i = 0; i <= 20; i++)
			rate.Add(i * 0.1, 1000 + i);
		CHECK(rate.Ready());
		rate.Add(2.1, 5);									// 새 게임: 게임 시간이 처음으로 돌아갔다
		CHECK(!rate.Ready() && rate.PerSecond() == 0);
		rate.Reset();
		CHECK(!rate.Ready());
	});

	Test("흐름: 기대한 배율인지 가린다", [] {
		CHECK(RateMatches(57.4, 114.8, 2));
		CHECK(RateMatches(57.4, 100, 2));					// 13% 모자라다. 허용 안이다
		CHECK(!RateMatches(57.4, 57.4, 2));				// 그대로다
		CHECK(!RateMatches(57.4, 0, 2));
		CHECK(!RateMatches(0, 100, 2));					// 누르기 전에 멈춰 있었다
		CHECK(RateMatches(57.4, 14.35, 0.25));
	});
}
