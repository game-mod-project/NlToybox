#pragma once
// 건물 종류(171개)를 걷는다(research/09): 이름은 gml_Script_building_generic_get_array_of_all_buildings()(인자 없음), 종류의 구조체는
// gml_Script_get_generic_building(이름)(게임이 (string)으로 부른다). ds_map 에 들어 있어 전역 탐색으로는 보이지 않는다.
// src/Build.cpp 와 src/Production.cpp 가 각자 들던 것을 2026-10-07 리팩토링 C 에서 하나로.

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>

namespace NlBuildings
{
	// 종류마다 Visit(이름, 구조체) 를 부른다(이름이 글이 아니거나 구조체를 얻지 못한 종류는 건너뛴다). Visit 이 거짓을 돌려주면 그만 돈다.
	// 이름의 목록을 얻지 못하면 거짓이고 Why 에 까닭.
	bool ForEachType(const std::function<bool(const std::string& Name, const YYTK::RValue& Generic)>& Visit, std::string& Why);
}
