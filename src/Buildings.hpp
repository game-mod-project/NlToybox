#pragma once
// 건물 종류(171개)를 걷는다(research/09): 이름은 gml_Script_building_generic_get_array_of_all_buildings()(인자 없음), 종류의 구조체는
// gml_Script_get_generic_building(이름)(게임이 (string)으로 부른다). ds_map 에 들어 있어 전역 탐색으로는 보이지 않는다.
// src/Build.cpp 와 src/Production.cpp 가 각자 들던 것을 2026-10-07 리팩토링 C 에서 하나로.

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>

namespace NlBuildings
{
	// 게임이 건물 자료를 올렸는가: 전역 global.__building_storage 가 있다. 이름을 얻는 스크립트는 `return global.__building_storage.get_all_names();` 한 줄이라
	// (기계어와 exe 의 변수 이름 표로 봤다. research/34) 그 전역이 없을 때(부팅 중) 부르면 게임이 "I32 argument is unset" 로 끝난다. 게임 스레드에서 부른다.
	bool Ready();

	// 종류마다 Visit(이름, 구조체) 를 부른다(이름이 글이 아니거나 구조체를 얻지 못한 종류는 건너뛴다). Visit 이 거짓을 돌려주면 그만 돈다.
	// 이름의 목록을 얻지 못하면 거짓이고 Why 에 까닭. 게임이 건물 자료를 올리기 전(Ready 가 거짓)에는 스크립트를 부르지 않고 거짓을 돌려준다.
	bool ForEachType(const std::function<bool(const std::string& Name, const YYTK::RValue& Generic)>& Visit, std::string& Why);
}
