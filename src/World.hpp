#pragma once
// 종교·이벤트 영역에서 한 번 하는 일(research/14): 이벤트 쿨다운 지우기, 주교 부르기.
// 월드 영역의 계절(research/25): 가혹한 계절까지 남은 시간을 보이고, 미루고, 지금 단계를 끝내고, 붙든다(치트 표의 season_hold).
// 그리는 쪽은 청만 쌓고 러너는 틱이 건드린다.

#include "core/WorldPlan.hpp"

#include <filesystem>
#include <functional>
#include <string>

namespace NlWorld
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번. GameDir: 게임 폴더(가혹한 계절의 화면 이름을 localization\main.csv 에서 읽는다. 러너를 부르지 않는다).
	void Init(LogFn Log, const std::filesystem::path& GameDir);

	// 게임 스레드의 틱. 쌓인 청을 하나 하고, 1초에 한 번 계절을 본다. Now: 모듈이 뜬 뒤의 초. Visible: 월드 패널이 보이는가.
	void GameTick(double Now, bool Visible);

	void DrawEvents();		// 이벤트: 쿨다운 지우기
	void DrawReligion();	// 종교: 주교 부르기
	void DrawWorld();		// 월드: 계절
	void DrawUtil();		// 유틸: 지금 저장

	// 원격 명령(창 없이 같은 길을 태운다). 게임 스레드에서 부른다. 돌려주는 것: 한 일.
	std::string Do(NlCore::WorldAct Act);
	// 이벤트 골라 일으키기(원격 world event name=…). 그 이벤트의 구조체를 감독의 __debug_forced_event 에 쓴다. 돌려주는 것: 한 일.
	std::string ForceEvent(const std::string& Name);
}
