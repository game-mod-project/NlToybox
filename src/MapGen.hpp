#pragma once
// 지도 탭(research/31): 새 게임의 생성기 화면에서 영지의 생성 설정 17개(지형 5, 막힘 4, 자원 8)를 보이고 고치고, 게임의 MapPreviewController.regenerate_map() 으로 다시 생성시킨다.
// 프리셋은 mods\Aurie\NlToyBox.maps.txt(사용자의 설정). 그리는 쪽은 청만 쌓고 틱이 러너를 건드린다. 2026-10-07.

#include "core/MapPlan.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace NlMapGen
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번. ModuleDir: 프리셋 파일이 있는 폴더(mods\Aurie).
	void Init(LogFn Log, const std::filesystem::path& ModuleDir);
	// 게임 스레드의 틱. 쌓인 청을 하나 하고, 패널이 보이면 1초마다 상태를 읽는다. Visible: 지도 패널이 보이는가.
	void Tick(double Now, bool Visible);
	// 지도 패널.
	void Draw();
	// 원격(창의 단추와 같은 길). 게임 스레드에서 부른다. show 는 여러 줄, 나머지는 한 줄.
	std::vector<std::string> Do(const NlCore::MapCommand& Command);
}
