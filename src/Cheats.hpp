#pragma once
// 치트 표(core/CheatTable)의 항목을 게임에 적용하고 영역의 패널을 그린다. 게임 속도도 여기서 건다.
// 스펙: 치트 메뉴 §6.2, §9. 창(Draw*)은 바라는 상태만 바꾸고, 값을 써 넣는 것은 GameTick 이다.

#include "core/CheatState.hpp"
#include "core/CheatTable.hpp"

#include <functional>
#include <map>
#include <set>
#include <string>

namespace NlCheats
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번. State 는 KeepKnown 을 거친 것이다. 켜져 있던 항목은 대상이 생기면 다시 적용된다.
	void Init(LogFn Log, const NlCore::CheatState& State);

	// 게임 스레드의 틱. Now: 모듈이 뜬 뒤의 초. Visible: 모드창이 열려 있는가(닫혀 있으면 켠 것만 건드린다).
	void GameTick(double Now, bool Visible);

	// 그 영역의 항목들을 그린다.
	void DrawArea(NlCore::Area Where);
	// "시간" 영역: 게임 속도.
	void DrawTime();

	// 그 영역에 표의 항목이 있는가.
	bool HasItems(NlCore::Area Where);

	// 켠 것이 바뀌었으면 상태 파일에 적을 것을 채우고 참을 돌려준다.
	bool TakeChanges(std::set<std::string>& On, std::map<std::string, double>& Numbers);

	// 켜 둔 항목의 수(게임 속도를 걸었으면 하나 더).
	int ActiveCount();
	// 모두 끄고 원래 값으로 되돌린다.
	void ReleaseAll();
}
