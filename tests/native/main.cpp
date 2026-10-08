// 러너에 기대지 않는 코어(src/core)의 시험. 게임을 켜지 않는다.
// 사용: nlcore_tests.exe <요청 파일 폴더>     (tools/test-native.ps1 이 부른다)
// 시험은 묶음마다 test_<묶음>.cpp 에 있다(common.hpp 의 Run<묶음>Tests). 여기는 센 수와 뼈대뿐이다.

#include "common.hpp"

int g_Failed = 0;
int g_Passed = 0;
std::string g_ProbesDir;

void Check(bool Ok, const char* What, int Line)
{
	if (Ok)
		return;
	std::printf("  FAIL line %d: %s\n", Line, What);
	g_Failed++;
}

void CheckStr(const std::string& Got, const std::string& Want, const char* What, int Line)
{
	if (Got == Want)
		return;
	std::printf("  FAIL line %d: %s\n    got  <%s>\n    want <%s>\n", Line, What, Got.c_str(), Want.c_str());
	g_Failed++;
}

void Test(const char* Name, void (*Body)())
{
	const int before = g_Failed;
	Body();
	std::printf("%s - %s\n", g_Failed == before ? "ok" : "not ok", Name);
	if (g_Failed == before)
		g_Passed++;
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::printf("usage: nlcore_tests <probes dir>\n");
		return 2;
	}
	g_ProbesDir = argv[1];

	RunTextTests();
	RunRequestTests();
	RunScheduleTests();
	RunAskPathTests();
	RunKnobsTests();
	RunEconomyTests();
	RunCheatTests();
	RunCostBookTests();
	RunBuildingTests();
	RunHooksTests();
	RunRateTests();
	RunRemoteTests();
	RunPeopleTests();
	RunPresetsTests();
	RunWorldTests();
	RunDiplomacyTests();
	RunCourtTests();
	RunLocalizationTests();
	RunFamilyTests();
	RunRoleTests();
	RunCrimeTests();
	RunEventsTests();

	if (g_Failed)
	{
		std::printf("core tests: %d FAILED\n", g_Failed);
		return 1;
	}
	std::printf("core tests: %d passed\n", g_Passed);
	return 0;
}
