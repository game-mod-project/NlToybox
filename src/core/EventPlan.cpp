#include "EventPlan.hpp"

#include "Localization.hpp"
#include "Text.hpp"

#include <algorithm>
#include <set>

namespace NlCore
{
	namespace
	{
		using F = EventFamily;
		// research/29 의 표. 차례는 묶음별(파일의 차례): GLOBAL_MAP, RAID, EPIDEMY, CAMP, DISTORTION, ECONOMICAL, GUEST, POLITICAL, UPRISING, REWARD.
		const std::vector<EventRow> k_Table = {
			{ "vassal_player_demand", "GLOBAL_MAP", 0, "", "봉신의 요구", F::None },
			{ "demand_forcing_neutrality", "GLOBAL_MAP", 0, "", "중립 강요", F::None },
			{ "town_king_tribute_light", "GLOBAL_MAP", 0, "", "왕의 조공 요구", F::None },
			{ "faceless_attack_player_town", "GLOBAL_MAP", 0, "", "얼굴 없는 자의 도시 공격", F::None },
			{ "attack_player_village", "GLOBAL_MAP", 0, "", "마을 공격", F::None },
			{ "player_vassal_rebellion", "GLOBAL_MAP", 0, "", "봉신의 반란", F::None },
			{ "forest_bandits_attack_player_village", "RAID", 0, "", "숲 도적의 마을 공격", F::Raid },
			{ "forest_bandits_attack_player_town", "RAID", 0, "", "숲 도적의 도시 공격", F::Raid },
			{ "mountain_bandits_attack_player_town", "RAID", 0, "", "산 도적의 도시 공격", F::Raid },
			{ "prophecy_wolf_attack", "RAID", 0, "prophecy.wolf_attack", "", F::Prophecy },
			{ "raid_bums_in_rags", "RAID", 0, "raid.bums_in_rags", "", F::Raid },
			{ "raid_yellow_fanatics", "RAID", 0, "raid.yellow_fanatics", "", F::Raid },
			{ "raid_flagellants", "RAID", 0, "raid.flagellants", "", F::Raid },
			{ "raid_faceless", "RAID", 0, "raid.faceless", "", F::Raid },
			{ "raid_dogheads", "RAID", 0, "raid.dogheads", "", F::Raid },
			{ "raid_deserters", "RAID", 0, "raid.deserters", "", F::Raid },
			{ "raid_bandits", "RAID", 0, "raid.bandits", "", F::Raid },
			{ "prophecy_cholera_epidemy", "EPIDEMY", 0, "prophecy.cholera_epidemy", "", F::Prophecy },
			{ "prophecy_bandits_camp", "CAMP", 1, "prophecy.bandits_capm", "", F::Prophecy },		// 게임의 오타 그대로(research/29)
			{ "prophecy_xenophoby", "DISTORTION", 1, "prophecy.xenophoby", "", F::Prophecy },
			{ "migrants_bandits", "DISTORTION", 1, "", "도적 이주민", F::None },
			{ "prophecy_tree_bug", "ECONOMICAL", 1, "prophecy.tree_bug", "", F::Prophecy },
			{ "prophecy_rats_invasion", "ECONOMICAL", 1, "prophecy.rats_invasion", "", F::Prophecy },
			{ "reward_increased_prices", "ECONOMICAL", 2, "", "보상: 물가 상승", F::None },
			{ "reward_farm_grow_boost", "ECONOMICAL", 2, "map.reward_farm_grow_boost", "", F::None },
			{ "reward_raw_collect_boost", "ECONOMICAL", 2, "map.reward_raw_collect_boost", "", F::None },
			{ "u_guest_assassin", "GUEST", 2, "guest.assassin", "", F::Guest },
			{ "u_guest_mountain_knight", "GUEST", 2, "guest.mountain_knight", "", F::Guest },
			{ "u_guest_escaped_lord", "GUEST", 2, "guest.escaped_lord", "", F::Guest },
			{ "u_guest_witch", "GUEST", 2, "guest.witch", "", F::Guest },
			{ "u_guest_bandit_and_slave", "GUEST", 2, "guest.bandit_and_slave", "", F::Guest },
			{ "u_guest_cultists", "GUEST", 2, "guest.cultists", "", F::Guest },
			{ "u_guest_joker", "GUEST", 2, "guest.joker", "", F::Guest },
			{ "u_guest_dog_seller", "GUEST", 2, "guest.dog_seller", "", F::Guest },
			{ "u_guest_bard", "GUEST", 2, "guest.bard", "", F::Guest },
			{ "u_guest_nectar_trader", "GUEST", 2, "guest.nectar_trader", "", F::Guest },
			{ "u_guest_piligrims", "GUEST", 2, "guest.piligrims", "", F::Guest },
			{ "u_guest_incognito_philosoph", "GUEST", 2, "", "익명의 손님 (철학자)", F::Guest },
			{ "u_guest_incognito_bishop", "GUEST", 2, "", "익명의 손님 (주교)", F::Guest },
			{ "u_guest_incognito_bandit", "GUEST", 1, "", "익명의 손님 (도적)", F::Guest },
			{ "u_guest_incognito_maniac", "GUEST", 1, "", "익명의 손님 (살인마)", F::Guest },
			{ "desire_politician_for_power_make_me_heir_low", "POLITICAL", 2, "", "후계자 요구 (약)", F::Unrest },
			{ "desire_politician_for_power_make_me_heir_middle", "POLITICAL", 1, "", "후계자 요구 (중)", F::Unrest },
			{ "desire_politician_for_power_make_me_heir_high", "POLITICAL", 0, "", "후계자 요구 (강)", F::Unrest },
			{ "politician_for_power_make_me_king_low", "POLITICAL", 2, "", "왕위 요구 (약)", F::Unrest },
			{ "politician_for_power_make_me_king_middle", "POLITICAL", 1, "", "왕위 요구 (중)", F::Unrest },
			{ "politician_for_power_make_me_king_high", "POLITICAL", 0, "", "왕위 요구 (강)", F::Unrest },
			{ "politician_bribe", "POLITICAL", 1, "", "정치가의 뇌물", F::Unrest },
			{ "dark_actions", "POLITICAL", 1, "", "어두운 행동", F::None },
			{ "conspiracy", "POLITICAL", 0, "map.conspiracy", "", F::Conspiracy },
			{ "rebellion_slaves", "UPRISING", 0, "", "노예 반란", F::Rebellion },
			{ "rebellion_culture", "UPRISING", 0, "", "문화 반란", F::Rebellion },
			{ "rebellion_religiosity", "UPRISING", 0, "", "종교 반란", F::Rebellion },
			{ "rebellion_loyalty", "UPRISING", 0, "", "충성 반란", F::Rebellion },
			{ "ai_preach", "REWARD", 3, "", "AI 영주의 설교", F::None },
			{ "reward_hostage_lord", "REWARD", 3, "", "보상: 인질 영주", F::None },
			{ "reward_neutralise_lord", "REWARD", 3, "", "보상: 영주 중립화", F::None },
			{ "reward_weak_bandits_camp", "REWARD", 3, "", "보상: 약한 도적 기지", F::None },
			{ "reward_rich_migrants", "REWARD", 3, "map.reward_rich_migrants", "", F::None },
			{ "reward_trade_request", "REWARD", 3, "map.reward_trade_request", "", F::None },
			{ "reward_enemy_squad_on_march", "REWARD", 3, "", "보상: 행군 중인 적 분대", F::None },
		};
	}

	const std::vector<EventRow>& EventTable()
	{
		return k_Table;
	}

	const EventRow* FindEvent(const std::string& Name)
	{
		for (const EventRow& row : k_Table)
			if (Name == row.Name)
				return &row;
		return nullptr;
	}

	std::vector<std::string> EventGroups()
	{
		std::set<std::string> groups;
		for (const EventRow& row : k_Table)
			groups.insert(row.Group);
		return std::vector<std::string>(groups.begin(), groups.end());
	}

	const char* EventTypeWord(int Type)
	{
		switch (Type)
		{
		case 0: return "큰 위협";
		case 1: return "작은 위협";
		case 2: return "보통";
		case 3: return "좋음";
		default: return "?";
		}
	}

	const char* EventFamilyWord(EventFamily Family)
	{
		switch (Family)
		{
		case EventFamily::Raid: return "습격";
		case EventFamily::Prophecy: return "예언";
		case EventFamily::Conspiracy: return "음모";
		case EventFamily::Guest: return "손님";
		case EventFamily::Unrest: return "소요";
		case EventFamily::Rebellion: return "반란";
		default: return "";
		}
	}

	const char* EventFamilyKey(EventFamily Family)
	{
		switch (Family)
		{
		case EventFamily::Raid: return "raid";
		case EventFamily::Prophecy: return "prophecy";
		case EventFamily::Conspiracy: return "conspiracy";
		case EventFamily::Guest: return "guest";
		case EventFamily::Unrest: return "unrest";
		case EventFamily::Rebellion: return "rebellion";
		default: return "";
		}
	}

	std::string EventLabel(const EventRow& Row, const std::unordered_map<std::string, std::string>& Captions)
	{
		if (Row.CaptionKey[0])
		{
			const auto found = Captions.find(Row.CaptionKey);
			if (found != Captions.end() && !found->second.empty())
				return found->second;
		}
		if (Row.Korean[0])
			return Row.Korean;
		return Row.Name;
	}

	bool EventMatches(std::string_view Filter, const std::string& Name, const std::string& Label)
	{
		return TraitMatches(Filter, Name, Label);
	}

	bool EventRowShown(const std::string& Group, std::string_view Filter, const std::string& RowGroup, const std::string& Name, const std::string& Label)
	{
		if (!Group.empty() && Group != RowGroup)
			return false;
		return EventMatches(Filter, Name, Label);
	}

	std::string CooldownText(double DaysLeft)
	{
		return DaysLeft < 0 ? "-" : Shortest(DaysLeft) + "일";
	}

	EventListing MergeEventNames(const std::vector<std::string>& GameNames)
	{
		EventListing out;
		std::set<std::string> game(GameNames.begin(), GameNames.end());
		for (const EventRow& row : k_Table)
		{
			if (game.erase(row.Name))
				out.Known.push_back(row.Name);
			else
				out.MissingFromGame++;
		}
		out.Extra.assign(game.begin(), game.end());		// set 이라 이름순
		return out;
	}

	namespace
	{
		// research/29 의 표. 확인 전(Verified false): 인자의 꼴을 모르고 효과도 못 봤다.
		const std::vector<EventEndRow> k_Ends = {
			{ EventFamily::Raid, "inst:o_game_map_controller.__raids_manager.__current_raid", "inst:o_game_map_controller.__raids_manager.try_to_remove_raid", "", false },
			{ EventFamily::Prophecy, "inst:o_game_map_controller.__province.__prophecy_manager.__current_prophecy", "inst:o_game_map_controller.__province.__prophecy_manager.__reset_prophecy", "", false },
			{ EventFamily::Conspiracy, "inst:o_game_map_controller.__province.__conspiracy.__current_conspiracy", "inst:o_game_map_controller.__province.__conspiracy.__reset_conspiracy", "", false },
			{ EventFamily::Guest, "inst:o_game_map_controller.__province.__unique_guests.__current_guest", "inst:o_game_map_controller.__province.__unique_guests.remove_guest_result", "", false },
			{ EventFamily::Unrest, "inst:o_game_map_controller.__province.__politics_manager.__is_unrest_active_struct", "inst:o_game_map_controller.__province.__politics_manager.__all_unrest_finish", "", false },
		};
	}

	const std::vector<EventEndRow>& EventEndTable()
	{
		return k_Ends;
	}

	const EventEndRow* FindEventEnd(EventFamily Family)
	{
		for (const EventEndRow& row : k_Ends)
			if (row.Family == Family)
				return &row;
		return nullptr;
	}

	bool EventEndAllowed(const EventEndRow& Row)
	{
		return Row.Verified && Row.ArgShape[0] == 0;		// 빈 글 = 인자 없음. 다른 꼴은 부르는 코드가 아직 없다
	}

	bool ParseEventFamily(const std::string& Word, EventFamily& Out)
	{
		for (const EventEndRow& row : k_Ends)
			if (Word == EventFamilyKey(row.Family))
			{
				Out = row.Family;
				return true;
			}
		return false;
	}

	CancelStep ChooseCancelStep(bool HasForced)
	{
		return HasForced ? CancelStep::Call : CancelStep::Nothing;
	}

	std::string EventStatusText(EventFamily Family, bool Read, bool Present, const std::string& Name)
	{
		if (Family == EventFamily::Rebellion)
			return "모름 (읽을 자리를 아직 모른다)";
		if (!Read)
			return "읽지 못함";
		if (!Present)
			return "없음";
		return Name.empty() ? "진행 중 (이름을 읽지 못함)" : "진행 중: " + Name;
	}

	std::string ForcedEventText(bool Read, bool Present, const std::string& Name)
	{
		if (!Read)
			return "읽지 못함";
		if (!Present)
			return "없음";
		return Name.empty() ? "(이름을 읽지 못함)" : Name;
	}

	std::string ForceEventReport(const std::string& Name, char Outcome, const std::string& Why)
	{
		switch (Outcome)
		{
		case 'n': return Name + ": 게임에 그 이름의 이벤트가 없습니다";
		case 'w': return Name + ": 강제 이벤트에 쓰지 못했습니다 (" + Why + ")";
		default: return Name + ": 강제 이벤트로 써 두었습니다. 게임의 감독이 다음에 이벤트를 뽑을 때(하루 한 번, 오후) 이것을 고릅니다";
		}
	}

	std::string CancelEventReport(char Outcome, const std::string& Detail)
	{
		switch (Outcome)
		{
		case 'n': return "예약된 이벤트가 없습니다";
		case 'f': return "예약을 지우는 함수를 부르지 못했습니다 (" + Detail + ")";
		case 'u': return "함수를 불렀지만 예약이 남아 있습니다: " + Detail;
		default: return "예약을 지웠습니다: " + Detail;
		}
	}

	std::string EndEventReport(EventFamily Family, char Outcome, const std::string& Detail)
	{
		const std::string word = EventFamilyWord(Family);
		switch (Outcome)
		{
		case 'x': return word + " 끝내기는 확인 전이라 부르지 않습니다 (research/29 의 절차로 게임에서 본 뒤에 켭니다)";
		case 'n': return "진행 중인 " + word + "이 없습니다";
		case 'f': return word + " 끝내기의 함수를 부르지 못했습니다 (" + Detail + ")";
		case 'u': return "함수를 불렀지만 " + word + "가 그대로입니다";
		default: return word + "를 끝냈습니다: " + Detail;
		}
	}
}
