#pragma once
// 건설비와 업그레이드비를 없앤다: 건물 종류마다 있는 등급별 비용(금화, 자원)을 0 으로 쓰고, 끄면 처음 본 값으로 되돌린다.
// 치트 표의 "build_free"(Custom)가 켜져 있는지를 보고 게임 스레드의 틱에서 한다. 자리와 함수는 research/09.
// 업그레이드를 바로 끝낸다("instant_upgrade"): 업그레이드 중인 건물에 게임의 build_instantly() 를 부른다.

#include <functional>
#include <string>

namespace NlBuild
{
	using LogFn = std::function<void(const std::string&)>;

	void Init(LogFn Log);

	// 게임 스레드의 틱. Now: 모듈이 뜬 뒤의 초(스스로 간격을 둔다).
	void GameTick(double Now);
}
