#pragma once
// 모드창의 배율에 쓰는 셈과 설정 파일. 러너에 기대지 않는다.

#include <istream>
#include <map>
#include <string>

namespace NlCore
{
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
