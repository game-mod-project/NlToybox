#pragma once
// 건설비와 업그레이드비를 없앤다("build_free"): 건물 종류마다 있는 등급별 비용(금화, 자원)의 자리를 걷어 src/Jobs 의 엔진에 넘긴다(0 으로 쓰고, 끄면 되돌린다).
// 자리와 함수는 research/09. 엔진은 2026-10-07 리팩토링 C 에서 옮겼다.
// 업그레이드를 바로 끝낸다("instant_upgrade"): 업그레이드 중인 건물에 게임의 build_instantly() 를 부른다.

#include <functional>
#include <string>

namespace NlBuild
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번(NlJobs::Init 뒤에). 건설비의 일을 NlJobs 에 등록한다.
	void Init(LogFn Log);

	// 게임 스레드의 틱(즉시 업그레이드). Now: 모듈이 뜬 뒤의 초(스스로 간격을 둔다).
	void GameTick(double Now);
}
