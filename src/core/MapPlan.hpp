#pragma once
// 지도 탭(research/31)에서 러너에 기대지 않는 것: 영지의 생성 설정 17개의 표(열쇠·칸·화면 이름·갈래·범위·프리셋에 드는가), 범위 당기기, 자리, 생성기 화면의 판단,
// 프리셋 파일(NlToyBox.maps.txt)의 읽기·쓰기·검사, 원격 map … 의 낱말, 결과의 글.
// 값의 자리: global.__new_game_initializer.__choosed_province.__initial_area.__generator_settings.<칸>(GlobalMapAreaGeneratorSettings). 다시 생성은 MapPreviewController.regenerate_map()(인자 없음).

#include <istream>
#include <map>
#include <string>
#include <vector>

namespace NlCore
{
	enum class MapGroup { Terrain, Blocked, Resource };

	struct MapKnob
	{
		const char* Key;		// 열쇠(프리셋 파일·원격·stashed 의 이름)
		const char* Field;		// 구조체의 칸("__" + 열쇠)
		const char* Label;		// 화면 이름
		MapGroup Group;
		double Min, Max;		// 지형은 창의 단계 수(research/31), 막힘·자원은 0~9(범위 미확인)
		bool InPreset;			// 막힘 4개는 아니다(영지의 모양)
	};
	const std::vector<MapKnob>& MapKnobs();				// 17줄
	const MapKnob* FindMapKnob(const std::string& Key);	// 없으면 nullptr
	// 범위 안으로: 수가 아니면 최소, 소수는 내림, 밖이면 끝으로.
	double ClampMapValue(const MapKnob& Knob, double Value);
	const char* MapGroupWord(MapGroup Group);			// "지형", "막힘", "자원"
	std::string HillsDistributionWord(double Value);	// 0 중심, 1 가장자리, 2 무작위, 그 밖 "?"

	constexpr const char* k_MapInitializerPath = "global.__new_game_initializer";
	constexpr const char* k_MapSettingsPath = "global.__new_game_initializer.__choosed_province.__initial_area.__generator_settings";
	constexpr const char* k_MapPreviewPath = "global.__new_game_initializer.__map_preview_controller";
	std::string MapFieldPath(const MapKnob& Knob);		// 설정의 칸
	std::string MapStashedPath(const MapKnob& Knob);	// 영지의 원래 값(__stashed_settings.<열쇠>)

	// 생성기 화면인가: 초기화기가 활성(__is_active)이고 설정의 구조체가 읽히고 게임 안(o_character 가 있는 화면)이 아니다. 아니면 쓰지도 부르지도 않는다.
	bool MapScreen(bool InitializerActive, bool SettingsRead, bool InGame);
	// 씨앗 고정의 훅(get_generator_seed 의 반환값 바꾸기)이 확인됐는가. 생성기가 그 함수를 읽는 것을 기록으로 본 뒤에 참으로 바꾼다(research/31).
	bool SeedHookVerified();

	enum class MapAct { Show, Set, Regenerate, Restore, PresetSave, PresetLoad, PresetDelete, Seed };
	// 결과의 글. Set: 'n' 화면이 아니다, 'w' 썼다(Detail "열쇠=수"), 'f' 쓰지 못했다(Detail), 'k' 모르는 열쇠(Detail). Regenerate: 'n', 'd' 불렀다, 'f' 부르지 못했다(Detail).
	// Restore: 'n', 'd' 되돌렸다(Detail 수), 'f' 읽거나 쓰지 못한 칸(Detail 수). PresetSave: 'n', 's' 저장(Detail 이름), 'b' 이름이 틀렸다, 'f' 파일에 쓰지 못했다.
	// PresetLoad: 'n', 'l' 불러옴(Detail "이름 (수)"), 'm' 없는 이름(Detail), 'b'. PresetDelete: 'x' 지움(Detail), 'm', 'b'. Seed: 'u' 확인 전, 'd' 고정(Detail 수), 'r' 풀었다, 'f' 걸지 못했다(Detail).
	std::string MapReport(MapAct Act, char Outcome, const std::string& Detail);
}
