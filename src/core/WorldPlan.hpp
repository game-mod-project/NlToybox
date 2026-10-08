#pragma once
// 외교·종교·이벤트·월드에서 한 번 하는 일(src/World.cpp)의 판단 가운데 러너에 기대지 않는 것. 잰 것은 research/14.

#include "PlaceKey.hpp"

#include <map>
#include <string>
#include <vector>

namespace NlCore
{
	// 창의 단추와 원격 `world <낱말>`이 같은 길을 탄다. 불러서 게임이 끝난 것(궁수 매복)은 넣지 않는다.
	// 계절의 일(research/25): 보기(읽기만), 미루기(시작 시각을 하루 뒤로), 지금 단계 끝내기(남은 시간을 줄인다). 셈은 core/SeasonPlan.
	// 지금 저장(save. 게임의 자동 저장 함수 save_game(0, 1). research/28)과 이벤트 골라 일으키기(event. 감독의 __debug_forced_event 에 그 이벤트의 구조체를 쓴다. 이름을 받는다).
	// 이벤트 바로 일으키기(event_now. 게임이 부르는 꼴 그대로 그 이벤트의 __is_available() 과 __spawn_method() 를 부른다. research/32. 이름을 받는다).
	enum class WorldAct { CooldownsClear, BishopSend, SeasonShow, SeasonDelay, SeasonEnd, SaveNow, EventForce, EventList, EventCancel, EventEnd, EventNow };
	bool ParseWorldAct(const std::string& Word, WorldAct& Out);
	// 이름(name=)을 받는 일인가(이벤트의 예약과 바로 일으키기).
	bool WorldActNeedsName(WorldAct Act);
	// 이벤트의 시스템 이름의 꼴: 비지 않고 글자·숫자·밑줄만(64자 안).
	bool GoodEventName(const std::string& Name);
	// 지금 저장의 결과. Outcome: 'd' 게임의 저장이 꺼져 있어 부르지 않았다, 'u' 꺼져 있는지 읽지 못했다, 'f' 함수를 부르지 못했다(Detail 에 까닭),
	// 'n' 불렀지만 새 파일이 아직 없다, 's' 새 파일이 생겼다(Detail 에 이름).
	std::string SaveNowReport(char Outcome, const std::string& Detail);
	const char* WorldActWord(WorldAct Act);
	// 되는 낱말을 쉼표로 이은 글(틀린 낱말에 답한다).
	std::string WorldActWords();
	// 게임의 자료를 바꾸는 일인가. 보기는 읽기만 한다.
	bool WorldActChanges(WorldAct Act);
	// 계절의 일인가(보기, 미루기, 끝내기). src/Season 이 하고 제 자리에 결과를 둔다: World 의 "마지막 한 일"에 섞지 않는다.
	bool IsSeasonAct(WorldAct Act);
	// 이벤트의 일인가(일으키기, 목록, 예약 취소, 끝내기, 쿨다운 지우기). src/Events 가 하고 제 자리에 결과를 둔다(계절과 같다).
	bool IsEventAct(WorldAct Act);
	// 가족(kind=)을 받는 일인가(끝내기만).
	bool WorldActNeedsKind(WorldAct Act);

	// 이벤트 쿨다운(남은 날)의 한 칸에 0 을 쓸지: 0 보다 큰 수에만 쓴다(수가 아닌 칸과 이미 0 인 칸은 건드리지 않는다).
	bool ShouldClearCooldown(bool IsNumber, double Value);

	// 쿨다운의 구조체 하나를 지운 결과. Opened: 그 구조체를 열었다. Cleared: 0 을 써서 남은 칸. Failed: 쓰지 못한 칸.
	struct ClearResult
	{
		bool Opened = false;
		int Cleared = 0;
		int Failed = 0;
	};
	// 두 구조체(이벤트, 묶음)의 결과를 창과 원격에 보일 글로. 한쪽만 된 것, 쓰지 못한 칸을 숨기지 않는다.
	std::string CooldownReport(const ClearResult& Events, const ClearResult& Groups);
	// 게임의 자료를 건드렸거나 건드리려다 실패했는가(로그를 남길지).
	bool CooldownTouched(const ClearResult& Events, const ClearResult& Groups);

	// 주교를 부를지. Read: 주교가 있는지(is_has_bishop)를 읽었다. 읽지 못했거나 이미 있으면 부르지 않는다.
	enum class BishopStep { Call, AlreadyHere, Unknown };
	BishopStep ChooseBishopStep(bool Read, bool Has);

	// 광산의 매장량 붙들기(치트 표의 mine_stock_hold. research/25): 켠 동안 광산(열쇠)마다 본 가장 큰 매장량을 기억하고, 줄었으면 그 값으로 되돌려 쓴다.
	// 돌려주는 값: 쓸 것이 있는가(Write 에 쓸 값). 수가 아니거나 음수인 값은 기억하지도 쓰지도 않는다.
	bool KeepStock(std::map<std::string, double>& Kept, const std::string& Key, double Now, double& Write);
	// 기억한 매장량과 그것이 어느 자리(게임·지도)의 것인지.
	struct StockBook
	{
		std::map<std::string, double> Kept;
		PlaceKey Place;
		double Seen = 0;		// 마지막으로 본 게임 시각
	};
	// 틱마다 먼저 부른다: 자리가 바뀌었거나 시각이 거꾸로 갔으면 기억한 것을 모두 버린다(같은 열쇠를 가진 다른 세이브의 광산에 앞의 수를 쓰지 않게).
	void EnterStockPlace(StockBook& Book, const PlaceKey& Place, double Now);

	// 종교 행동의 비용이 든 게임 변수(global.__gameplay_vars 의 열쇠. research/21): 고해, 이혼, 구걸, 시성(금화, 영지마다), 제물 설교.
	const std::vector<const char*>& ReligionCostVars();
	// 기도와 예배가 신앙심을 되돌리는 양이 든 게임 변수: 교회의 기도, 제단의 기도, 아침 예배, 성인과의 대화.
	const std::vector<const char*>& PietyRestoreVars();
	// 설교 전환 계수가 든 게임 변수(하나. 이름으로 보아 설교가 사람을 바꾸는 정도에 곱하는 수다. 원래 1).
	const std::vector<const char*>& PreachFactorVars();
}
