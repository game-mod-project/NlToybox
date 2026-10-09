#include "MapPlan.hpp"

#include "Text.hpp"

#include <cmath>

namespace NlCore
{
	namespace
	{
		using G = MapGroup;
		// research/31. 지형의 범위는 창의 단계 수(호수·산 6단계, 언덕 4단계, 언덕 배치 3, 강 2). 막힘·자원은 범위 미확인(본 값 0~5) — 0~9 로 둔다.
		const std::vector<MapKnob> k_Knobs = {
			{ "lakes", "__lakes", "호수", G::Terrain, 0, 5, true },
			{ "hills", "__hills", "언덕", G::Terrain, 0, 3, true },
			{ "hills_distribution", "__hills_distribution", "언덕 배치", G::Terrain, 0, 2, true },
			{ "mountains", "__mountains", "산", G::Terrain, 0, 5, true },
			{ "river", "__river", "강", G::Terrain, 0, 1, true },
			{ "blocked_up", "__blocked_up", "막힘 위", G::Blocked, 0, 9, false },
			{ "blocked_down", "__blocked_down", "막힘 아래", G::Blocked, 0, 9, false },
			{ "blocked_left", "__blocked_left", "막힘 왼쪽", G::Blocked, 0, 9, false },
			{ "blocked_right", "__blocked_right", "막힘 오른쪽", G::Blocked, 0, 9, false },
			{ "berry", "__berry", "열매", G::Resource, 0, 9, true },
			{ "bush", "__bush", "덤불", G::Resource, 0, 9, true },
			{ "clay", "__clay", "점토", G::Resource, 0, 9, true },
			{ "fertile", "__fertile", "비옥지", G::Resource, 0, 9, true },
			{ "hop", "__hop", "홉", G::Resource, 0, 9, true },
			{ "iron", "__iron", "철", G::Resource, 0, 9, true },
			{ "plants", "__plants", "식물", G::Resource, 0, 9, true },
			{ "tree", "__tree", "나무", G::Resource, 0, 9, true },
		};
		constexpr const char* k_NotScreen = "새 게임의 지도 화면(영주관 배치 전)에서만 됩니다";
	}

	const std::vector<MapKnob>& MapKnobs()
	{
		return k_Knobs;
	}

	const MapKnob* FindMapKnob(const std::string& Key)
	{
		for (const MapKnob& knob : k_Knobs)
			if (Key == knob.Key)
				return &knob;
		return nullptr;
	}

	double ClampMapValue(const MapKnob& Knob, double Value)
	{
		if (!std::isfinite(Value))
			return Knob.Min;
		const double whole = std::floor(Value);
		return whole < Knob.Min ? Knob.Min : whole > Knob.Max ? Knob.Max : whole;
	}

	const char* MapGroupWord(MapGroup Group)
	{
		switch (Group)
		{
		case MapGroup::Terrain: return "지형";
		case MapGroup::Blocked: return "막힘";
		default: return "자원";
		}
	}

	std::string HillsDistributionWord(double Value)
	{
		if (Value == 0) return "중심";
		if (Value == 1) return "가장자리";
		if (Value == 2) return "무작위";
		return "?";
	}

	std::string MapFieldPath(const MapKnob& Knob)
	{
		return std::string(k_MapSettingsPath) + "." + Knob.Field;
	}

	std::string MapStashedPath(const MapKnob& Knob)
	{
		return std::string(k_MapSettingsPath) + ".__stashed_settings." + Knob.Key;
	}

	bool MapScreen(bool InitializerActive, bool SettingsRead, bool InGame)
	{
		return InitializerActive && SettingsRead && !InGame;
	}

	bool SeedHookVerified()
	{
		return false;		// research/31: 생성기가 get_generator_seed() 를 읽는지 못 봤다. 본 뒤에 참으로
	}

	std::string MapReport(MapAct Act, char Outcome, const std::string& Detail)
	{
		if (Outcome == 'n')
			return k_NotScreen;
		switch (Act)
		{
		case MapAct::Set:
			switch (Outcome)
			{
			case 'w': return Detail + " 을 썼습니다";
			case 'k': return "모르는 열쇠: " + Detail;
			default: return "쓰지 못했습니다: " + Detail;
			}
		case MapAct::Regenerate:
			return Outcome == 'd' ? "다시 생성했습니다 (regenerate_map)" : "다시 생성을 부르지 못했습니다 (" + Detail + ")";
		case MapAct::Restore:
			return Outcome == 'd' ? "원래 값으로 되돌렸습니다 (" + Detail + "개)" : "원래 값을 읽거나 쓰지 못한 칸이 있습니다 (" + Detail + "개)";
		case MapAct::PresetSave:
			switch (Outcome)
			{
			case 's': return "프리셋을 저장했습니다: " + Detail;
			case 'b': return "프리셋의 이름이 틀렸습니다 (비지 않고 32자 안, 빈칸과 = 없이)";
			default: return "프리셋 파일에 쓰지 못했습니다";
			}
		case MapAct::PresetLoad:
			switch (Outcome)
			{
			case 'l': return "프리셋을 불러왔습니다: " + Detail + " - 값만 채웠습니다. 생성은 '다시 생성'으로";
			case 'b': return "프리셋의 이름이 틀렸습니다 (비지 않고 32자 안, 빈칸과 = 없이)";
			default: return "그 이름의 프리셋이 없습니다: " + Detail;
			}
		case MapAct::PresetDelete:
			switch (Outcome)
			{
			case 'x': return "프리셋을 지웠습니다: " + Detail;
			case 'b': return "프리셋의 이름이 틀렸습니다 (비지 않고 32자 안, 빈칸과 = 없이)";
			default: return "그 이름의 프리셋이 없습니다: " + Detail;
			}
		case MapAct::Seed:
			switch (Outcome)
			{
			case 'd': return "씨앗을 고정했습니다: " + Detail;
			case 'r': return "씨앗 고정을 풀었습니다 (무작위)";
			case 'f': return "씨앗 고정을 걸지 못했습니다 (" + Detail + ")";
			default: return "씨앗 고정은 확인 전입니다 (research/31)";
			}
		default:
			return std::string();
		}
	}
}
