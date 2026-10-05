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
	};

	// 왼쪽 목록의 차례대로. 차례는 Area 의 열거 차례와 같다.
	const std::vector<AreaInfo>& Areas();
	const AreaInfo& GetArea(Area Id);
	// Key 로 찾는다. 없으면 nullptr.
	const AreaInfo* FindArea(const std::string& Key);

	enum class CheatKind { Toggle, Number };

	struct Cheat
	{
		const char* Id;			// 상태 파일의 이름
		Area Where;
		const char* Label;		// 창에 보이는 이름
		const char* Path;		// AskPath 의 주소
		CheatKind Kind;
		double On, Off;			// Toggle: 켤 때와 끌 때 써 넣는 값
		double Min, Max;		// Number: 범위
		bool Verified;			// 플레이에서 효과를 봤는가
		const char* Help;		// 변수 이름에서 읽은 뜻. Verified 가 아니면 추정이다
	};

	const std::vector<Cheat>& Cheats();
	const Cheat* FindCheat(const std::string& Id);

	// 표에 없는 Id 와 종류가 다른 Id 를 버리고, 수를 범위 안으로 당긴다. 즐겨찾기와 잠금은 그대로 둔다.
	CheatState KeepKnown(CheatState State);
}
