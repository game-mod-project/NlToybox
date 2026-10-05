#pragma once
// 프리셋: 치트 표의 확인된 항목의 묶음(스펙 §8 의 프리셋). 누르면 묶음에 없는 항목은 끄고 있는 항목은 켠다.

#include <string>
#include <vector>

namespace NlCore
{
	struct PresetItem
	{
		const char* Id;		// 치트 표의 Id
		double Number;		// 배율 항목(HookScale, CustomScale)에 걸 배율. 체크 하나짜리 항목은 0
	};

	struct Preset
	{
		const char* Key;	// 원격 명령과 시험의 이름
		const char* Label;	// 창에 보일 이름
		const char* Help;
		std::vector<PresetItem> Items;
	};

	const std::vector<Preset>& Presets();
	const Preset* FindPreset(const std::string& Key);

	// 켜 둔 표의 항목 하나. Number 는 배율 항목의 배율(체크 하나짜리는 0).
	struct CheatOn
	{
		std::string Id;
		double Number = 0;
	};
	// 켜 둔 표의 항목들이 어느 묶음과 꼭 같은가(항목이 같고 배율 항목은 배율도 같다). 아무것도 켜 있지 않으면 normal. 같은 묶음이 없으면 nullptr.
	const Preset* MatchPreset(const std::vector<CheatOn>& On);

	// 묶음이 표와 맞는가: 표에 있는 항목, 플레이에서 확인된 항목(Verified), 겹치지 않는다, 배율 항목이면 범위 안의 배율, 아니면 0.
	// 값을 써 넣는 항목(CheatKind::Number)은 넣지 않는다(세이브에 남는 값을 쓰는 것이 있다). 틀리면 거짓이고 Why 에 까닭.
	bool CheckPreset(const Preset& Preset, std::string& Why);
}
