#include "common.hpp"

#include <set>

void RunEventsTests()
{
	Test("이벤트: 표 61줄, 묶음, 갈래, 가족, 열쇠, 이름", [] {
		const std::vector<EventRow>& table = EventTable();
		CHECK(table.size() == 61);
		std::set<std::string> names;
		const std::set<std::string> groups = { "GLOBAL_MAP", "RAID", "EPIDEMY", "CAMP", "ECONOMICAL", "GUEST", "POLITICAL", "UPRISING", "DISTORTION", "REWARD", "FOREST_BANDIT" };
		for (const EventRow& row : table)
		{
			const std::string name = row.Name;
			CHECK(names.insert(name).second);				// 겹치지 않는다
			CHECK(groups.count(row.Group) == 1);
			CHECK(row.Type >= 0 && row.Type <= 3);
			CHECK(std::string(row.CaptionKey).empty() != std::string(row.Korean).empty());		// 열쇠가 있거나 지은 이름이 있거나(둘 다는 아니다)
			// 가족은 이름의 규칙과 맞다(research/29: 늑대 습격 예언은 Prophecy, 도적의 공격 셋은 Raid, 정치 여섯과 뇌물은 Unrest)
			EventFamily want = EventFamily::None;
			if (name.rfind("u_guest_", 0) == 0) want = EventFamily::Guest;
			else if (name.rfind("prophecy_", 0) == 0) want = EventFamily::Prophecy;
			else if (name.rfind("raid_", 0) == 0 || name.find("_bandits_attack_") != std::string::npos) want = EventFamily::Raid;
			else if (name.rfind("rebellion_", 0) == 0) want = EventFamily::Rebellion;
			else if (name == "conspiracy") want = EventFamily::Conspiracy;
			else if (name.rfind("desire_politician", 0) == 0 || name.rfind("politician_", 0) == 0) want = EventFamily::Unrest;
			CHECK(row.Family == want);
		}
		// 열쇠의 규칙(research/29 에서 본 것). 도적 기지는 게임의 오타 그대로.
		CHECK_STR(FindEvent("u_guest_bard")->CaptionKey, "guest.bard");
		CHECK_STR(FindEvent("raid_bandits")->CaptionKey, "raid.bandits");
		CHECK_STR(FindEvent("prophecy_bandits_camp")->CaptionKey, "prophecy.bandits_capm");
		CHECK_STR(FindEvent("prophecy_wolf_attack")->CaptionKey, "prophecy.wolf_attack");
		CHECK_STR(FindEvent("reward_rich_migrants")->CaptionKey, "map.reward_rich_migrants");
		CHECK_STR(FindEvent("conspiracy")->CaptionKey, "map.conspiracy");
		CHECK(std::string(FindEvent("u_guest_incognito_bishop")->CaptionKey).empty() && !std::string(FindEvent("u_guest_incognito_bishop")->Korean).empty());
		CHECK(std::string(FindEvent("rebellion_slaves")->CaptionKey).empty() && std::string(FindEvent("rebellion_slaves")->Korean) == "노예 반란");
		CHECK(FindEvent("no_such_event") == nullptr && FindEvent("") == nullptr);
		CHECK(FindEvent("u_guest_bard")->Type == 2 && FindEvent("raid_bandits")->Type == 0 && FindEvent("dark_actions")->Type == 1 && FindEvent("ai_preach")->Type == 3);
		// 묶음의 목록: 표에서 모아 이름순(겹치지 않게). FOREST_BANDIT 은 파일의 묶음에만 있고 이벤트에는 없다 → 표의 묶음은 10개.
		const std::vector<std::string> list = EventGroups();
		CHECK(list.size() == 10 && list.front() == "CAMP" && list.back() == "UPRISING");
		CHECK_STR(EventTypeWord(0), "큰 위협"); CHECK_STR(EventTypeWord(1), "작은 위협"); CHECK_STR(EventTypeWord(2), "보통"); CHECK_STR(EventTypeWord(3), "좋음"); CHECK_STR(EventTypeWord(7), "?");
		CHECK_STR(EventFamilyWord(EventFamily::Raid), "습격"); CHECK_STR(EventFamilyWord(EventFamily::Unrest), "소요"); CHECK_STR(EventFamilyWord(EventFamily::None), "");
		CHECK_STR(EventFamilyKey(EventFamily::Prophecy), "prophecy"); CHECK_STR(EventFamilyKey(EventFamily::None), "");
		// 화면 이름: 열쇠의 줄이 있으면 그 글, 없으면 지은 이름, 그것도 없으면 원시 이름
		const std::unordered_map<std::string, std::string> captions = { { "guest.bard", "음유시인" } };
		CHECK_STR(EventLabel(*FindEvent("u_guest_bard"), captions), "음유시인");
		CHECK_STR(EventLabel(*FindEvent("raid_bandits"), captions), "raid_bandits");				// 열쇠는 있는데 줄이 없다
		CHECK_STR(EventLabel(*FindEvent("rebellion_slaves"), captions), "노예 반란");
		// 찾기: 원시 이름과 화면 이름에서, 대소문자 없이. 빈 글은 모두 맞는다
		CHECK(EventMatches("", "u_guest_bard", "음유시인") && EventMatches("BARD", "u_guest_bard", "음유시인") && EventMatches("음유", "u_guest_bard", "음유시인"));
		CHECK(!EventMatches("raid", "u_guest_bard", "음유시인"));
		// 보일 줄: 묶음(빈 글이면 전체)과 찾기 둘 다 맞아야 한다
		CHECK(EventRowShown("", "", "GUEST", "u_guest_bard", "음유시인") && EventRowShown("GUEST", "bard", "GUEST", "u_guest_bard", "음유시인"));
		CHECK(!EventRowShown("RAID", "", "GUEST", "u_guest_bard", "음유시인") && !EventRowShown("GUEST", "raid", "GUEST", "u_guest_bard", "음유시인"));
		// 쿨다운의 칸: 없으면(-1) "-", 아니면 "N일"
		CHECK_STR(CooldownText(-1), "-"); CHECK_STR(CooldownText(18), "18일"); CHECK_STR(CooldownText(0), "0일"); CHECK_STR(CooldownText(2.5), "2.5일");
		// 게임의 이름 목록과 표를 합친다: 둘 다 있는 것(표의 차례), 게임에만 있는 것(이름순), 표에만 있는 것의 수
		const EventListing merged = MergeEventNames({ "raid_bandits", "zzz_new_event", "u_guest_bard", "aaa_new" });
		CHECK(merged.Known.size() == 2 && merged.Known[0] == "raid_bandits" && merged.Known[1] == "u_guest_bard");		// 표의 차례(RAID 가 GUEST 보다 앞)
		CHECK(merged.Extra.size() == 2 && merged.Extra[0] == "aaa_new" && merged.Extra[1] == "zzz_new_event");
		CHECK(merged.MissingFromGame == 59);
		CHECK(MergeEventNames({}).Known.empty() && MergeEventNames({}).MissingFromGame == 61);
	});
}
