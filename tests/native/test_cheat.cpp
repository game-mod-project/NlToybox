#include "common.hpp"

void RunCheatTests()
{
	Test("치트 상태: 최소값(floor)의 줄", [] {
		CheatState state;
		state.Floors = { { "gold", 5000 }, { "wood", 250 } };
		const std::string text = FormatCheatState(state);
		CHECK(text.find("floor gold=5000\n") != std::string::npos && text.find("floor wood=250\n") != std::string::npos);
		// 읽을 수 없는 줄(열쇠에 빈칸, 0 이하, 수가 아닌 값)은 버린다. 같은 열쇠는 뒤의 것이 이긴다.
		std::istringstream in(text + "floor bad key=3\nfloor rune=0\nfloor iron=-2\nfloor food=x\nfloor wood=300\nfloor stone=1e12\nfloor coal=inf\n");
		const CheatState again = ParseCheatState(in);
		CHECK(again.Floors.size() == 2 && again.Floors.at("gold") == 5000 && again.Floors.at("wood") == 300);		// 너무 큰 수와 inf 는 버린다
		// 메모리의 잘못된 항목은 파일에 적지 않는다
		CheatState odd;
		odd.Floors = { { "wood", 250 }, { "bad key", 5 }, { "iron", 0 }, { "stone", 1e12 } };
		CHECK_STR(FormatCheatState(odd), "# NlToyBox 의 치트 상태. 모드창(F8)에서 바꾸면 여기에 저장된다.\nfloor wood=250\n");
		CHECK(KeepKnown(again).Floors == again.Floors);
	});

	Test("치트 상태: 읽고 쓰면 같다", [] {
		CheatState state;
		state.On = { "instant_build", "no_dodge" };
		state.Numbers = { { "rest_decrease", 0 }, { "piety_decrease", 0.83 } };
		state.Pins = { "inst:o_time_controller.time_warp", "map:128@{messenger_cost }" };
		state.Locks = { { "inst:o_character:2.starving_hours", 0 }, { "global.a.b[3]", -4.5 } };

		const std::string text = FormatCheatState(state);
		std::istringstream in(text);
		const CheatState again = ParseCheatState(in);
		CHECK(again.On == state.On && again.Numbers == state.Numbers && again.Pins == state.Pins);
		CHECK(again.Locks.size() == 2 && again.Locks[0].Path == "inst:o_character:2.starving_hours" && again.Locks[0].Value == 0
			&& again.Locks[1].Path == "global.a.b[3]" && again.Locks[1].Value == -4.5);
		CHECK_STR(FormatCheatState(again), text);
	});

	Test("치트 상태: 읽을 수 없는 줄은 버린다", [] {
		std::istringstream in(
			"# 주석\n"
			"\n"
			"on\n"								// 이름이 없다
			"on a b\n"							// 이름에 공백
			"num x=abc\n"						// 수가 아니다
			"num x=nan\n"						// 유한하지 않다
			"num =3\n"							// 이름이 없다
			"pin o_debug.x\n"					// 뿌리를 모른다
			"pin global\n"						// 단계가 없다
			"lock inst:o_debug.x\n"				// 값이 없다
			"lock inst:o_debug.x=1e999\n"		// 유한하지 않다
			"what inst:o_debug.x=1\n"			// 모르는 낱말
			"  on  good  \n"
			"num n = 2.5\n"
			"pin inst:o_debug.x\n"
			"pin inst:o_debug.x\n"				// 같은 주소는 한 번만
			"lock map:1@{a=b}=7\n"				// 마지막 '=' 에서 가른다
			"lock map:1@{a=b}=8\n");			// 같은 주소는 뒤의 것이 이긴다
		const CheatState state = ParseCheatState(in);
		CHECK(state.On == std::set<std::string>{ "good" });
		CHECK(state.Numbers.size() == 1 && state.Numbers.at("n") == 2.5);
		CHECK(state.Pins == std::vector<std::string>{ "inst:o_debug.x" });
		CHECK(state.Locks.size() == 1 && state.Locks[0].Path == "map:1@{a=b}" && state.Locks[0].Value == 8);
	});

	Test("치트 표: 주소가 모두 읽히고 Id 가 겹치지 않는다", [] {
		std::set<std::string> ids;
		for (const Cheat& cheat : Cheats())
		{
			const AskPath path = ParseAskPath(cheat.Path);
			if ((!path.Error.empty() || path.Steps.empty()) && !(IsHook(cheat.Kind) && std::string(cheat.Path).rfind("gml_Script_", 0) == 0))
			{
				std::printf("  FAIL %s: %s (%s)\n", cheat.Id, cheat.Path, path.Error.c_str());
				g_Failed++;
			}
			// 표에는 ds 번호를 적지 않는다. 훅 항목은 이름 있는 스크립트를 이름으로 적을 수 있다(그 이름은 이 게임 버전의 것이다).
			const bool script = IsHook(cheat.Kind) && std::string(cheat.Path).rfind("gml_Script_", 0) == 0;
			CHECK(script || path.Root == "global" || path.Root == "inst");
			CHECK(ids.insert(cheat.Id).second);
			CHECK(std::string(cheat.Id).find_first_of(" =") == std::string::npos);
			CHECK(cheat.Label[0] != 0 && cheat.Help[0] != 0);
			if (cheat.Kind == CheatKind::HookScale || cheat.Kind == CheatKind::CustomScale)		// On: 창이 처음 내놓는 배율. 0 보다 크고 범위 안이다
				CHECK(cheat.Min > 0 && cheat.On >= cheat.Min && cheat.On <= cheat.Max && cheat.On != 1);
			if (HasNumber(cheat.Kind))
				CHECK(cheat.Min < cheat.Max);
			else
				CHECK(cheat.On != cheat.Off);		// Toggle: 써 넣는 두 값. Hook: 바꿔 돌려줄 값(On). Custom: 켬과 끔
		}
		CHECK(Cheats().size() == 80);
		// 거주 칸과 효과의 범위(research/32): 건물 종류의 자료에 모듈이 배율을 쓴다(CustomScale). 주택과 병영은 배율이 따로다(영역도: 인구, 군대).
		CHECK(FindCheat("housing_capacity") && FindCheat("housing_capacity")->Kind == CheatKind::CustomScale && FindCheat("housing_capacity")->Where == Area::People);
		CHECK(FindCheat("barrack_capacity") && FindCheat("barrack_capacity")->Kind == CheatKind::CustomScale && FindCheat("barrack_capacity")->Where == Area::Army);
		CHECK(FindCheat("effect_range") && FindCheat("effect_range")->Kind == CheatKind::CustomScale && FindCheat("effect_range")->Where == Area::Build);
		// 늘리는 배율이다: 1 아래로는 받지 않는다(정원을 줄이면 살던 사람이 넘친다). 효과는 재기 전이다.
		for (const char* id : { "housing_capacity", "barrack_capacity", "effect_range" })
			CHECK(FindCheat(id) && !FindCheat(id)->Verified && FindCheat(id)->Min == 1 && FindCheat(id)->On == 2);
		// 범죄(research/26). 주민이 범죄자가 되기 직전에 게임이 그 사람의 is_criminal_immunity()(불리언)를 묻는다. 참을 돌려주게 한 저녁에는 두 번의 시도에도
		// 범죄자가 생기지 않았고, 그러지 않은 두 저녁에는 다섯 번·한 번의 시도에 다섯·한 명이 생겼다. 주소는 o_character 의 것으로 삼는다(게임 화면에는 o_character 가 언제나 있다).
		CHECK(FindCheat("no_new_criminals") && FindCheat("no_new_criminals")->Kind == CheatKind::Hook && FindCheat("no_new_criminals")->On == 1
			&& FindCheat("no_new_criminals")->Where == Area::Crime && FindCheat("no_new_criminals")->Verified
			&& std::string(FindCheat("no_new_criminals")->Path) == "inst:o_character:0.c_criminal.is_criminal_immunity");
		// 게임 변수를 쓰는 넷은 모듈의 일이고 효과를 보기 전이다(이틀 동안 범죄자들이 범죄를 저지르지 않았다).
		for (const char* id : { "no_bandit_turn", "crime_minds_off", "theft_none" })
			CHECK(FindCheat(id) && FindCheat(id)->Kind == CheatKind::Custom && FindCheat(id)->Where == Area::Crime && !FindCheat(id)->Verified);
		CHECK(FindCheat("thug_days") && FindCheat("thug_days")->Kind == CheatKind::CustomScale && FindCheat("thug_days")->Where == Area::Crime && !FindCheat("thug_days")->Verified
			&& FindCheat("thug_days")->Min >= 1 && FindCheat("thug_days")->On > 1);		// 날을 늘리는 배율이다: 1 아래로는 받지 않는다
		// 지도 공개(research/25): GlobalMapManager.is_initial_area_visible(지역 구조체) -> 불리언. 참을 돌려주게 하자 세계 지도의 안개가 걷혔다
		// (화면에서 봤다: 지역의 밝기, 마을의 이름, 자원 아이콘). 물체의 is_visible 만으로는 지역이 어두운 채였고 is_in_fog_of_war 만으로는 화면이 그대로였다.
		CHECK(FindCheat("reveal_map") && FindCheat("reveal_map")->Kind == CheatKind::Hook && FindCheat("reveal_map")->On == 1 && FindCheat("reveal_map")->Where == Area::World
			&& std::string(FindCheat("reveal_map")->Path) == "inst:o_global_map.__m_global_map.is_initial_area_visible" && HookForcedKind(FindCheat("reveal_map")->Kind) == 'b');
		// 늑대의 최대 수: WolvesManager.get_max_number_of_wolves()(인자 없음)가 밤에 한 번 불려 수(11.1)를 돌려줬다(research/25). 0 을 돌려주게 한다(수를 돌려주던 함수에 수).
		CHECK(FindCheat("no_wolves") && FindCheat("no_wolves")->Kind == CheatKind::HookNumber && FindCheat("no_wolves")->On == 0 && FindCheat("no_wolves")->Where == Area::World
			&& HookForcedKind(FindCheat("no_wolves")->Kind) == 'n');
		// 광산의 매장량 붙들기는 모듈의 일이다(src/World.cpp 가 줄어든 매장량을 되돌려 쓴다).
		CHECK(FindCheat("mine_stock_hold") && FindCheat("mine_stock_hold")->Kind == CheatKind::Custom && FindCheat("mine_stock_hold")->Where == Area::World);
		// 실행 2 에서 플레이로 본 것(research/25): 붙든 네 시간 동안 단계가 넘어가지 않았고 끄자 다음 정각에 넘어갔다, 켠 채 자동 저장 시각을 세 번 넘겨도 파일이 생기지 않았다,
		// 켠 두 밤에는 늑대를 만들지 않았고 끈 두 밤에는 만들었다, 게임이 네 번 캐도 매장량이 그대로였다, 켜자 지도의 안개가 걷히고 끄자 돌아왔다.
		for (const char* id : { "season_hold", "no_autosave", "no_wolves", "mine_stock_hold", "reveal_map" })
			CHECK(FindCheat(id)->Verified);
		CHECK(!FindCheat("fast_map_moving")->Verified && !FindCheat("fast_global_tasks")->Verified);		// 재지 못했다
		// 확인된 훅과 모듈의 일은 켠 채 저장돼 있으면 다음 실행에서도 켜진 채로 시작한다.
		// 저장 끄기는 그 실행에서만 간다(검토 I4): 켠 것을 잊고 다음 날 몇 시간을 해도 자동 저장이 하나도 생기지 않는 일이 없게, 켠 채 저장돼 있어도 꺼진 채로 시작한다.
		CheatState world;
		world.On = { "season_hold", "no_wolves", "mine_stock_hold", "reveal_map", "no_autosave" };
		CHECK(KeepKnown(world).On == (std::set<std::string>{ "season_hold", "no_wolves", "mine_stock_hold", "reveal_map" }));
		CHECK(FindCheat("no_autosave")->ThisRunOnly);
		for (const Cheat& cheat : Cheats())
			CHECK(cheat.ThisRunOnly == (std::string(cheat.Id) == "no_autosave"));
		// 월드(research/25). 계절 붙들기는 모듈의 일(src/World.cpp 가 1초마다 시작 시각을 따라 민다), 세계 지도의 빠른 이동과 자동 저장 끄기는 게임의 디버그 깃발이다.
		// (어느 것이 확인됐는지는 아래에서 본다)
		CHECK(FindCheat("season_hold") && FindCheat("season_hold")->Kind == CheatKind::Custom && FindCheat("season_hold")->Where == Area::World);
		CHECK(FindCheat("fast_map_moving") && FindCheat("fast_map_moving")->Kind == CheatKind::Toggle && FindCheat("fast_map_moving")->Where == Area::World
			&& std::string(FindCheat("fast_map_moving")->Path) == "inst:o_global_map.__m_global_map.__debug_fast_moving");
		CHECK(FindCheat("no_autosave") && FindCheat("no_autosave")->Kind == CheatKind::Toggle && FindCheat("no_autosave")->Where == Area::Util
			&& std::string(FindCheat("no_autosave")->Path) == "inst:o_debug.is_save_disabled" && FindCheat("no_autosave")->On == 1 && FindCheat("no_autosave")->Off == 0);
		// 종교(research/21): 신앙심 채워 두기, 성스러운 보호 유지(수 1 을 돌려주게 한다), 종교 반란 없음, 종교 비용 없음, 신앙 회복 배율, 설교 전환 계수.
		CHECK(FindCheat("piety_full") && FindCheat("piety_full")->Kind == CheatKind::Custom && FindCheat("piety_full")->Where == Area::Religion);
		// 신앙심 채워 두기는 플레이에서 봤다(켜고 40분 뒤 플레이어의 사람 14명이 모두 100, 플레이어의 사람이 아닌 둘은 평소대로 줄었다. research/21)
		CHECK(FindCheat("piety_full")->Verified);
		CHECK(FindCheat("holy_defence") && FindCheat("holy_defence")->Kind == CheatKind::HookNumber && FindCheat("holy_defence")->On == 1
			&& FindCheat("holy_defence")->Where == Area::Religion && !FindCheat("holy_defence")->Verified);
		CHECK(FindCheat("no_religious_riot") && FindCheat("no_religious_riot")->Kind == CheatKind::Hook && FindCheat("no_religious_riot")->On == 0);
		CHECK(FindCheat("religion_free") && FindCheat("religion_free")->Kind == CheatKind::Custom && FindCheat("religion_free")->Where == Area::Religion);
		CHECK(FindCheat("piety_restore") && FindCheat("piety_restore")->Kind == CheatKind::CustomScale && FindCheat("piety_restore")->On == 3
			&& FindCheat("piety_restore")->Min == 1 && FindCheat("piety_restore")->Max == 10);
		// 설교 전환 계수는 값 써 넣기(Number)가 아니라 모듈의 배율이다: global 뿌리에 값을 써 넣는 항목은 메인 메뉴에서도 써지고 창이 보이는 동안 전역을 훑는다.
		CHECK(FindCheat("preach_conversion") && FindCheat("preach_conversion")->Kind == CheatKind::CustomScale && FindCheat("preach_conversion")->On == 3
			&& FindCheat("preach_conversion")->Min == 1 && FindCheat("preach_conversion")->Max == 20 && !FindCheat("preach_conversion")->Verified);
		// 표에 global 뿌리로 값을 써 넣는 항목(Toggle, Number)이 없다
		for (const Cheat& cheat : Cheats())
			if (cheat.Kind == CheatKind::Toggle || cheat.Kind == CheatKind::Number)
				CHECK(std::string(cheat.Path).rfind("global.", 0) != 0);
		// 훅이 바꿔 돌려줄 값의 형: 불리언 판정은 'b', 수를 돌려주는 판정은 'n', 배율은 'x'
		CHECK(HookForcedKind(CheatKind::Hook) == 'b' && HookForcedKind(CheatKind::HookNumber) == 'n' && HookForcedKind(CheatKind::HookScale) == 'x');
		// 신앙 감소는 게임이 따르는 것을 쟀다(0 으로 쓰자 14명의 신앙심이 줄지 않았다)
		CHECK(FindCheat("piety_decrease") && FindCheat("piety_decrease")->Verified);
		// 수를 돌려주게 하는 훅: 함수의 답을 바꾸는 종류이고 창에서 수를 정하지 않는다. 확인 전의 것은 켠 채 저장돼 있어도 꺼진 채로 시작한다.
		CHECK(IsHook(CheatKind::HookNumber) && !HasNumber(CheatKind::HookNumber));
		{
			CheatState saved;
			saved.On = { "holy_defence", "no_religious_riot", "piety_full", "religion_free" };
			const CheatState kept = KeepKnown(saved);
			CHECK(std::find(kept.On.begin(), kept.On.end(), "holy_defence") == kept.On.end());
			CHECK(std::find(kept.On.begin(), kept.On.end(), "no_religious_riot") == kept.On.end());
			// 확인 전의 모듈 항목(religion_free)도 꺼진 채로 시작한다. 확인된 것(piety_full)만 남는다.
			CHECK(kept.On.size() == 1 && kept.On.count("piety_full") == 1);
			// 수가 있는 것: 확인 전의 배율(piety_restore, preach_conversion)은 버리고, 확인된 값 써 넣기(piety_decrease)는 0 인 채로 남긴다
			saved.Numbers = { { "piety_restore", 3 }, { "preach_conversion", 5 }, { "piety_decrease", 0 } };
			const CheatState numbers = KeepKnown(saved);
			CHECK(numbers.Numbers.size() == 1 && numbers.Numbers.count("piety_decrease") == 1 && numbers.Numbers.at("piety_decrease") == 0);
		}
		// 종교의 게임 변수(global.__gameplay_vars 의 열쇠): 비용 여섯과 신앙 회복 넷. 겹치지 않는다.
		{
			const auto& costs = ReligionCostVars();
			const auto& restores = PietyRestoreVars();
			CHECK(costs.size() == 6 && restores.size() == 4);
			const auto& preach = PreachFactorVars();
			CHECK(preach.size() == 1 && std::string(preach[0]) == "church_preach_conversion_factor");
			const auto has = [](const std::vector<const char*>& list, const char* name) {
				return std::find_if(list.begin(), list.end(), [&](const char* item) { return std::string(item) == name; }) != list.end();
			};
			CHECK(has(costs, "religiosity_confession_cost") && has(costs, "religiosity_divorce_cost") && has(costs, "religiosity_begging_cost")
				&& has(costs, "religiosity_canonization_cost_gold") && has(costs, "religiosity_canonization_cost_per_province") && has(costs, "religiosity_sacrificer_cost_gold"));
			CHECK(has(restores, "church_pray_piety_restore") && has(restores, "altar_pray_piety_restore") && has(restores, "church_pray_morning_service_restore")
				&& has(restores, "trait_saint_piety_talk_restore"));
			std::set<std::string> all;
			for (const char* name : costs) all.insert(name);
			for (const char* name : restores) all.insert(name);
			for (const char* name : preach) all.insert(name);
			CHECK(all.size() == 11);
			CHECK(all.count("church_max_capacity") == 0);		// 교회 수용은 배율(src/Tweaks.cpp)이 다룬다. 두 곳이 한 자리를 쓰지 않는다
		}
		// 최소값 유지(research/18): 경제 패널의 코드가 한다(Custom). 켜고 끄는 것만 표에 있고 바닥은 상태 파일의 floor 줄에 있다.
		CHECK(FindCheat("resource_floor") && FindCheat("resource_floor")->Kind == CheatKind::Custom && FindCheat("resource_floor")->Where == Area::Economy);
		// 플레이에서 확인했다(research/18): 켠 채 저장돼 있으면 다음 실행에서도 켜진 채로 시작한다.
		CheatState floor_on;
		floor_on.On = { "resource_floor" };
		floor_on.Floors = { { "wood", 100 } };
		CHECK(FindCheat("resource_floor")->Verified && KeepKnown(floor_on).On.count("resource_floor") == 1 && KeepKnown(floor_on).Floors.size() == 1);
		// 전투(research/16, 23): 영혼의 두 함수에 아군과 적의 배율을 따로 건다(모듈의 코드가 한다: CustomScale).
		for (const char* id : { "ally_power", "enemy_power", "ally_toughness", "enemy_toughness" })
			CHECK(FindCheat(id) && FindCheat(id)->Kind == CheatKind::CustomScale && FindCheat(id)->Where == Area::Army);
		// 전투 기술의 둘은 실제 싸움 둘에서 봤다(두 진영의 수가 바뀌고 공격 추첨의 배율이 그 수를 따른다. research/23. 사용자가 확인으로 올리기로 했다).
		// 맷집의 둘은 다음 싸움으로 미뤘다(적이 낮춘 한도에서 죽는지를 2초 간격의 읽기로는 가리지 못했다).
		CHECK(FindCheat("ally_power")->Verified && FindCheat("enemy_power")->Verified);
		CHECK(!FindCheat("ally_toughness")->Verified && !FindCheat("enemy_toughness")->Verified);
		CheatState battle;
		battle.Numbers = { { "ally_power", 2 }, { "enemy_power", 0.5 }, { "ally_toughness", 3 }, { "enemy_toughness", 0.3 } };
		const CheatState kept_battle = KeepKnown(battle);
		CHECK(kept_battle.Numbers.count("ally_power") == 1 && kept_battle.Numbers.count("enemy_power") == 1);			// 확인된 것은 다음 실행에서도 걸린 채
		CHECK(kept_battle.Numbers.count("ally_toughness") == 0 && kept_battle.Numbers.count("enemy_toughness") == 0);	// 확인 전의 것은 꺼진 채로 시작한다
		CHECK(FindCheat("ally_power")->Min >= 1 && FindCheat("ally_toughness")->Min >= 1);			// 아군의 것은 올리기만
		CHECK(FindCheat("enemy_power")->Max <= 1 && FindCheat("enemy_toughness")->Max <= 1 && FindCheat("enemy_power")->Min > 0);		// 적의 것은 내리기만(0 은 아니다)
		// 외교(research/14): 세력의 적대 판정 Faction.is_enemy_with(세력) -> 불리언을 false 로. 모든 세력에 걸린다. 효과는 보지 못했다.
		CHECK(FindCheat("no_enemies") && FindCheat("no_enemies")->Kind == CheatKind::Hook && FindCheat("no_enemies")->Where == Area::Diplomacy
			&& FindCheat("no_enemies")->On == 0 && !FindCheat("no_enemies")->Verified);
		// 아군 무적(research/13): 상처를 입히는 함수를 플레이어의 사람에게만 건너뛴다. 모듈의 코드가 건다(Custom). 가려지는 것은 아직 보지 못했다.
		CHECK(FindCheat("ally_invincible") && FindCheat("ally_invincible")->Kind == CheatKind::Custom && FindCheat("ally_invincible")->Where == Area::Army
			&& FindCheat("ally_invincible")->Verified);		// research/13, 16: 직접 부른 호출이 가려졌고, 게임의 호출에서 self 가 영혼이다
		// 군대(research/13): 병사의 고용 값. SoulBasic.get_soldier_cost() 가 돌려주는 수에 곱한다. 고용 창의 값과 실제로 빠진 금화로 봤다(160 → 16).
		CHECK(FindCheat("hire_cost") && FindCheat("hire_cost")->Kind == CheatKind::HookScale && FindCheat("hire_cost")->Where == Area::Army
			&& FindCheat("hire_cost")->Verified && FindCheat("hire_cost")->Max <= 1 && FindCheat("hire_cost")->Off == 1);		// 값은 정수로 남긴다
		// 5단계: 연구 시간(research/12). 도서관 관리자의 get_learn_time 이 돌려주는 수에 곱한다. 효과는 보지 못했다.
		CHECK(FindCheat("research_time") && FindCheat("research_time")->Kind == CheatKind::HookScale && FindCheat("research_time")->Where == Area::Knowledge
			&& !FindCheat("research_time")->Verified && FindCheat("research_time")->Max <= 1);
		// 4단계: 인구·욕구(research/11). 플레이어의 사람을 돌며 쓰는 항목은 모듈의 코드가 한다(src/People.cpp).
		for (const char* id : { "no_hunger", "no_tiredness", "needs_full", "always_happy", "no_old_age_death" })
			CHECK(FindCheat(id) && FindCheat(id)->Kind == CheatKind::Custom && FindCheat(id)->Where == Area::People);
		// 플레이에서 봤다(research/11): 욕구가 100 으로 유지되고(손님은 그대로), 기분이 35 → 98 이 됐다. 노화 깃발은 써지는 것까지만 봤다.
		for (const char* id : { "no_hunger", "no_tiredness", "needs_full", "always_happy" })
			CHECK(FindCheat(id)->Verified);
		CHECK(!FindCheat("no_old_age_death")->Verified);
		// 이주민 보너스는 플레이에서 봤다: 3 을 쓰자 그날 저녁 3명이 왔다.
		CHECK(FindCheat("daily_migrants") && FindCheat("daily_migrants")->Kind == CheatKind::Number && FindCheat("daily_migrants")->Verified
			&& FindCheat("daily_migrants")->Min == 0 && FindCheat("daily_migrants")->Max == 50);
		// 3나-3: 거래·생산·창고 용량(research/10). 게임의 함수가 돌려주는 수에 배율을 곱하는 훅(HookScale)과, 모듈이 자료를 돌며 배율을 쓰는 항목(CustomScale).
		for (const char* id : { "buy_price", "sell_price", "market_depth", "production_time", "worker_performance" })
			CHECK(FindCheat(id) && FindCheat(id)->Kind == CheatKind::HookScale);
		for (const char* id : { "storage_capacity", "production_amount" })
			CHECK(FindCheat(id) && FindCheat(id)->Kind == CheatKind::CustomScale);
		CHECK(FindCheat("production_free") && FindCheat("production_free")->Kind == CheatKind::Custom);
		CHECK(FindCheat("buy_price") && FindCheat("buy_price")->Where == Area::Economy && FindCheat("buy_price")->Max <= 1);		// 싸게 산다
		CHECK(FindCheat("sell_price") && FindCheat("sell_price")->Min >= 1);														// 비싸게 판다
		CHECK(FindCheat("production_time") && FindCheat("production_time")->Where == Area::Build && FindCheat("production_time")->Max <= 1);
		CHECK(HasNumber(CheatKind::Number) && HasNumber(CheatKind::HookScale) && HasNumber(CheatKind::CustomScale));
		CHECK(!HasNumber(CheatKind::Hook) && !HasNumber(CheatKind::Toggle) && !HasNumber(CheatKind::Custom));
		CHECK(IsHook(CheatKind::Hook) && IsHook(CheatKind::HookScale) && !IsHook(CheatKind::Custom) && !IsHook(CheatKind::CustomScale) && !IsHook(CheatKind::Number));
		// 건설 조건과 건설비(research/09). 조건은 게임의 함수가 돌려주는 값을 바꾸는 훅이고, 비용은 모듈이 건물 종류를 돌며 0 으로 쓴다.
		CHECK(FindCheat("build_any") && FindCheat("build_any")->Kind == CheatKind::Hook && FindCheat("build_any")->On == 1 && FindCheat("build_any")->Verified);
		CHECK(FindCheat("build_marks") && FindCheat("build_marks")->Kind == CheatKind::Hook && !FindCheat("build_marks")->Verified);
		CHECK(FindCheat("build_free") && FindCheat("build_free")->Kind == CheatKind::Custom && FindCheat("build_free")->Where == Area::Build);
		// 비용 없음은 플레이에서 봤다(research/09: 돼지 농장을 짓고 올려도 나무가 줄지 않았다. 켠 채 저장한 세이브를 불러와도 비용은 원래 값이었다).
		CHECK(FindCheat("build_free")->Verified);
		// 즉시 업그레이드: 업그레이드 중인 건물에 게임의 build_instantly() 를 부른다(모듈의 코드가 한다). 사용자가 누른 주택 세 채가 바로 올랐다.
		CHECK(FindCheat("instant_upgrade") && FindCheat("instant_upgrade")->Kind == CheatKind::Custom && FindCheat("instant_upgrade")->Where == Area::Build);
		CHECK(FindCheat("instant_upgrade")->Verified);
		// 사용자가 플레이에서 본 것(research/07): 즉시 건설은 된다. 자원 편집 모드는 쓸 수 없어 표에서 뺐다(경제 패널이 맡는다).
		// 건설 목록 모두 열기도 사용자가 본 것이다(목록은 풀리고 조건은 그대로). 2026-10-07 의 확인 캠페인에서 그 범위로 확인으로 올렸다(research/27).
		CHECK(FindCheat("instant_build")->Verified && FindCheat("build_all")->Verified);
		CHECK(FindCheat("resources_edit_mode") == nullptr);
		// 창고 용량은 넣지 않는다: 게임이 다시 채우는 캐시라 "원래대로"가 낡은 값을 써 넣는다. 자리를 잰 뒤(3나-2)에 넣는다.
		CHECK(FindCheat("cap_food") == nullptr);
	});

	Test("치트 표: 켤 때 먼저 있어야 하는 자리(Gate) — 즉시 건설은 영주관이 놓인 뒤에만 쓴다", [] {
		// 새 게임의 영주관 배치 때 is_instant_build_buildings 가 1 이면 게임이 끝난다(InspectionManager.check_hall_inspection. 2026-10-07 가르기: 즉시 건설만 켜도 끝났고, 아무것도 안 켜면 됐다. research/30).
		const Cheat* instant = FindCheat("instant_build");
		CHECK(instant && instant->Gate && std::string(instant->Gate) == "inst:o_game_map_controller.__province.__cached_hall");
		CHECK(GateWaits(*instant, false, false));			// 자리를 읽지 못했다: 기다린다
		CHECK(GateWaits(*instant, true, false));			// 읽었는데 없다(undefined, noone): 기다린다
		CHECK(!GateWaits(*instant, true, true));			// 영주관이 있다: 쓴다
		CHECK_STR(GateNote(*instant), "영주관이 놓인 뒤에 적용");
		const Cheat* all = FindCheat("build_all");
		CHECK(all && !all->Gate && !GateWaits(*all, false, false) && !GateWaits(*all, true, false));		// 자리가 없는 항목은 기다리지 않는다
		for (const Cheat& cheat : Cheats())
			if (cheat.Gate)
				CHECK(ParseAskPath(cheat.Gate).Error.empty());
	});

	Test("치트 표: 영역은 Key 로 찾고 목록의 차례가 열거형과 같다", [] {
		std::set<std::string> keys;
		for (const AreaInfo& area : Areas())
		{
			CHECK(keys.insert(area.Key).second);
			CHECK(FindArea(area.Key) == &area);
			CHECK(&GetArea(area.Id) == &area);
			CHECK(area.Stage >= 2 && area.Stage <= 7);
		}
		CHECK(Areas().size() == 17);
		// 범죄 영역(research/26): 제 패널(src/Crime.cpp)이 있다. 표의 차례와 열거의 차례가 같아야 한다(GetArea 가 번호로 집는다).
		CHECK(FindArea("crime") && FindArea("crime")->Id == Area::Crime && FindArea("crime")->Panel && &GetArea(Area::Crime) == FindArea("crime"));
		for (const AreaInfo& area : Areas())
			CHECK(&GetArea(area.Id) == &area);
		// 표의 항목이 없어도 제 패널이 있는 영역은 목록에서 켜져 있어야 한다. 경제는 표의 항목을 모두 뺀 뒤 목록에서 꺼져 있었다(research/08).
		for (const AreaInfo& area : Areas())
		{
			// 인물·영주·인구는 제 패널(src/People.cpp)이 있다.
			const bool panel = area.Id == Area::Explorer || area.Id == Area::Economy || area.Id == Area::Time
				|| area.Id == Area::Person || area.Id == Area::Lord || area.Id == Area::People
				|| area.Id == Area::Knowledge || area.Id == Area::Items		// 지식·아이템도 제 패널이 있다(src/People.cpp)
				|| area.Id == Area::Army									// 군대: 병사를 만드는 단추
				|| area.Id == Area::Events || area.Id == Area::Religion		// 이벤트 쿨다운 지우기, 주교 부르기(src/World.cpp)
				|| area.Id == Area::Diplomacy								// 왕국과의 관계(src/Diplomacy.cpp)
				|| area.Id == Area::Crime									// 부랑자와 영주의 죄(src/Crime.cpp)
				|| area.Id == Area::Presets;								// 프리셋: 확인된 항목의 묶음(core/Presets)
			CHECK(area.Panel == panel);
		}
		CHECK(FindArea("nope") == nullptr);
		CHECK(FindArea("tweaks") == nullptr);		// "배율" 영역은 없어졌다. 배율 7개는 제 영역의 패널에 그린다(아래)
		CHECK_STR(GetArea(Area::Time).Key, "time");
		for (const Cheat& cheat : Cheats())
			CHECK(FindArea(GetArea(cheat.Where).Key) != nullptr);
	});

	Test("치트 표: 모르는 Id 와 종류가 다른 Id 를 버리고 수를 범위 안으로 당긴다", [] {
		CheatState state;
		state.On = { "instant_build", "rest_decrease", "nope" };			// rest_decrease 는 Number 다
		state.Numbers = { { "rest_decrease", 999 }, { "instant_build", 1 }, { "nope", 1 } };
		state.Pins = { "inst:o_debug.x" };
		state.Locks = { { "inst:o_debug.y", 3 } };
		const CheatState kept = KeepKnown(state);
		CHECK(kept.On == std::set<std::string>{ "instant_build" });
		CHECK(kept.Numbers.size() == 1 && kept.Numbers.at("rest_decrease") == FindCheat("rest_decrease")->Max);
		CHECK(kept.Pins == state.Pins && kept.Locks.size() == 1);
		CHECK(FindCheat("nope") == nullptr && FindCheat("instant_build")->Kind == CheatKind::Toggle);
		// 훅과 모듈 항목은 효과를 확인한 것만 켠 채로 시작한다(수 항목은 On 에서 빠진다).
		// 확인 전의 것은 켠 채 저장돼 있어도 꺼진 채로 시작한다: 창을 열지도 않았는데 게임의 판정이 바뀌거나(build_marks 는 지식 창도 쓰는 함수다)
		// 건설비가 0 으로 쓰여 세이브에 굳는 일(build_free. 세이브에 들어가는지 재지 않았다)이 없게.
		CheatState hooks;
		hooks.On = { "build_any", "build_free", "build_marks", "instant_upgrade", "rest_decrease", "build_all" };
		const CheatState kept_hooks = KeepKnown(hooks);
		CHECK(FindCheat("build_any")->Verified && !FindCheat("build_marks")->Verified);
		// 확인한 훅·모듈 항목은 남고(build_any, build_free, instant_upgrade) 확인 전의 것(build_marks)은 빠진다.
		// 값을 쓰는 스위치(Toggle: build_all)는 확인 전이어도 그대로다.
		CHECK(kept_hooks.On == (std::set<std::string>{ "build_any", "build_free", "instant_upgrade", "build_all" }));

		// 수가 있는 훅·모듈 항목(배율)은 Numbers 에 든다. 수는 범위 안으로 당기고, 확인 전의 것은 버린다(꺼진 채로 시작한다).
		CheatState scales;
		scales.On = { "production_time", "storage_capacity" };				// 수가 있는 항목은 On 에 들지 않는다
		scales.Numbers = { { "production_time", 0.0000001 }, { "storage_capacity", 5000000 }, { "rest_decrease", 3 }, { "build_any", 1 } };
		const CheatState kept_scales = KeepKnown(scales);
		CHECK(kept_scales.On.empty());
		CHECK(kept_scales.Numbers.count("rest_decrease") == 1 && kept_scales.Numbers.count("build_any") == 0);
		// 창고 용량 배율은 플레이에서 확인했다(research/10): 남고, 범위의 끝(Max)으로 당겨진다.
		CHECK(FindCheat("storage_capacity")->Verified && kept_scales.Numbers.count("storage_capacity") == 1
			&& kept_scales.Numbers.at("storage_capacity") == FindCheat("storage_capacity")->Max);
		for (const char* id : { "production_time", "storage_capacity" })
		{
			const Cheat* cheat = FindCheat(id);
			CHECK(cheat != nullptr);
			if (cheat)
				CHECK(kept_scales.Numbers.count(id) == (cheat->Verified ? 1u : 0u));
			if (cheat && cheat->Verified)
				CHECK(kept_scales.Numbers.at(id) >= cheat->Min && kept_scales.Numbers.at(id) <= cheat->Max);
		}
	});
}
