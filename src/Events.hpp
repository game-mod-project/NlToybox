#pragma once
// 이벤트(research/28, 29): 게임의 이벤트 61개의 표와 줄마다 일으키기(감독의 __debug_forced_event 에 구조체를 쓴다), 예약 취소(reset_debug_forced_event()),
// 가족 다섯(습격·예언·음모·손님·소요)의 진행 중 상태와 확인된 가족만의 끝내기, 이벤트 쿨다운 지우기. 2026-10-07 에 src/World.cpp 의 이벤트 부분을 옮기고 더했다.
// 그리는 쪽은 청만 쌓고 틱이 러너를 건드린다. 스냅샷은 글과 수다.

#include "core/EventPlan.hpp"
#include "core/WorldPlan.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace NlEvents
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번(NlWorld::Init 뒤). GameDir: 게임 폴더(화면 이름을 localization\main.csv 에서 읽는다. 러너를 부르지 않는다).
	void Init(LogFn Log, const std::filesystem::path& GameDir);
	// 게임 스레드의 틱(NlWorld::GameTick 뒤). 쌓인 청을 하나 하고, 이름을 한 번 읽고, 패널이 보이면 1초마다 상태를 읽는다. Visible: 이벤트 패널이 보이는가.
	void Tick(double Now, bool Visible);
	// 이벤트 패널(치트 표의 항목 아래).
	void Draw();
	// 이벤트의 일(EventForce 는 ForceEvent 로, EventList 는 List 로. 그 밖: EventCancel, EventEnd(가족은 SetEndFamily 로 먼저), CooldownsClear)을 지금 한다(원격). 돌려주는 것: 한 일.
	std::string Do(NlCore::WorldAct Act);
	// 이벤트 골라 일으키기(원격 world event name=…). 돌려주는 것: 한 일.
	std::string ForceEvent(const std::string& Name);
	// 이벤트의 표(원격 world events [group=] [find=]). 지금 읽어서: 첫 줄 예약, 가족마다 한 줄 상태, 그 뒤 거른 줄들, 끝에 "(N of M)". 게임 화면이 아니면 그 한 줄.
	std::vector<std::string> List(const std::string& Group, const std::string& Find);
	// 끝내기(원격 world event_end kind=…). 확인 전인 가족은 부르지 않는다. 돌려주는 것: 한 일.
	std::string End(NlCore::EventFamily Family);
}
