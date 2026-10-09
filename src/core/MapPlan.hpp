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

	// 생성기 화면인가: 초기화기가 활성(__is_active)이고 설정의 구조체가 읽히고 미리 보기 제어기(regenerate_map 을 가진 구조체)가 읽히고 게임 안(o_character 가 있는 화면)이 아니다.
	// 아니면 쓰지도 부르지도 않는다(게임 화면 밖에서 부르는 스크립트는 그것이 쓰는 전역이 있는지부터 본다. research/34).
	bool MapScreen(bool InitializerActive, bool SettingsRead, bool PreviewRead, bool InGame);
	// 고정할 씨앗으로 쓸 수 있는 수인가: 유한하고 0 이상인 정수(게임은 -1 을 무작위로 쓴다).
	bool ValidSeed(double Value);
	// 씨앗 고정은 뺐다(2026-10-10 의 확인: regenerate_map() 이 안에서 set_generator_seed(-1) 을 부르고, 생성기는 get_generator_seed() 를 읽지 않는다 — 모듈의 호출과 게임의 "생성" 둘 다 0번. research/31).
	// 원격 map seed 는 그 까닭의 글만 돌려준다(MapReport(Seed, 'u')).

	enum class MapAct { Show, Set, Regenerate, Restore, PresetSave, PresetLoad, PresetDelete, Seed };
	// 결과의 글. Set: 'n' 화면이 아니다, 'w' 썼다(Detail "열쇠=수"), 'f' 쓰지 못했다(Detail), 'k' 모르는 열쇠(Detail). Regenerate: 'n', 'd' 불렀다, 'f' 부르지 못했다(Detail).
	// Restore: 'n', 'd' 되돌렸다(Detail 수), 'f' 읽거나 쓰지 못한 칸(Detail 수). PresetSave: 'n', 's' 저장(Detail 이름), 'b' 이름이 틀렸다, 'f' 파일에 쓰지 못했다.
	// PresetLoad: 'n', 'l' 불러옴(Detail "이름 (수)"), 'm' 없는 이름(Detail), 'b'. PresetDelete: 'x' 지움(Detail), 'm', 'b', 'f' 파일에 쓰지 못했다(지운 것이 되살아난다). Seed: 언제나 'u'(되지 않는다: 생성기가 씨앗 함수를 읽지 않는다).
	std::string MapReport(MapAct Act, char Outcome, const std::string& Detail);

	// 프리셋(NlToyBox.maps.txt. 사용자의 설정). 줄: preset <이름> <열쇠>=<수> … seed=<수>. 막힘 4개는 들지 않는다(InPreset). 씨앗 -1 은 무작위.
	struct MapPreset
	{
		std::string Name;
		std::map<std::string, double> Values;		// 열쇠 → 수(범위 안의 정수)
		double Seed = -1;
	};
	// 이름의 꼴: 비지 않고 32바이트 안, 빈칸·'='·제어 문자 없음. 규칙의 글은 PresetNameRule()(창의 안내와 결과의 글이 같이 쓴다).
	bool GoodPresetName(const std::string& Name);
	const char* PresetNameRule();
	// 파일을 읽는다. 틀린 줄(이름이 없거나 틀리다, 모르는 열쇠, 수가 아닌 값)은 버린다. 막힘 열쇠는 무시한다. 범위 밖의 수는 당긴다. 같은 이름은 뒤의 것이 이긴다. '#' 줄은 주석.
	std::vector<MapPreset> ParseMapPresets(std::istream& In);
	// 파일로. 이름순. 값은 표의 차례(InPreset 만), 씨앗은 끝.
	std::string FormatMapPresets(const std::vector<MapPreset>& Presets);
	void UpsertPreset(std::vector<MapPreset>& Presets, MapPreset Preset);		// 같은 이름이면 바꾼다(InPreset 이 아닌 열쇠는 뗀다)
	bool ErasePreset(std::vector<MapPreset>& Presets, const std::string& Name);
	const MapPreset* FindPreset(const std::vector<MapPreset>& Presets, const std::string& Name);

	// 원격 map … 과 창의 단추가 같은 길을 탄다. Target: show|set|regenerate|restore|preset|seed. Options: set 은 열쇠 → 수의 글, preset 은 op(save|load|delete)·name, seed 는 value(수|random).
	struct MapCommand
	{
		MapAct Act = MapAct::Show;
		std::vector<std::pair<std::string, double>> Sets;		// set: (열쇠, 당긴 수)
		std::string Name;										// preset
		double Seed = -1;										// seed
		bool SeedRandom = false;
	};
	bool MapCommandFromParts(const std::string& Target, const std::map<std::string, std::string>& Options, MapCommand& Out, std::string& Why);
}
