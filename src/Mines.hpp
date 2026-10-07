#pragma once
// 광산 매장량 붙들기(research/25. 치트 표의 mine_stock_hold): 켠 동안 광산(자리)마다 본 가장 큰 매장량을 기억하고, 줄었으면 되돌려 쓴다.
// 기억한 값은 그 자리(관리자 구조체의 주소 + 지도 관리 인스턴스)의 것이다(core/WorldPlan 의 StockBook·PlaceKey). 2026-10-07 리팩토링 C 에서 src/World.cpp 에서 옮겼다.

#include <functional>
#include <string>

namespace NlMines
{
	using LogFn = std::function<void(const std::string&)>;
	void Init(LogFn Log);
	// 게임 스레드의 틱. 1초에 한 번 치트 표를 보고 붙든다. 끈 틱에 한 번 정리한다.
	void Tick(double Now);
}
