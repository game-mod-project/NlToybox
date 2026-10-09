#pragma once
// 전투의 항목들(research/13·16·23): 아군 무적(take_damage 를 self 가 플레이어의 영혼일 때 건너뛴다), 아군·적의 전투력과 맷집(한 함수에 두 배율).
// 훅은 self 가 플레이어의 영혼인지로 가린다: 0.5초마다 플레이어의 사람들의 __soul 구조체 주소를 모아 NlRecorder::SetPlayerSelves 에 넣는다(주소부터 넣고 건다).
// 2026-10-07 리팩토링 C 에서 src/People.cpp 에서 옮겼다. People 의 틱 안에서만 불린다(People 의 잠금 아래. 제 뮤텍스는 없다).

#include <functional>
#include <string>

namespace NlShield
{
	using LogFn = std::function<void(const std::string&)>;
	void Init(LogFn Log);
	// 게임 스레드의 틱(People::GameTick 이 부른다). 0.5초에 한 번.
	void Tick(double Now);
	// 다음 틱에 주소를 바로 다시 모은다(모듈이 사람을 만든 뒤: 병사 추가, 소환).
	void RefreshSoon();
}
