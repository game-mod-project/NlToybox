#include "CheatTable.hpp"

#include <algorithm>

namespace NlCore
{
	const std::vector<AreaInfo>& Areas()
	{
		static const std::vector<AreaInfo> areas = {
			{ Area::Explorer, "explorer", "탐색기", 2 },
			{ Area::Economy, "economy", "경제", 3 },
			{ Area::Build, "build", "건설·생산", 3 },
			{ Area::Person, "person", "인물", 4 },
			{ Area::Lord, "lord", "영주", 4 },
			{ Area::People, "people", "인구·욕구", 4 },
			{ Area::Knowledge, "knowledge", "지식", 5 },
			{ Area::Items, "items", "아이템", 5 },
			{ Area::Army, "army", "군대·전투", 5 },
			{ Area::Diplomacy, "diplomacy", "외교", 6 },
			{ Area::Religion, "religion", "종교", 6 },
			{ Area::Time, "time", "시간", 2 },
			{ Area::World, "world", "월드", 6 },
			{ Area::Events, "events", "이벤트", 6 },
			{ Area::Util, "util", "유틸", 7 },
			{ Area::Presets, "presets", "프리셋", 7 },
			{ Area::Tweaks, "tweaks", "배율", 2 },
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
		constexpr CheatKind T = CheatKind::Toggle, N = CheatKind::Number;

		// 이름과 값은 새 게임 덤프(refs/runtime/stage0b-run2.late1.json 의 instances.o_debug.members)에서 봤다.
		// Off 는 그 덤프의 값이다. 뜻은 변수 이름에서 읽은 것이고 효과는 아직 재지 않았다(Verified = false).
		// 차례: Id, 영역, 이름, 주소, 종류, On, Off, Min, Max, Verified, 설명
		static const std::vector<Cheat> cheats = {
			// o_debug.is_resources_edit_mode 는 넣지 않는다: 켜도 자원의 목록이 나오지 않아 쓸 수 없었다(사용자가 플레이에서 봤다. research/07).
			// 금화와 자원은 경제 패널(src/Economy.cpp)이 게임의 함수로 바꾼다.

			// 창고의 갈래별 용량. 자리는 research/07 에서 읽기만 했다(새 게임의 값은 설명에). 이 구조체는 게임을 시작한 직후에는 비어 있었고
			// 뒤에 여섯이 찼다. 게임이 다시 채우면 0.5초마다 다시 써 넣는다. 게임이 이 값을 따르는지는 모른다.
			{ "cap_food", Area::Economy, "용량: 음식", "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type.food",
				N, 0, 0, 0, 1000000, false, "음식 갈래의 창고 용량으로 보인다(새 게임 250)" },
			{ "cap_liquid", Area::Economy, "용량: 액체", "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type.liquid",
				N, 0, 0, 0, 1000000, false, "액체 갈래의 창고 용량으로 보인다(새 게임 100)" },
			{ "cap_resources", Area::Economy, "용량: 물자", "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type.resources",
				N, 0, 0, 0, 1000000, false, "물자 갈래의 창고 용량으로 보인다(새 게임 50)" },
			{ "cap_armory", Area::Economy, "용량: 전쟁 물자", "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type.armory",
				N, 0, 0, 0, 1000000, false, "전쟁 물자 갈래의 창고 용량으로 보인다(새 게임 20)" },
			{ "cap_herbs", Area::Economy, "용량: 약초·작물", "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type.herbs",
				N, 0, 0, 0, 1000000, false, "약초·작물 갈래의 창고 용량으로 보인다(새 게임 300)" },
			{ "cap_raw", Area::Economy, "용량: 원자재", "inst:o_game_map_controller.__province.__warehouse.__cached_total_capacity_for_storage_type.raw",
				N, 0, 0, 0, 1000000, false, "원자재 갈래의 창고 용량으로 보인다(새 게임 300)" },

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
			return !cheat || cheat->Kind != CheatKind::Toggle;
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
