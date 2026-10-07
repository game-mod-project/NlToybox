#pragma once
// 종교·유틸 영역에서 한 번 하는 일(research/14, 28): 주교 부르기, 지금 저장. 이벤트는 src/Events, 계절은 src/Season, 광산 붙들기는 src/Mines (2026-10-07).
// 그리는 쪽은 청만 쌓고 러너는 틱이 건드린다.

#include "core/WorldPlan.hpp"

#include <filesystem>
#include <functional>
#include <string>

namespace NlWorld
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번(NlJobs::Init 뒤에. 종교의 일 셋을 등록한다).
	void Init(LogFn Log);

	// 게임 스레드의 틱. 쌓인 청을 하나 한다. Now: 모듈이 뜬 뒤의 초.
	void GameTick(double Now);

	void DrawReligion();	// 종교: 주교 부르기
	void DrawWorld();		// 월드: 계절(src/Season 의 Draw 를 부른다)
	void DrawUtil();		// 유틸: 지금 저장

	// 원격 명령(창 없이 같은 길을 태운다). 게임 스레드에서 부른다. 돌려주는 것: 한 일.
	std::string Do(NlCore::WorldAct Act);
}
