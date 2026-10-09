#pragma once
// 수 입력 칸의 편집. 치는 동안의 수는 들고만 있다가 칸을 떠날 때(Enter, Tab, 다른 곳을 누름) 한 번 넣는다. 러너에 기대지 않는다.
// Dear ImGui 의 수 입력 칸(InputScalar)은 "Enter 를 눌렀을 때만 참"(EnterReturnsTrue)을 지원하지 않는다(그 함수의 단언. 편집을 마친 것은
// IsItemDeactivatedAfterEdit 로 보라고 적혀 있다). 그 플래그에 기대던 칸은 Enter 말고는 수를 넣을 길이 없었고, 칸을 떠나면 친 수가 버려졌다(research/18 의 끝).
// 최소값 칸(0.24.2)에서 고친 것을 모든 수 입력 칸(치트의 수·배율, 탐색기의 값·잠금 값)으로 넓혔다(2026-10-07 리뷰 R1). 그리는 쪽은 NlUi::InputNumber 를 쓴다.

namespace NlCore
{
	struct NumberEdit
	{
		bool Has = false;		// 치고 있는 수가 있다
		double Value = 0;
	};
	// 프레임마다 칸을 그린 뒤에 부른다. Typed: 이번 프레임에 칸의 수가 바뀌었다(그 수가 Value). Left: 편집한 뒤 칸을 떠났다. Active: 칸이 아직 잡혀 있다.
	// 참이면 Out 을 넣는다(한 번만). 치지 않고 떠났으면 넣지 않고, 잡혀 있지도 떠나지도 않았는데 남은 수는 버린다.
	bool StepNumberEdit(NumberEdit& Edit, bool Typed, double Value, bool Left, bool Active, double& Out);
}
