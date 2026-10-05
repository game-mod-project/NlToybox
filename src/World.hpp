#pragma once
// 종교·이벤트 영역에서 한 번 하는 일(research/14): 이벤트 쿨다운 지우기, 주교 부르기.
// 그리는 쪽은 청만 쌓고 러너는 틱이 건드린다.

#include "core/WorldPlan.hpp"

#include <functional>
#include <string>

namespace NlWorld
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번.
	void Init(LogFn Log);

	// 게임 스레드의 틱. 쌓인 청을 하나 한다.
	void GameTick();

	void DrawEvents();		// 이벤트: 쿨다운 지우기
	void DrawReligion();	// 종교: 주교 부르기

	// 원격 명령(창 없이 같은 길을 태운다). 게임 스레드에서 부른다. 돌려주는 것: 한 일.
	std::string Do(NlCore::WorldAct Act);
}
