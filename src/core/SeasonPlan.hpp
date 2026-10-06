#pragma once
// 계절(가혹한 계절)의 셈 가운데 러너에 기대지 않는 것(src/World.cpp 가 쓴다). 잰 것은 research/25.
// 게임의 단계는 시작 시각(__start_phase_time, 게임 시각의 초)에서 시작해 단계의 길이만큼 간다. 남은 시간은 게임도 그 시각에서 셈한다.

#include "PlaceKey.hpp"

#include <string>

namespace NlCore
{
	// 지금 단계가 끝나기까지 남은 초(게임의 __get_remain_time_of_current_phase 와 같은 셈).
	double PhaseRemain(double Now, double Start, double Duration);

	// 남은 시간을 "6일 2시간" 꼴로. 한 시간이 안 되면 "1시간 미만", 음수는 "0", 수가 아니면 "?".
	std::string SpanText(double Seconds);

	// 미루기: 시작 시각을 Seconds 초 뒤로 민 값. 지금보다 뒤로는 밀지 않는다(지나간 시간이 음수가 되지 않게).
	// 밀 것이 없으면(시작이 이미 지금이거나 지금보다 뒤다, 수를 읽지 못했다) false.
	bool DelaySeasonStart(double Now, double Start, double Seconds, double& Out);

	// 미룬 결과의 글. Moved: 실제로 밀린 초, Asked: 청한 초. 지금에 막혀 덜 밀렸으면 그렇게 말한다.
	std::string DelayReport(double Moved, double Asked);

	// 끝내기: 지금 단계의 남은 시간이 Lead 초가 되게 당긴 시작 시각. 이미 그만큼밖에 남지 않았거나 길이를 읽지 못했으면 false.
	// 0 보다 앞의 시각은 쓰지 않는다(그런 시작 시각을 게임에서 본 적이 없다): PhaseEndTooEarly 일 때도 false.
	bool EndPhaseStart(double Now, double Start, double Duration, double Lead, double& Out);
	// 게임을 시작한 지 단계의 길이만큼 지나지 않아, 끝내려면 0 보다 앞의 시작 시각을 써야 하는가.
	bool PhaseEndTooEarly(double Now, double Duration, double Lead);

	// 붙들기(지금 단계에 머물게 한다): 켠 뒤 처음 본 "지나간 시간"을 기억하고, 그 시간이 그대로이게 시작 시각을 따라 민다.
	struct SeasonHold
	{
		bool Has = false;
		double Elapsed = 0;		// 기억한 지나간 시간(초)
		double Phase = 0;		// 그때의 단계
		double Seen = 0;		// 마지막으로 본 게임 시각
		PlaceKey Place;			// 그때의 자리(어느 게임·어느 지도의 계절 관리자인가)
	};
	// 이번 틱에 시작 시각을 쓸 것이 있으면 true 와 쓸 값. 단계나 자리가 바뀌었거나 시각이 거꾸로 갔으면(다른 세이브, 다른 지도) 다시 기억하고 쓰지 않는다.
	bool StepSeasonHold(SeasonHold& Hold, bool On, double Now, double Start, double Phase, const PlaceKey& Place, double& Write);
	// 밖에서 시작 시각을 바꿨다(미루기, 끝내기): 다음 틱에 다시 기억하게 한다.
	void ForgetSeasonHold(SeasonHold& Hold);

	// 시작 시각을 쓴 뒤 게임의 함수가 돌려주는 남은 시간이 바라던 쪽으로(Later: 늘었다, 아니면 줄었다) 1초 넘게 움직였는가.
	bool RemainMoved(bool Later, double Before, double After);

	// "단계 1, 이 단계는 2일 2시간 남음". 게임의 단계는 0 부터이고 창에는 1 부터 센다.
	std::string PhaseNote(double Phase, double Remain);

	// 상태의 한 줄. Name 은 가혹한 계절의 화면 이름(없으면 빈 글). ToExtreme: 올 때까지, ToEnd: 끝날 때까지의 초.
	std::string SeasonLine(bool Extreme, const std::string& Name, double ToExtreme, double ToEnd);
}
