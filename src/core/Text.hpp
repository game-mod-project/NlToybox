#pragma once
// 러너에 기대지 않는 글 처리. tests/native 가 직접 부른다.

#include <string>

namespace NlCore
{
	std::string Trim(const std::string& Text);

	// JSON 문자열로 쓴다. MaxBytes 보다 길면 UTF-8 글자 경계에서 자르고 "..." 를 붙인다. 잘못된 바이트는 '?' 로 바꾼다.
	std::string Quote(std::string Text, size_t MaxBytes = 200);

	// JSON 수로 쓴다. 유한하지 않으면 "nan" / "inf" / "-inf" 문자열로 쓴다. 로캘과 무관하게 소수점은 '.' 이다.
	std::string Number(double Value);

	// 소수 Digits 자리로 쓴다. 로그의 시각에 쓴다.
	std::string Fixed(double Value, int Digits);

	// 다시 읽으면 같은 수가 되는 가장 짧은 글("0.83", "3", "-4"). 유한하지 않으면 "nan" / "inf" / "-inf".
	std::string Shortest(double Value);

	// 정수로 맞춰 세 자리마다 쉼표를 넣는다("3,600", "-1,250,000"). 창에 보이는 수에 쓴다(Shortest 는 큰 수를 지수로 쓴다).
	// 유한하지 않으면 Shortest 와 같다.
	std::string Thousands(double Value);

	// 글 전체가 수일 때만 참.
	bool ParseNumber(const std::string& Text, double& Out);

	// Name 안에 Part 가 들어 있는가(대소문자를 가린다). 빈 Part 는 아무것에도 맞지 않는다.
	bool Contains(const std::string& Name, const std::string& Part);

	// 게임 스크립트 함수의 정식 이름("gml_Script_x"). 접두가 없으면 붙인다. 이름이 비었거나 빈칸이 끼어 있으면 빈 글.
	// 접두 없는 이름은 러너에서 다른 루틴을 가리킨다(research/07). 부르거나 훅을 걸 때는 이 이름만 쓴다.
	std::string ScriptRoutineName(const std::string& Given);
}
