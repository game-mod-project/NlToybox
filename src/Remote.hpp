#pragma once
// 원격 질의. 켜져 있는 게임에 도구가 파일로 묻는다. 스펙: 치트 메뉴 §14.
// 모듈 폴더의 NlToyBox.ask.txt 를 0.25초마다 보고, 있으면 줄들을 읽고 지운 뒤 게임 스레드에서 차례로 실행한다.
// 답은 NlToyBox.answer.txt 에 이어 쓴다. 줄의 꼴은 core/RemoteCommand.hpp 에 있다.

#include <filesystem>
#include <functional>
#include <string>

namespace NlRemote
{
	// ModuleInitialize 에서 한 번.
	void Init(const std::filesystem::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log);

	// 게임 스레드의 콜백에서 부른다.
	void GameTick();
}
