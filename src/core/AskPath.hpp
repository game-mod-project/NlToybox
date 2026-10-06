#pragma once
// 켜져 있는 게임에 묻는 경로(NlToyBox.ask.txt)의 형식. 러너에 기대지 않는다.
//   global.a.b[3].c            전역 → 구조체 멤버 → 배열 원소
//   inst:o_debug.is_x          오브젝트의 첫 인스턴스의 변수
//   inst:o_building:1.generic  두 번째 인스턴스
//   map:150@building_resources@woodcutter_lvl_1#0#1   ds_map 의 키, ds_list 의 자리
//   map:44@{building.menu.x}   점이 든 키는 중괄호로 싼다

#include <string>
#include <vector>

namespace NlCore
{
	struct PathStep
	{
		char Kind = 0;			// '.' 멤버, '[' 배열 원소, '@' ds_map 의 키, '#' ds_list 의 자리
		std::string Name;		// '.' 과 '@'
		double Index = 0;		// '[' 과 '#'
	};

	struct AskPath
	{
		std::string Root;		// "global", "inst", "map", "list"
		std::string Object;		// inst 일 때 오브젝트 이름
		double Number = 0;		// inst 면 몇 번째 인스턴스인가, map·list 면 번호
		std::vector<PathStep> Steps;
		std::string Error;		// 비어 있지 않으면 읽지 못했다
	};

	AskPath ParseAskPath(const std::string& Text);

	// 읽히는 주소인가. NeedSteps 면 뿌리만 있는 주소(그릇이지 값이 아니다)는 거짓이다.
	bool GoodPath(const std::string& Text, bool NeedSteps = true);

	// 단계 하나를 경로에 붙일 글로. ParseAskPath 가 다시 읽을 수 있다.
	std::string FormatStep(const PathStep& Step);

	// 경로 전체를 글로. ParseAskPath 가 다시 읽을 수 있다. 읽지 못한 경로(Error 가 있다)는 빈 글이다.
	std::string FormatAskPath(const AskPath& Path);

	// 마지막 단계를 뗀 경로. 단계가 없으면 그대로다.
	AskPath ParentPath(AskPath Path);

	// 단계를 하나 붙인 경로.
	AskPath ChildPath(AskPath Path, PathStep Step);
}
