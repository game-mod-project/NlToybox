#pragma once
// 치트 표(core/CheatTable)의 항목을 게임에 적용하고 영역의 패널을 그린다. 게임 속도도 여기서 건다.
// 스펙: 치트 메뉴 §6.2, §9. 창(Draw*)은 바라는 상태만 바꾸고, 값을 써 넣는 것은 GameTick 이다.

#include "core/CheatState.hpp"
#include "core/CheatTable.hpp"

#include <functional>
#include <map>
#include <set>
#include <string>

namespace NlCheats
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번. State 는 KeepKnown 을 거친 것이다. 켜져 있던 항목은 대상이 생기면 다시 적용된다.
	void Init(LogFn Log, const NlCore::CheatState& State);

	// 게임 스레드의 틱. Now: 모듈이 뜬 뒤의 초. Visible: 모드창이 열려 있는가(닫혀 있으면 켠 것만 건드린다).
	void GameTick(double Now, bool Visible);

	// 그 영역의 항목들을 그린다.
	void DrawArea(NlCore::Area Where);
	// "시간" 영역: 게임 속도.
	void DrawTime();
	// "프리셋" 영역: 확인된 항목의 묶음(core/Presets).
	void DrawPresets();
	// 묶음을 건다: 묶음에 없는 항목은 끄고 있는 항목은 켠다(배율은 묶음의 것으로). 없는 이름이면 거짓. Text 에 한 일.
	bool ApplyPreset(const std::string& Key, std::string& Text);
	// 게임의 시간을 멈추거나(Pause) 다시 흐르게 한다. 다음 틱이 게임의 함수를 부른다.
	void AskTime(bool Pause);

	// 그 영역에 표의 항목이 있는가.
	bool HasItems(NlCore::Area Where);

	// 켠 것이 바뀌었으면 상태 파일에 적을 것을 채우고 참을 돌려준다.
	bool TakeChanges(std::set<std::string>& On, std::map<std::string, double>& Numbers);

	// 그 항목이 켜져 있는가. 모듈의 코드가 하는 항목(Custom)이 본다.
	bool IsOn(const std::string& Id);
	// 그 항목 옆에 보일 글(한 일, 못 한 까닭). Custom 항목을 하는 코드가 적는다.
	void SetNote(const std::string& Id, const std::string& Note);
	// 그 항목이 켜져 있으면 창에서 정한 수(배율)를 돌려준다. 수가 없는 항목이거나 꺼져 있으면 거짓. CustomScale 을 하는 코드가 본다.
	bool Factor(const std::string& Id, double& Out);
	// 항목을 켜고 끈다(모드창의 체크와 같다. 원격 명령 cheat). 배율 항목을 켜면 표가 내놓는 배율(또는 앞서 정한 배율)로 켠다.
	// 값을 써 넣는 수 항목(Number)은 끄기만 된다. 없는 Id 면 거짓.
	bool Set(const std::string& Id, bool On);
	// 수가 있는 항목을 그 수로 켠다(범위 안으로 당긴다). 수가 없는 항목이거나 없는 Id 면 거짓.
	bool SetNumber(const std::string& Id, double Value);

	// 켜 둔 항목의 수.
	int ActiveCount();
	// 모두 끄고 원래 값으로 되돌린다.
	void ReleaseAll();
}
