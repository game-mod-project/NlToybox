#pragma once
// 게임 스크립트의 함수에 거는 훅. 게임이 스스로 부를 때의 인자와 반환값을 적고(호출 기록기. 스펙: 치트 메뉴 §14),
// 돌려주는 값을 바꾼다(스펙 §3 의 수단 D).
// 인자의 수와 형을 모르는 스크립트를 부르면 게임이 끝난다(CLAUDE.md). 부르기 전에 이것으로 꼴을 확인한다.
// 모든 함수는 게임 스레드의 틱에서 부른다.

#include <YYTK_Shared.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace NlRecorder
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번.
	void Init(Aurie::AurieModule* Module, LogFn Log);

	// 그 함수의 호출을 기록하기 시작한다. Target: 스크립트의 이름("gml_Script_x". 접두는 없어도 된다) 또는 메서드의 주소
	// ("inst:o_building.set_level"). Name 에 찾은 스크립트의 이름을 돌려준다. 이미 기록 중이면 기록을 비우고 다시 시작한다.
	bool Watch(const std::string& Target, std::string& Name, std::string& Why);

	// 기록을 멈춘다. Name: 스크립트의 이름 또는 "all". 훅은 떼지 않는다(그 함수가 호출 스택에 있을 때 떼면 죽는다).
	// 돌려주는 값: 멈춘 수.
	int Unwatch(const std::string& Name);

	// 기록을 글로. Name 이 비어 있으면 전부. 값을 바꾸고 있는 함수는 그것도 적는다.
	std::string Report(const std::string& Name);

	// 바꿔서 돌려줄 값. 수, 불리언, undefined 뿐이다(훅 안에서 만들 수 있는 것).
	struct Forced
	{
		char Kind = 'u';		// 'n' 수, 'b' 불리언, 'u' undefined, 'x' 원래 함수가 돌려준 수에 Number 를 곱한다
		double Number = 0;
		bool Skip = false;		// 원래 함수를 부르지 않는다(값을 치르는 함수처럼 한 일 자체를 없애야 할 때). 'x' 에는 쓰지 않는다
		bool Whole = false;		// 'x': 정수는 정수로 남기고 양수는 1 아래로 내리지 않는다(가격). core/Hooks 의 ScaleResult
		char Who = 'a';			// 누구의 호출에 걸지: 'a' 모두, 'p' self 가 플레이어의 영혼일 때만, 'o' 아닐 때만(SetPlayerSelves 가 넣은 주소와 견준다)
	};

	// 플레이어의 영혼(구조체)의 주소들. Who 가 'p'·'o' 인 바꾸기가 self 와 견준다. 틱이 통째로 갈아 끼운다(src/People.cpp).
	// 훅 안에서는 빌트인을 부를 수 없어 진영을 그 자리에서 읽지 못한다. 그래서 주소를 미리 모아 둔다.
	void SetPlayerSelves(std::vector<std::uintptr_t> Selves);

	// 그 함수가 돌려주는 값을 바꾼다. 훅이 없으면 건다. 바꾼 호출이 보이게 기록도 한다. Target 은 Watch 와 같다.
	// 게임의 판정을 바꾸는 일이다: 그 함수가 무엇을 돌려주는지 기록으로 본 뒤에만 쓴다.
	bool Override(const std::string& Target, const Forced& Value, std::string& Name, std::string& Why);

	// 바꾸기를 그만둔다(훅은 남고 원래대로 지나간다). Name: 스크립트의 이름 또는 "all". 돌려주는 값: 그만둔 수.
	int Unoverride(const std::string& Name);

	// 그 스크립트(Override 가 Name 에 돌려준 이름)가 돌려주는 값을 지금 바꾸고 있는가.
	bool Overriding(const std::string& Name);
}
