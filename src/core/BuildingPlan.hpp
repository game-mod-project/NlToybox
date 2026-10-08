#pragma once
// 건물 종류(GenericBuilding)의 자료를 돌며 배율을 쓰는 일의 판단. 러너에 기대지 않는다. 걷는 것은 src/Build.cpp, 쓰는 것은 src/Jobs 의 엔진이다.
// 거주 칸: 건물 종류.__number_of_living_places. 효과의 범위: 건물 종류.__effect.__range. research/32.

#include <cstddef>
#include <map>
#include <string>

namespace NlCore
{
	// 건물 종류의 효과(__effect.__effect)가 주변 건물의 안락도에 더하는가(좋은 효과: 교회 7, 공공장소 3) 빼는가(나쁜 효과: 대장간 -4, 훈련장 -8).
	// 0 이거나 수가 아니면 None(다루지 않는다).
	enum class EffectSide { None, Good, Bad };
	EffectSide EffectSideOf(double EffectValue);

	// 건물 종류마다 처음 본 효과의 부호를 기억한다. 나쁜 효과를 없애려고 값을 0 으로 써 둔 뒤에도 그 종류를 나쁜 효과로 알아야
	// 되돌릴 자리를 다시 찾는다. 0 으로 처음 본 종류는 기억하지 않는다(나중에 값이 보이면 그때 기억한다).
	class EffectSides
	{
	public:
		EffectSide Of(const std::string& BuildingName, double EffectValue);

	private:
		std::map<std::string, EffectSide> m_Seen;
	};

	// 병영인가. 병영과 그 밖의 거주 건물(오두막, 영주 저택, 영주관)은 배율을 따로 건다.
	// 건물 종류의 이름이 "barrack_" 로 시작한다(barrack_10x6, barrack_6x10_grade_3). 노예 막사(slaves_barrack_*)는 병영이 아니다.
	bool IsBarracksName(const std::string& BuildingName);

	// 자료를 돌며 값을 쓰는 일(src/Jobs)을 지금 해도 되는가. 게임 화면에서는 언제나 한다. 게임 화면이 아니면(부팅 중, 메인 메뉴) 메뉴에서도 쓰는 일(AnyScreen)이고
	// 게임이 건물 자료를 올린 뒤(DataReady)일 때만 한다: 부팅 중에는 건물 종류를 얻는 스크립트가 읽는 전역이 아직 없어서, 부르는 것만으로 게임이 GML 오류로 끝난다(research/34).
	bool JobMayRun(bool InGame, bool AnyScreen, bool DataReady);

	// 걸었는데 쓸 자리가 하나도 없었는가(되돌리는 중이 아닐 때). 게임이 그 자료를 아직 다 올리지 않은 것이다: 건물의 저장소는 전역이 먼저 생기고 파일을 하나씩 읽어 채워진다.
	// 그런 걷기는 "다 썼다"로 치지 않고 곧 다시 걷는다(주기를 기다리지 않는다). Held: 바라는 값을 가진 자리의 수, Failed: 써지지 않은 자리의 수(실패는 따로 다룬다).
	bool WalkFoundNothing(bool Restoring, size_t Held, size_t Failed);
}
