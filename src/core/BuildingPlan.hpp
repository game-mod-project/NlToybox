#pragma once
// 건물 종류(GenericBuilding)의 자료를 돌며 배율을 쓰는 일의 판단. 러너에 기대지 않는다. 걷는 것은 src/Build.cpp, 쓰는 것은 src/Jobs 의 엔진이다.
// 거주 칸: 건물 종류.__number_of_living_places. 효과의 범위: 건물 종류.__effect.__range. research/32.

#include <string>

namespace NlCore
{
	// 병영인가. 병영과 그 밖의 거주 건물(오두막, 영주 저택, 영주관)은 배율을 따로 건다.
	// 건물 종류의 이름이 "barrack_" 로 시작한다(barrack_10x6, barrack_6x10_grade_3). 노예 막사(slaves_barrack_*)는 병영이 아니다.
	bool IsBarracksName(const std::string& BuildingName);
}
