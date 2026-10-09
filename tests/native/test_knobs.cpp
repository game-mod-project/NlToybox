#include "common.hpp"

void RunKnobsTests()
{
	Test("배율: 정수였던 값은 정수로 맞추고 반은 올린다", [] {
		CHECK(Scale(30, 0.5) == 15);
		CHECK(Scale(5, 0.5) == 3);			// 2.5 는 3
		CHECK(Scale(15, 0.5) == 8);			// 7.5 는 8
		CHECK(Scale(300, 3) == 900);
		CHECK(Scale(10, 1) == 10);
		CHECK(Scale(-6, 2) == -12);
	});

	Test("배율: 0 보다 큰 정수는 0 이 되지 않는다", [] {
		CHECK(Scale(5, 0.05) == 1);
		CHECK(Scale(1, 0.1) == 1);
		CHECK(Scale(0, 3) == 0);			// 원래 0 인 값은 0 이다
	});

	Test("배율: 소수였던 값은 그대로 곱한다", [] {
		CHECK(Scale(0.5, 3) == 1.5);
		CHECK(Scale(2.5, 0.5) == 1.25);
	});

	Test("배율은 허용 범위 안으로 당긴다", [] {
		CHECK(ClampFactor(0) == k_MinFactor);
		CHECK(ClampFactor(-3) == k_MinFactor);
		CHECK(ClampFactor(1000) == k_MaxFactor);
		CHECK(ClampFactor(0.5) == 0.5);
		CHECK(ClampFactor(std::nan("")) == 1);
	});

	Test("설정 파일: 쓴 것을 그대로 읽고, 읽을 수 없는 줄은 버린다", [] {
		std::istringstream in("# 주석\nbuilding_cost=0.5\n start_resources = 3 \nbroken\nbad=abc\nhuge=999\n=1\n");
		const std::map<std::string, double> values = ParseSettings(in);
		CHECK(values.size() == 3);
		CHECK(values.at("building_cost") == 0.5 && values.at("start_resources") == 3 && values.at("huge") == k_MaxFactor);

		std::istringstream again(FormatSettings(values));
		CHECK(ParseSettings(again) == values);
	});

	Test("치트 표: 배율 7개는 제 영역에 놓인다", [] {
		// 이름은 NlToyBox.settings.txt 의 것 그대로다(사용자의 설정 파일이 그대로 읽힌다).
		const std::map<std::string, Area> want = {
			{ "building_cost", Area::Build }, { "start_resources", Area::Economy }, { "book_exp", Area::Knowledge }, { "bribe_cost", Area::Diplomacy },
			{ "free_lord_stay", Area::Lord }, { "church_capacity", Area::Religion }, { "tavern_capacity", Area::People },
		};
		CHECK(KnobPlaces().size() == want.size());
		for (const KnobPlace& place : KnobPlaces())
		{
			const auto it = want.find(place.Id);
			CHECK(it != want.end());
			if (it != want.end())
				CHECK(it->second == place.Where && KnobArea(place.Id) == place.Where);
		}
		CHECK(HasKnobs(Area::Build) && HasKnobs(Area::Knowledge) && HasKnobs(Area::Lord) && !HasKnobs(Area::Explorer) && !HasKnobs(Area::Time) && !HasKnobs(Area::Army));
		CHECK(KnobArea("no_such_knob") == Area::Explorer);		// 모르는 이름: 어느 영역의 패널에도 그리지 않는다(탐색기에는 배율이 없다)
	});

	Test("배율 7개의 정의(KnobDefs)는 그리는 자리(KnobPlaces)와 같은 Id 집합이다", [] {
		// Tweaks 가 쓰는 정의의 표와 CheatTable 의 자리의 표가 따로 있어, 한쪽에 빠뜨리면 창에 나오지 않거나 그려지지 않았다(2026-10-07 리뷰 R19).
		std::set<std::string> defs, places;
		for (const KnobDef& def : KnobDefs())
		{
			CHECK(def.Id && *def.Id && def.Label && *def.Label);
			CHECK(defs.insert(def.Id).second);		// Id 가 겹치지 않는다
			CHECK((def.What == KnobTarget::GameplayVar) == (def.Var != nullptr && *def.Var));		// 게임 변수의 배율만 멤버 이름을 가진다
		}
		for (const KnobPlace& place : KnobPlaces())
			places.insert(place.Id);
		CHECK(defs.size() == 7 && defs == places);
	});
}
