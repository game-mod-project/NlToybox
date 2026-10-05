#pragma once
// 탐색기. 게임의 아무 값이나 보고 고친다: 훑기, 즐겨찾기·잠금, 찾기. 스펙: 치트 메뉴 §7.
// 그리는 쪽(Draw)은 글로 된 스냅샷만 읽고 명령을 큐에 넣는다. 러너는 GameTick 만 건드린다.

#include "core/CheatState.hpp"

#include <functional>
#include <string>
#include <vector>

namespace NlExplorer
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번. 상태 파일의 즐겨찾기와 잠금을 받는다. 잠금은 꺼진 채로 둔다.
	void Init(LogFn Log, const std::vector<std::string>& Pins, const std::vector<NlCore::LockLine>& Locks);

	// 게임 스레드의 틱. Now: 모듈이 뜬 뒤의 초. Shown: 탐색기가 화면에 보이는가(안 보이면 잠금만 건다).
	void GameTick(double Now, bool Shown);

	// 패널 안을 그린다.
	void Draw();

	// 그 주소의 그릇으로 간다(시험 설정의 path= 가 쓴다). 다음 틱에 읽는다.
	void Navigate(const std::string& Address);

	// 즐겨찾기나 잠금이 바뀌었으면 상태 파일에 적을 것을 채우고 참을 돌려준다.
	bool TakeChanges(std::vector<std::string>& Pins, std::vector<NlCore::LockLine>& Locks);

	// 지금 걸려 있는 잠금의 수.
	int ActiveLocks();
	// 잠금을 모두 푼다(값은 남긴다).
	void ReleaseAll();
}
