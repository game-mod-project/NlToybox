#pragma once
// 모드창의 안쪽: 왼쪽에 영역의 목록, 오른쪽에 고른 영역의 패널. 스펙: 치트 메뉴 §8.
// 치트의 상태 파일(NlToyBox.cheats.txt)을 읽고 쓰며, 시험 설정의 ask= 와 poke= 를 실행한다.

#include <filesystem>
#include <functional>
#include <string>

namespace NlMenu
{
	// ModuleInitialize 에서 한 번. NlUi::Init 뒤에 부른다(시험 설정을 NlUi 가 읽어 둔다).
	void Init(const std::filesystem::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log);

	// 게임 스레드의 콜백에서 부른다. 탐색기와 치트의 틱을 돌리고, 바뀐 상태를 파일에 적는다.
	void GameTick();

	// 모드창 안을 그린다.
	void Draw();
}
