#pragma once
// 지식 탭의 "도서관의 책"(research/33). 게임의 도서관(LibraryManager)에 있는 책을 보이고, 책을 넣고 뺀다: 게임이 책 사본이 만들어질 때 부르는
// change_books(지식 구조체, 1)을 같은 꼴로 부른다. 틱이 읽어 글로 두고 그리는 쪽은 그것만 그린다. 단추는 청만 쌓고 러너는 틱이 건드린다.

#include "core/LibraryPlan.hpp"

#include <functional>
#include <string>
#include <vector>

namespace NlLibrary
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번.
	void Init(LogFn Log);

	// 게임 스레드의 틱. 쌓인 청을 하나 하고, 패널이 보이면 1초에 한 번 다시 읽는다. Now: 모듈이 뜬 뒤의 초.
	void GameTick(double Now, bool Visible);

	void Draw();

	// 원격 명령(창 없이 같은 길을 태운다). 게임 스레드에서 부른다. 돌려주는 것: 답의 줄들.
	std::vector<std::string> Do(const NlCore::LibraryCommand& Command);
}
