#pragma once
// 호출 기록기. 게임 스크립트의 함수에 훅을 걸어, 게임이 스스로 부를 때의 인자와 반환값을 적는다. 스펙: 치트 메뉴 §14.
// 인자의 수와 형을 모르는 스크립트를 부르면 게임이 끝난다(CLAUDE.md). 부르기 전에 이것으로 꼴을 확인한다.
// Watch, Unwatch, Report 는 게임 스레드의 틱에서 부른다.

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>

namespace NlRecorder
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번.
	void Init(Aurie::AurieModule* Module, LogFn Log);

	// 그 함수의 호출을 기록하기 시작한다. Target: 스크립트의 이름("gml_Script_x". 접두는 없어도 된다) 또는 메서드의 주소
	// ("inst:o_building.set_level"). Name 에 찾은 스크립트의 이름을 돌려준다. 이미 기록 중이면 기록을 비우고 다시 시작한다.
	bool Watch(const std::string& Target, std::string& Name, std::string& Why);

	// 기록을 멈춘다. Name: 스크립트의 이름 또는 "all". 훅은 떼지 않는다(그 함수가 호출 스택에 있을 때 떼면 죽는다).
	// 돌려주는 값: 멈춘 수.
	int Unwatch(const std::string& Name);

	// 기록을 글로. Name 이 비어 있으면 전부.
	std::string Report(const std::string& Name);
}
