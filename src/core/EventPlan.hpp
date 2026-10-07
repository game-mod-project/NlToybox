#pragma once
// 이벤트 탭(research/29)에서 러너에 기대지 않는 것: 이벤트 61개의 표(이름·묶음·갈래·화면 이름의 열쇠·모드가 지은 이름·가족), 보일 줄의 판단, 글.
// 이름·묶음은 게임 파일 director_params.json 의 열쇠, 갈래는 런타임의 __type(0 BIG_THREAT, 1 SMALL_THREAT, 2 NEUTRAL, 3 GOOD). 값(tickets, cooldown_days)은 옮기지 않는다.
// 화면 이름의 열쇠는 localization\main.csv 에서 본 꼴(guest.<이름>, raid.<이름>, prophecy.<이름>, map.reward_*, map.conspiracy). 열쇠가 없는 것은 모드가 한국어 이름을 지었다(추정).

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NlCore
{
	// 진행 중의 자리와 끝내기의 단위. Rebellion 은 읽을 자리를 아직 모른다(창에 "모름"). None 은 보상·세계 지도·어두운 행동·도적 이주민.
	enum class EventFamily { None, Raid, Prophecy, Conspiracy, Guest, Unrest, Rebellion };

	struct EventRow
	{
		const char* Name;			// 게임의 시스템 이름
		const char* Group;			// 묶음(GUEST, RAID …)
		int Type;					// 갈래 0..3
		const char* CaptionKey;		// 화면 이름의 열쇠(main.csv). 없으면 빈 글
		const char* Korean;			// 열쇠가 없을 때 모드가 지은 이름(추정). 열쇠가 있으면 빈 글
		EventFamily Family;
	};
	const std::vector<EventRow>& EventTable();
	const EventRow* FindEvent(const std::string& Name);		// 없으면 nullptr
	std::vector<std::string> EventGroups();					// 표의 묶음을 이름순으로(겹치지 않게)
	const char* EventTypeWord(int Type);					// "큰 위협", "작은 위협", "보통", "좋음". 그 밖은 "?"
	const char* EventFamilyWord(EventFamily Family);		// "습격", "예언", "음모", "손님", "소요", "반란". None 은 빈 글
	const char* EventFamilyKey(EventFamily Family);			// "raid", "prophecy", "conspiracy", "guest", "unrest", "rebellion". None 은 빈 글

	// 창에 보일 이름: 열쇠의 줄이 Captions 에 있으면 그 글, 없으면 지은 이름, 그것도 없으면 원시 이름.
	std::string EventLabel(const EventRow& Row, const std::unordered_map<std::string, std::string>& Captions);
	// 찾기: 원시 이름이나 화면 이름에 들어 있다(영문은 대소문자를 가리지 않는다. core/Localization 의 TraitMatches). 빈 글은 모두 맞는다.
	bool EventMatches(std::string_view Filter, const std::string& Name, const std::string& Label);
	// 보일 줄인가: 고른 묶음(빈 글이면 전체)과 찾기가 둘 다 맞는다.
	bool EventRowShown(const std::string& Group, std::string_view Filter, const std::string& RowGroup, const std::string& Name, const std::string& Label);
	// 쿨다운의 칸: 남은 날이 없으면(음수) "-", 아니면 "N일".
	std::string CooldownText(double DaysLeft);

	// 게임의 이름 목록(ds_map 의 열쇠)과 표를 합친 것: Known 은 둘 다 있는 이름(표의 차례), Extra 는 게임에만 있는 이름(이름순. 게임이 갱신되면 생긴다), MissingFromGame 은 표에만 있는 수.
	struct EventListing
	{
		std::vector<std::string> Known, Extra;
		size_t MissingFromGame = 0;
	};
	EventListing MergeEventNames(const std::vector<std::string>& GameNames);
}
