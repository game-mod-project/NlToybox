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

	// 끝내기의 후보(research/29 에서 이름만 봤다). StatusPath: 진행 중의 자리(undefined·-4 면 없음, 구조체면 진행 중). EndPath: 끝내는 함수.
	// ArgShape: 인자의 꼴. **빈 글 = 인자 없음**(지금 부를 수 있는 유일한 꼴). 다른 글("(struct)" 같은)은 게임에서 본 꼴을 적어 두되 아직 부르지 못한다(그 꼴을 부르는 코드를 더한 뒤에 빈 글이 아닌 꼴을 허용한다).
	// Verified: 게임에서 인자와 효과를 본 뒤에만 참(스펙 §7 의 절차). 그때까지 부르지 않는다.
	struct EventEndRow
	{
		EventFamily Family;
		const char* StatusPath;
		const char* EndPath;
		const char* ArgShape;
		bool Verified;
	};
	const std::vector<EventEndRow>& EventEndTable();			// 다섯: 습격, 예언, 음모, 손님, 소요
	const EventEndRow* FindEventEnd(EventFamily Family);		// 없으면 nullptr(None, Rebellion)
	bool EventEndAllowed(const EventEndRow& Row);				// Verified 이고 인자의 꼴이 빈 글(인자 없음)일 때만
	bool ParseEventFamily(const std::string& Word, EventFamily& Out);	// 끝내기의 가족 다섯의 열쇠(raid …)만. rebellion 은 받지 않는다

	// 예약 취소: 예약이 없으면 함수를 부르지 않는다.
	enum class CancelStep { Nothing, Call };
	CancelStep ChooseCancelStep(bool HasForced);

	// 진행 중의 글. Read: 자리를 읽었다. Present: 구조체가 있다. Name: 그 구조체의 이름(없으면 빈 글). Rebellion 은 언제나 "모름".
	std::string EventStatusText(EventFamily Family, bool Read, bool Present, const std::string& Name);
	// 예약된 강제 이벤트의 글. Read: 자리(__debug_forced_event)를 읽었다. Present: 구조체가 있다. Name: 그 __system_name(글일 때). 읽지 못한 것을 "없음"으로 적지 않는다.
	std::string ForcedEventText(bool Read, bool Present, const std::string& Name);
	// 일으키기의 결과. Outcome: 'n' 그 이름의 이벤트가 없다, 'w' 쓰지 못했다(Why), 'd' 써 두었다(감독이 다음에 뽑을 때 고른다. research/28).
	std::string ForceEventReport(const std::string& Name, char Outcome, const std::string& Why);
	// 바로 일으키기(research/32). 게임은 이벤트를 뽑을 때 그 이벤트의 __is_available() 을 묻고, 생길 시각에 __spawn_method() 를 인자 없이 부른다.
	// 조건의 답이 참일 때만 일으킨다. IsNumber: 답이 수(불리언 포함)다. Value: 그 수. 거짓과 undefined(손님이 이미 와 있을 때의 손님 이벤트)는 아니다.
	bool EventAvailable(bool IsNumber, double Value);
	// 바로 일으키기의 결과. 'n' 그 이름의 이벤트가 없다, 'm' 조건 함수를 부르지 못했다(Detail), 'a' 조건이 맞지 않는다(Detail 에 답),
	// 'f' 만드는 함수를 부르지 못했다(Detail), 'd' 일으켰고 쿨다운에 올랐다(Detail 에 남은 날), 'c' 일으켰지만 쿨다운은 올리지 못했다(Detail).
	std::string InstantEventReport(const std::string& Name, char Outcome, const std::string& Detail);
	// 취소의 결과. 'n' 예약이 없었다, 'f' 함수를 부르지 못했다(Detail 에 까닭), 'u' 부른 뒤에도 남아 있다(Detail 에 이름), 'd' 지웠다(Detail 에 이름).
	std::string CancelEventReport(char Outcome, const std::string& Detail);
	// 끝내기의 결과. 'x' 확인 전, 'n' 진행 중이 아니다, 'f' 부르지 못했다(Detail), 'u' 부른 뒤에도 그대로다, 'd' 끝냈다(Detail 에 이름).
	std::string EndEventReport(EventFamily Family, char Outcome, const std::string& Detail);
}
