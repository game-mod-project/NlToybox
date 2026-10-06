#pragma once
// 외교·종교·이벤트·월드에서 한 번 하는 일(src/World.cpp)의 판단 가운데 러너에 기대지 않는 것. 잰 것은 research/14.

#include <string>
#include <vector>

namespace NlCore
{
	// 창의 단추와 원격 `world <낱말>`이 같은 길을 탄다. 불러서 게임이 끝난 것(궁수 매복)은 넣지 않는다.
	enum class WorldAct { CooldownsClear, BishopSend };
	bool ParseWorldAct(const std::string& Word, WorldAct& Out);
	const char* WorldActWord(WorldAct Act);

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

	// 종교 행동의 비용이 든 게임 변수(global.__gameplay_vars 의 열쇠. research/21): 고해, 이혼, 구걸, 시성(금화, 영지마다), 제물 설교.
	const std::vector<const char*>& ReligionCostVars();
	// 기도와 예배가 신앙심을 되돌리는 양이 든 게임 변수: 교회의 기도, 제단의 기도, 아침 예배, 성인과의 대화.
	const std::vector<const char*>& PietyRestoreVars();
}
