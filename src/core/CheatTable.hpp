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
		Explorer, Economy, Build, Person, Lord, People, Knowledge, Items, Army, Crime,
		Diplomacy, Religion, Time, World, Events, Util, Presets,
	};

	struct AreaInfo
	{
		Area Id;
		const char* Key;		// 시험 설정의 page= 에 쓰는 이름
		const char* Label;		// 왼쪽 목록에 보이는 이름
		int Stage;				// 스펙 §10 의 몇 단계에서 채우는가
		bool Panel = false;		// 표의 항목과 따로 제 패널이 있는가(탐색기, 경제, 시간). 있으면 표가 비어도 목록에서 켜져 있다
	};

	// 왼쪽 목록의 차례대로. 차례는 Area 의 열거 차례와 같다.
	const std::vector<AreaInfo>& Areas();
	const AreaInfo& GetArea(Area Id);
	// Key 로 찾는다. 없으면 nullptr.
	const AreaInfo* FindArea(const std::string& Key);

	// 배율(src/Tweaks.cpp 의 7개. 데이터 파일을 읽은 자리에 배율을 써 넣는다)이 놓이는 영역. Id 는 NlToyBox.settings.txt 의 이름이다.
	struct KnobPlace
	{
		const char* Id;
		Area Where;
	};
	const std::vector<KnobPlace>& KnobPlaces();
	// 그 배율이 놓이는 영역. 모르는 이름이면 Area::Explorer(그 패널에는 배율을 그리지 않는다).
	Area KnobArea(const std::string& Id);
	// 그 영역에 놓이는 배율이 있는가.
	bool HasKnobs(Area Where);

	// Toggle: 주소에 On/Off 를 써 넣는다. Number: 주소에 고른 수를 써 넣는다.
	// Hook: Path(메서드의 주소나 스크립트의 이름)의 함수가 돌려주는 값을 불리언 On 으로 바꾼다(NlRecorder::Override). 끄면 원래대로 지나간다.
	// Custom: 모듈의 코드가 Id 로 알아보고 한다(NlCheats::IsOn). Path 는 그것이 다루는 것이 놓인 자리다(보여 주기만 한다).
	// HookScale: Path 의 함수가 돌려주는 수에 창에서 정한 배율을 곱한다(게임의 자료는 건드리지 않는다. 세이브에 남는 것이 없다).
	//            Path 는 메서드의 주소이거나 이름 있는 스크립트("gml_Script_…")다.
	// CustomScale: 모듈의 코드가 창에서 정한 배율로 한다(NlCheats::Factor).
	// HookNumber: Path 의 함수가 돌려주는 값을 수 On 으로 바꾼다(불리언이 아니라 수를 돌려주는 판정 함수. is_under_holy_defence 가 1 을 돌려줬다).
	enum class CheatKind { Toggle, Number, Hook, Custom, HookScale, CustomScale, HookNumber };

	// 창에서 수를 정하는 종류인가(상태 파일의 num 줄에 든다).
	constexpr bool HasNumber(CheatKind Kind)
	{
		return Kind == CheatKind::Number || Kind == CheatKind::HookScale || Kind == CheatKind::CustomScale;
	}

	// 훅이 바꿔 돌려줄 값의 형(NlRecorder::Forced 의 Kind): 불리언 판정은 'b', 수를 돌려주는 판정은 'n', 배율은 'x'. 게임이 돌려주던 형 그대로 바꾼다.
	constexpr char HookForcedKind(CheatKind Kind)
	{
		return Kind == CheatKind::HookScale ? 'x' : Kind == CheatKind::HookNumber ? 'n' : 'b';
	}

	// 함수가 돌려주는 값을 바꾸는 종류인가.
	constexpr bool IsHook(CheatKind Kind)
	{
		return Kind == CheatKind::Hook || Kind == CheatKind::HookScale || Kind == CheatKind::HookNumber;
	}

	struct Cheat
	{
		const char* Id;			// 상태 파일의 이름
		Area Where;
		const char* Label;		// 창에 보이는 이름
		const char* Path;		// AskPath 의 주소
		CheatKind Kind;
		double On, Off;			// Toggle: 켤 때와 끌 때 써 넣는 값. Hook: 바꿔 돌려줄 불리언(On). HookNumber: 바꿔 돌려줄 수(On).
								// HookScale·CustomScale: On 은 창이 처음 내놓는 배율, Off 는(HookScale) 정수를 정수로 남길지(1) 그대로 곱할지(0)
		double Min, Max;		// 수가 있는 종류의 범위(Number 는 값, 배율은 배율)
		bool Verified;			// 플레이에서 효과를 봤는가
		const char* Help;		// 변수 이름에서 읽은 뜻. Verified 가 아니면 추정이다
		bool ThisRunOnly = false;	// 켠 채 저장돼 있어도 다음 실행은 꺼진 채로 시작한다(게임의 저장 끄기: 켠 것을 잊으면 잃는 것이 크다)
	};

	const std::vector<Cheat>& Cheats();
	const Cheat* FindCheat(const std::string& Id);

	// 상태 파일에서 읽은 것을 표에 맞춘다: 표에 없는 Id 와 종류가 다른 Id 를 버리고, 수를 범위 안으로 당긴다. 즐겨찾기와 잠금은 그대로 둔다.
	// 훅(Hook, HookNumber)과 Custom 은 Verified 인 것만 켠 채로 남긴다(확인 전의 것은 켠 채 저장돼 있어도 꺼진 채로 시작한다).
	// ThisRunOnly 인 항목은 언제나 꺼진 채로 시작한다.
	CheatState KeepKnown(CheatState State);
}
