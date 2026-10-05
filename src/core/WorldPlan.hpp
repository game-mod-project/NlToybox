#pragma once
// 외교·종교·이벤트·월드에서 한 번 하는 일(src/World.cpp)의 판단 가운데 러너에 기대지 않는 것. 잰 것은 research/14.

#include <string>

namespace NlCore
{
	// 창의 단추와 원격 `world <낱말>`이 같은 길을 탄다. 불러서 게임이 끝난 것(궁수 매복)은 넣지 않는다.
	enum class WorldAct { CooldownsClear, BishopSend };
	bool ParseWorldAct(const std::string& Word, WorldAct& Out);
	const char* WorldActWord(WorldAct Act);

	// 이벤트 쿨다운(남은 날)의 한 칸에 0 을 쓸지: 0 보다 큰 수에만 쓴다(수가 아닌 칸과 이미 0 인 칸은 건드리지 않는다).
	bool ShouldClearCooldown(bool IsNumber, double Value);

	// 주교를 부를지. Read: 주교가 있는지(is_has_bishop)를 읽었다. 읽지 못했거나 이미 있으면 부르지 않는다.
	enum class BishopStep { Call, AlreadyHere, Unknown };
	BishopStep ChooseBishopStep(bool Read, bool Has);
}
