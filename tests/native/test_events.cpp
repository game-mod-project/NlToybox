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

	Test("이벤트: 끝내기의 표, 취소의 걸음, 상태와 결과의 글, 원격의 낱말", [] {
		// 끝내기의 후보 다섯(research/29). 처음에는 모두 확인 전이라 부르지 않는다.
		const std::vector<EventEndRow>& ends = EventEndTable();
		CHECK(ends.size() == 5);
		for (const EventEndRow& row : ends)
		{
			CHECK(row.Family != EventFamily::None && row.Family != EventFamily::Rebellion);
			CHECK(std::string(row.StatusPath).rfind("inst:o_game_map_controller.", 0) == 0 && std::string(row.EndPath).rfind("inst:o_game_map_controller.", 0) == 0);
			CHECK(!row.Verified && !EventEndAllowed(row));
		}
		CHECK_STR(FindEventEnd(EventFamily::Raid)->StatusPath, "inst:o_game_map_controller.__raids_manager.__current_raid");
		CHECK_STR(FindEventEnd(EventFamily::Raid)->EndPath, "inst:o_game_map_controller.__raids_manager.try_to_remove_raid");
		CHECK_STR(FindEventEnd(EventFamily::Prophecy)->EndPath, "inst:o_game_map_controller.__province.__prophecy_manager.__reset_prophecy");
		CHECK_STR(FindEventEnd(EventFamily::Unrest)->StatusPath, "inst:o_game_map_controller.__province.__politics_manager.__is_unrest_active_struct");
		CHECK(FindEventEnd(EventFamily::Rebellion) == nullptr && FindEventEnd(EventFamily::None) == nullptr);
		const EventEndRow verified{ EventFamily::Raid, "a", "b", "", true };
		CHECK(EventEndAllowed(verified));
		// 인자의 꼴: 빈 글만 "인자 없음"으로 부를 수 있다. 확인됐어도 다른 꼴이 적혀 있으면 아직 부르지 못한다(최종 리뷰 2번: 헤더와 코드의 규약을 하나로).
		const EventEndRow shaped{ EventFamily::Raid, "a", "b", "(struct)", true };
		CHECK(!EventEndAllowed(shaped));
		EventFamily family = EventFamily::None;
		CHECK(ParseEventFamily("raid", family) && family == EventFamily::Raid);
		CHECK(ParseEventFamily("unrest", family) && family == EventFamily::Unrest);
		CHECK(!ParseEventFamily("rebellion", family) && !ParseEventFamily("", family) && !ParseEventFamily("RAID", family));
		// 예약 취소: 예약이 없으면 함수를 부르지 않는다
		CHECK(ChooseCancelStep(false) == CancelStep::Nothing && ChooseCancelStep(true) == CancelStep::Call);
		// 예약의 글(최종 리뷰 3번): 자리를 읽지 못한 것과 없는 것을 가른다. 구조체는 있는데 이름을 못 읽은 것도 따로.
		CHECK_STR(ForcedEventText(false, false, ""), "읽지 못함"); CHECK_STR(ForcedEventText(true, false, ""), "없음");
		CHECK_STR(ForcedEventText(true, true, ""), "(이름을 읽지 못함)"); CHECK_STR(ForcedEventText(true, true, "u_guest_joker"), "u_guest_joker");
		// 상태의 글
		CHECK_STR(EventStatusText(EventFamily::Raid, true, false, ""), "없음");
		CHECK_STR(EventStatusText(EventFamily::Raid, true, true, "raid_bandits"), "진행 중: raid_bandits");
		CHECK_STR(EventStatusText(EventFamily::Raid, true, true, ""), "진행 중 (이름을 읽지 못함)");
		CHECK_STR(EventStatusText(EventFamily::Raid, false, false, ""), "읽지 못함");
		CHECK_STR(EventStatusText(EventFamily::Rebellion, false, false, ""), "모름 (읽을 자리를 아직 모른다)");
		// 결과의 글(일으키기는 WorldPlan 에서 옮겨 왔다)
		CHECK_STR(ForceEventReport("u_guest_bard", 'n', ""), "u_guest_bard: 게임에 그 이름의 이벤트가 없습니다");
		CHECK_STR(ForceEventReport("u_guest_bard", 'w', "why"), "u_guest_bard: 강제 이벤트에 쓰지 못했습니다 (why)");
		CHECK_STR(ForceEventReport("u_guest_bard", 'd', ""), "u_guest_bard: 강제 이벤트로 써 두었습니다. 게임의 감독이 다음에 이벤트를 뽑을 때(하루 한 번, 오후) 이것을 고릅니다");
		CHECK_STR(CancelEventReport('n', ""), "예약된 이벤트가 없습니다");
		CHECK_STR(CancelEventReport('f', "no method"), "예약을 지우는 함수를 부르지 못했습니다 (no method)");
		CHECK_STR(CancelEventReport('u', "u_guest_joker"), "함수를 불렀지만 예약이 남아 있습니다: u_guest_joker");
		CHECK_STR(CancelEventReport('d', "u_guest_joker"), "예약을 지웠습니다: u_guest_joker");
		CHECK_STR(EndEventReport(EventFamily::Raid, 'x', ""), "습격 끝내기는 확인 전이라 부르지 않습니다 (research/29 의 절차로 게임에서 본 뒤에 켭니다)");
		CHECK_STR(EndEventReport(EventFamily::Prophecy, 'n', ""), "진행 중인 예언이 없습니다");
		CHECK_STR(EndEventReport(EventFamily::Guest, 'f', "why"), "손님 끝내기의 함수를 부르지 못했습니다 (why)");
		CHECK_STR(EndEventReport(EventFamily::Conspiracy, 'u', ""), "함수를 불렀지만 음모가 그대로입니다");
		CHECK_STR(EndEventReport(EventFamily::Unrest, 'd', "x"), "소요를 끝냈습니다: x");
		// WorldAct 셋과 원격의 낱말
		WorldAct act = WorldAct::CooldownsClear;
		CHECK(ParseWorldAct("events", act) && act == WorldAct::EventList && !WorldActChanges(WorldAct::EventList));
		CHECK(ParseWorldAct("event_cancel", act) && act == WorldAct::EventCancel && WorldActChanges(WorldAct::EventCancel));
		CHECK(ParseWorldAct("event_end", act) && act == WorldAct::EventEnd && WorldActChanges(WorldAct::EventEnd));
		CHECK(IsEventAct(WorldAct::EventForce) && IsEventAct(WorldAct::EventCancel) && IsEventAct(WorldAct::EventEnd) && IsEventAct(WorldAct::EventList) && IsEventAct(WorldAct::CooldownsClear));
		CHECK(!IsEventAct(WorldAct::BishopSend) && !IsEventAct(WorldAct::SaveNow) && !IsEventAct(WorldAct::SeasonShow));
		CHECK(WorldActNeedsKind(WorldAct::EventEnd) && !WorldActNeedsKind(WorldAct::EventForce) && !WorldActNeedsName(WorldAct::EventEnd));
		CHECK(WorldActWords() == "cooldowns_clear, bishop, season, season_delay, season_end, save, event, events, event_cancel, event_end, event_now");
		const RemoteCommand list = ParseRemoteLine("world events group=GUEST find=bard");
		CHECK(list.Error.empty() && list.Target == "events" && list.Options.at("group") == "GUEST" && list.Options.at("find") == "bard");
		CHECK(ParseRemoteLine("world events").Error.empty());
		CHECK(ParseRemoteLine("world event_cancel").Error.empty() && !ParseRemoteLine("world event_cancel name=x").Error.empty());
		const RemoteCommand end = ParseRemoteLine("world event_end kind=raid");
		CHECK(end.Error.empty() && end.Target == "event_end" && end.Options.at("kind") == "raid");
		CHECK(!ParseRemoteLine("world event_end").Error.empty() && !ParseRemoteLine("world event_end kind=x").Error.empty() && !ParseRemoteLine("world event_end kind=rebellion").Error.empty());
	});

	Test("이벤트: 바로 일으키기의 낱말과 원격의 줄", [] {
		WorldAct act = WorldAct::CooldownsClear;
		CHECK(ParseWorldAct("event_now", act) && act == WorldAct::EventNow && std::string(WorldActWord(WorldAct::EventNow)) == "event_now");
		CHECK(WorldActChanges(WorldAct::EventNow) && IsEventAct(WorldAct::EventNow) && WorldActNeedsName(WorldAct::EventNow) && !WorldActNeedsKind(WorldAct::EventNow));
		const RemoteCommand now = ParseRemoteLine("world event_now name=reward_rich_migrants");
		CHECK(now.Error.empty() && now.Target == "event_now" && now.Options.at("name") == "reward_rich_migrants");
		// 이름이 없거나 꼴이 틀리면 받지 않는다(예약과 같다)
		CHECK(!ParseRemoteLine("world event_now").Error.empty() && !ParseRemoteLine("world event_now name=").Error.empty() && !ParseRemoteLine("world event_now name=a-b").Error.empty());
	});

	Test("이벤트: 게임의 조건 함수가 참이라고 답할 때만 바로 일으킨다", [] {
		// __is_available() 의 답(research/32): 보상·늑대 예언은 true, 습격·반란·음모는 false, 손님이 이미 와 있을 때의 손님 이벤트는 undefined.
		CHECK(EventAvailable(true, 1));
		CHECK(!EventAvailable(true, 0));		// false
		CHECK(!EventAvailable(false, 0));		// undefined 나 수가 아닌 값
		CHECK(!EventAvailable(false, 1));
		CHECK(!EventAvailable(true, std::numeric_limits<double>::quiet_NaN()));
	});

	Test("이벤트: 바로 일으키기의 결과의 글", [] {
		CHECK_STR(InstantEventReport("u_guest_bard", 'n', ""), "u_guest_bard: 게임에 그 이름의 이벤트가 없습니다");
		CHECK_STR(InstantEventReport("raid_bandits", 'm', "why"), "raid_bandits: 게임의 조건 함수(__is_available)를 부르지 못해 일으키지 않았습니다 (why)");
		CHECK_STR(InstantEventReport("raid_bandits", 'a', "false"), "raid_bandits: 게임의 조건이 지금 맞지 않아 일으키지 않았습니다 (__is_available 의 답: false)");
		CHECK_STR(InstantEventReport("raid_bandits", 'f', "why"), "raid_bandits: 이벤트를 만드는 함수(__spawn_method)를 부르지 못했습니다 (why)");
		CHECK_STR(InstantEventReport("reward_rich_migrants", 'd', "14"), "reward_rich_migrants: 바로 일으켰습니다 (쿨다운 14일)");
		// 생성은 불렀지만 쿨다운에 오르지 않았다: 일으킨 것까지만 말한다
		CHECK_STR(InstantEventReport("reward_rich_migrants", 'c', "why"), "reward_rich_migrants: 바로 일으켰습니다. 쿨다운은 올리지 못했습니다 (why)");
	});
}
