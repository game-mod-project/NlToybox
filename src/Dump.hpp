#pragma once
// 요청 파일(NlToyBox.probe.txt)이 있을 때만 도는 기록과 덤프. 형식은 스펙 §3.2, §3.7.
// 읽기만 한다. 예외는 ds 자가 점검의 표식 둘(Finder.cpp)과 요청의 script= 호출이다.

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>

namespace NlDump
{
	// 모듈 폴더에서 요청 파일을 읽고 지운다. 없으면 아무 일도 하지 않는다. 콜백을 등록하기 전에 부른다.
	void Init(const Aurie::fs::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log);

	// 게임 스레드의 콜백에서 매번 부른다. Code 는 지금 도는 이벤트의 코드 객체다(없으면 nullptr).
	// 0.5초마다 상태를 재고, 일정에 따라 덤프한다.
	void Tick(YYTK::CCode* Code);
}
