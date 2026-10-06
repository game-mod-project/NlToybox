#include "CheatTable.hpp"

#include <algorithm>

namespace NlCore
{
	const std::vector<AreaInfo>& Areas()
	{
		static const std::vector<AreaInfo> areas = {
			{ Area::Explorer, "explorer", "탐색기", 2, true },
			{ Area::Economy, "economy", "경제", 3, true },
			{ Area::Build, "build", "건설·생산", 3 },
			// 인물·영주·인구는 제 패널(src/People.cpp)이 있다.
			{ Area::Person, "person", "인물", 4, true },
			{ Area::Lord, "lord", "영주", 4, true },
			{ Area::People, "people", "인구·욕구", 4, true },
			// 지식·아이템도 제 패널(src/People.cpp)이 있다.
			{ Area::Knowledge, "knowledge", "지식", 5, true },
			{ Area::Items, "items", "아이템", 5, true },
			{ Area::Army, "army", "군대·전투", 5, true },		// 병사를 만드는 단추(src/People.cpp)
			{ Area::Diplomacy, "diplomacy", "외교", 6, true },		// 왕국과의 관계(src/Diplomacy.cpp)
			{ Area::Religion, "religion", "종교", 6, true },		// 주교 부르기(src/World.cpp)
			{ Area::Time, "time", "시간", 2, true },
			{ Area::World, "world", "월드", 6 },
			{ Area::Events, "events", "이벤트", 6, true },			// 이벤트 쿨다운 지우기(src/World.cpp)
			{ Area::Util, "util", "유틸", 7 },
			{ Area::Presets, "presets", "프리셋", 7, true },		// 확인된 항목의 묶음(core/Presets, src/Cheats.cpp)
		};
		return areas;
	}

	const AreaInfo& GetArea(Area Id)
	{
		return Areas()[static_cast<size_t>(Id)];
	}

	const AreaInfo* FindArea(const std::string& Key)
	{
		for (const AreaInfo& area : Areas())
			if (Key == area.Key)
				return &area;
		return nullptr;
	}

	const std::vector<KnobPlace>& KnobPlaces()
	{
		static const std::vector<KnobPlace> places = {
			{ "building_cost", Area::Build },
			{ "start_resources", Area::Economy },
			{ "book_exp", Area::Knowledge },
			{ "bribe_cost", Area::Diplomacy },
			{ "free_lord_stay", Area::Lord },
			{ "church_capacity", Area::Religion },
			{ "tavern_capacity", Area::People },
		};
		return places;
	}

	Area KnobArea(const std::string& Id)
	{
		for (const KnobPlace& place : KnobPlaces())
			if (Id == place.Id)
				return place.Where;
		return Area::Explorer;
	}

	bool HasKnobs(Area Where)
	{
		for (const KnobPlace& place : KnobPlaces())
			if (place.Where == Where)
				return true;
		return false;
	}

	const std::vector<Cheat>& Cheats()
	{
		constexpr CheatKind T = CheatKind::Toggle, N = CheatKind::Number, H = CheatKind::Hook, C = CheatKind::Custom;
		constexpr CheatKind HS = CheatKind::HookScale, CS = CheatKind::CustomScale, HN = CheatKind::HookNumber;

		// 이름과 값은 새 게임 덤프(refs/runtime/stage0b-run2.late1.json 의 instances.o_debug.members)에서 봤다.
		// Off 는 그 덤프의 값이다. 뜻은 변수 이름에서 읽은 것이고 효과는 아직 재지 않았다(Verified = false).
		// 차례: Id, 영역, 이름, 주소, 종류, On, Off, Min, Max, Verified, 설명
		static const std::vector<Cheat> cheats = {
			// o_debug.is_resources_edit_mode 는 넣지 않는다: 켜도 자원의 목록이 나오지 않아 쓸 수 없었다(사용자가 플레이에서 봤다. research/07).
			// 금화와 자원은 경제 패널(src/Economy.cpp)이 게임의 함수로 바꾼다.

			// 최소값 유지(research/18). 경제 패널의 '최소' 칸에 적은 수보다 적어진 금화·신성 반지·자원을 그 수까지 채운다(src/Economy.cpp 가 1초마다 본다).
			// 채우는 길은 패널의 단추와 같다: budget_money_change(변화량), 영지 창고의 change(자원 번호, 변화량). 바닥은 상태 파일의 floor 줄에 남는다.
			// 플레이에서 봤다(2026-10-06, 0.18.0): 켜자 3초 안에 금화 3000·반지 10·나무 6000·당근 400 까지 찼고, 낮추면 다시 찼고, 네 시간 동안 먹히는 당근이
			// 바닥 아래로 내려가지 않았고, 끄자 채우지 않았다.
			{ "resource_floor", Area::Economy, "최소값 유지", "inst:o_game_map_controller.__province.__warehouse.change", C, 1, 0, 0, 1, true,
				"아래의 '최소' 칸에 적은 수보다 적어진 금화, 신성 반지, 자원을 그 수까지 채운다(1초마다). 줄이지는 않는다" },

			// 창고 용량(…__warehouse.__cached_total_capacity_for_storage_type.<갈래>)은 아직 넣지 않는다: 게임이 다시 채우는 캐시라
			// "원래대로"가 낡은 값을 써 넣게 된다. 게임이 그 값을 언제 다시 만드는지 잰 뒤(3나-2)에 넣는다.

			// 거래(research/10). 거래 탁자의 상품은 값을 들고 있지 않다. TradeManager 의 buy_default_get(자원)·sell_default_get(자원)이 돌려주는
			// 기본 가격(__fair_trade_default_price_buy·_sell)에서 그때그때 셈한다(__get_raw_price(방향)). 그 함수가 돌려주는 수에 배율을 곱한다.
			// 켠 채 그 함수를 부르면 곱한 값이 나온다(룬 100 → 50, 당근 판매 5 → 15, 나무의 시장 깊이 130 → 1300). 상인과의 거래 창에서는 보지 못했다(상단이 없었다).
			{ "buy_price", Area::Economy, "구매가 배율", "inst:o_game_map_controller.__trade_manager.buy_default_get", HS, 0.5, 1, 0.01, 1, false,
				"상인에게서 살 때의 기본 가격에 곱한다(0.5 면 반값). 거래 창을 다시 열면 보인다" },
			{ "sell_price", Area::Economy, "판매가 배율", "inst:o_game_map_controller.__trade_manager.sell_default_get", HS, 2, 1, 1, 50, false,
				"상인에게 팔 때의 기본 가격에 곱한다. 거래 창을 다시 열면 보인다" },
			// 시장 깊이: 값이 떨어지기 전까지 팔 수 있는 양(자원마다 0~130). MarketSaturationManager.get_market_depth(자원).
			{ "market_depth", Area::Economy, "거래량(시장 깊이) 배율", "inst:o_game_map_controller.__trade_manager.__prices_manager.get_market_depth", HS, 10, 1, 1, 100, false,
				"값이 떨어지기 전까지 팔 수 있는 양에 곱한다" },
			// 창고 용량(research/10). 창고 종류(hall, storage, granary, armory)마다 갈래별 용량이 있다. 그 값에 배율을 쓴다(src/Production.cpp).
			// 용량을 돌려주는 함수(get_total_capacity_for_category)만 바꾸면 HUD 만 바뀌고 자정의 부패 처리는 원래 용량으로 깎는다.
			// 플레이에서 봤다(2026-10-05, 0.9.0): 10 으로 켜자 15칸이 10배가 되고 HUD 가 원자재 4500/30000 이 됐다. 그 채로 부패 처리를 불러도
			// 나무 4,500(원래 용량 3,000)이 그대로였고, 끄자 15칸이 원래 값으로 돌아왔다. 세이브에는 남지 않는다(세이브 파일에 그 열쇠가 없다).
			{ "storage_capacity", Area::Economy, "창고 용량 배율", "inst:o_data.__building_warehouse_data.__generic_warehouses", CS, 10, 0, 1, 1000, true,
				"창고 종류마다의 갈래별 용량에 곱한다. 용량을 넘겨 썩던 자원이 썩지 않는다. 끄면 원래 용량으로 되돌린다" },

			// 생산(research/10). 물건 하나를 만드는 데 드는 생산 점수는 resource_production_points_cost_get(자원)이 돌려준다
			// (o_province_controller.production_cost[자원] × 3600). 그 수에 배율을 곱한다.
			// 플레이에서 봤다(2026-10-05, 0.9.0): 생산 시간 0.1·작업 효율 5·생산량 5 를 함께 켠 하루에 광산이 다섯 시간이 안 되는 동안 철 1,176개를 만들었다
			// (바닐라는 시간당 1.4개쯤). 셋 가운데 둘만 먹었다면 많아야 350개다. 따로는 함수 수준에서 봤다: 게임이 받은 비용 1260 => 126, 모은 점수 72초에 107 → 380.
			{ "production_time", Area::Build, "생산 시간 배율", "gml_Script_resource_production_points_cost_get", HS, 0.1, 1, 0.01, 1, true,
				"물건 하나를 만드는 데 드는 일의 양에 곱한다(0.1 이면 열 배 빨리 만든다)" },
			// BuildingComponentProduction.get_worker_base_performance_factor(일꾼) → 1. 생성자의 정적 메서드라 건물이 없어도 이름으로 건다
			// (이름의 번호는 이 게임 버전의 것이다).
			{ "worker_performance", Area::Build, "작업 효율 배율",
				"gml_Script_anon_BuildingComponentProduction_gml_GlobalScript_BuildingComponentProduction_2239513882_BuildingComponentProduction_gml_GlobalScript_BuildingComponentProduction",
				HS, 5, 0, 1, 20, true, "생산 건물에서 일꾼의 기본 작업 효율에 곱한다" },
			// 조리법: 건물 종류의 __production.__map_of_production(ds_map: 만드는 자원 → { 재료의 배열, 만들어지는 수 }). src/Production.cpp 가 돌며 쓴다.
			{ "production_amount", Area::Build, "생산량 배율", "inst:o_building.generic.__production", CS, 2, 0, 1, 100, true,
				"한 번에 만들어지는 수에 곱한다. 끄면 원래 수로 되돌린다" },
			// 켜면 철·목재가 0 인데도 작업장의 is_can_produce_product(칼)가 참이 되고, 끄면 거짓이 되는 것까지 봤다. 재료가 드는 물건이 실제로 만들어지는 것은 보지 못했다.
			{ "production_free", Area::Build, "생산 재료 없음", "inst:o_building.generic.__production", C, 1, 0, 0, 0, false,
				"모든 조리법의 재료를 0 으로 쓴다. 끄면 원래 재료로 되돌린다" },

			// 건설 조건(research/09). 건설 창이 건물마다 이 함수에 (건물 이름, 등급)을 묻는다: 제단은 true, 잠긴 창고·사원은 false 였다.
			// true 로 바꾼 채 사용자가 잠겨 있던 창고·곡창·무기고를 지었다. 건설 창의 빨간 표시와 안내문은 남는다(아래 항목).
			{ "build_any", Area::Build, "건설 조건 없이 짓기 (지식)", "inst:o_game_map_controller.__knowledge_manager.is_have_knowledge_to_upgrade_building",
				H, 1, 0, 0, 0, true, "지식이 없어 잠긴 건물도 지을 수 있다. 건설 창의 빨간 표시는 그대로 남는다" },
			// 건설 창이 잠긴 건물에 커서를 올릴 때 필요한 지식마다 이 함수를 부른다(false 가 나왔다). 지식 창도 이 함수를 쓴다.
			// true 로 바꿨을 때 건설 창의 표시가 풀리는지는 화면으로 보지 못했다.
			{ "build_marks", Area::Build, "건설 창의 잠금 표시 지우기", "inst:o_game_map_controller.__knowledge_manager.is_knowledge_unlocked",
				H, 1, 0, 0, 0, false, "모든 지식을 해금된 것으로 답하게 한다. 지식 창에도 그렇게 보인다. 건설 창의 표시가 풀리는지는 확인 전" },
			// 건설비와 업그레이드비(research/09). 건물 종류마다 등급별 비용(금화, 자원 39칸)이 있다. 모듈(src/Build.cpp)이 모두 0 으로 쓰고 끌 때 되돌린다.
			// 업그레이드 단추가 "자원이 부족"으로 꺼지는 것도 이 비용이다(주택 2등급: 나무 10, 목재 5).
			// 사용자가 플레이에서 봤다(2026-10-05): 목재가 0 인데 주택 11채의 업그레이드가 눌렸고, 돼지 농장(짓기 30 + 올리기 30 나무)을 짓고 올려도 나무가 줄지 않았다.
			// 0 으로 쓴 비용은 세이브에 남지 않는다: 켠 채 저장한 세이브를 치트 없이 불러오니 비용이 원래 값이었다.
			{ "build_free", Area::Build, "건설·업그레이드 비용 없음", "inst:o_game_map_controller.__construction_manager",
				C, 1, 0, 0, 0, true, "모든 건물 종류의 등급별 건설비를 0 으로 쓴다. 자원이 모자라도 업그레이드를 누를 수 있다. 끄면 원래 값으로 되돌린다.\n"
					"켠 채로 저장해도 세이브에는 남지 않는다" },
			// 즉시 업그레이드(research/09). 게임의 즉시 건설(o_debug.is_instant_build_buildings)은 새로 짓는 건물만 끝낸다(사용자가 봤다).
			// 업그레이드 중인 건물(c_construction.__construction_status 가 3)에 게임의 build_instantly() 를 부르면 바로 끝난다(src/Build.cpp).
			// 사용자가 플레이에서 봤다(2026-10-05): 이 항목만 켠 채 주택 세 채의 업그레이드를 눌렀고 모두 바로 올랐다. 나무와 도기는 줄지 않았다(시간을 흘린 뒤에도).
			{ "instant_upgrade", Area::Build, "건물 즉시 업그레이드", "inst:o_building.c_construction.build_instantly",
				C, 1, 0, 0, 0, true, "업그레이드를 누른 건물을 1초 안에 끝낸다. 자원은 들지 않는다. 건물 즉시 건설은 새로 짓는 건물만 끝낸다.\n"
					"업그레이드를 누르려면 자원이 있어야 한다. 없으면 비용 없음과 함께 켠다" },

			// 사용자가 플레이에서 봤다(2026-10-05, research/07): 즉시 건설은 된다.
			{ "instant_build", Area::Build, "건물 즉시 건설", "inst:o_debug.is_instant_build_buildings", T, 1, 0, 0, 0, true,
				"건물을 놓으면 바로 다 지어진다" },
			// 사용자가 플레이에서 봤다: 건설 목록은 풀리지만 조건에 걸리는 건물은 여전히 지을 수 없다. 조건까지 푸는 것은 아직 없다.
			{ "build_all", Area::Build, "건설 목록 모두 열기 (조건은 그대로)", "inst:o_debug.is_can_build_all_buildings", T, 1, 0, 0, 0, false,
				"건설 목록의 건물이 모두 보인다. 조건에 걸리는 건물은 여전히 지을 수 없다(어느 조건인지는 재지 않았다)" },
			{ "build_duration", Area::Build, "건설 시간 계수", "inst:o_debug.debug_building_duration_factor", N, 0, 0, 0, 5, false,
				"debug_params.json 의 building_duration_factor 가 옮겨진 값이다(원래 0.5). 작을수록 빨리 지어질 것으로 보인다" },

			// 인구·욕구(research/11). 욕구는 __soul.__motive.__motive[6](0 수면, 1 음식, 2 휴식, 3 신앙심, 4 성관계, 5 돌봄)에 있다.
			// 모듈(src/People.cpp)이 플레이어의 사람(영주와 주민)을 돌며 그 칸을 상한으로 써 둔다.
			// 플레이에서 봤다(2026-10-06, 0.10.0): 켜자 플레이어의 사람 29명의 칸이 100 으로 유지됐고 손님은 그대로였다. "배고픔 없음"은 음식 칸만,
			// "피로 없음"은 수면·휴식 칸만 채웠다. 끄자 그때부터 평소대로 줄었다. 켜 둔 채 식량이 줄지 않는지는 하루를 돌려 보지 못했다.
			{ "no_hunger", Area::People, "배고픔 없음", "inst:o_character.__soul.__motive.__motive", C, 1, 0, 0, 0, true,
				"플레이어의 사람 모두의 음식 욕구를 가득 채워 둔다. 끄면 그때부터 평소대로 줄어든다" },
			{ "no_tiredness", Area::People, "피로 없음 (수면·휴식)", "inst:o_character.__soul.__motive.__motive", C, 1, 0, 0, 0, true,
				"플레이어의 사람 모두의 수면과 휴식 욕구를 가득 채워 둔다" },
			{ "needs_full", Area::People, "모든 욕구 채워 두기", "inst:o_character.__soul.__motive.__motive", C, 1, 0, 0, 0, true,
				"플레이어의 사람 모두의 욕구 여섯(수면, 음식, 휴식, 신앙심, 성관계, 돌봄)을 가득 채워 둔다" },
			// 기분은 게임이 생각(minds)의 합으로 10분쯤마다 다시 셈한다. 바로 쓴 값은 남지 않는다. 게임의 디버그용 생각(mind_debug_totally_happy:
			// 기분 +100, 하루)을 모듈이 플레이어의 사람마다 생각의 합이 100 아래면 붙인다(60초에 한 바퀴).
			// 플레이에서 봤다: 켜고 20분(게임 시간)쯤 뒤 기분이 35 → 98, 70 → 98 이 됐다. 게임의 인물 창에는 그 생각이 "mind.debug_totally_happy"로 보인다(번역이 없다).
			{ "always_happy", Area::People, "언제나 행복", "inst:o_data.mind_debug_totally_happy", C, 1, 0, 0, 0, true,
				"플레이어의 사람 모두에게 게임의 디버그용 생각(기분 +100, 하루)을 붙여 둔다. 끄면 붙어 있던 것은 하루 뒤에 사라진다" },
			// 인물마다 __soul.__aging.__old.__debug_is_can_die_of_old_age(true)가 있다. 켜면 false 로, 끄면 다시 true 로 써지는 것까지 봤다(손님은 그대로).
			// 이름에서 읽은 뜻이고, 늙어도 죽지 않는지는 보지 못했다. 세이브에는 남지 않는다(열쇠 0건).
			{ "no_old_age_death", Area::People, "노화로 죽지 않음", "inst:o_character.__soul.__aging.__old.__debug_is_can_die_of_old_age", C, 1, 0, 0, 0, false,
				"플레이어의 사람 모두의 '노화로 죽을 수 있다'를 끈다. 끄면 다시 켠다" },
			// 이주 관리자의 다음 이주 보너스. 3 을 쓰자 그날 저녁 "3명의 이주자가 도착했습니다"가 뜨고 주민이 31 → 34 가 됐다(2026-10-06, 0.9.1).
			// 게임이 이주 때 이 값을 0 으로 되돌린다. 값을 정해 두면 다시 써 넣으므로 날마다 그만큼 더 온다. 켠 채 저장하면 세이브에 남는다(그 열쇠가 있다).
			{ "daily_migrants", Area::People, "날마다 추가 이주민", "inst:o_game_map_controller.__province.__migration_manager.__next_day_migrants_bonus",
				N, 0, 0, 0, 50, true, "다음 이주 때(하루에 한 번, 저녁) 이 수만큼 더 온다. 값을 정해 두면 날마다 그만큼 더 온다" },

			{ "rest_decrease", Area::People, "휴식 감소(시간당)", "inst:o_debug.debug_rest_decrease_per_hour", N, 0, 0, 0, 20, false,
				"한 시간에 휴식이 줄어드는 양으로 보인다(원래 3). 0 이면 피로가 쌓이지 않을 것으로 보인다" },
			{ "no_occupational_disease", Area::People, "직업병 끔", "inst:o_debug.debug_is_occupational_disease_enabled", T, 0, 1, 0, 0, false,
				"직업병이 생기는지를 정하는 값으로 보인다(원래 켜져 있다)" },

			// 연구 시간(research/12). 도서관 관리자의 get_learn_time(지식, 영주, 불리언)이 11.9 를 돌려주는 것을 봤다. 그 수에 배율을 곱한다.
			// 배율을 건 뒤로는 그 함수가 불리지 않아 효과를 보지 못했다(연구를 새로 시작할 때 불리는 것으로 보인다).
			{ "research_time", Area::Knowledge, "연구 시간 배율", "inst:o_game_map_controller.__library_manager.get_learn_time", HS, 0.1, 0, 0.01, 1, false,
				"영주가 지식을 익히는 데 드는 시간에 곱한다(0.1 이면 열 배 빨리). 새로 시작하는 연구부터 먹을 것으로 보인다" },

			// 병사의 고용 값(research/13). 고용 창의 값은 SoulBasic.get_soldier_cost()(인자 없음)가 돌려주는 수다(100~160, 풀려난 수감자 42~52).
			// 생성자의 정적 메서드라 사람이 없어도 이름으로 건다(이름의 번호는 이 게임 버전의 것이다).
			// 플레이에서 봤다(2026-10-06, 0.11.1): 0.1 을 걸자 고용 창의 값이 16·14·12·10 이 됐고, 160짜리를 고용하자 금화가 1,901 → 1,885 가 됐다.
			{ "hire_cost", Area::Army, "병사 고용 값 배율",
				"gml_Script_anon_SoulBasic_gml_GlobalScript_SoulBasic_11987516072_SoulBasic_gml_GlobalScript_SoulBasic",
				HS, 0.1, 1, 0.01, 1, true, "병사를 고용할 때 내는 금화에 곱한다(0.1 이면 10분의 1). 고용 창을 다시 열면 보인다" },

			// 아군 무적(research/13). 상처는 SoulBasic.take_damage("상처의 이름", 구조체, 불리언) -> true 가 입힌다. 그 함수를 모두에게 건너뛰게 하자
			// 도적 무리와 6,371번 맞는 동안(치명상 포함) 새 상처가 하나도 생기지 않았다. 이 항목은 self 가 플레이어의 영혼일 때만 건너뛴다
			// (src/People.cpp 가 영혼의 주소를 모아 건다). 켠 채 그 함수를 같은 인자로 직접 부르자 플레이어의 병사에게는 건너뛰어졌고(멍 없음)
			// 플레이어의 사람이 아닌 상인에게는 멍이 생겼다(0.13.0). 게임이 영혼의 메서드를 부를 때 self 가 그 영혼이라는 것도 봤다
			// (0.16.0: 통증 한도 함수의 표본 [self in] 2,091번. research/16). 게임이 스스로 건 싸움에서 플레이어의 사람이 맞는 장면은 스무 시간 동안 없었다.
			{ "ally_invincible", Area::Army, "아군 무적 (상처를 입지 않음)",
				"inst:o_character.__soul.take_damage",
				C, 1, 0, 0, 0, true, "플레이어의 사람(영주, 주민, 병사)에게 상처를 입히는 호출을 건너뛴다. 플레이어의 사람끼리 싸울 때도 상처가 없다. "
				"옆의 '막은 상처'가 실제로 막은 수다" },

			// 전투의 배율(research/13, 16). 영혼의 두 함수가 돌려주는 수에 곱한다: 싸울 때의 전투 기술 get_combat_level_in_battle() -> 10, 7 과
			// 치명적인 통증의 한도 get_mortal_pain_threshold() -> 40. self 가 플레이어의 영혼인지로 아군과 적을 가린다(src/People.cpp 의 BattleTick 이 한 함수에 둘을 함께 건다).
			// 전투 기술은 올린 값이 20 에서 멈춘다(기술은 0~20 이다). 게임이 받는 수가 바뀌는 것까지 봤다(0.16.0. research/16):
			// 통증 한도는 아군 40 => 120, 그 밖 40 => 12. 전투 기술은 그 밖의 5 => 3, 3 => 2 만(플레이어의 사람은 싸우지 않았다). 싸움의 결과가 달라지는 것은 보지 못했다.
			{ "ally_power", Area::Army, "아군 전투력 배율", "inst:o_character.__soul.get_combat_level_in_battle", CS, 2, 0, 1, 5, false,
				"플레이어의 사람이 싸울 때의 전투 기술에 곱한다(올린 값은 20 에서 멈춘다)" },
			{ "enemy_power", Area::Army, "적 전투력 배율", "inst:o_character.__soul.get_combat_level_in_battle", CS, 0.5, 0, 0.1, 1, false,
				"플레이어의 사람이 아닌 모두(손님, 상인, 다른 세력, 짐승)가 싸울 때의 전투 기술에 곱한다. 1 아래로는 내려가지 않는다" },
			{ "ally_toughness", Area::Army, "아군 맷집 배율", "inst:o_character.__soul.get_mortal_pain_threshold", CS, 3, 0, 1, 10, false,
				"플레이어의 사람이 버티는 통증의 한도(치명적인 통증)에 곱한다. 더 잘 버틸 것으로 보인다" },
			{ "enemy_toughness", Area::Army, "적 맷집 배율", "inst:o_character.__soul.get_mortal_pain_threshold", CS, 0.3, 0, 0.1, 1, false,
				"플레이어의 사람이 아닌 모두(손님, 상인, 다른 세력, 짐승)가 버티는 통증의 한도에 곱한다. 더 빨리 쓰러질 것으로 보인다" },

			{ "combat_no_injuries", Area::Army, "부상 없는 전투", "inst:o_debug.is_combat_without_injuries", T, 1, 0, 0, 0, false,
				"전투에서 부상이 생기지 않게 하는 개발자 스위치로 보인다" },
			{ "no_dodge", Area::Army, "회피 끔", "inst:o_debug.is_disable_dodge", T, 1, 0, 0, 0, false,
				"전투에서 회피를 끄는 개발자 스위치로 보인다(양쪽 모두일 수 있다)" },
			{ "no_firefight", Area::Army, "사격전 끔", "inst:o_debug.is_disable_firefight", T, 1, 0, 0, 0, false,
				"사격전을 끄는 개발자 스위치로 보인다" },
			{ "no_equipment_destroy", Area::Army, "장비 파손 끔", "inst:o_debug.is_disable_equipment_destroy", T, 1, 0, 0, 0, false,
				"장비가 부서지지 않게 하는 개발자 스위치로 보인다" },
			{ "no_surrender", Area::Army, "항복 끔", "inst:o_debug.is_surrender_disable", T, 1, 0, 0, 0, false,
				"항복이 일어나지 않게 하는 개발자 스위치로 보인다" },
			{ "no_ambush_attack", Area::Army, "적 매복이 인물을 공격하지 않음", "inst:o_debug.is_enemy_ambush_attack_actors", T, 0, 1, 0, 0, false,
				"적의 매복이 인물을 공격하는지를 정하는 값으로 보인다(원래 켜져 있다)" },
			{ "dodge_base", Area::Army, "회피 기본값", "inst:o_debug.battle_dodge_base", N, 0, 0, 0, 100, false,
				"battle_params.json 의 battle_dodge_base 가 옮겨진 값이다(원래 20)" },
			{ "hire_price_factor", Area::Army, "병사 고용가 계수", "inst:o_debug.soldier_hiring_price_skill_factor", N, 0, 0, 0, 20, false,
				"battle_params.json 의 soldier_hiring_price_skill_factor 가 옮겨진 값이다(원래 5). 전투 기술에 따른 고용가로 보인다" },

			// 세력의 적대 판정(research/14): Faction.is_enemy_with(세력) -> 불리언. 게임이 구조체 하나로 네 시간에 270번 불렀고 true 와 false 를 모두 봤다.
			// 생성자의 정적 메서드라 플레이어의 세력에서 스크립트를 찾는다. 건 훅은 모든 세력의 호출에 걸린다. 효과(전쟁, 습격, 지도의 표시)는 보지 못했다.
			{ "no_enemies", Area::Diplomacy, "세력끼리 적대하지 않음 (모든 세력)", "inst:o_game_map_controller.__factions_manager.__player_faction.is_enemy_with",
				H, 0, 1, 0, 0, false, "어느 세력도 다른 세력을 적으로 보지 않는다고 답하게 한다(플레이어만이 아니라 모든 세력끼리). 효과는 확인 전" },
			{ "no_rebellions", Area::Diplomacy, "반란이 일어나지 않음", "inst:o_debug.is_rebellions_can_started", T, 0, 1, 0, 0, false,
				"반란이 시작될 수 있는지를 정하는 값으로 보인다(원래 켜져 있다)" },

			// 신앙심(욕구 3번)의 감소: 0 으로 쓰고 1.4시간을 흘리자 기도하지 않는 14명의 신앙심이 그대로였다
			// (그 앞의 1.8시간에는 시간당 0.81 이나 0.40 씩 줄었다. research/21). 잰 것은 0 뿐이다: 다른 수가 어떻게 먹는지는 보지 않았다.
			{ "piety_decrease", Area::Religion, "신앙 감소(시간당)", "inst:o_debug.debug_piety_decrease_per_hour", N, 0, 0, 0, 10, true,
				"0 으로 두면 신앙심이 줄지 않는다(그렇게 되는 것을 봤다). 원래 값은 0.83 이고, 이름으로 보아 한 시간에 줄어드는 양이다" },
			// 신앙심 채워 두기: 욕구를 채워 두는 항목들과 같은 길(src/People.cpp 의 바퀴)로 욕구 3번만 채운다.
			{ "piety_full", Area::Religion, "신앙심 채워 두기", "inst:o_character.__soul.__motive.__motive", C, 1, 0, 0, 0, false,
				"플레이어의 사람 모두의 신앙심을 가득 채워 둔다. 끄면 그때부터 평소대로 줄어든다" },
			// 성스러운 보호: GameOnboardingManager.is_under_holy_defence()(인자 없음)가 __is_under_holy_defence(1)를 돌려준다. 게임이 여섯 시간에 한 번 불렀다.
			// 훅으로 1 을 돌려주게 하면 자료를 0 으로 써도 1 이 나온다(research/21). 보호가 풀릴 인구(65)를 넘겨도 공격받지 않는지는 보지 못했다.
			{ "holy_defence", Area::Religion, "성스러운 보호 유지", "inst:o_game_map_controller.__onboard_manager.is_under_holy_defence", HN, 1, 0, 0, 0, false,
				"게임이 '성스러운 보호 아래인가'를 물을 때 언제나 그렇다고 답하게 한다. 효과는 확인 전" },
			// is_allow_to_religious_riot()(인자 없음) -> false 를 한 번 봤다(광신도가 없는 세이브). 참일 때를 보지 못했다.
			{ "no_religious_riot", Area::Religion, "종교 반란이 일어나지 않음", "inst:o_game_map_controller.__onboard_manager.is_allow_to_religious_riot", H, 0, 1, 0, 0, false,
				"게임이 '종교 반란을 일으켜도 되는가'를 물을 때 언제나 아니라고 답하게 한다. 효과는 확인 전" },
			// 종교의 비용으로 보이는 수들: 게임 변수 여섯(religiosity_confession_cost 3, _divorce_cost 5, _begging_cost 250, _canonization_cost_gold 300,
			// _canonization_cost_per_province 200, _sacrificer_cost_gold 50)과 설교 열 가지의 __cost(0 이 아닌 것은 일곱: 30 ~ 150).
			// 써지고 되돌려지는 것을 봤다. 세이브에는 그 열쇠가 없다. 뜻(무엇의 값인지, 반지인지 금화인지)은 이름에서 읽은 것이고 게임의 창이 따르는지는 보지 못했다.
			{ "religion_free", Area::Religion, "종교 행동·설교의 비용 없음", "global.__gameplay_vars.religiosity_confession_cost", C, 1, 0, 0, 0, false,
				"이름으로 보아 고해, 이혼, 구걸, 시성, 제물 설교의 비용인 게임 변수 여섯과 설교마다의 비용(0 이 아닌 것)을 0 으로 쓴다. 끄면 원래 값으로 되돌린다. 효과는 확인 전" },
			// 게임 변수 넷(church_pray_piety_restore 15, altar_pray_piety_restore 15, church_pray_morning_service_restore 30, trait_saint_piety_talk_restore 20)에 배율을 쓴다.
			{ "piety_restore", Area::Religion, "기도·예배의 신앙 회복 배율", "global.__gameplay_vars.church_pray_piety_restore", CS, 3, 0, 1, 10, false,
				"이름으로 보아 기도, 아침 예배, 성인과의 대화가 되돌리는 신앙심인 게임 변수 넷에 곱한다. 끄면 원래 값으로 되돌린다. 효과는 확인 전" },
			// 게임 변수 church_preach_conversion_factor(1)에 배율을 쓴다. 값 써 넣기(Number)로 두지 않는다: global 뿌리의 Number 는 메인 메뉴에서도 써지고
			// 창이 보이는 동안 전역을 훑는다. 모듈의 일(src/Production.cpp)은 게임 화면에서만 한다.
			{ "preach_conversion", Area::Religion, "설교 전환 배율", "global.__gameplay_vars.church_preach_conversion_factor", CS, 3, 0, 1, 20, false,
				"이름으로 보아 설교가 사람을 바꾸는 정도에 곱하는 게임 변수(원래 1)에 곱한다. 끄면 원래 값으로 되돌린다. 효과는 확인 전" },
			{ "donation_runes", Area::Religion, "헌금 룬", "inst:o_debug.church_donation_runes", N, 0, 0, 0, 100, false,
				"교회 헌금으로 내는 룬의 수로 보인다(원래 1)" },
			{ "donation_runes_fanatic", Area::Religion, "헌금 룬(광신도)", "inst:o_debug.church_donation_runes_fanatic", N, 0, 0, 0, 100, false,
				"광신도가 헌금으로 내는 룬의 수로 보인다(원래 2)" },

			{ "fast_global_tasks", Area::World, "전역 지도의 행동을 빠르게", "inst:o_debug.is_fast_action_task_on_global_map", T, 1, 0, 0, 0, false,
				"전역 지도에서 하는 행동을 빨리 끝내는 개발자 스위치로 보인다" },
			{ "no_tree_growth", Area::World, "나무가 자라지 않음", "inst:o_debug.is_disable_trees_grow", T, 1, 0, 0, 0, false,
				"나무의 성장을 끄는 개발자 스위치로 보인다" },

			{ "hide_events", Area::Events, "이벤트 표시 끔", "inst:o_debug.is_display_event_disabled", T, 1, 0, 0, 0, false,
				"이벤트 알림을 띄우지 않는 개발자 스위치로 보인다(이벤트 자체를 막는지는 모른다)" },

			{ "game_debug", Area::Util, "게임의 디버그 모드", "inst:o_debug.is_debug_enabled", T, 1, 0, 0, 0, false,
				"게임에 들어 있는 디버그 기능의 큰 스위치로 보인다. 켜면 게임의 디버그 창이 뜰 수 있다" },
			{ "debug_managers", Area::Util, "게임의 디버그 창: 매니저", "inst:o_debug.is_show_debug_managers", T, 1, 0, 0, 0, false,
				"게임의 'Debug managers' 창을 여는 스위치로 보인다" },
			{ "traits_windows", Area::Util, "게임의 디버그 창: 특성", "inst:o_debug.is_show_traits_windows", T, 1, 0, 0, 0, false,
				"게임의 특성 디버그 창을 여는 스위치로 보인다" },
			{ "production_window", Area::Util, "게임의 디버그 창: 생산", "inst:o_debug.is_visible_production_window", T, 1, 0, 0, 0, false,
				"게임의 생산 디버그 창을 여는 스위치로 보인다" },
			{ "debug_log", Area::Util, "게임의 디버그 로그", "inst:o_debug.debug_log_is_enabled", T, 1, 0, 0, 0, false,
				"게임의 'Debug Log' 창을 여는 스위치로 보인다" },
			{ "show_grid", Area::Util, "격자 보기", "inst:o_debug.is_show_grid", T, 1, 0, 0, 0, false,
				"지도의 격자를 그리는 개발자 스위치로 보인다" },
			{ "hide_gui", Area::Util, "게임 UI 숨기기", "inst:o_debug.is_gw_gui_draw_disabled", T, 1, 0, 0, 0, false,
				"게임의 UI 를 그리지 않는 개발자 스위치로 보인다(스크린샷용)" },
			{ "hide_popups", Area::Util, "알림 팝업 숨기기", "inst:o_debug.is_hide_popup_messages", T, 1, 0, 0, 0, false,
				"인물 위의 알림 글을 숨기는 개발자 스위치로 보인다" },
			{ "hide_bubbles", Area::Util, "말풍선 숨기기", "inst:o_debug.is_hide_speech_bubbles", T, 1, 0, 0, 0, false,
				"말풍선을 숨기는 개발자 스위치로 보인다" },
			{ "hide_names", Area::Util, "인물 이름 숨기기", "inst:o_debug.is_hide_character_names", T, 1, 0, 0, 0, false,
				"인물의 이름표를 숨기는 개발자 스위치로 보인다" },
		};
		return cheats;
	}

	const Cheat* FindCheat(const std::string& Id)
	{
		for (const Cheat& cheat : Cheats())
			if (Id == cheat.Id)
				return &cheat;
		return nullptr;
	}

	CheatState KeepKnown(CheatState State)
	{
		std::erase_if(State.On, [](const std::string& id) {
			const Cheat* cheat = FindCheat(id);
			if (!cheat || HasNumber(cheat->Kind))		// 수가 있는 항목은 Numbers 에 든다. 그 밖(Toggle, Hook, Custom)은 켠 것의 목록에 든다
				return true;
			// 함수의 답을 바꾸거나 모듈이 게임의 값을 고쳐 쓰는 항목은 효과를 확인한 것만 켠 채로 시작한다. 확인 전의 것은 그 실행에서 사용자가 켠다:
			// 창을 열지도 않았는데 게임의 판정이 바뀌거나 값이 고쳐 쓰여 세이브에 굳는 일이 없게.
			return (IsHook(cheat->Kind) || cheat->Kind == CheatKind::Custom) && !cheat->Verified;
		});
		for (auto it = State.Numbers.begin(); it != State.Numbers.end();)
		{
			const Cheat* cheat = FindCheat(it->first);
			// 배율을 거는 훅·모듈 항목도 확인한 것만 켠 채로 시작한다(위와 같은 까닭).
			if (!cheat || !HasNumber(cheat->Kind) || (cheat->Kind != CheatKind::Number && !cheat->Verified))
			{
				it = State.Numbers.erase(it);
				continue;
			}
			it->second = std::clamp(it->second, cheat->Min, cheat->Max);
			++it;
		}
		return State;
	}
}
