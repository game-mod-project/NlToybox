#pragma once
// 모드창의 배율을 실행 중인 게임의 값에 써 넣는다. 파일을 고치지 않는다.
// 값이 앉는 자리는 research/02, 03, 04 에서 찾은 것이다(데이터 파일을 읽은 ds_map, global.__gameplay_vars, 지식의 구조체).
// 써 넣은 값을 게임이 따르는지는 항목마다 다를 수 있다. 창에 "써 넣었다"까지만 적는다.

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace NlTweaks
{
	// ModuleInitialize 에서 한 번. 저장된 배율(NlToyBox.settings.txt)을 읽는다.
	// TestSets: 시험용 "이름:배율" 들. 있으면 그 값으로 시작하고 설정 파일에 저장하지 않는다.
	void Init(const std::filesystem::path& ModuleDir, std::function<void(const std::string&)> Log,
		const std::vector<std::string>& TestSets);

	// 게임 스레드의 콜백에서 부른다. 배율이 1 이 아닌 항목의 대상을 찾아 값을 써 넣는다.
	void GameTick();

	// 모드창 안을 그린다.
	void Draw();
}
