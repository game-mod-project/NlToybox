#include "common.hpp"

#include <cmath>
#include <set>

void RunMapTests()
{
	Test("지도: 설정 표 17줄, 범위, 자리, 화면의 판단, 결과의 글", [] {
		// research/31: 영지의 생성 설정 17개. 지형 5(창의 단계 수가 범위), 막힘 4, 자원 8(범위 미확인이라 0~9).
		const std::vector<MapKnob>& knobs = MapKnobs();
		CHECK(knobs.size() == 17);
		std::set<std::string> keys;
		int terrain = 0, blocked = 0, resource = 0, preset = 0;
		for (const MapKnob& knob : knobs)
		{
			CHECK(keys.insert(knob.Key).second);
			CHECK(std::string(knob.Field).rfind("__", 0) == 0 && std::string(knob.Field).substr(2) == knob.Key);		// 칸은 "__" + 열쇠(stashed 는 열쇠 그대로)
			CHECK(knob.Min == 0 && knob.Max >= 1 && knob.Max <= 9 && !std::string(knob.Label).empty());
			if (knob.Group == MapGroup::Terrain) terrain++;
			if (knob.Group == MapGroup::Blocked) blocked++;
			if (knob.Group == MapGroup::Resource) resource++;
			if (knob.InPreset) preset++;
			CHECK(knob.InPreset == (knob.Group != MapGroup::Blocked));		// 막힘은 프리셋에 들지 않는다
		}
		CHECK(terrain == 5 && blocked == 4 && resource == 8 && preset == 13);
		CHECK(FindMapKnob("lakes")->Max == 5 && FindMapKnob("hills")->Max == 3 && FindMapKnob("hills_distribution")->Max == 2 && FindMapKnob("mountains")->Max == 5 && FindMapKnob("river")->Max == 1);
		CHECK(FindMapKnob("iron")->Max == 9 && FindMapKnob("blocked_up")->Max == 9 && FindMapKnob("no_such") == nullptr);
		CHECK_STR(FindMapKnob("hills_distribution")->Label, "언덕 배치"); CHECK_STR(FindMapKnob("plants")->Label, "식물");
		CHECK_STR(MapGroupWord(MapGroup::Terrain), "지형"); CHECK_STR(MapGroupWord(MapGroup::Blocked), "막힘"); CHECK_STR(MapGroupWord(MapGroup::Resource), "자원");
		// 범위 당기기: 밖이면 끝으로, 소수는 내림, 수가 아니면 최소
		const MapKnob& lakes = *FindMapKnob("lakes");
		CHECK(ClampMapValue(lakes, 7) == 5 && ClampMapValue(lakes, -3) == 0 && ClampMapValue(lakes, 2.9) == 2 && ClampMapValue(lakes, std::nan("")) == 0 && ClampMapValue(lakes, 3) == 3);
		CHECK_STR(HillsDistributionWord(0), "중심"); CHECK_STR(HillsDistributionWord(1), "가장자리"); CHECK_STR(HillsDistributionWord(2), "무작위"); CHECK_STR(HillsDistributionWord(5), "?");
		// 자리
		CHECK_STR(k_MapSettingsPath, "global.__new_game_initializer.__choosed_province.__initial_area.__generator_settings");
		CHECK_STR(k_MapPreviewPath, "global.__new_game_initializer.__map_preview_controller");
		CHECK_STR(k_MapInitializerPath, "global.__new_game_initializer");
		CHECK_STR(MapFieldPath(lakes), "global.__new_game_initializer.__choosed_province.__initial_area.__generator_settings.__lakes");
		CHECK_STR(MapStashedPath(lakes), "global.__new_game_initializer.__choosed_province.__initial_area.__generator_settings.__stashed_settings.lakes");
		for (const MapKnob& knob : knobs)
			CHECK(ParseAskPath(MapFieldPath(knob)).Error.empty() && ParseAskPath(MapStashedPath(knob)).Error.empty());
		// 생성기 화면인가: 초기화기가 활성이고 설정이 읽히고 게임 안이 아닐 때만(리뷰 포커스 1)
		CHECK(MapScreen(true, true, false) && !MapScreen(false, true, false) && !MapScreen(true, false, false) && !MapScreen(true, true, true));
		// 씨앗 고정은 확인 전(research/31: 생성기가 get_generator_seed() 를 읽는지 못 봤다)
		CHECK(!SeedHookVerified());
		// 결과의 글
		CHECK_STR(MapReport(MapAct::Set, 'n', ""), "새 게임의 지도 화면(영주관 배치 전)에서만 됩니다");
		CHECK_STR(MapReport(MapAct::Set, 'w', "lakes=3"), "lakes=3 을 썼습니다");
		CHECK_STR(MapReport(MapAct::Set, 'f', "lakes: why"), "쓰지 못했습니다: lakes: why");
		CHECK_STR(MapReport(MapAct::Set, 'k', "bogus"), "모르는 열쇠: bogus");
		CHECK_STR(MapReport(MapAct::Regenerate, 'd', ""), "다시 생성했습니다 (regenerate_map)");
		CHECK_STR(MapReport(MapAct::Regenerate, 'f', "why"), "다시 생성을 부르지 못했습니다 (why)");
		CHECK_STR(MapReport(MapAct::Restore, 'd', "17"), "원래 값으로 되돌렸습니다 (17개)");
		CHECK_STR(MapReport(MapAct::Restore, 'f', "3"), "원래 값을 읽거나 쓰지 못한 칸이 있습니다 (3개)");
		CHECK_STR(MapReport(MapAct::PresetSave, 's', "평야"), "프리셋을 저장했습니다: 평야");
		CHECK_STR(MapReport(MapAct::PresetSave, 'b', ""), "프리셋의 이름이 틀렸습니다 (비지 않고 32자 안, 빈칸과 = 없이)");
		CHECK_STR(MapReport(MapAct::PresetSave, 'f', ""), "프리셋 파일에 쓰지 못했습니다");
		CHECK_STR(MapReport(MapAct::PresetLoad, 'l', "평야 (13)"), "프리셋을 불러왔습니다: 평야 (13) - 값만 채웠습니다. 생성은 '다시 생성'으로");
		CHECK_STR(MapReport(MapAct::PresetLoad, 'm', "x"), "그 이름의 프리셋이 없습니다: x");
		CHECK_STR(MapReport(MapAct::PresetDelete, 'x', "평야"), "프리셋을 지웠습니다: 평야");
		CHECK_STR(MapReport(MapAct::Seed, 'u', ""), "씨앗 고정은 확인 전입니다 (research/31)");
		CHECK_STR(MapReport(MapAct::Seed, 'd', "12345"), "씨앗을 고정했습니다: 12345");
		CHECK_STR(MapReport(MapAct::Seed, 'r', ""), "씨앗 고정을 풀었습니다 (무작위)");
	});
}
