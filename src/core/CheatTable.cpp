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
			{ Area::Person, "person", "인물", 4 },
			{ Area::Lord, "lord", "영주", 4 },
			{ Area::People, "people", "인구·욕구", 4 },
			{ Area::Knowledge, "knowledge", "지식", 5 },
			{ Area::Items, "items", "아이템", 5 },
			{ Area::Army, "army", "군대·전투", 5 },
			{ Area::Diplomacy, "diplomacy", "외교", 6 },
			{ Area::Religion, "religion", "종교", 6 },
			{ Area::Time, "time", "시간", 2, true },
			{ Area::World, "world", "월드", 6 },
			{ Area::Events, "events", "이벤트", 6 },
			{ Area::Util, "util", "유틸", 7 },
			{ Area::Presets, "presets", "프리셋", 7 },
			{ Area::Tweaks, "tweaks", "배율", 2, true },
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

	const std::vector<Cheat>& Cheats()
	{
		constexpr CheatKind T = CheatKind::Toggle, N = CheatKind::Number, H = CheatKind::Hook, C = CheatKind::Custom;

		// 이름과 값은 새 게임 덤프(refs/runtime/stage0b-run2.late1.json 의 instances.o_debug.members)에서 봤다.
		// Off 는 그 덤프의 값이다. 뜻은 변수 이름에서 읽은 것이고 효과는 아직 재지 않았다(Verified = false).
		// 차례: Id, 영역, 이름, 주소, 종류, On, Off, Min, Max, Verified, 설명
		static const std::vector<Cheat> cheats = {
			// o_debug.is_resources_edit_mode 는 넣지 않는다: 켜도 자원의 목록이 나오지 않아 쓸 수 없었다(사용자가 플레이에서 봤다. research/07).
			// 금화와 자원은 경제 패널(src/Economy.cpp)이 게임의 함수로 바꾼다.

			// 창고 용량(…__warehouse.__cached_total_capacity_for_storage_type.<갈래>)은 아직 넣지 않는다: 게임이 다시 채우는 캐시라
			// "원래대로"가 낡은 값을 써 넣게 된다. 게임이 그 값을 언제 다시 만드는지 잰 뒤(3나-2)에 넣는다.

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
			// 사용자가 플레이에서 봤다(2026-10-05): 목재가 0 인데 주택 12채의 업그레이드가 눌렸고, 돼지 농장(짓기 30 + 올리기 30 나무)을 짓고 올려도 나무가 줄지 않았다.
			// 켠 것을 다음 실행까지 기억하지 않는다(맨 끝의 false): 0 으로 쓴 비용이 세이브에 들어가는지 재지 않았다.
			{ "build_free", Area::Build, "건설·업그레이드 비용 없음", "inst:o_game_map_controller.__construction_manager",
				C, 1, 0, 0, 0, true, "모든 건물 종류의 등급별 건설비를 0 으로 쓴다. 자원이 모자라도 업그레이드를 누를 수 있다. 끄면 원래 값으로 되돌린다.\n"
					"켠 채로 저장하지 말 것: 0 이 세이브에 남을 수 있다(남는지는 재지 않았다). 저장하기 전에 끈다.\n"
					"게임을 켤 때마다 꺼진 채로 시작한다", false },
			// 즉시 업그레이드(research/09). 게임의 즉시 건설(o_debug.is_instant_build_buildings)은 새로 짓는 건물만 끝낸다(사용자가 봤다).
			// 업그레이드 중인 건물(c_construction.__construction_status 가 3)에 게임의 build_instantly() 를 부르면 바로 끝난다:
			// 원격으로 불러 주택·돼지 농장·병영의 등급이 오르는 것을 봤다. 모듈이 스스로 부르는 길(src/Build.cpp)은 아직 보지 못했다.
			{ "instant_upgrade", Area::Build, "건물 즉시 업그레이드", "inst:o_building.c_construction.build_instantly",
				C, 1, 0, 0, 0, false, "업그레이드를 누른 건물을 1초 안에 끝낸다(게임의 build_instantly). 건물 즉시 건설은 새로 짓는 건물만 끝낸다" },

			// 사용자가 플레이에서 봤다(2026-10-05, research/07): 즉시 건설은 된다.
			{ "instant_build", Area::Build, "건물 즉시 건설", "inst:o_debug.is_instant_build_buildings", T, 1, 0, 0, 0, true,
				"건물을 놓으면 바로 다 지어진다" },
			// 사용자가 플레이에서 봤다: 건설 목록은 풀리지만 조건에 걸리는 건물은 여전히 지을 수 없다. 조건까지 푸는 것은 아직 없다.
			{ "build_all", Area::Build, "건설 목록 모두 열기 (조건은 그대로)", "inst:o_debug.is_can_build_all_buildings", T, 1, 0, 0, 0, false,
				"건설 목록의 건물이 모두 보인다. 조건에 걸리는 건물은 여전히 지을 수 없다(어느 조건인지는 재지 않았다)" },
			{ "build_duration", Area::Build, "건설 시간 계수", "inst:o_debug.debug_building_duration_factor", N, 0, 0, 0, 5, false,
				"debug_params.json 의 building_duration_factor 가 옮겨진 값이다(원래 0.5). 작을수록 빨리 지어질 것으로 보인다" },

			{ "rest_decrease", Area::People, "휴식 감소(시간당)", "inst:o_debug.debug_rest_decrease_per_hour", N, 0, 0, 0, 20, false,
				"한 시간에 휴식이 줄어드는 양으로 보인다(원래 3). 0 이면 피로가 쌓이지 않을 것으로 보인다" },
			{ "no_occupational_disease", Area::People, "직업병 끔", "inst:o_debug.debug_is_occupational_disease_enabled", T, 0, 1, 0, 0, false,
				"직업병이 생기는지를 정하는 값으로 보인다(원래 켜져 있다)" },

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

			{ "no_rebellions", Area::Diplomacy, "반란이 일어나지 않음", "inst:o_debug.is_rebellions_can_started", T, 0, 1, 0, 0, false,
				"반란이 시작될 수 있는지를 정하는 값으로 보인다(원래 켜져 있다)" },

			{ "piety_decrease", Area::Religion, "신앙 감소(시간당)", "inst:o_debug.debug_piety_decrease_per_hour", N, 0, 0, 0, 10, false,
				"한 시간에 신앙이 줄어드는 양으로 보인다(원래 0.83)" },
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
			if (!cheat || cheat->Kind == CheatKind::Number)		// 수 항목은 Numbers 에 든다. 그 밖(Toggle, Hook, Custom)은 켠 것의 목록에 든다
				return true;
			// 함수의 답을 바꾸거나 모듈이 게임의 값을 고쳐 쓰는 항목은 효과를 확인한 것만 켠 채로 시작한다. 확인 전의 것은 그 실행에서 사용자가 켠다:
			// 창을 열지도 않았는데 게임의 판정이 바뀌거나 값이 고쳐 쓰여 세이브에 굳는 일이 없게.
			if (!cheat->Remember)
				return true;
			return (cheat->Kind == CheatKind::Hook || cheat->Kind == CheatKind::Custom) && !cheat->Verified;
		});
		for (auto it = State.Numbers.begin(); it != State.Numbers.end();)
		{
			const Cheat* cheat = FindCheat(it->first);
			if (!cheat || cheat->Kind != CheatKind::Number)
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
