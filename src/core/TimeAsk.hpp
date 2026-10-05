#pragma once
// 시간의 멈춤·다시 흐르게(src/Cheats.cpp)가 돌려주는 글. 러너에 기대지 않는다. 잰 것은 research/15.

#include <string>

namespace NlCore
{
	enum class TimeCall { NotInGame, Failed, Called };

	// 멈춤(Pause)이나 다시 흐르게를 청한 결과의 글. 게임 화면이 아니면 부르지 않는다(메인 메뉴에서 부른 적이 없다).
	// 부른 뒤의 상태는 단정하지 않는다: time_warp 는 부른 틱에는 앞의 값으로 읽히고, 게임의 알림 창이 멈추고 있으면 풀리지 않을 수 있다.
	inline std::string TimeReport(bool Pause, TimeCall Outcome, const std::string& Why)
	{
		if (Outcome == TimeCall::NotInGame)
			return "게임 화면에서만 됩니다";
		if (Outcome == TimeCall::Failed)
			return std::string(Pause ? "멈춤을" : "다시 흐르게를") + " 부르지 못했습니다: " + Why;
		return Pause
			? "멈춤을 불렀습니다. time_warp 가 0 이 되면 멈춘 것입니다(다음 틱부터 그렇게 읽힙니다)"
			: "다시 흐르게를 불렀습니다(게임의 첫 속도 x1 로 풉니다). time_warp 가 0 으로 남으면 게임의 알림 창이 멈추고 있을 수 있습니다";
	}
}
