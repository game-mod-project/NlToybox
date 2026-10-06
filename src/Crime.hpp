#pragma once
// 범죄 패널(research/26). 범죄자("부랑자")가 된 플레이어의 주민을 보이고 주민으로 되돌리고, 플레이어의 영주의 죄와 범죄 혐의(특성)를 지운다.
// 틱이 읽어 글로 두고 그리는 쪽은 그것만 그린다. 단추는 청만 쌓고 러너는 틱이 건드린다.

#include "core/CrimePlan.hpp"

#include <functional>
#include <string>
#include <vector>

namespace NlCrime
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번.
	void Init(LogFn Log);

	// 게임 스레드의 틱. 쌓인 청을 하나 하고, 패널이 보이면 1초에 한 번 다시 읽는다. Now: 모듈이 뜬 뒤의 초.
	void GameTick(double Now, bool Visible);

	void Draw();

	// 원격 명령(창 없이 같은 길을 태운다). 게임 스레드에서 부른다. 돌려주는 것: 답의 줄들.
	std::vector<std::string> Do(const NlCore::CrimeCommand& Command);
}
