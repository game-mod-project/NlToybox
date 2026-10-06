#pragma once
// 게임의 현지화 파일을 읽어 모드창에 보일 글을 얻는다. 러너에 기대지 않는다. 파일의 꼴은 research/20.
// 게임 파일의 글은 레포에 싣지 않는다: 모듈이 실행 중에 사용자의 게임 폴더(localization\main.csv, hints.csv)에서 읽는다.

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NlCore
{
	// 현지화 CSV 한 장을 읽는다. 첫 줄은 머리(첫 칸은 열쇠의 이름 Key 또는 Code, 그 뒤로 언어 이름: Russian, English, …, Korean, …).
	// 칸은 쉼표로 나뉘고, 큰따옴표로 싼 칸은 쉼표와 줄바꿈을 담으며 그 안의 "" 는 따옴표 하나다. 줄은 CRLF 나 LF 로 끝난다(따옴표 안의 CRLF 는 LF 하나로 둔다).
	// Prefix 로 시작하는 열쇠만 받는다(빈 글이면 모두). 값은 Languages 의 차례로 비어 있지 않은 첫 칸이다. 값이 없는 줄은 받지 않는다.
	// 같은 열쇠가 또 나오면 앞의 것을 둔다. 닫히지 않은 따옴표로 끝난 마지막 줄은 버린다.
	// 머리에 Languages 가 하나도 없거나 글이 비었으면 거짓이고 Why 에 까닭.
	bool ReadLocalization(std::string_view Csv, std::string_view Prefix, const std::vector<std::string>& Languages,
		std::unordered_map<std::string, std::string>& Out, std::string& Why);

	// 특성의 화면 이름의 열쇠: "trait.<게임의 이름>"(main.csv 에서 본 꼴).
	std::string TraitCaptionKey(const std::string& Name);

	// 힌트의 글을 모드창에 보일 글로 고친다(게임은 힌트 창에서 표식을 풀고 자리를 값으로 채운다. 모드창은 그러지 못한다):
	//   첫 줄이 Caption 과 같으면 뗀다(힌트의 첫 줄은 제목이다). <b>, <hint=…>, </hint> 같은 꺾쇠 표식은 지우고 안의 글은 둔다(<nbsp> 는 빈칸).
	//   {beauty} 같은 자리는 "(값)"으로 바꾼다(게임이 채우는 값을 모른다). [hint_…] 같은 다른 힌트의 끼움은 지운다.
	//   글꼴에 없는 긴 줄표(U+2014)는 "-"로. 빈 줄은 하나까지만 두고 앞뒤의 빈 글을 뗀다.
	std::string PlainHint(std::string_view Raw, std::string_view Caption);

	// 찾는 글이 그 특성에 맞는가: 게임의 이름이나 화면 이름에 들어 있다(영문은 대소문자를 가리지 않는다). 빈 글은 모두 맞는다.
	bool TraitMatches(std::string_view Filter, std::string_view Name, std::string_view Caption);
}
