#pragma once
// 특성의 글(research/20): 화면 이름은 게임 폴더의 localization\main.csv(Korean 칸, 비면 English 칸)에서, 설명은 힌트 파일 셋에서 읽는다(게임의 글을 레포에 싣지 않는다).
// 화면 이름의 열쇠와 설명의 열쇠는 게임의 gml_Script_trait_property_get(이름, 번호)에 묻는다(0 이름, 1 화면 이름의 열쇠, 21 힌트의 열쇠. 배치는 쓰기 전에 본다).
// 게임의 특성 이름 목록(inst:o_data.game_trait_list)도 여기서 읽는다. 2026-10-07 리팩토링 C 에서 src/People.cpp 에서 옮겼다.
// 그리기(툴팁, 특성 목록)에서도 읽으므로 제 뮤텍스를 둔다.

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace NlTraitText
{
	using LogFn = std::function<void(const std::string&)>;

	void Init(LogFn Log, const std::filesystem::path& GameDir);		// ModuleInitialize 에서. 파일만 읽는다
	// 게임 스레드에서, 게임을 불러올 때마다: 게임의 특성 이름 목록을 읽고 글을 붙인다(People 의 Scan 이 처음 성공할 때 부른다).
	void LoadForGame();
	const std::vector<std::string>& Names();		// 게임에 있는 특성의 이름(이름순. 이분 탐색에 쓴다)
	const std::vector<std::string>& Shown();		// 같은 이름들을 창에 보일 차례로(core 의 TraitBefore)
	bool Known(const std::string& Name);			// 게임에 그 이름의 특성이 있는가
	const std::string& Caption(const std::string& Name);	// 화면 이름. 없으면 빈 글
	const std::string& Hint(const std::string& Name);		// 설명. 없으면 빈 글
	bool Titled(const std::string& Name);					// 화면 이름이 힌트의 제목에서 온 것인가(흐린 글씨)
	std::string Label(const std::string& Name);				// "화면 이름 (게임의 이름)" 또는 게임의 이름
	const char* SkillLabel(size_t Index);					// 능력치의 화면 이름(없으면 모듈의 이름)
	struct Notes
	{
		std::string Names, Hints, Files;		// 이름을 어디서 몇 개 읽었는가, 설명이 몇 개 붙었는가, 힌트 파일을 읽은 결과
	};
	Notes GetNotes();
}
