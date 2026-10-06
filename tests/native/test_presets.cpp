#include "common.hpp"

void RunPresetsTests()
{
	Test("프리셋: 표의 확인된 항목만, 범위 안의 수로", [] {
		CHECK(Presets().size() == 4);
		std::vector<std::string> keys;
		for (const Preset& preset : Presets())
		{
			std::string why;
			CHECK(CheckPreset(preset, why));
			CHECK(std::find(keys.begin(), keys.end(), preset.Key) == keys.end());
			keys.push_back(preset.Key);
			CHECK(FindPreset(preset.Key) == &preset);
		}
		CHECK(FindPreset("normal") && FindPreset("normal")->Items.empty());		// 기본: 모두 끈다
		CHECK(FindPreset("easy") && FindPreset("sandbox") && FindPreset("god"));
		CHECK(!FindPreset("nope") && !FindPreset(""));

		// 확인 전의 항목, 없는 항목, 범위 밖의 수, 수가 없는 항목에 준 수, 겹친 항목, 값을 써 넣는 항목은 거부한다
		std::string why;
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "no_enemies", 0 } } }, why));				// 확인 전의 항목
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "no_such_cheat", 0 } } }, why));
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "production_time", 5 } } }, why));
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "production_time", 0 } } }, why));			// 배율 항목은 배율을 준다
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "build_free", 2 } } }, why));
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "build_free", 0 }, { "build_free", 0 } } }, why));
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "daily_migrants", 5 } } }, why));			// 세이브에 남는 값을 쓰는 항목은 묶음에 넣지 않는다
		CHECK(CheckPreset(Preset{ "x", "x", "", { { "build_free", 0 }, { "production_time", 0.5 } } }, why));

		// 확인된 전투 기술의 배율은 신 묶음에 든다(research/23). 맷집의 배율은 확인 전이라 들지 않는다.
		const auto in_god = [](const char* id) {
			for (const PresetItem& item : FindPreset("god")->Items)
				if (std::string(item.Id) == id)
					return item.Number;
			return -1.0;
		};
		CHECK(in_god("ally_power") == 2 && in_god("enemy_power") == 0.5 && in_god("ally_toughness") == -1.0 && in_god("enemy_toughness") == -1.0);

		// god 는 sandbox 가 켜는 것을 모두 켠다
		for (const PresetItem& item : FindPreset("sandbox")->Items)
		{
			bool found = false;
			for (const PresetItem& other : FindPreset("god")->Items)
				found = found || std::string(other.Id) == item.Id;
			CHECK(found);
		}

		// 표의 지금 상태가 어느 묶음과 같은가(검토의 지적: 프리셋은 저장되어 다음 실행에서도 켜진 채 시작한다. 패널이 지금의 상태를 보인다)
		CHECK(MatchPreset({}) == FindPreset("normal"));
		std::vector<CheatOn> on;
		for (const PresetItem& item : FindPreset("sandbox")->Items)
			on.push_back({ item.Id, item.Number });
		CHECK(MatchPreset(on) == FindPreset("sandbox"));
		on.push_back({ "no_hunger", 0 });
		CHECK(MatchPreset(on) == nullptr);								// 하나 더 켜져 있다
		on.pop_back();
		on.pop_back();
		CHECK(MatchPreset(on) == nullptr);								// 하나가 꺼져 있다
		on.clear();
		for (const PresetItem& item : FindPreset("easy")->Items)
			on.push_back({ item.Id, std::string(item.Id) == "production_time" ? 0.25 : item.Number });
		CHECK(MatchPreset(on) == nullptr);								// 배율이 다르다
		CHECK(MatchPreset({ { "ally_invincible", 0 } }) == nullptr);		// 묶음에 없는 항목만 켜져 있다

		// 시간의 멈춤·다시 흐르게가 돌려주는 글: 게임 화면이 아니면 부르지 않고, 부른 뒤의 상태를 단정하지 않는다
		CHECK(TimeReport(true, TimeCall::NotInGame, "") == "게임 화면에서만 됩니다" && TimeReport(false, TimeCall::NotInGame, "") == "게임 화면에서만 됩니다");
		CHECK(TimeReport(true, TimeCall::Failed, "no member") == "멈춤을 부르지 못했습니다: no member");
		CHECK(TimeReport(false, TimeCall::Failed, "x") == "다시 흐르게를 부르지 못했습니다: x");
		CHECK(TimeReport(true, TimeCall::Called, "").find("멈춤을 불렀습니다") == 0 && TimeReport(true, TimeCall::Called, "").find("멈췄습니다") == std::string::npos);
		CHECK(TimeReport(false, TimeCall::Called, "").find("다시 흐르게를 불렀습니다") == 0 && TimeReport(false, TimeCall::Called, "").find("x1") != std::string::npos);

		RemoteCommand c = ParseRemoteLine("preset god");
		CHECK(c.Error.empty() && c.Verb == "preset" && c.Target == "god");
		CHECK(!ParseRemoteLine("preset").Error.empty() && !ParseRemoteLine("preset nope").Error.empty() && !ParseRemoteLine("preset god now").Error.empty());
		CHECK(ParseRemoteLine("time pause").Error.empty() && ParseRemoteLine("time resume").Target == "resume");
		CHECK(!ParseRemoteLine("time stop").Error.empty() && !ParseRemoteLine("time").Error.empty() && !ParseRemoteLine("time pause now").Error.empty());
	});
}
