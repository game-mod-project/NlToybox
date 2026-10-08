#pragma once
// 코어 시험의 공용 뼈대: CHECK 매크로, Test, 센 수. 묶음마다 파일 하나(test_<묶음>.cpp)의 Run<묶음>Tests 가 있고 main.cpp 가 차례로 부른다.
// 한 파일 3,500줄이던 것을 코어 헤더의 묶음마다 나눴다(2026-10-07 리뷰 R19). 게임을 켜지 않는다.

#include "core/AskPath.hpp"
#include "core/JobTally.hpp"
#include "core/BattlePlan.hpp"
#include "core/Binding.hpp"
#include "core/BuildingPlan.hpp"
#include "core/CallLog.hpp"
#include "core/CheatState.hpp"
#include "core/CheatTable.hpp"
#include "core/CostBook.hpp"
#include "core/CourtPlan.hpp"
#include "core/CrimePlan.hpp"
#include "core/DiplomacyPlan.hpp"
#include "core/EconomyPlan.hpp"
#include "core/EventPlan.hpp"
#include "core/Guard.hpp"
#include "core/Hooks.hpp"
#include "core/Knobs.hpp"
#include "core/Localization.hpp"
#include "core/NumberEdit.hpp"
#include "core/PathTable.hpp"
#include "core/PeoplePlan.hpp"
#include "core/Presets.hpp"
#include "core/TimeAsk.hpp"
#include "core/WorldPlan.hpp"
#include "core/Rate.hpp"
#include "core/RemoteCommand.hpp"
#include "core/Request.hpp"
#include "core/FamilyPlan.hpp"
#include "core/Retry.hpp"
#include "core/RolePlan.hpp"
#include "core/Schedule.hpp"
#include "core/SeasonPlan.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace NlCore;

extern int g_Failed;
extern int g_Passed;
extern std::string g_ProbesDir;		// tools/probes (요청 파일의 시험이 읽는다)

void Check(bool Ok, const char* What, int Line);
void CheckStr(const std::string& Got, const std::string& Want, const char* What, int Line);

#define CHECK(cond) Check((cond), #cond, __LINE__)
#define CHECK_STR(got, want) CheckStr((got), (want), #got, __LINE__)

// 시험 하나: 이름과 몸. "ok - 이름" / "not ok - 이름" 을 찍는다.
void Test(const char* Name, void (*Body)());

// 묶음마다의 시험(파일 이름과 같다).
void RunTextTests();
void RunRequestTests();
void RunScheduleTests();
void RunAskPathTests();
void RunKnobsTests();
void RunEconomyTests();
void RunCheatTests();
void RunCostBookTests();
void RunBuildingTests();
void RunHooksTests();
void RunRateTests();
void RunRemoteTests();
void RunPeopleTests();
void RunPresetsTests();
void RunWorldTests();
void RunDiplomacyTests();
void RunCourtTests();
void RunLocalizationTests();
void RunFamilyTests();
void RunRoleTests();
void RunCrimeTests();
void RunEventsTests();
