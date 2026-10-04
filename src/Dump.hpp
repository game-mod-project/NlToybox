#pragma once
// 요청 파일(NlToyBox.probe.txt)이 있을 때만 도는 덤프. 읽기만 한다. 형식은 스펙 §3.2.

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>

namespace NlDump
{
	// 모듈 폴더에서 요청 파일을 읽는다. 없으면 아무 일도 하지 않는다.
	void Init(YYTK::YYTKInterface* Yytk, const Aurie::fs::path& ModuleDir, std::function<void(const std::string&)> Log);

	// 게임 스레드의 콜백에서 매번 부른다. 요청이 있고 기다릴 시간이 지났으면 한 번 덤프한다.
	void Tick();
}
