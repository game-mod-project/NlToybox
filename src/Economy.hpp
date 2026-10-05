#pragma once
// "경제" 영역의 위쪽: 금화와 영지 창고의 자원. 값을 바로 쓰지 않고 게임의 함수로 바꾼다(그래야 화면이 따라온다. research/07).
// 창(Draw)은 명령만 쌓고, 게임 스레드의 틱이 지금 값을 읽어 명령을 변화량으로 풀고(core/EconomyPlan) 부른다.

#include "core/EconomyPlan.hpp"

#include <functional>
#include <string>

namespace NlEconomy
{
	using LogFn = std::function<void(const std::string&)>;

	void Init(LogFn Log);

	// 게임 스레드의 틱. Active: 경제 패널이 보이는가(보일 때만 값을 다시 읽는다). 쌓인 명령은 보이지 않아도 한다.
	void GameTick(double Now, bool Active);

	// 패널을 그린다. 러너를 부르지 않는다.
	void Draw();

	// 명령 하나를 지금 한다. 게임 스레드에서만 부른다(원격 명령 economy). 돌려주는 글: 한 일.
	std::string Do(const NlCore::EconomyCommand& Command);
}
