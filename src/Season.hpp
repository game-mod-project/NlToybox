#pragma once
// 계절(research/25): 지금 지도의 ExtremeSeasonManager 에서 남은 시간을 게임의 함수로 읽고, 시작 시각(__start_phase_time)에 써서 미루거나 끝내고,
// 치트 표의 season_hold 가 켜진 동안 시작 시각을 흐른 만큼 따라 민다. 월드 패널의 "계절" 블록을 그린다.
// 2026-10-07 리팩토링 C 에서 src/World.cpp 에서 옮겼다. 그리는 쪽은 청만 쌓고 틱이 한다.

#include "core/WorldPlan.hpp"

#include <filesystem>
#include <functional>
#include <string>

namespace NlSeason
{
	using LogFn = std::function<void(const std::string&)>;

	// GameDir: 게임 폴더(가혹한 계절의 화면 이름을 localization\main.csv 에서 읽는다. 러너를 부르지 않는다).
	void Init(LogFn Log, const std::filesystem::path& GameDir);
	// 게임 스레드의 틱. 1초에 한 번: 쌓인 청, 붙들기, 패널이 보이면 읽기. Visible: 월드 패널이 보이는가.
	void Tick(double Now, bool Visible);
	// 월드 패널의 계절 블록(제목, 글, 단추 둘, 마지막 결과).
	void Draw();
	// 계절의 일(SeasonShow·SeasonDelay·SeasonEnd)을 지금 한다(원격). 그 밖의 일에는 빈 글. 돌려주는 것: 한 일.
	std::string Do(NlCore::WorldAct Act);
}
