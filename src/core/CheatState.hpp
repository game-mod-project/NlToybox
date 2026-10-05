#pragma once
// 치트의 상태 파일(NlToyBox.cheats.txt). 러너에 기대지 않는다. 스펙: 치트 메뉴 §6.3.
//   on <id>              켠 스위치
//   num <id>=<수>        값을 정한 항목
//   pin <주소>           즐겨찾기
//   lock <주소>=<수>     잠금 값. 불러올 때는 꺼진 채다(인스턴스의 차례가 실행마다 다를 수 있다)

#include <istream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace NlCore
{
	struct LockLine
	{
		std::string Path;
		double Value = 0;
	};

	struct CheatState
	{
		std::set<std::string> On;
		std::map<std::string, double> Numbers;
		std::vector<std::string> Pins;		// 적힌 차례대로. 같은 주소는 한 번만
		std::vector<LockLine> Locks;		// 적힌 차례대로. 같은 주소는 뒤의 것이 이긴다
	};

	// 읽을 수 없는 줄(모르는 낱말, 수가 아닌 값, 읽히지 않는 주소)은 버린다.
	CheatState ParseCheatState(std::istream& In);

	// ParseCheatState 가 읽는 꼴로 쓴다.
	std::string FormatCheatState(const CheatState& State);
}
