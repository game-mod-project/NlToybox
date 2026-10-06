#pragma once
// "외교" 영역의 패널: 왕국들과의 관계를 보이고, 게임의 함수로 왕의 평판에 디버그 평판(±5)을 붙여 관계를 움직인다(research/19).
// 관계의 종류는 게임이 평판에서 다시 셈한다(행렬에 바로 쓴 값은 되돌아간다). 그래서 평판을 붙이고 게임의 다시 셈하기를 부른다.
// 창(Draw)은 일을 쌓기만 하고, 게임 스레드의 틱이 한다.

#include "core/DiplomacyPlan.hpp"

#include <functional>
#include <string>
#include <vector>

namespace NlDiplomacy
{
	using LogFn = std::function<void(const std::string&)>;

	void Init(LogFn Log);

	// 게임 스레드의 틱. Active: 외교 패널이 보이는가(보일 때만 관계를 다시 읽는다). 쌓인 일은 보이지 않아도 한다.
	void GameTick(double Now, bool Active);

	// 패널을 그린다. 러너를 부르지 않는다.
	void Draw();

	// 명령을 지금 끝까지 한다(원격 명령 diplomacy). 게임 스레드에서만 부른다. 돌려주는 것: 한 일(줄마다).
	std::vector<std::string> Do(const NlCore::DiplomacyCommand& Command);

	// 명령을 창의 단추처럼 쌓기만 한다(원격 diplomacy … queue=1). 틱이 조금씩 한다. 돌려주는 글: 쌓은 일의 수나 거부한 까닭.
	std::string Queue(const NlCore::DiplomacyCommand& Command);

	// 왕국들과 지금의 관계(원격 diplomacy list). 게임 스레드에서만 부른다.
	std::vector<std::string> List();
}
