#pragma once
// 치트 표. 영역(왼쪽 목록)과 항목(이름, 주소, 종류). 러너에 기대지 않는다. 스펙: 치트 메뉴 §6, §8.
// 항목을 더하는 일은 CheatTable.cpp 의 표에 한 줄을 더하는 일이다.

#include "CheatState.hpp"

#include <string>
#include <vector>

namespace NlCore
{
	enum class Area
	{
		Explorer, Economy, Build, Person, Lord, People, Knowledge, Items, Army,
		Diplomacy, Religion, Time, World, Events, Util, Presets, Tweaks,
	};

	struct AreaInfo
	{
		Area Id;
		const char* Key;		// 시험 설정의 page= 에 쓰는 이름
		const char* Label;		// 왼쪽 목록에 보이는 이름
		int Stage;				// 스펙 §10 의 몇 단계에서 채우는가
		bool Panel = false;		// 표의 항목과 따로 제 패널이 있는가(탐색기, 경제, 시간, 배율). 있으면 표가 비어도 목록에서 켜져 있다
	};

	// 왼쪽 목록의 차례대로. 차례는 Area 의 열거 차례와 같다.
	const std::vector<AreaInfo>& Areas();
	const AreaInfo& GetArea(Area Id);
	// Key 로 찾는다. 없으면 nullptr.
	const AreaInfo* FindArea(const std::string& Key);

	// Toggle: 주소에 On/Off 를 써 넣는다. Number: 주소에 고른 수를 써 넣는다.
	// Hook: Path(메서드의 주소나 스크립트의 이름)의 함수가 돌려주는 값을 불리언 On 으로 바꾼다(NlRecorder::Override). 끄면 원래대로 지나간다.
	// Custom: 모듈의 코드가 Id 로 알아보고 한다(NlCheats::IsOn). Path 는 그것이 다루는 것이 놓인 자리다(보여 주기만 한다).
	enum class CheatKind { Toggle, Number, Hook, Custom };

	struct Cheat
	{
		const char* Id;			// 상태 파일의 이름
		Area Where;
		const char* Label;		// 창에 보이는 이름
		const char* Path;		// AskPath 의 주소
		CheatKind Kind;
		double On, Off;			// Toggle: 켤 때와 끌 때 써 넣는 값. Hook: 바꿔 돌려줄 불리언(On)
		double Min, Max;		// Number: 범위
		bool Verified;			// 플레이에서 효과를 봤는가
		const char* Help;		// 변수 이름에서 읽은 뜻. Verified 가 아니면 추정이다
		bool Remember = true;	// 켠 것을 다음 실행까지 기억하는가. 거짓이면 켠 채 저장돼 있어도 꺼진 채로 시작한다(KeepKnown)
	};

	const std::vector<Cheat>& Cheats();
	const Cheat* FindCheat(const std::string& Id);

	// 상태 파일에서 읽은 것을 표에 맞춘다: 표에 없는 Id 와 종류가 다른 Id 를 버리고, 수를 범위 안으로 당긴다. 즐겨찾기와 잠금은 그대로 둔다.
	// Hook 과 Custom 은 Verified 인 것만 켠 채로 남긴다(확인 전의 것은 켠 채 저장돼 있어도 꺼진 채로 시작한다). Remember 가 거짓인 것도 버린다.
	CheatState KeepKnown(CheatState State);
}
