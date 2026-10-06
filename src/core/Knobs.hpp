#pragma once
// 모드창의 배율에 쓰는 셈과 설정 파일. 러너에 기대지 않는다.

#include <istream>
#include <map>
#include <string>
#include <vector>

namespace NlCore
{
	// 배율이 쓰는 자리의 종류(값이 앉는 자리는 research/02, 03, 04).
	enum class KnobTarget
	{
		BuildingCost,		// debug_params.json 의 building_resources: 건물 → [[자원, 수량], …]
		StartResources,		// debug_params.json 의 product_count: 자원 → 수량
		BookExp,			// 지식 가운데 교본의 upgrade_skill[n].value
		GameplayVar,		// global.__gameplay_vars 의 멤버 하나
	};

	// 배율 하나의 정의. Id 는 설정 파일(NlToyBox.settings.txt)의 이름 그대로다. 그리는 자리는 CheatTable 의 KnobPlaces 가 정한다:
	// 둘의 Id 집합은 같아야 한다(시험이 본다. 전에는 Tweaks.cpp 의 표와 따로 있어 한쪽에 빠뜨리면 창에 나오지 않았다).
	struct KnobDef
	{
		const char* Id;
		const char* Label;
		const char* Help;			// 없으면 nullptr
		KnobTarget What;
		const char* Var;			// GameplayVar 일 때 멤버 이름. 아니면 nullptr
	};

	// 배율 7개의 정의.
	const std::vector<KnobDef>& KnobDefs();

	constexpr double k_MinFactor = 0.05;
	constexpr double k_MaxFactor = 20.0;

	// 바탕 값에 배율을 곱한다. 바탕이 정수였으면 정수로 맞춘다(반은 올린다).
	// 0 보다 큰 정수는 0 이 되지 않는다(비용 5 에 0.05 를 곱해도 1 이다).
	double Scale(double Base, double Factor);

	// 배율을 허용 범위 안으로 당긴다. 수가 아니면 1 이다.
	double ClampFactor(double Factor);

	// "이름=배율" 줄들을 읽는다. 읽을 수 없는 줄은 버린다. 배율은 범위 안으로 당긴다.
	std::map<std::string, double> ParseSettings(std::istream& In);

	// ParseSettings 가 읽는 꼴로 쓴다. 이름의 차례는 사전순이다.
	std::string FormatSettings(const std::map<std::string, double>& Values);
}
