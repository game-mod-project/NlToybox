#include "common.hpp"

#include <cmath>
#include <limits>
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
		// 미리 보기 제어기(regenerate_map 을 가진 구조체)가 읽히는 것도 조건이다(리뷰 Important 1: 그 스크립트가 쓰는 전역이 있는지부터 본다)
		CHECK(MapScreen(true, true, true, false) && !MapScreen(false, true, true, false) && !MapScreen(true, false, true, false) && !MapScreen(true, true, false, false) && !MapScreen(true, true, true, true));
		// 씨앗: 유한하고 0 이상인 정수만(게임은 -1 을 무작위로 쓴다. 리뷰 Important 2·Minor 5)
		CHECK(ValidSeed(0) && ValidSeed(12345) && !ValidSeed(-1) && !ValidSeed(-5) && !ValidSeed(2.5) && !ValidSeed(std::nan("")) && !ValidSeed(std::numeric_limits<double>::infinity()));
		// 씨앗 고정은 뺐다(2026-10-10 의 확인: 생성기가 get_generator_seed() 를 읽지 않는다. 모듈의 호출과 게임의 "생성" 둘 다 0번. research/31)
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
		CHECK_STR(MapReport(MapAct::Seed, 'u', ""), "씨앗 고정은 되지 않습니다: 생성기가 get_generator_seed() 를 읽지 않습니다 (research/31)");
	});

	Test("지도: 프리셋 파일 — 읽고 쓰면 같다, 틀린 줄은 버린다, 같은 이름은 덮어쓴다, 막힘은 들지 않는다", [] {
		CHECK(GoodPresetName("평야") && GoodPresetName("map_1") && !GoodPresetName("") && !GoodPresetName("a b") && !GoodPresetName("a=b") && !GoodPresetName(std::string(33, 'x')) && !GoodPresetName("a\tb"));
		std::vector<MapPreset> presets;
		MapPreset plain;
		plain.Name = "평야";
		plain.Values = { { "lakes", 3 }, { "hills", 0 }, { "mountains", 1 }, { "iron", 4 }, { "blocked_up", 2 } };		// 막힘은 쓰지 않는다
		plain.Seed = 12345;
		UpsertPreset(presets, plain);
		MapPreset hills;
		hills.Name = "산악";
		hills.Values = { { "mountains", 5 }, { "hills", 3 } };
		UpsertPreset(presets, hills);
		const std::string text = FormatMapPresets(presets);
		CHECK(text.find("preset 산악 ") != std::string::npos && text.find("preset 평야 ") != std::string::npos);
		CHECK(text.find("blocked_up") == std::string::npos);										// 막힘은 들지 않는다
		CHECK(text.find("preset 평야 lakes=3 hills=0 mountains=1 iron=4 seed=12345\n") != std::string::npos);		// 표의 차례, 씨앗은 끝
		CHECK(text.find("preset 산악 hills=3 mountains=5 seed=-1\n") != std::string::npos);
		std::istringstream in(text);
		const std::vector<MapPreset> again = ParseMapPresets(in);
		CHECK(again.size() == 2 && FindPreset(again, "평야") && FindPreset(again, "평야")->Values.at("iron") == 4 && FindPreset(again, "평야")->Seed == 12345 && FindPreset(again, "산악")->Seed == -1);
		CHECK(FindPreset(again, "없음") == nullptr);
		// 같은 이름으로 다시 저장하면 한 줄로 덮어쓴다(리뷰 포커스 3)
		MapPreset plain2;
		plain2.Name = "평야";
		plain2.Values = { { "lakes", 1 } };
		UpsertPreset(presets, plain2);
		CHECK(presets.size() == 2 && FindPreset(presets, "평야")->Values.size() == 1 && FormatMapPresets(presets).find("preset 평야 lakes=1 seed=-1\n") != std::string::npos);
		CHECK(ErasePreset(presets, "산악") && presets.size() == 1 && !ErasePreset(presets, "산악"));
		// 틀린 줄: 이름 없음, 모르는 열쇠, 수가 아닌 값, 이름에 =. 범위 밖은 당기고 소수는 내린다. 막힘 열쇠는 무시한다(줄은 산다). 같은 이름은 뒤의 것(리뷰 포커스 2)
		std::istringstream bad("preset\npreset x bogus=1\npreset y lakes=abc\npreset a=b lakes=1\n# 주석\npreset ok lakes=9 iron=2.7 blocked_up=5 seed=7\npreset ok lakes=2\n");
		const std::vector<MapPreset> parsed = ParseMapPresets(bad);
		CHECK(parsed.size() == 1 && parsed[0].Name == "ok" && parsed[0].Values.at("lakes") == 2 && parsed[0].Values.count("iron") == 0 && parsed[0].Values.count("blocked_up") == 0 && parsed[0].Seed == -1);
		std::istringstream clamp("preset c lakes=9 iron=2.7 blocked_up=5 seed=7\n");
		const std::vector<MapPreset> clamped = ParseMapPresets(clamp);
		CHECK(clamped.size() == 1 && clamped[0].Values.at("lakes") == 5 && clamped[0].Values.at("iron") == 2 && clamped[0].Values.count("blocked_up") == 0 && clamped[0].Seed == 7);
		// 프리셋의 틀린 씨앗(음수, 소수)은 줄을 버리지 않고 무작위(-1)로 읽는다
		std::istringstream badseed("preset s lakes=1 seed=-5\npreset t lakes=1 seed=2.5\n");
		const std::vector<MapPreset> seeds = ParseMapPresets(badseed);
		CHECK(seeds.size() == 2 && FindPreset(seeds, "s")->Seed == -1 && FindPreset(seeds, "t")->Seed == -1);
	});

	Test("지도: 원격 map 의 낱말", [] {
		MapCommand command;
		std::string why;
		CHECK(MapCommandFromParts("show", {}, command, why) && command.Act == MapAct::Show);
		CHECK(MapCommandFromParts("set", { { "lakes", "3" }, { "iron", "4" }, { "blocked_up", "1" } }, command, why) && command.Act == MapAct::Set && command.Sets.size() == 3);		// 막힘도 받는다(리뷰 포커스 4)
		CHECK(!MapCommandFromParts("set", { { "bogus", "1" } }, command, why) && why.find("bogus") != std::string::npos);
		CHECK(!MapCommandFromParts("set", { { "lakes", "x" } }, command, why) && !MapCommandFromParts("set", {}, command, why));
		CHECK(MapCommandFromParts("regenerate", {}, command, why) && command.Act == MapAct::Regenerate);
		CHECK(MapCommandFromParts("restore", {}, command, why) && command.Act == MapAct::Restore);
		CHECK(MapCommandFromParts("preset", { { "op", "save" }, { "name", "평야" } }, command, why) && command.Act == MapAct::PresetSave && command.Name == "평야");
		CHECK(MapCommandFromParts("preset", { { "op", "load" }, { "name", "a" } }, command, why) && command.Act == MapAct::PresetLoad);
		CHECK(MapCommandFromParts("preset", { { "op", "delete" }, { "name", "a" } }, command, why) && command.Act == MapAct::PresetDelete);
		CHECK(!MapCommandFromParts("preset", { { "op", "load" } }, command, why) && !MapCommandFromParts("preset", { { "op", "x" }, { "name", "a" } }, command, why) && !MapCommandFromParts("preset", { { "op", "save" }, { "name", "a b" } }, command, why));
		CHECK(MapCommandFromParts("seed", { { "value", "12345" } }, command, why) && command.Act == MapAct::Seed && command.Seed == 12345 && !command.SeedRandom);
		CHECK(MapCommandFromParts("seed", { { "value", "random" } }, command, why) && command.SeedRandom);
		CHECK(!MapCommandFromParts("seed", {}, command, why) && !MapCommandFromParts("seed", { { "value", "x" } }, command, why) && !MapCommandFromParts("bogus", {}, command, why));
		CHECK(!MapCommandFromParts("seed", { { "value", "-5" } }, command, why) && why.find("0 이상의 정수") != std::string::npos && !MapCommandFromParts("seed", { { "value", "2.5" } }, command, why));
		// 줄의 읽기(ParseRemoteLine)
		const RemoteCommand set = ParseRemoteLine("map set lakes=3 iron=4");
		CHECK(set.Error.empty() && set.Target == "set" && set.Options.at("lakes") == "3" && set.Options.at("iron") == "4");
		CHECK(!ParseRemoteLine("map set bogus=1").Error.empty() && !ParseRemoteLine("map set lakes=x").Error.empty() && !ParseRemoteLine("map set").Error.empty());
		CHECK(ParseRemoteLine("map show").Error.empty() && ParseRemoteLine("map regenerate").Error.empty() && ParseRemoteLine("map restore").Error.empty());
		const RemoteCommand save = ParseRemoteLine("map preset save name=평야");
		CHECK(save.Error.empty() && save.Target == "preset" && save.Options.at("op") == "save" && save.Options.at("name") == "평야");
		CHECK(!ParseRemoteLine("map preset load").Error.empty() && !ParseRemoteLine("map preset").Error.empty());
		const RemoteCommand seed = ParseRemoteLine("map seed 12345");
		CHECK(seed.Error.empty() && seed.Target == "seed" && seed.Options.at("value") == "12345");
		CHECK(ParseRemoteLine("map seed random").Error.empty() && !ParseRemoteLine("map seed").Error.empty() && !ParseRemoteLine("map").Error.empty() && !ParseRemoteLine("map bogus").Error.empty());
		CHECK(!ParseRemoteLine("map seed -5").Error.empty() && !ParseRemoteLine("map seed 2.5").Error.empty());
	});
}
