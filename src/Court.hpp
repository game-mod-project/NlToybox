#pragma once
// 영주의 호감과 충성(스펙: 치트 메뉴 §8. 자리와 게임의 함수는 research/20).
// 영주가 다른 영주를 보는 평판은 평판들의 합이라 수를 바로 쓰지 않는다. 게임의 디버그용 평판을 게임의 함수로 하나씩 붙이고 떼어 움직인다(외교 패널과 같은 길).
// 왕에 대한 충성은 왕을 보는 평판이다. 주민·병사의 충성 대상(fealty)은 게임의 함수로 지운다.
// 영주들과 값은 틱에서 글로 떠 두고, 창의 단추는 일을 쌓고 틱이 조금씩 한다. 그리는 쪽은 러너를 부르지 않는다.

#include "core/CourtPlan.hpp"

#include <functional>
#include <string>
#include <vector>

namespace NlCourt
{
	using LogFn = std::function<void(const std::string&)>;

	void Init(LogFn Log);

	// 게임 스레드의 틱. Active: 영주 패널이나 종교 패널이 보이는가(보일 때만 영주들을 새로 읽는다. 쌓인 일은 보이지 않아도 한다).
	void GameTick(double Now, bool Active);

	// 그리는 쪽(영주 영역의 아래에 그린다).
	void Draw();
	// 종교 영역의 아래에 그린다: 주교가 우리 왕을 보는 평판(게임의 get_bishop_opinion 이 돌려주는 수)을 같은 길로 움직인다(research/21).
	void DrawBishop();

	// 원격 명령. 게임 스레드에서 부른다. 돌려주는 것: 답의 줄들.
	// Do 는 지금 끝까지 한다. Queue 는 창의 단추와 같은 길로 쌓기만 한다(틱이 조금씩 한다. 결과는 List 의 끝에 나온다).
	std::vector<std::string> Do(const NlCore::CourtCommand& Command);
	std::string Queue(const NlCore::CourtCommand& Command);
	std::vector<std::string> List();
}
