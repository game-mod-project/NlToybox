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

	// 힌트의 글을 제목(첫 줄)과 본문으로 가른다. 특성의 힌트 228개에서 첫 줄은 언제나 짧은 제목이었다(18자 이하, 한 줄뿐인 힌트는 없었다. research/20).
	// 제목과 본문을 모두 PlainHint 로 다듬는다. 앞의 빈 줄과, 표식뿐이라 다듬으면 비는 줄은 건너뛴다(그 다음 줄이 제목이다).
	struct HintText
	{
		std::string Title, Body;
	};
	HintText SplitHint(std::string_view Raw);

	// 게임의 속성 함수 gml_Script_trait_property_get(특성의 이름, 번호)가 돌려주는 것(0.5588.9777.0 에서 281개에 불러 봤다. research/20):
	// 0번은 이름, 1번은 화면 이름의 열쇠("trait.<이름>"), 21번은 힌트(설명)의 열쇠("hint_trait_…", "hint_talent_…", "hint_…". 없으면 빈 글).
	constexpr int k_TraitNameProperty = 0, k_TraitCaptionKeyProperty = 1, k_TraitHintProperty = 21;
	// 번호의 배치가 잰 것과 같은가: 0번이 그 이름이고 1번이 "trait.<이름>"이다. 아니면 21번을 설명의 열쇠로 믿지 않는다(게임이 갱신되면 번호가 밀릴 수 있다).
	bool TraitLayoutOk(const std::string& Name, const std::string& Property0, const std::string& Property1);
	// 배치를 확인할 특성들: Names(이름순)에서 화면 이름의 줄이 있는 것(Captions 에 빈 글이 아닌 이름이 있다)을 앞에서부터 Max 개.
	// 줄이 없는 특성("__…__" 꼴의 안쪽 특성, human …)에는 1번을 재지 않았다(게임의 이름 함수가 그런 이름에 빈 글이나 열쇠 그대로를 돌려줬다). 그런 것으로 확인하지 않는다.
	std::vector<std::string> TraitLayoutProbes(const std::vector<std::string>& Names, const std::unordered_map<std::string, std::string>& Captions, size_t Max);
	// 21번이 힌트의 열쇠가 맞는가의 양성 대조: 게임이 준 열쇠(Keys 개) 가운데 힌트 파일에 있는 것(Found 개)이 절반은 돼야 한다(이 빌드: 246 가운데 230).
	// 0번·1번은 그대로인데 그 사이에 속성이 끼어 21번이 다른 것이 된 경우를 가린다. 열쇠가 10개 미만이면 판정하지 않는다(참).
	bool HintKeysPlausible(size_t Keys, size_t Found);
	// 힌트의 제목을 명칭으로 써도 되는가(화면 이름의 줄이 없는 특성): 본문이 있고 제목이 60바이트 이하다. 한 줄뿐인 힌트의 글은 제목이 아니라 문장이다.
	bool GoodHintTitle(const HintText& Text);

	// 목록의 차례: 화면 이름이 있는 것을 그 이름의 차례로 먼저(UTF-8 의 차례가 한글의 가나다 차례다), 없는 것을 게임의 이름의 차례로 뒤에. 같은 이름끼리는 게임의 이름으로.
	bool TraitBefore(const std::string& NameA, const std::string& CaptionA, const std::string& NameB, const std::string& CaptionB);

	// 찾는 글이 그 특성에 맞는가: 게임의 이름이나 화면 이름에 들어 있다(영문은 대소문자를 가리지 않는다). 빈 글은 모두 맞는다.
	bool TraitMatches(std::string_view Filter, std::string_view Name, std::string_view Caption);
}
