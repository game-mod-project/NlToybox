#pragma once
// 게임의 자료를 돌며 값을 쓰는 일(Job)의 엔진. 치트 표의 Custom(0 쓰기)·CustomScale(배율) 항목이 등록한다(영역 파일이 제 Init 에서 Add).
// 처음 본 값을 장부(core/CostBook)에 적고 끄면 그 값으로 되돌린다. 쓴 뒤에는 다시 읽어 남았는지 본다. 한 자리에 쓸 값은 core/PlanValue 가 정한다.
// 잰 것은 research/10(창고 용량, 조리법), 21(종교), 24(임신), 26(범죄), 09(건설비). 2026-10-07 리팩토링 C 에서 src/Production.cpp 의 엔진을 옮겼다.

#include "core/AskPath.hpp"

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>
#include <vector>

namespace NlJobs
{
	using LogFn = std::function<void(const std::string&)>;

	// 대상 하나: In 안의 Step 자리에 수가 있다. Key·Level·Slot 은 장부에서 그 자리를 가리는 이름이다.
	using Visit = std::function<void(const YYTK::RValue& In, const NlCore::PathStep& Step, const std::string& Key, int Level, int Slot, const YYTK::RValue& Value)>;
	// 걷는 함수: 대상마다 Visit 을 부른다. 자료를 열지 못하면 거짓이고 Why 에 까닭.
	using WalkFn = bool (*)(const Visit& V, std::string& Why);

	struct JobDef
	{
		const char* Cheat;			// 치트 표의 Id
		const char* What;			// 로그에 쓸 이름
		bool Zero;					// 켜면 0 을 쓴다(배율이 없다). 아니면 창에서 정한 배율을 곱한다
		WalkFn Walk;
		void (*After)();			// 값을 쓴 뒤에 부른다(없으면 nullptr)
		double Period;				// 다 쓴 뒤 다시 훑는 간격(초). 게임이 자료를 다시 만들면 그때 다시 쓴다
		bool AnyScreen = false;		// 게임 화면이 아니어도(메인 메뉴) 한다. 게임이 켜질 때 만들어지는 자료(건물 종류)를 세이브를 불러오기 전에 써 둘 때
	};

	void Init(LogFn Log);
	// 일을 등록한다(ModuleInitialize 의 Init 들에서. 틱의 차례는 등록한 차례다). 같은 Cheat 를 두 번 등록하면 로그에 적고 무시한다.
	void Add(const JobDef& Def);
	// 게임 스레드의 틱. Now: 모듈이 뜬 뒤의 초(1초에 한 번 모든 일을 본다).
	void GameTick(double Now);

	// 게임 변수(global.__gameplay_vars)의 열쇠들을 넘긴다. Key 는 그 열쇠다. 없는 열쇠는 건너뛴다(한 번 로그). 영역의 걷는 함수가 쓴다.
	bool WalkVars(const Visit& V, const std::vector<const char*>& Keys, std::string& Why);
	// 영역의 걷는 함수가 로그를 남길 때.
	void Log(const std::string& Line);
}
