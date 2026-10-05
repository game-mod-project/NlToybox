#pragma once
// 게임의 자료를 돌며 배율을 쓰는 치트들(치트 표의 Custom·CustomScale). 자리와 잰 것은 research/10.
//   storage_capacity   창고 종류마다의 갈래별 용량에 배율을 쓴다
//   production_amount  조리법의 "만들어지는 수"에 배율을 쓴다
//   production_free    조리법의 재료를 0 으로 쓴다
// 처음 본 값을 장부(core/CostBook)에 적고, 끄면 그 값으로 되돌린다. 쓴 뒤에는 다시 읽어 남았는지 본다.

#include <functional>
#include <string>

namespace NlProduction
{
	using LogFn = std::function<void(const std::string&)>;

	void Init(LogFn Log);

	// 게임 스레드의 틱. Now: 모듈이 뜬 뒤의 초(스스로 간격을 둔다).
	void GameTick(double Now);
}
