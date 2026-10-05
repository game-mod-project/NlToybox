// 러너에 기대지 않는 코어(src/core)의 시험. 게임을 켜지 않는다.
// 사용: nlcore_tests.exe <요청 파일 폴더>     (tools/test-native.ps1 이 부른다)

#include "core/AskPath.hpp"
#include "core/Binding.hpp"
#include "core/CallLog.hpp"
#include "core/CheatState.hpp"
#include "core/CheatTable.hpp"
#include "core/CostBook.hpp"
#include "core/EconomyPlan.hpp"
#include "core/Hooks.hpp"
#include "core/Knobs.hpp"
#include "core/PathTable.hpp"
#include "core/PeoplePlan.hpp"
#include "core/Presets.hpp"
#include "core/WorldPlan.hpp"
#include "core/Rate.hpp"
#include "core/RemoteCommand.hpp"
#include "core/Request.hpp"
#include "core/Retry.hpp"
#include "core/Schedule.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace NlCore;

namespace
{
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

#define CHECK(cond) Check((cond), #cond, __LINE__)
#define CHECK_STR(got, want) CheckStr((got), (want), #got, __LINE__)

	void Test(const char* Name, void (*Body)())
	{
		const int before = g_Failed;
		Body();
		std::printf("%s - %s\n", g_Failed == before ? "ok" : "not ok", Name);
		if (g_Failed == before)
			g_Passed++;
	}

	Request Parse(const std::string& Text)
	{
		std::istringstream in(Text);
		return ParseRequest(in);
	}

}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::printf("usage: nlcore_tests <probes dir>\n");
		return 2;
	}
	g_ProbesDir = argv[1];

	Test("Trim 은 양끝의 공백과 줄바꿈을 뗀다", [] {
		CHECK_STR(Trim("  a b \r\n"), "a b");
		CHECK_STR(Trim(" \t "), "");
	});

	Test("Quote 는 따옴표, 역슬래시, 제어 문자를 JSON 으로 쓴다", [] {
		CHECK_STR(Quote("a\"b\\c\n\x01"), "\"a\\\"b\\\\c\\n\\u0001\"");
	});

	Test("Quote 는 긴 글을 UTF-8 글자 경계에서 자른다", [] {
		// 3바이트 글자 넷(12바이트). 7바이트에서 자르면 두 글자(6바이트)만 남아야 한다.
		const std::string text = "\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80";
		CHECK_STR(Quote(text, 7), "\"\xEA\xB0\x80\xEA\xB0\x80...\"");
	});

	Test("Quote 는 잘못된 UTF-8 바이트를 물음표로 바꾼다", [] {
		CHECK_STR(Quote("a\xFF" "b"), "\"a?b\"");
	});

	Test("Number 는 정수는 정수로, 유한하지 않은 수는 글로 쓴다", [] {
		CHECK_STR(Number(2345), "2345");
		CHECK_STR(Number(0.5), "0.5");
		CHECK_STR(Number(std::nan("")), "\"nan\"");
		CHECK_STR(Number(std::numeric_limits<double>::infinity()), "\"inf\"");
		CHECK_STR(Number(-std::numeric_limits<double>::infinity()), "\"-inf\"");
	});

	Test("Fixed 는 소수 자릿수를 맞춘다", [] {
		CHECK_STR(Fixed(12.345, 1), "12.3");
		CHECK_STR(Fixed(7, 1), "7.0");
	});

	Test("ParseNumber 는 글 전체가 수일 때만 받는다", [] {
		double value = 0;
		CHECK(ParseNumber("60", value) && value == 60);
		CHECK(ParseNumber("-1.5", value) && value == -1.5);
		CHECK(!ParseNumber("60s", value));
		CHECK(!ParseNumber("", value));
		CHECK(!ParseNumber("abc", value));
	});

	Test("Contains 는 부분 일치를 본다. 빈 낱말은 아무것에도 맞지 않는다", [] {
		CHECK(Contains("battle_battle_dodge_base", "dodge_base"));
		CHECK(!Contains("dodge", "dodge_base"));
		CHECK(!Contains("abc", ""));
	});

	Test("요청: 줄이 없으면 기본값이다", [] {
		const Request r = Parse("# 주석\n\n");
		CHECK(r.Errors.empty());
		CHECK(r.DelaySeconds == 60 && r.RepeatSeconds == 0 && r.KeepLast == 3 && !r.TraceEvents);
		CHECK(r.FindValues.empty() && r.FindNames.empty() && r.Watches.empty() && r.Skip.empty() && r.Scripts.empty());
	});

	Test("요청: 모든 키를 읽는다", [] {
		const Request r = Parse(
			"delay_seconds=45\r\n"
			"repeat_seconds=30\n"
			"keep_last=2\n"
			"trace_events=1\n"
			"skip=ds\n"
			"skip=room\n"
			"find=2345\n"
			"find_name= dodge_base \n"
			"watch=global.a.b\n");
		CHECK(r.Errors.empty());
		CHECK(r.DelaySeconds == 45 && r.RepeatSeconds == 30 && r.KeepLast == 2 && r.TraceEvents);
		CHECK(r.Skip.size() == 2 && r.Skip[0] == "ds" && r.Skip[1] == "room");
		CHECK(r.FindValues.size() == 1 && r.FindValues[0] == 2345);
		CHECK(r.FindNames.size() == 1 && r.FindNames[0] == "dodge_base");
		CHECK(r.Watches.size() == 1 && r.Watches[0] == "global.a.b");

		const Request s = Parse("script=gml_Script_x|1|wood\n");
		CHECK(s.Errors.empty());
		CHECK(s.Scripts.size() == 1 && s.Scripts[0].Name == "gml_Script_x" && s.Scripts[0].Args.size() == 2 && s.Scripts[0].Args[1] == "wood");
	});

	Test("요청: 모르는 키와 잘못된 값은 오류로 남는다", [] {
		CHECK(Parse("dealy_seconds=60\n").Errors.size() == 1);		// 오타
		CHECK(Parse("delay_seconds=abc\n").Errors.size() == 1);
		CHECK(Parse("delay_seconds=-1\n").Errors.size() == 1);
		CHECK(Parse("repeat_seconds=5\n").Errors.size() == 1);		// 너무 짧다. 덤프가 이어 붙는다
		CHECK(Parse("keep_last=0\n").Errors.size() == 1);
		CHECK(Parse("keep_last=10\n").Errors.size() == 1);
		CHECK(Parse("keep_last=1.5\n").Errors.size() == 1);
		CHECK(Parse("trace_events=yes\n").Errors.size() == 1);
		CHECK(Parse("find=wood\n").Errors.size() == 1);
		CHECK(Parse("find_name=\n").Errors.size() == 1);
		CHECK(Parse("watch=o_main_menu.x\n").Errors.size() == 1);	// global. 로 시작해야 한다
		CHECK(Parse("skip=everything\n").Errors.size() == 1);
		CHECK(Parse("script=|1\n").Errors.size() == 1);
		CHECK(Parse("no equals sign\n").Errors.size() == 1);
		// 되풀이 덤프와 스크립트 호출은 함께 쓸 수 없다(같은 스크립트를 여러 번 부르지 않는다).
		CHECK(Parse("repeat_seconds=45\nscript=gml_Script_x\n").Errors.size() == 1);
	});

	Test("요청: 찾기의 한도는 기본값이 있고 요청으로 바꿀 수 있다", [] {
		const Request d = Parse("");
		CHECK(d.Bounds.MaxDepth == 6 && d.Bounds.MaxVisited == 2000000 && d.Bounds.MaxArray == 64 && d.Bounds.MaxHits == 2000);
		CHECK(d.Bounds.MaxDsKeys == 20000 && d.Bounds.MaxDsId == 100000 && d.Bounds.MaxInstances == 16);

		const Request r = Parse(
			"max_depth=8\n"
			"max_visited=5000000\n"
			"max_array=256\n"
			"max_hits=5000\n"
			"max_ds_keys=50000\n"
			"max_ds_id=300000\n"
			"max_instances=64\n");
		CHECK(r.Errors.empty());
		CHECK(r.Bounds.MaxDepth == 8 && r.Bounds.MaxVisited == 5000000 && r.Bounds.MaxArray == 256 && r.Bounds.MaxHits == 5000);
		CHECK(r.Bounds.MaxDsKeys == 50000 && r.Bounds.MaxDsId == 300000 && r.Bounds.MaxInstances == 64);

		CHECK(Parse("max_depth=0\n").Errors.size() == 1);
		CHECK(Parse("max_depth=2.5\n").Errors.size() == 1);
		CHECK(Parse("max_hits=abc\n").Errors.size() == 1);
		CHECK(Parse("max_ds_id=-5\n").Errors.size() == 1);
		CHECK(Parse("max_visited=10\n").Errors.size() == 1);		// 너무 작다
	});

	Test("일정: 첫 덤프는 delay 뒤의 menu 다", [] {
		Schedule s(60, 0, 3);
		CHECK_STR(s.Due(59.9), "");
		CHECK_STR(s.Due(60), "menu");
		CHECK(s.Count() == 0 && !s.Exhausted());
	});

	Test("일정: 되풀이하지 않으면 첫 덤프 뒤에는 없다", [] {
		Schedule s(60, 0, 3);
		s.Finished(62);
		CHECK(s.Exhausted() && !s.Repeats() && s.Count() == 1);
		CHECK_STR(s.Due(100000), "");
	});

	Test("일정: 되풀이는 앞 덤프가 끝난 때부터 센다", [] {
		Schedule s(60, 45, 3);
		s.Finished(70);		// menu 덤프가 10초 걸렸다
		CHECK(!s.Exhausted() && s.Repeats());
		CHECK_STR(s.Due(114.9), "");
		CHECK_STR(s.Due(115), "late0");
	});

	Test("일정: 되풀이 덤프는 마지막 keep_last 개의 이름을 돌려 쓴다", [] {
		Schedule s(60, 45, 3);
		s.Finished(60);		// menu
		CHECK_STR(s.Due(105), "late0");
		s.Finished(105);
		CHECK_STR(s.Due(150), "late1");
		s.Finished(150);
		CHECK_STR(s.Due(195), "late2");
		s.Finished(195);
		CHECK_STR(s.Due(240), "late0");
		s.Finished(240);
		CHECK(s.Count() == 5);
	});

	Test("PathTable 은 뿌리부터 이어 붙인 경로를 돌려준다", [] {
		PathTable paths;
		const int root = paths.Add(-1, "global");
		const int a = paths.Add(root, ".a");
		const int item = paths.Add(a, "[3]");
		CHECK_STR(paths.Path(item), "global.a[3]");
		CHECK_STR(paths.Path(root), "global");
		CHECK_STR(paths.Path(-1), "");
	});

	Test("배율: 정수였던 값은 정수로 맞추고 반은 올린다", [] {
		CHECK(Scale(30, 0.5) == 15);
		CHECK(Scale(5, 0.5) == 3);			// 2.5 는 3
		CHECK(Scale(15, 0.5) == 8);			// 7.5 는 8
		CHECK(Scale(300, 3) == 900);
		CHECK(Scale(10, 1) == 10);
		CHECK(Scale(-6, 2) == -12);
	});

	Test("배율: 0 보다 큰 정수는 0 이 되지 않는다", [] {
		CHECK(Scale(5, 0.05) == 1);
		CHECK(Scale(1, 0.1) == 1);
		CHECK(Scale(0, 3) == 0);			// 원래 0 인 값은 0 이다
	});

	Test("배율: 소수였던 값은 그대로 곱한다", [] {
		CHECK(Scale(0.5, 3) == 1.5);
		CHECK(Scale(2.5, 0.5) == 1.25);
	});

	Test("배율은 허용 범위 안으로 당긴다", [] {
		CHECK(ClampFactor(0) == k_MinFactor);
		CHECK(ClampFactor(-3) == k_MinFactor);
		CHECK(ClampFactor(1000) == k_MaxFactor);
		CHECK(ClampFactor(0.5) == 0.5);
		CHECK(ClampFactor(std::nan("")) == 1);
	});

	Test("설정 파일: 쓴 것을 그대로 읽고, 읽을 수 없는 줄은 버린다", [] {
		std::istringstream in("# 주석\nbuilding_cost=0.5\n start_resources = 3 \nbroken\nbad=abc\nhuge=999\n=1\n");
		const std::map<std::string, double> values = ParseSettings(in);
		CHECK(values.size() == 3);
		CHECK(values.at("building_cost") == 0.5 && values.at("start_resources") == 3 && values.at("huge") == k_MaxFactor);

		std::istringstream again(FormatSettings(values));
		CHECK(ParseSettings(again) == values);
	});

	Test("묻는 경로: 뿌리와 단계를 읽는다", [] {
		const AskPath g = ParseAskPath("global.a.b[3].c");
		CHECK(g.Error.empty() && g.Root == "global" && g.Steps.size() == 4);
		CHECK(g.Steps[0].Kind == '.' && g.Steps[0].Name == "a" && g.Steps[2].Kind == '[' && g.Steps[2].Index == 3 && g.Steps[3].Name == "c");

		const AskPath i = ParseAskPath("inst:o_building:1.generic");
		CHECK(i.Error.empty() && i.Root == "inst" && i.Object == "o_building" && i.Number == 1 && i.Steps.size() == 1);
		CHECK(ParseAskPath("inst:o_debug").Number == 0 && ParseAskPath("inst:o_debug").Steps.empty());

		const AskPath m = ParseAskPath("map:150@building_resources@woodcutter_lvl_1#0#1");
		CHECK(m.Error.empty() && m.Root == "map" && m.Number == 150 && m.Steps.size() == 4);
		CHECK(m.Steps[0].Kind == '@' && m.Steps[0].Name == "building_resources" && m.Steps[3].Kind == '#' && m.Steps[3].Index == 1);

		const AskPath k = ParseAskPath("map:44@{building.menu.x}.y");
		CHECK(k.Error.empty() && k.Steps.size() == 2 && k.Steps[0].Name == "building.menu.x" && k.Steps[1].Name == "y");
		CHECK(ParseAskPath("list:281#0").Root == "list");
	});

	Test("묻는 경로: 읽을 수 없으면 오류를 낸다", [] {
		CHECK(!ParseAskPath("").Error.empty());
		CHECK(!ParseAskPath("o_debug.x").Error.empty());			// 뿌리를 모른다
		CHECK(!ParseAskPath("inst:").Error.empty());
		CHECK(!ParseAskPath("inst:o_x:abc").Error.empty());
		CHECK(!ParseAskPath("map:abc").Error.empty());
		CHECK(!ParseAskPath("global.").Error.empty());				// 빈 이름
		CHECK(!ParseAskPath("global.a[x]").Error.empty());
		CHECK(!ParseAskPath("global.a[3").Error.empty());
		CHECK(!ParseAskPath("map:1@{open").Error.empty());
		CHECK(!ParseAskPath("global.a#-1").Error.empty());
	});

	Test("묻는 경로: 단계를 글로 쓰면 다시 읽힌다", [] {
		CHECK_STR(FormatStep({ '.', "a", 0 }), ".a");
		CHECK_STR(FormatStep({ '[', "", 12 }), "[12]");
		CHECK_STR(FormatStep({ '#', "", 3 }), "#3");
		CHECK_STR(FormatStep({ '@', "wood", 0 }), "@wood");
		CHECK_STR(FormatStep({ '@', "a.b[c]", 0 }), "@{a.b[c]}");
		const AskPath again = ParseAskPath("map:1" + FormatStep({ '@', "a.b[c]", 0 }) + FormatStep({ '#', "", 3 }));
		CHECK(again.Error.empty() && again.Steps.size() == 2 && again.Steps[0].Name == "a.b[c]" && again.Steps[1].Index == 3);
	});

	Test("묻는 경로: 전체를 글로 쓰면 다시 읽힌다", [] {
		for (const char* text : { "global", "global.a.b[3].c", "inst:o_debug.is_x", "inst:o_building:1.generic",
			"map:150@building_resources@woodcutter_lvl_1#0#1", "map:44@{building.menu.x}.y", "list:281#0" })
			CHECK_STR(FormatAskPath(ParseAskPath(text)), text);
		CHECK_STR(FormatAskPath(ParseAskPath("inst:o_debug:0.x")), "inst:o_debug.x");		// 첫 인스턴스는 번호를 적지 않는다
		CHECK_STR(FormatAskPath(ParseAskPath("o_debug.x")), "");							// 읽지 못한 경로
	});

	Test("묻는 경로: 부모와 자식", [] {
		const AskPath path = ParseAskPath("inst:o_debug.a[2]");
		CHECK_STR(FormatAskPath(ParentPath(path)), "inst:o_debug.a");
		CHECK_STR(FormatAskPath(ParentPath(ParentPath(ParentPath(path)))), "inst:o_debug");	// 단계가 없으면 그대로다
		CHECK_STR(FormatAskPath(ChildPath(ParentPath(path), { '@', "k.x", 0 })), "inst:o_debug.a@{k.x}");
		CHECK_STR(FormatAskPath(ChildPath(ParseAskPath("global"), { '.', "v", 0 })), "global.v");
	});

	Test("Shortest 는 다시 읽으면 같은 수가 되는 가장 짧은 글을 쓴다", [] {
		CHECK_STR(Shortest(0.83), "0.83");
		CHECK_STR(Shortest(3), "3");
		CHECK_STR(Shortest(-4), "-4");
		CHECK_STR(Shortest(0.1 + 0.2), "0.30000000000000004");
		CHECK_STR(Shortest(std::numeric_limits<double>::quiet_NaN()), "nan");
		CHECK_STR(Shortest(-std::numeric_limits<double>::infinity()), "-inf");
		for (const double value : { 36550.20449999981, 1e20, -0.000123, 57.4 })
		{
			double again = 0;
			CHECK(ParseNumber(Shortest(value), again) && again == value);
		}
	});

	Test("Thousands 는 정수로 맞춰 세 자리마다 쉼표를 넣는다", [] {
		// Shortest 는 큰 수를 지수로 쓴다(100000 → "1e+05"). 창에 보이는 금화와 자원의 수는 이것으로 쓴다.
		CHECK_STR(Thousands(0), "0");
		CHECK_STR(Thousands(999), "999");
		CHECK_STR(Thousands(1000), "1,000");
		CHECK_STR(Thousands(3600.4), "3,600");
		CHECK_STR(Thousands(100000), "100,000");
		CHECK_STR(Thousands(1e6), "1,000,000");
		CHECK_STR(Thousands(-1250000), "-1,250,000");
		CHECK_STR(Thousands(-0.2), "0");
		CHECK_STR(Thousands(std::numeric_limits<double>::infinity()), "inf");
	});

	Test("ScriptRoutineName 은 접두 없는 이름에 gml_Script_ 를 붙인다", [] {
		// 접두 없는 이름은 러너에서 다른 루틴을 가리킨다(research/07). 부르거나 훅을 걸 이름은 하나뿐이어야 한다.
		CHECK_STR(ScriptRoutineName("budget_money_get"), "gml_Script_budget_money_get");
		CHECK_STR(ScriptRoutineName("gml_Script_budget_money_get"), "gml_Script_budget_money_get");
		CHECK_STR(ScriptRoutineName("  budget_money_get "), "gml_Script_budget_money_get");
		CHECK_STR(ScriptRoutineName("gml_Script_"), "");
		CHECK_STR(ScriptRoutineName(""), "");
		CHECK_STR(ScriptRoutineName("a b"), "");
	});

	Test("경제: 자원의 이름과 갈래의 이름", [] {
		CHECK_STR(ResourceKey("resource.wood"), "wood");
		CHECK_STR(ResourceKey("wood"), "wood");
		CHECK_STR(ResourceKey(""), "");
		CHECK_STR(ResourceLabel("wood"), "나무 (wood)");
		CHECK_STR(ResourceLabel("something_new"), "something_new");
		CHECK_STR(CategoryLabel("food"), "음식");
		CHECK_STR(CategoryLabel("resources"), "자원");		// 게임의 화면이 쓰는 말(research/08 의 화면)
		CHECK_STR(CategoryLabel("herbs"), "식물");
		CHECK_STR(CategoryLabel("unknown"), "unknown");
	});

	Test("경제: 명령을 변화량으로 푼다", [] {
		const std::vector<double> counts = { 0, 300, 0, 5 };
		const std::vector<int> stocked = { 1, 2, 3 };
		const auto one = [&](EconomyAct act, int resource, double amount, double gold = 3000) {
			return PlanEconomy({ act, resource, amount }, gold, counts, counts, stocked);
		};
		// 금화: 더하기는 그대로, 맞추기는 차이만큼
		const auto add = one(EconomyAct::GoldAdd, -1, 1000);
		CHECK(add.size() == 1 && add[0].Resource == -1 && add[0].Delta == 1000);
		CHECK(one(EconomyAct::GoldSet, -1, 5000)[0].Delta == 2000 && one(EconomyAct::GoldSet, -1, 100)[0].Delta == -2900);
		CHECK(one(EconomyAct::GoldSet, -1, 3000).empty());					// 이미 그 수다
		CHECK(one(EconomyAct::GoldSet, -1, -50)[0].Delta == -3000);			// 음수로 맞추지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, -5000)[0].Delta == -3000);		// 0 아래로 내리지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, -1000, 0).empty());				// 0 에서 더 내리지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, -10, -50).empty());				// 이미 음수면 더 내리지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, 100, -50)[0].Delta == 100);
		CHECK(one(EconomyAct::GoldAdd, -1, 0.4).empty() && one(EconomyAct::GoldAdd, -1, 1.6)[0].Delta == 2);	// 정수로
		// 자원
		const auto wood = one(EconomyAct::ResourceAdd, 1, 50);
		CHECK(wood.size() == 1 && wood[0].Resource == 1 && wood[0].Delta == 50);
		CHECK(one(EconomyAct::ResourceAdd, 3, -100)[0].Delta == -5);
		CHECK(one(EconomyAct::ResourceSet, 1, 120)[0].Delta == -180 && one(EconomyAct::ResourceSet, 2, 7)[0].Delta == 7);
		CHECK(one(EconomyAct::ResourceAdd, 4, 10).empty() && one(EconomyAct::ResourceAdd, -1, 10).empty() && one(EconomyAct::ResourceSet, 99, 10).empty());
		// 모든 자원: 갈래에 든 것만(0번은 들지 않았다), 변화가 있는 것만
		const auto all = one(EconomyAct::AllAdd, -1, 100);
		CHECK(all.size() == 3 && all[0].Resource == 1 && all[1].Resource == 2 && all[1].Delta == 100 && all[2].Resource == 3);
		const auto less = one(EconomyAct::AllAdd, -1, -5);
		CHECK(less.size() == 2 && less[0].Resource == 1 && less[0].Delta == -5 && less[1].Resource == 3 && less[1].Delta == -5);
		// 수가 아니거나 터무니없으면 아무것도 하지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, std::numeric_limits<double>::quiet_NaN()).empty());
		CHECK(one(EconomyAct::GoldAdd, -1, std::numeric_limits<double>::infinity()).empty() && one(EconomyAct::AllAdd, -1, 1e12).empty());

		// 지금 수가 정수가 아니어도 넘기는 변화량은 정수다. 줄여도 0 아래로 가지 않는다.
		CHECK(one(EconomyAct::GoldSet, -1, 5000, 3000.5)[0].Delta == 2000);
		const std::vector<double> half = { 0, 5.5 };
		CHECK(PlanEconomy({ EconomyAct::ResourceAdd, 1, -10 }, 0, half, half, { 1 })[0].Delta == -5);
		CHECK(PlanEconomy({ EconomyAct::ResourceSet, 1, 0 }, 0, half, half, { 1 })[0].Delta == -5);
		// 읽은 수가 수가 아니면 부르지 않는다(NaN 을 게임의 함수에 넘기지 않는다).
		const double nan = std::numeric_limits<double>::quiet_NaN();
		CHECK(one(EconomyAct::GoldAdd, -1, 100, nan).empty() && one(EconomyAct::GoldSet, -1, 100, nan).empty());
		CHECK(PlanEconomy({ EconomyAct::ResourceAdd, 1, 10 }, 0, { 0, nan }, { 0, nan }, { 1 }).empty());
		// 맞추기의 차이가 터무니없이 크면 하지 않는다.
		CHECK(one(EconomyAct::GoldSet, -1, 0, 3e9).empty());
		// 갈래에 없는 자원(0번 rune)은 하나씩도 건드리지 않는다: 창고의 change 가 그런 자원에 무엇을 하는지 잰 적이 없다.
		CHECK(one(EconomyAct::ResourceAdd, 0, 5).empty() && one(EconomyAct::ResourceSet, 0, 5).empty());
		// 예약된 몫은 줄이지 않는다: 줄일 수 있는 양은 예약되지 않은 수까지다. 더하는 것은 그대로다.
		const std::vector<double> total = { 0, 300 }, unreserved = { 0, 250 };
		CHECK(PlanEconomy({ EconomyAct::ResourceSet, 1, 0 }, 0, total, unreserved, { 1 })[0].Delta == -250);
		CHECK(PlanEconomy({ EconomyAct::ResourceAdd, 1, -1000 }, 0, total, unreserved, { 1 })[0].Delta == -250);
		CHECK(PlanEconomy({ EconomyAct::ResourceAdd, 1, 10 }, 0, total, unreserved, { 1 })[0].Delta == 10);
		// 같은 번호가 두 번 들어 있어도 한 번만 한다. 범위 밖 번호는 건너뛴다.
		CHECK(PlanEconomy({ EconomyAct::AllAdd, -1, 5 }, 0, total, total, { 1, 1, 7 }).size() == 1);
	});

	Test("경제: 앞뒤의 수로 다 들어가지 않은 것을 가린다", [] {
		// 함수의 반환값이 "적용된 양"인지는 모른다(본 반환은 둘 다 청한 수와 같았다). 앞뒤의 수를 견준다.
		const std::vector<EconomyChange> done = { { -1, 1000 }, { 1, 50 }, { 2, 10 }, { 9, 5 } };
		const auto shorts = EconomyShortfall(done, 3000, { 0, 300, 0 }, 4000, { 0, 300, 10 });
		CHECK(shorts.size() == 2 && shorts[0].Resource == 1 && shorts[0].Asked == 50 && shorts[0].Applied == 0);
		CHECK(shorts[1].Resource == 9);		// 뒤의 수를 읽지 못한 것을 다 들어갔다고 하지 않는다
		const auto gold = EconomyShortfall({ { -1, 1000 } }, 3000, {}, 3600, {});
		CHECK(gold.size() == 1 && gold[0].Resource == -1 && gold[0].Applied == 600);
		CHECK(EconomyShortfall({ { -1, -500 }, { 1, -20 } }, 3000, { 0, 300 }, 2500, { 0, 280 }).empty());
	});

	Test("경제: 원격 명령의 낱말", [] {
		EconomyAct act = EconomyAct::GoldAdd;
		CHECK(ParseEconomyAct("gold_set", act) && act == EconomyAct::GoldSet && !NeedsResource(act));
		CHECK(ParseEconomyAct("add", act) && act == EconomyAct::ResourceAdd && NeedsResource(act));
		CHECK(ParseEconomyAct("set", act) && act == EconomyAct::ResourceSet && NeedsResource(act));
		CHECK(ParseEconomyAct("all", act) && act == EconomyAct::AllAdd && !NeedsResource(act));
		CHECK(ParseEconomyAct("gold_add", act) && act == EconomyAct::GoldAdd && !ParseEconomyAct("gold", act) && !ParseEconomyAct("", act));
	});


	Test("건설비 장부: 처음 본 값을 기억하고 0 은 기억하지 않는다", [] {
		CostBook book;
		CHECK(book.Empty());
		book.Remember("altar", 1, 1, 30);
		book.Remember("altar", 1, -1, 0);			// 0 인 자리는 바꿀 것이 없다
		book.Remember("hut_6x10", 2, 32, 5);
		book.Remember("altar", 1, 1, 0);			// 0 으로 쓴 뒤 다시 보면 0 이 보인다. 처음 본 30 을 지킨다
		book.Remember("altar", 1, 1, 99);			// 이미 기억한 자리는 바꾸지 않는다
		CHECK(book.Size() == 2 && !book.Empty());
		CHECK(book.Entries()[0].Building == "altar" && book.Entries()[0].Level == 1 && book.Entries()[0].Slot == 1 && book.Entries()[0].Value == 30);
		CHECK(book.Entries()[1].Building == "hut_6x10" && book.Entries()[1].Slot == 32 && book.Entries()[1].Value == 5);
		book.Remember("altar", 2, 1, 7);			// 등급이 다르면 다른 자리다
		book.Remember("altar", 1, -1, 12);			// 금화(-1)도 한 자리다
		CHECK(book.Size() == 4);
		book.Clear();
		CHECK(book.Empty() && book.Entries().empty());
		book.Remember("altar", 1, 1, std::numeric_limits<double>::quiet_NaN());		// 수가 아니면 기억하지 않는다
		CHECK(book.Empty());
	});

	Test("건설비 장부: 되돌릴 값이 있는 자리인지 알려 주고, 되돌린 자리만 잊는다", [] {
		CostBook book;
		CHECK(book.Remember("altar", 1, 1, 30));			// 새로 기억했다. 0 으로 써도 된다
		CHECK(book.Remember("altar", 1, 1, 0));				// 이미 기억한 자리다
		CHECK(!book.Remember("altar", 1, 2, 0));			// 0 인 자리: 쓸 것이 없다
		// 유한하지 않은 값은 되돌릴 수 없다. 그런 자리는 0 으로 쓰지도 않는다(건물 데이터에 inf 가 있다: __limit. research/09).
		CHECK(!book.Remember("altar", 1, 3, std::numeric_limits<double>::infinity()));
		CHECK(book.Remember("hut_6x10", 2, 32, 5) && book.Remember("hut_6x10", 2, 1, 10));
		CHECK(book.Size() == 3);
		// 배율을 곱할 때는 지금 값이 아니라 처음 본 값(바탕)에 곱한다.
		double base = 0;
		CHECK(book.Find("hut_6x10", 2, 32, base) && base == 5);
		size_t index = 99;
		CHECK(book.Find("hut_6x10", 2, 1, base, &index) && base == 10 && index == 2 && book.Entries()[index].Slot == 1);
		base = 5;
		CHECK(!book.Find("hut_6x10", 2, 33, base) && !book.Find("altar", 1, 2, base) && base == 5);

		book.Forget({ 1, 0, 1 });							// 되돌린 자리만 잊는다. 못 되돌린 자리는 남아 다음에 다시 되돌린다
		CHECK(book.Size() == 1 && book.Entries()[0].Building == "hut_6x10" && book.Entries()[0].Slot == 32 && book.Entries()[0].Value == 5);
		book.Forget({ 1, 1 });								// 수가 맞지 않으면 아무것도 잊지 않는다
		CHECK(book.Size() == 1);
		book.Forget({ 0 });
		CHECK(book.Size() == 1);
		book.Forget({ 1 });
		CHECK(book.Empty());
	});

	Test("자료의 배율: 처음 본 값에 곱하고, 다시 훑어도 두 번 곱하지 않는다", [] {
		CostBook book;
		double wanted = -1;
		size_t index = 99;
		CHECK(PlanValue(book, "storage.raw", 0, -1, 300, 10, false, wanted, index) && wanted == 3000 && index == 0);
		CHECK(PlanValue(book, "storage.raw", 0, -1, 3000, 10, false, wanted, index) && wanted == 3000);		// 써 둔 값을 다시 봤다. 쓸 것이 없다
		CHECK(PlanValue(book, "storage.raw", 0, -1, 3000, 2, false, wanted, index) && wanted == 600);			// 켠 채 배율을 바꿨다: 바탕에 곱한다
		CHECK(PlanValue(book, "storage.raw", 0, -1, 300, 2, false, wanted, index) && wanted == 600);			// 게임이 자료를 다시 만들었다(같은 열쇠, 원래 값)
		CHECK(PlanValue(book, "storage.raw", 0, -1, 600, 1, false, wanted, index) && wanted == 300 && index == 0);	// 끄면 바탕으로 되돌린다
		CHECK(book.Size() == 1);

		// 되돌리는 중에 처음 보는 자리는 건드린 적이 없다. 0 이거나 유한하지 않은 값은 장부가 받지 않는다(쓰지 않는다).
		CHECK(!PlanValue(book, "hall.raw", 0, -1, 300, 1, false, wanted, index));
		CHECK(!PlanValue(book, "default.raw", 0, -1, 0, 10, false, wanted, index));
		CHECK(!PlanValue(book, "x.y", 0, -1, std::numeric_limits<double>::infinity(), 10, false, wanted, index));
		CHECK(book.Size() == 1);

		// 0 으로 쓰는 항목(생산 재료 없음). 0 이 된 자리도 장부로 알아보고, 끄면 되살린다.
		CHECK(PlanValue(book, "workshop", 14, 4, 1, 0, true, wanted, index) && wanted == 0 && index == 1);
		CHECK(PlanValue(book, "workshop", 14, 4, 0, 0, true, wanted, index) && wanted == 0 && index == 1);
		CHECK(PlanValue(book, "workshop", 14, 4, 0, 1, true, wanted, index) && wanted == 1 && index == 1);

		// 정수는 정수로 남는다(만들어지는 수 1 에 2.5 를 곱하면 3. 0 이 되지 않는다).
		CHECK(PlanValue(book, "mine", 4, -1, 1, 2.5, false, wanted, index) && wanted == 3);
		CHECK(PlanValue(book, "mine", 4, -1, 3, 0.1, false, wanted, index) && wanted == 1);
	});

	Test("다시 해 보기: 실패가 이어지면 간격을 두 배씩 늘리고 성공하면 처음으로 돌아간다", [] {
		Retry retry(2, 60);
		CHECK(retry.Due(0) && retry.Failures() == 0);
		retry.Failed(10);
		CHECK(!retry.Due(11.9) && retry.Due(12) && retry.Failures() == 1);
		retry.Failed(12);
		CHECK(!retry.Due(15.9) && retry.Due(16) && retry.Failures() == 2);
		for (int i = 0; i < 10; i++)
			retry.Failed(100);
		CHECK(!retry.Due(159.9) && retry.Due(160));			// 가장 긴 간격(60초)을 넘지 않는다
		retry.Succeeded();
		CHECK(retry.Due(100) && retry.Failures() == 0);
		retry.Failed(200);
		CHECK(!retry.Due(201.9) && retry.Due(202));			// 다시 처음 간격부터
	});

	Test("훅 항목: 켠 것과 실제로 걸린 것이 어긋나면 다시 건다", [] {
		CHECK(ChooseHookStep(true, false, false) == HookStep::Apply);
		CHECK(ChooseHookStep(true, true, true) == HookStep::None);
		CHECK(ChooseHookStep(true, true, false) == HookStep::Apply);		// 다른 곳(원격 unoverride all)이 껐다. 체크가 켜져 있으면 다시 건다
		CHECK(ChooseHookStep(true, false, true) == HookStep::Apply);		// 원격이 걸어 둔 것을 이 항목의 값으로 맞춘다
		CHECK(ChooseHookStep(false, true, true) == HookStep::Remove);
		CHECK(ChooseHookStep(false, true, false) == HookStep::Remove);		// 걸었다는 표시를 지운다
		CHECK(ChooseHookStep(false, false, false) == HookStep::None);
		CHECK(ChooseHookStep(false, false, true) == HookStep::None);		// 이 항목이 걸지 않은 바꾸기(원격 override)는 건드리지 않는다
	});

	Test("훅 항목: 배율이 바뀌면 다시 걸고, 다시 걸다 실패해도 앞서 건 것을 끌 수 있다", [] {
		CHECK(HookCurrent(true, true, false, 0, 0));					// 고정값 훅: 걸었으면 그대로다
		CHECK(HookCurrent(true, true, true, 0.5, 0.5));
		CHECK(!HookCurrent(true, true, true, 0.5, 0.3));				// 배율이 바뀌었다. 다시 건다
		CHECK(!HookCurrent(true, false, true, 0.5, 0.5));
		CHECK(HookCurrent(false, true, true, 0.5, 0.3));				// 끄는 중에는 배율을 견주지 않는다(걸어 둔 것을 끈다)

		// 0.5 로 걸어 둔 채 0.3 으로 바꿨는데 다시 걸기에 실패했다(메뉴로 나가 주소가 풀리지 않는다). 앞서 건 바꾸기는 살아 있다.
		bool applied = true;
		const bool live = true;
		CHECK(ChooseHookStep(true, HookCurrent(true, applied, true, 0.5, 0.3), live) == HookStep::Apply);
		applied = AppliedAfterFailure(live);
		CHECK(applied);		// "걸었다"로 남는다. 그래야 끌 때 그 바꾸기를 이름으로 끈다(주인 없는 바꾸기가 남지 않는다)
		CHECK(ChooseHookStep(false, HookCurrent(false, applied, true, 0.5, 0.3), live) == HookStep::Remove);
		CHECK(!AppliedAfterFailure(false));								// 살아 있는 것이 없으면 걸지 않은 것이다
	});

	Test("훅의 배율: 원래 값에 곱하고, 정수로 남길지 고른다", [] {
		CHECK(ScaleResult(8, 0.1, true) == 1);					// 가격: 정수는 정수로 남고 0 이 되지 않는다
		CHECK(ScaleResult(100, 0.1, true) == 10 && ScaleResult(3600, 0.1, true) == 360 && ScaleResult(250, 100, true) == 25000);
		CHECK(ScaleResult(1, 5, false) == 5 && ScaleResult(1.2, 5, false) == 6);
		CHECK(ScaleResult(1, 0.5, false) == 0.5 && ScaleResult(1, 0.5, true) == 1);		// 계수는 그대로 곱한다. 정수로 남기면 1 아래로 내려가지 않는다
		// 0 이하의 값은 곱하지 않는다: "없음"을 0 이나 음수로 돌려주는 함수의 표식을 깨지 않는다.
		CHECK(ScaleResult(0, 10, true) == 0 && ScaleResult(-4, 2, true) == -4 && ScaleResult(-1, 0.5, false) == -1);
		const double nan = std::numeric_limits<double>::quiet_NaN();
		CHECK(ScaleResult(5, nan, true) == 5 && ScaleResult(5, 0, true) == 5 && ScaleResult(5, -1, false) == 5);		// 쓸 수 없는 배율이면 원래 값 그대로
		CHECK(ScaleResult(5, std::numeric_limits<double>::infinity(), false) == 5);
	});

	Test("훅의 자리: 걸린 대상은 그 자리, 걸다 실패한 대상에는 새 자리를 주지 않는다", [] {
		const int a = 0, b = 0, c = 0, d = 0;
		std::vector<HookSlot> slots(3);
		int index = -9;
		CHECK(PickHookSlot(slots, &a, index) == SlotPick::Free && index == 0);
		slots[0] = { true, false, &a };
		slots[1] = { true, true, &b };						// 훅을 걸다 실패했다
		CHECK(PickHookSlot(slots, &a, index) == SlotPick::Existing && index == 0);
		// 같은 대상을 0.5초마다 다시 걸면 자리 64개가 32초에 없어진다. 실패한 대상은 그 실행에서 다시 걸지 않는다.
		CHECK(PickHookSlot(slots, &b, index) == SlotPick::Failed && index == 1);
		CHECK(PickHookSlot(slots, &c, index) == SlotPick::Free && index == 2);
		slots[2] = { true, false, &c };
		CHECK(PickHookSlot(slots, &d, index) == SlotPick::Full && index == -1);
		CHECK(PickHookSlot(slots, &c, index) == SlotPick::Existing && index == 2);
	});

	Test("치트 상태: 읽고 쓰면 같다", [] {
		CheatState state;
		state.On = { "instant_build", "no_dodge" };
		state.Numbers = { { "rest_decrease", 0 }, { "piety_decrease", 0.83 } };
		state.Pins = { "inst:o_time_controller.time_warp", "map:128@{messenger_cost }" };
		state.Locks = { { "inst:o_character:2.starving_hours", 0 }, { "global.a.b[3]", -4.5 } };

		const std::string text = FormatCheatState(state);
		std::istringstream in(text);
		const CheatState again = ParseCheatState(in);
		CHECK(again.On == state.On && again.Numbers == state.Numbers && again.Pins == state.Pins);
		CHECK(again.Locks.size() == 2 && again.Locks[0].Path == "inst:o_character:2.starving_hours" && again.Locks[0].Value == 0
			&& again.Locks[1].Path == "global.a.b[3]" && again.Locks[1].Value == -4.5);
		CHECK_STR(FormatCheatState(again), text);
	});

	Test("치트 상태: 읽을 수 없는 줄은 버린다", [] {
		std::istringstream in(
			"# 주석\n"
			"\n"
			"on\n"								// 이름이 없다
			"on a b\n"							// 이름에 공백
			"num x=abc\n"						// 수가 아니다
			"num x=nan\n"						// 유한하지 않다
			"num =3\n"							// 이름이 없다
			"pin o_debug.x\n"					// 뿌리를 모른다
			"pin global\n"						// 단계가 없다
			"lock inst:o_debug.x\n"				// 값이 없다
			"lock inst:o_debug.x=1e999\n"		// 유한하지 않다
			"what inst:o_debug.x=1\n"			// 모르는 낱말
			"  on  good  \n"
			"num n = 2.5\n"
			"pin inst:o_debug.x\n"
			"pin inst:o_debug.x\n"				// 같은 주소는 한 번만
			"lock map:1@{a=b}=7\n"				// 마지막 '=' 에서 가른다
			"lock map:1@{a=b}=8\n");			// 같은 주소는 뒤의 것이 이긴다
		const CheatState state = ParseCheatState(in);
		CHECK(state.On == std::set<std::string>{ "good" });
		CHECK(state.Numbers.size() == 1 && state.Numbers.at("n") == 2.5);
		CHECK(state.Pins == std::vector<std::string>{ "inst:o_debug.x" });
		CHECK(state.Locks.size() == 1 && state.Locks[0].Path == "map:1@{a=b}" && state.Locks[0].Value == 8);
	});

	Test("치트 표: 주소가 모두 읽히고 Id 가 겹치지 않는다", [] {
		std::set<std::string> ids;
		for (const Cheat& cheat : Cheats())
		{
			const AskPath path = ParseAskPath(cheat.Path);
			if ((!path.Error.empty() || path.Steps.empty()) && !(IsHook(cheat.Kind) && std::string(cheat.Path).rfind("gml_Script_", 0) == 0))
			{
				std::printf("  FAIL %s: %s (%s)\n", cheat.Id, cheat.Path, path.Error.c_str());
				g_Failed++;
			}
			// 표에는 ds 번호를 적지 않는다. 훅 항목은 이름 있는 스크립트를 이름으로 적을 수 있다(그 이름은 이 게임 버전의 것이다).
			const bool script = IsHook(cheat.Kind) && std::string(cheat.Path).rfind("gml_Script_", 0) == 0;
			CHECK(script || path.Root == "global" || path.Root == "inst");
			CHECK(ids.insert(cheat.Id).second);
			CHECK(std::string(cheat.Id).find_first_of(" =") == std::string::npos);
			CHECK(cheat.Label[0] != 0 && cheat.Help[0] != 0);
			if (cheat.Kind == CheatKind::HookScale || cheat.Kind == CheatKind::CustomScale)		// On: 창이 처음 내놓는 배율. 0 보다 크고 범위 안이다
				CHECK(cheat.Min > 0 && cheat.On >= cheat.Min && cheat.On <= cheat.Max && cheat.On != 1);
			if (HasNumber(cheat.Kind))
				CHECK(cheat.Min < cheat.Max);
			else
				CHECK(cheat.On != cheat.Off);		// Toggle: 써 넣는 두 값. Hook: 바꿔 돌려줄 값(On). Custom: 켬과 끔
		}
		CHECK(Cheats().size() == 52);
		// 외교(research/14): 세력의 적대 판정 Faction.is_enemy_with(세력) -> 불리언을 false 로. 모든 세력에 걸린다. 효과는 보지 못했다.
		CHECK(FindCheat("no_enemies") && FindCheat("no_enemies")->Kind == CheatKind::Hook && FindCheat("no_enemies")->Where == Area::Diplomacy
			&& FindCheat("no_enemies")->On == 0 && !FindCheat("no_enemies")->Verified);
		// 아군 무적(research/13): 상처를 입히는 함수를 플레이어의 사람에게만 건너뛴다. 모듈의 코드가 건다(Custom). 가려지는 것은 아직 보지 못했다.
		CHECK(FindCheat("ally_invincible") && FindCheat("ally_invincible")->Kind == CheatKind::Custom && FindCheat("ally_invincible")->Where == Area::Army
			&& !FindCheat("ally_invincible")->Verified);
		// 군대(research/13): 병사의 고용 값. SoulBasic.get_soldier_cost() 가 돌려주는 수에 곱한다. 고용 창의 값과 실제로 빠진 금화로 봤다(160 → 16).
		CHECK(FindCheat("hire_cost") && FindCheat("hire_cost")->Kind == CheatKind::HookScale && FindCheat("hire_cost")->Where == Area::Army
			&& FindCheat("hire_cost")->Verified && FindCheat("hire_cost")->Max <= 1 && FindCheat("hire_cost")->Off == 1);		// 값은 정수로 남긴다
		// 5단계: 연구 시간(research/12). 도서관 관리자의 get_learn_time 이 돌려주는 수에 곱한다. 효과는 보지 못했다.
		CHECK(FindCheat("research_time") && FindCheat("research_time")->Kind == CheatKind::HookScale && FindCheat("research_time")->Where == Area::Knowledge
			&& !FindCheat("research_time")->Verified && FindCheat("research_time")->Max <= 1);
		// 4단계: 인구·욕구(research/11). 플레이어의 사람을 돌며 쓰는 항목은 모듈의 코드가 한다(src/People.cpp).
		for (const char* id : { "no_hunger", "no_tiredness", "needs_full", "always_happy", "no_old_age_death" })
			CHECK(FindCheat(id) && FindCheat(id)->Kind == CheatKind::Custom && FindCheat(id)->Where == Area::People);
		// 플레이에서 봤다(research/11): 욕구가 100 으로 유지되고(손님은 그대로), 기분이 35 → 98 이 됐다. 노화 깃발은 써지는 것까지만 봤다.
		for (const char* id : { "no_hunger", "no_tiredness", "needs_full", "always_happy" })
			CHECK(FindCheat(id)->Verified);
		CHECK(!FindCheat("no_old_age_death")->Verified);
		// 이주민 보너스는 플레이에서 봤다: 3 을 쓰자 그날 저녁 3명이 왔다.
		CHECK(FindCheat("daily_migrants") && FindCheat("daily_migrants")->Kind == CheatKind::Number && FindCheat("daily_migrants")->Verified
			&& FindCheat("daily_migrants")->Min == 0 && FindCheat("daily_migrants")->Max == 50);
		// 3나-3: 거래·생산·창고 용량(research/10). 게임의 함수가 돌려주는 수에 배율을 곱하는 훅(HookScale)과, 모듈이 자료를 돌며 배율을 쓰는 항목(CustomScale).
		for (const char* id : { "buy_price", "sell_price", "market_depth", "production_time", "worker_performance" })
			CHECK(FindCheat(id) && FindCheat(id)->Kind == CheatKind::HookScale);
		for (const char* id : { "storage_capacity", "production_amount" })
			CHECK(FindCheat(id) && FindCheat(id)->Kind == CheatKind::CustomScale);
		CHECK(FindCheat("production_free") && FindCheat("production_free")->Kind == CheatKind::Custom);
		CHECK(FindCheat("buy_price") && FindCheat("buy_price")->Where == Area::Economy && FindCheat("buy_price")->Max <= 1);		// 싸게 산다
		CHECK(FindCheat("sell_price") && FindCheat("sell_price")->Min >= 1);														// 비싸게 판다
		CHECK(FindCheat("production_time") && FindCheat("production_time")->Where == Area::Build && FindCheat("production_time")->Max <= 1);
		CHECK(HasNumber(CheatKind::Number) && HasNumber(CheatKind::HookScale) && HasNumber(CheatKind::CustomScale));
		CHECK(!HasNumber(CheatKind::Hook) && !HasNumber(CheatKind::Toggle) && !HasNumber(CheatKind::Custom));
		CHECK(IsHook(CheatKind::Hook) && IsHook(CheatKind::HookScale) && !IsHook(CheatKind::Custom) && !IsHook(CheatKind::CustomScale) && !IsHook(CheatKind::Number));
		// 건설 조건과 건설비(research/09). 조건은 게임의 함수가 돌려주는 값을 바꾸는 훅이고, 비용은 모듈이 건물 종류를 돌며 0 으로 쓴다.
		CHECK(FindCheat("build_any") && FindCheat("build_any")->Kind == CheatKind::Hook && FindCheat("build_any")->On == 1 && FindCheat("build_any")->Verified);
		CHECK(FindCheat("build_marks") && FindCheat("build_marks")->Kind == CheatKind::Hook && !FindCheat("build_marks")->Verified);
		CHECK(FindCheat("build_free") && FindCheat("build_free")->Kind == CheatKind::Custom && FindCheat("build_free")->Where == Area::Build);
		// 비용 없음은 플레이에서 봤다(research/09: 돼지 농장을 짓고 올려도 나무가 줄지 않았다. 켠 채 저장한 세이브를 불러와도 비용은 원래 값이었다).
		CHECK(FindCheat("build_free")->Verified);
		// 즉시 업그레이드: 업그레이드 중인 건물에 게임의 build_instantly() 를 부른다(모듈의 코드가 한다). 사용자가 누른 주택 세 채가 바로 올랐다.
		CHECK(FindCheat("instant_upgrade") && FindCheat("instant_upgrade")->Kind == CheatKind::Custom && FindCheat("instant_upgrade")->Where == Area::Build);
		CHECK(FindCheat("instant_upgrade")->Verified);
		// 사용자가 플레이에서 본 것(research/07): 즉시 건설은 된다. 자원 편집 모드는 쓸 수 없어 표에서 뺐다(경제 패널이 맡는다).
		CHECK(FindCheat("instant_build")->Verified && !FindCheat("build_all")->Verified);
		CHECK(FindCheat("resources_edit_mode") == nullptr);
		// 창고 용량은 넣지 않는다: 게임이 다시 채우는 캐시라 "원래대로"가 낡은 값을 써 넣는다. 자리를 잰 뒤(3나-2)에 넣는다.
		CHECK(FindCheat("cap_food") == nullptr);
	});

	Test("치트 표: 영역은 Key 로 찾고 목록의 차례가 열거형과 같다", [] {
		std::set<std::string> keys;
		for (const AreaInfo& area : Areas())
		{
			CHECK(keys.insert(area.Key).second);
			CHECK(FindArea(area.Key) == &area);
			CHECK(&GetArea(area.Id) == &area);
			CHECK(area.Stage >= 2 && area.Stage <= 7);
		}
		CHECK(Areas().size() == 16);
		// 표의 항목이 없어도 제 패널이 있는 영역은 목록에서 켜져 있어야 한다. 경제는 표의 항목을 모두 뺀 뒤 목록에서 꺼져 있었다(research/08).
		for (const AreaInfo& area : Areas())
		{
			// 인물·영주·인구는 제 패널(src/People.cpp)이 있다.
			const bool panel = area.Id == Area::Explorer || area.Id == Area::Economy || area.Id == Area::Time
				|| area.Id == Area::Person || area.Id == Area::Lord || area.Id == Area::People
				|| area.Id == Area::Knowledge || area.Id == Area::Items		// 지식·아이템도 제 패널이 있다(src/People.cpp)
				|| area.Id == Area::Army									// 군대: 병사를 만드는 단추
				|| area.Id == Area::Events || area.Id == Area::Religion		// 이벤트 쿨다운 지우기, 주교 부르기(src/World.cpp)
				|| area.Id == Area::Presets;								// 프리셋: 확인된 항목의 묶음(core/Presets)
			CHECK(area.Panel == panel);
		}
		CHECK(FindArea("nope") == nullptr);
		CHECK(FindArea("tweaks") == nullptr);		// "배율" 영역은 없어졌다. 배율 7개는 제 영역의 패널에 그린다(아래)
		CHECK_STR(GetArea(Area::Time).Key, "time");
		for (const Cheat& cheat : Cheats())
			CHECK(FindArea(GetArea(cheat.Where).Key) != nullptr);
	});

	Test("치트 표: 배율 7개는 제 영역에 놓인다", [] {
		// 이름은 NlToyBox.settings.txt 의 것 그대로다(사용자의 설정 파일이 그대로 읽힌다).
		const std::map<std::string, Area> want = {
			{ "building_cost", Area::Build }, { "start_resources", Area::Economy }, { "book_exp", Area::Knowledge }, { "bribe_cost", Area::Diplomacy },
			{ "free_lord_stay", Area::Lord }, { "church_capacity", Area::Religion }, { "tavern_capacity", Area::People },
		};
		CHECK(KnobPlaces().size() == want.size());
		for (const KnobPlace& place : KnobPlaces())
		{
			const auto it = want.find(place.Id);
			CHECK(it != want.end());
			if (it != want.end())
				CHECK(it->second == place.Where && KnobArea(place.Id) == place.Where);
		}
		CHECK(HasKnobs(Area::Build) && HasKnobs(Area::Knowledge) && HasKnobs(Area::Lord) && !HasKnobs(Area::Explorer) && !HasKnobs(Area::Time) && !HasKnobs(Area::Army));
		CHECK(KnobArea("no_such_knob") == Area::Explorer);		// 모르는 이름: 어느 영역의 패널에도 그리지 않는다(탐색기에는 배율이 없다)
	});

	Test("치트 표: 모르는 Id 와 종류가 다른 Id 를 버리고 수를 범위 안으로 당긴다", [] {
		CheatState state;
		state.On = { "instant_build", "rest_decrease", "nope" };			// rest_decrease 는 Number 다
		state.Numbers = { { "rest_decrease", 999 }, { "instant_build", 1 }, { "nope", 1 } };
		state.Pins = { "inst:o_debug.x" };
		state.Locks = { { "inst:o_debug.y", 3 } };
		const CheatState kept = KeepKnown(state);
		CHECK(kept.On == std::set<std::string>{ "instant_build" });
		CHECK(kept.Numbers.size() == 1 && kept.Numbers.at("rest_decrease") == FindCheat("rest_decrease")->Max);
		CHECK(kept.Pins == state.Pins && kept.Locks.size() == 1);
		CHECK(FindCheat("nope") == nullptr && FindCheat("instant_build")->Kind == CheatKind::Toggle);
		// 훅과 모듈 항목은 효과를 확인한 것만 켠 채로 시작한다(수 항목은 On 에서 빠진다).
		// 확인 전의 것은 켠 채 저장돼 있어도 꺼진 채로 시작한다: 창을 열지도 않았는데 게임의 판정이 바뀌거나(build_marks 는 지식 창도 쓰는 함수다)
		// 건설비가 0 으로 쓰여 세이브에 굳는 일(build_free. 세이브에 들어가는지 재지 않았다)이 없게.
		CheatState hooks;
		hooks.On = { "build_any", "build_free", "build_marks", "instant_upgrade", "rest_decrease", "build_all" };
		const CheatState kept_hooks = KeepKnown(hooks);
		CHECK(FindCheat("build_any")->Verified && !FindCheat("build_marks")->Verified);
		// 확인한 훅·모듈 항목은 남고(build_any, build_free, instant_upgrade) 확인 전의 것(build_marks)은 빠진다.
		// 값을 쓰는 스위치(Toggle: build_all)는 확인 전이어도 그대로다.
		CHECK(kept_hooks.On == (std::set<std::string>{ "build_any", "build_free", "instant_upgrade", "build_all" }));

		// 수가 있는 훅·모듈 항목(배율)은 Numbers 에 든다. 수는 범위 안으로 당기고, 확인 전의 것은 버린다(꺼진 채로 시작한다).
		CheatState scales;
		scales.On = { "production_time", "storage_capacity" };				// 수가 있는 항목은 On 에 들지 않는다
		scales.Numbers = { { "production_time", 0.0000001 }, { "storage_capacity", 5000000 }, { "rest_decrease", 3 }, { "build_any", 1 } };
		const CheatState kept_scales = KeepKnown(scales);
		CHECK(kept_scales.On.empty());
		CHECK(kept_scales.Numbers.count("rest_decrease") == 1 && kept_scales.Numbers.count("build_any") == 0);
		// 창고 용량 배율은 플레이에서 확인했다(research/10): 남고, 범위의 끝(Max)으로 당겨진다.
		CHECK(FindCheat("storage_capacity")->Verified && kept_scales.Numbers.count("storage_capacity") == 1
			&& kept_scales.Numbers.at("storage_capacity") == FindCheat("storage_capacity")->Max);
		for (const char* id : { "production_time", "storage_capacity" })
		{
			const Cheat* cheat = FindCheat(id);
			CHECK(cheat != nullptr);
			if (cheat)
				CHECK(kept_scales.Numbers.count(id) == (cheat->Verified ? 1u : 0u));
			if (cheat && cheat->Verified)
				CHECK(kept_scales.Numbers.at(id) >= cheat->Min && kept_scales.Numbers.at(id) <= cheat->Max);
		}
	});

	Test("흐름: 일정하게 느는 값의 빠르기를 잰다", [] {
		Rate rate(1.0);
		CHECK(!rate.Ready() && rate.PerSecond() == 0);
		for (int i = 0; i <= 4; i++)
			rate.Add(i * 0.1, 100 + i * 5.74);			// 실제 1초에 57.4
		CHECK(!rate.Ready());								// 0.4초: 창의 절반이 안 된다
		for (int i = 5; i <= 30; i++)
			rate.Add(i * 0.1, 100 + i * 5.74);
		CHECK(rate.Ready() && std::abs(rate.PerSecond() - 57.4) < 1e-6);
	});

	Test("흐름: 창보다 오래된 표본은 잊는다", [] {
		Rate rate(1.0);
		double value = 0;
		for (int i = 0; i <= 20; i++)						// 2초 동안 1초에 10
			rate.Add(i * 0.1, value = i * 1.0);
		for (int i = 21; i <= 35; i++)						// 그 뒤 1.5초 동안 1초에 50
			rate.Add(i * 0.1, value += 5.0);
		CHECK(std::abs(rate.PerSecond() - 50) < 1e-6);
	});

	Test("흐름: 값이 줄면 처음부터 다시 잰다", [] {
		Rate rate(1.0);
		for (int i = 0; i <= 20; i++)
			rate.Add(i * 0.1, 1000 + i);
		CHECK(rate.Ready());
		rate.Add(2.1, 5);									// 새 게임: 게임 시간이 처음으로 돌아갔다
		CHECK(!rate.Ready() && rate.PerSecond() == 0);
		rate.Reset();
		CHECK(!rate.Ready());
	});

	Test("흐름: 기대한 배율인지 가린다", [] {
		CHECK(RateMatches(57.4, 114.8, 2));
		CHECK(RateMatches(57.4, 100, 2));					// 13% 모자라다. 허용 안이다
		CHECK(!RateMatches(57.4, 57.4, 2));				// 그대로다
		CHECK(!RateMatches(57.4, 0, 2));
		CHECK(!RateMatches(0, 100, 2));					// 누르기 전에 멈춰 있었다
		CHECK(RateMatches(57.4, 14.35, 0.25));
	});

	Test("원격 명령: 줄을 읽는다", [] {
		CHECK(ParseRemoteLine("").Verb.empty() && ParseRemoteLine("# 주석").Verb.empty() && ParseRemoteLine("# 주석").Error.empty());
		const RemoteCommand ask = ParseRemoteLine("  ask inst:o_debug.is_x ");
		CHECK(ask.Error.empty() && ask.Verb == "ask" && ask.Target == "inst:o_debug.is_x");
		const RemoteCommand list = ParseRemoteLine("list inst:o_production_manager.ppm_map_of_process as=map max=50");
		CHECK(list.Error.empty() && list.Options.at("as") == "map" && OptionNumber(list, "max", 400) == 50 && OptionNumber(list, "depth", 2) == 2);
		const RemoteCommand tree = ParseRemoteLine("tree map:128@{messenger_cost } depth=3");
		CHECK(tree.Error.empty() && tree.Target == "map:128@{messenger_cost }" && OptionNumber(tree, "depth", 2) == 3);
		const RemoteCommand find = ParseRemoteLine("find name=gold value=3000 in=global,inst");
		CHECK(find.Error.empty() && find.Options.at("name") == "gold" && OptionNumber(find, "value", 0) == 3000 && find.Options.at("in") == "global,inst");
		const RemoteCommand refine = ParseRemoteLine("refine value=2950");
		CHECK(refine.Error.empty() && refine.Number == 2950);
		const RemoteCommand write = ParseRemoteLine("write map:1@{a=b}=-4.5");
		CHECK(write.Error.empty() && write.Target == "map:1@{a=b}" && write.Number == -4.5);
		CHECK(ParseRemoteLine("poke inst:o_debug.x=1").Verb == "poke" && ParseRemoteLine("state").Error.empty());
		CHECK(ParseRemoteLine("record gml_Script_budget_money_get").Target == "gml_Script_budget_money_get");
		CHECK(ParseRemoteLine("records").Error.empty() && ParseRemoteLine("unrecord all").Target == "all");
		CHECK(ParseRemoteLine("shot hud-1").Target == "hud-1" && ParseRemoteLine("window close").Target == "close");
		const RemoteCommand about = ParseRemoteLine("about inst:o_game_map_controller.__province.__warehouse.change");
		CHECK(about.Error.empty() && about.Verb == "about" && about.Target == "inst:o_game_map_controller.__province.__warehouse.change");
		const RemoteCommand gold = ParseRemoteLine("economy gold_add amount=1000");
		CHECK(gold.Error.empty() && gold.Verb == "economy" && gold.Target == "gold_add" && gold.Number == 1000);
		const RemoteCommand wood = ParseRemoteLine("economy add resource=1 amount=-5");
		CHECK(wood.Error.empty() && wood.Target == "add" && wood.Number == -5 && OptionNumber(wood, "resource", -1) == 1);
		CHECK(ParseRemoteLine("economy all amount=100").Error.empty());
		const RemoteCommand page = ParseRemoteLine("page economy");
		CHECK(page.Error.empty() && page.Verb == "page" && page.Target == "economy");
		const RemoteCommand cheat = ParseRemoteLine("cheat build_free on");
		CHECK(cheat.Error.empty() && cheat.Verb == "cheat" && cheat.Target == "build_free" && cheat.Number == 1);
		CHECK(ParseRemoteLine("cheat build_free off").Number == 0 && ParseRemoteLine("cheat build_free off").Error.empty());
		CHECK(!ParseRemoteLine("cheat").Error.empty() && !ParseRemoteLine("cheat build_free").Error.empty()
			&& !ParseRemoteLine("cheat build_free maybe").Error.empty() && !ParseRemoteLine("cheat no_such_cheat on").Error.empty());
		const RemoteCommand statics = ParseRemoteLine("statics inst:o_building.generic max=200");
		CHECK(statics.Error.empty() && statics.Verb == "statics" && statics.Target == "inst:o_building.generic" && OptionNumber(statics, "max", 400) == 200);
	});

	Test("원격 명령: 반환값을 바꾸는 훅의 줄을 읽는다", [] {
		const RemoteCommand yes = ParseRemoteLine("override inst:o_building.is_building_locked b:0");
		CHECK(yes.Error.empty() && yes.Verb == "override" && yes.Target == "inst:o_building.is_building_locked");
		CHECK(yes.Args.size() == 1 && yes.Args[0].Kind == 'b' && yes.Args[0].Number == 0 && !yes.Options.count("skip"));
		const RemoteCommand skip = ParseRemoteLine("override gml_Script_x n:-1.5 skip");
		CHECK(skip.Error.empty() && skip.Args[0].Kind == 'n' && skip.Args[0].Number == -1.5 && skip.Options.count("skip") == 1);
		const RemoteCommand treecall = ParseRemoteLine("treecall building_generic_get_array_of_all_buildings depth=3 max=50");
		CHECK(treecall.Error.empty() && treecall.Verb == "treecall" && treecall.Target == "building_generic_get_array_of_all_buildings");
		CHECK(OptionNumber(treecall, "depth", 2) == 3 && OptionNumber(treecall, "max", 300) == 50);
		CHECK(!ParseRemoteLine("treecall").Error.empty() && !ParseRemoteLine("treecall a b").Error.empty());
		const RemoteCommand undefined = ParseRemoteLine("override gml_Script_x u");
		CHECK(undefined.Args.size() == 1 && undefined.Args[0].Kind == 'u');
		CHECK(ParseRemoteLine("unoverride all").Target == "all" && ParseRemoteLine("unoverride gml_Script_x").Error.empty());
		// 바꿀 값은 수, 불리언, undefined 뿐이다. 글이나 주소의 값으로 바꾸지 않는다(훅 안에서 만들 수 없다).
		for (const char* line : { "override", "override gml_Script_x", "override gml_Script_x s:yes", "override gml_Script_x p:global.a",
			"override gml_Script_x n:1 always", "override gml_Script_x n:1 skip extra", "override gml_Script_x n:abc", "unoverride", "unoverride a b",
			"override gml_Script_x n:nan", "override gml_Script_x n:inf", "override gml_Script_x n:-inf",
			"statics", "statics o_x.y" })
		{
			if (ParseRemoteLine(line).Error.empty())
			{
				std::printf("  FAIL accepted: %s\n", line);
				g_Failed++;
			}
		}
	});

	Test("원격 명령: 반환값에 배율을 곱하는 훅의 줄을 읽는다", [] {
		const RemoteCommand half = ParseRemoteLine("override gml_Script_x x:0.5");
		CHECK(half.Error.empty() && half.Target == "gml_Script_x" && half.Args.size() == 1 && half.Args[0].Kind == 'x' && half.Args[0].Number == 0.5
			&& half.Options.count("whole") == 0);
		const RemoteCommand whole = ParseRemoteLine("override inst:o_a.get_price x:10 whole");
		CHECK(whole.Error.empty() && whole.Args.size() == 1 && whole.Args[0].Kind == 'x' && whole.Args[0].Number == 10 && whole.Options.count("whole") == 1);
		// 배율은 0 보다 큰 유한한 수다. 원래 값이 있어야 곱하므로 skip 과 함께 쓰지 않는다. whole 은 배율에만 붙는다.
		for (const char* line : { "override gml_Script_x x:0", "override gml_Script_x x:-2", "override gml_Script_x x:abc", "override gml_Script_x x:inf",
			"override gml_Script_x x:nan", "override gml_Script_x x:2 skip", "override gml_Script_x n:2 whole", "override gml_Script_x x:2 whole extra" })
			CHECK(!ParseRemoteLine(line).Error.empty());
	});

	Test("원격 명령: 수가 있는 치트는 수로 켠다", [] {
		const RemoteCommand set = ParseRemoteLine("cheat production_time 0.1");
		CHECK(set.Error.empty() && set.Target == "production_time" && set.Number == 1 && set.Args.size() == 1 && set.Args[0].Kind == 'n' && set.Args[0].Number == 0.1);
		CHECK(ParseRemoteLine("cheat production_time off").Error.empty() && ParseRemoteLine("cheat production_time off").Number == 0);
		const RemoteCommand on = ParseRemoteLine("cheat production_time on");			// 수 없이 켜면 표가 내놓는 배율로 켠다
		CHECK(on.Error.empty() && on.Number == 1 && on.Args.empty());
		CHECK(!ParseRemoteLine("cheat build_any 0.5").Error.empty());					// 수가 없는 항목에 수를 주지 않는다
		CHECK(!ParseRemoteLine("cheat production_time abc").Error.empty() && !ParseRemoteLine("cheat production_time nan").Error.empty());
		CHECK(ParseRemoteLine("cheat rest_decrease 0").Error.empty() && ParseRemoteLine("cheat rest_decrease 0").Args.size() == 1);		// 수 항목(Number)도 수로 켠다
	});

	Test("원격 명령: 부르는 인자를 읽는다", [] {
		const RemoteCommand call = ParseRemoteLine("call gml_Script_x n:-3.5 s:wood s:{two words} b:1 u p:inst:o_building:1");
		CHECK(call.Error.empty() && call.Target == "gml_Script_x" && call.Args.size() == 6);
		CHECK(call.Args[0].Kind == 'n' && call.Args[0].Number == -3.5 && call.Args[1].Kind == 's' && call.Args[1].Text == "wood");
		CHECK(call.Args[2].Text == "two words" && call.Args[3].Kind == 'b' && call.Args[3].Number == 1 && call.Args[4].Kind == 'u');
		CHECK(call.Args[5].Kind == 'p' && call.Args[5].Text == "inst:o_building:1");
		const RemoteCommand method = ParseRemoteLine("method inst:o_building.get_level");
		CHECK(method.Error.empty() && method.Target == "inst:o_building.get_level" && method.Args.empty());
	});

	Test("원격 명령: 읽을 수 없으면 오류를 낸다", [] {
		for (const char* line : { "dance", "ask", "ask o_debug.x", "ask global.a global.b", "list", "list global.a max", "find", "find value=abc",
			"refine", "refine value=x", "write inst:o_debug.x", "write inst:o_debug=3", "write inst:o_debug.x=abc", "poke global=1",
			"record", "record a b", "shot", "shot ../x", "window", "window maybe", "call", "call x n:abc", "call x q:1", "call x b:2",
			"call x p:nowhere.x", "method global", "method o_x.y", "state now", "about", "about global.a global.b", "about o_x.y",
			"economy", "economy gold", "economy gold_add", "economy gold_add amount=x", "economy add amount=5", "economy add resource=1.5 amount=5",
			"economy set resource=-1 amount=5", "economy add resource=1e300 amount=5", "economy all resource=3 amount=100",
			"economy gold_add resource=1e300 amount=5", "page", "page nowhere", "page economy now" })
		{
			if (ParseRemoteLine(line).Error.empty())
			{
				std::printf("  FAIL accepted: %s\n", line);
				g_Failed++;
			}
		}
	});


	Test("원격 요청 파일: 이름을 바꿔 집은 것만 읽는다", [] {
		namespace fs = std::filesystem;
		const fs::path dir = fs::temp_directory_path() / "nltoybox-remote-test";
		fs::remove_all(dir);
		fs::create_directories(dir);
		const fs::path ask = dir / "ask.txt", taken = dir / "ask.taken";
		std::vector<std::string> lines = { "stale" };

		CHECK(!TakeRemoteRequest(ask, taken, lines) && lines.empty());		// 없으면 아무것도 하지 않는다

		{ std::ofstream(ask) << "id 1\ncall gml_Script_x n:1\n"; }
		CHECK(TakeRemoteRequest(ask, taken, lines) && lines.size() == 2 && lines[1] == "call gml_Script_x n:1");
		CHECK(!fs::exists(ask) && !fs::exists(taken));
		CHECK(!TakeRemoteRequest(ask, taken, lines) && lines.empty());		// 같은 요청을 두 번 주지 않는다

		// 다른 프로그램이 파일을 잡고 있어 이름을 바꿀 수 없으면 읽지도 않는다(읽고 못 지우면 같은 호출이 되풀이된다).
		{
			std::ofstream held(ask);
			held << "id 2\nstate\n";
			held.flush();
			CHECK(!TakeRemoteRequest(ask, taken, lines) && lines.empty() && fs::exists(ask));
		}
		CHECK(TakeRemoteRequest(ask, taken, lines) && lines.size() == 2 && lines[0] == "id 2");

		// 앞의 요청이 실행 도중 끊겨 남은 것은 다시 주지 않는다.
		{ std::ofstream(taken) << "id 3\ncall gml_Script_dangerous\n"; }
		{ std::ofstream(ask) << "id 4\nstate\n"; }
		CHECK(TakeRemoteRequest(ask, taken, lines) && lines.size() == 2 && lines[0] == "id 4");

		// 모듈이 뜰 때 남아 있던 요청은 버린다(죽은 도구가 남긴 호출이 다음 실행에서 불리지 않게).
		{ std::ofstream(ask) << "id 5\n"; }
		{ std::ofstream(taken) << "id 6\n"; }
		DropStaleRemoteRequest(ask, taken);
		CHECK(!fs::exists(ask) && !fs::exists(taken));
		fs::remove_all(dir);
	});

	Test("메서드 묶기: 묶인 곳이 없으면 가진 구조체에 묶고, 가진 것이 구조체가 아니면 부르지 않는다", [] {
		for (const OwnerKind owner : { OwnerKind::Global, OwnerKind::Struct, OwnerKind::Instance, OwnerKind::Other })
			CHECK(ChooseBinding(true, owner) == Binding::AsIs);
		CHECK(ChooseBinding(false, OwnerKind::Struct) == Binding::ToOwner && ChooseBinding(false, OwnerKind::Instance) == Binding::ToOwner);
		// 전역, 배열의 원소, ds 의 값: 그대로 부르면 self 가 전역이 되어 본문이 엉뚱한 곳을 읽고 쓴다.
		CHECK(ChooseBinding(false, OwnerKind::Global) == Binding::Refuse && ChooseBinding(false, OwnerKind::Other) == Binding::Refuse);
	});

	Test("호출 기록: 처음 몇 개와 처음 보는 꼴만 글로 남긴다", [] {
		const int one[] = { 0 }, two[] = { 0, 1 };
		CHECK(ShapeKey(one, 1) != ShapeKey(two, 2) && ShapeKey(one, 1) == ShapeKey(one, 1) && ShapeKey(nullptr, 0) != ShapeKey(one, 1));
		CallLog log(2, 10);
		const uint64_t a = ShapeKey(one, 1), b = ShapeKey(two, 2);
		CHECK(log.Note(a) == 1);
		log.Sample(1, a, "(number)", "(50)", "undefined");
		CHECK(log.Note(a) == 2);
		log.Sample(2, a, "(number)", "(60)", "undefined");
		CHECK(log.Note(a) == 0 && log.Note(a) == 0);				// 셋째부터는 세기만 한다
		CHECK(log.Note(b) == 5);									// 처음 보는 꼴은 남긴다
		log.Sample(5, b, "(number, string)", "(-20, \"tax\")", "1");
		CHECK(log.Calls() == 5 && log.ShapeCount() == 2);
		CHECK_STR(log.Format("  "), "  calls 5, shapes 2\n  shape (number) x4\n  shape (number, string) x1\n"
			"  #1 (50) -> undefined\n  #2 (60) -> undefined\n  #5 (-20, \"tax\") -> 1\n");
		log.Clear();
		CHECK(log.Calls() == 0 && log.Note(a) == 1);
	});

	Test("호출 기록: 글로 남기는 수에 한도가 있다", [] {
		CallLog log(100, 3);
		const int kind[] = { 0 };
		const uint64_t key = ShapeKey(kind, 1);
		for (int i = 0; i < 10; i++)
			if (const size_t index = log.Note(key))
				log.Sample(index, key, "(number)", "(1)", "undefined");
		CHECK(log.Calls() == 10);
		CHECK(log.Format("").find("#3 ") != std::string::npos && log.Format("").find("#4 ") == std::string::npos);
	});


	// 요청 파일의 오타로 게임 실행 한 번을 버리지 않는다.
	Test("인물: 능력치 여덟과 욕구 여섯의 이름", [] {
		// 능력치의 열쇠는 __soul.__skills.__level 의 이름, 욕구의 차례는 게임의 번호다(research/11).
		const std::vector<NamedKey>& skills = SkillNames();
		CHECK(skills.size() == 8);
		const char* keys[] = { "combat", "command", "education", "knowledge", "management", "manners", "negotiation", "oratory" };
		for (size_t i = 0; i < skills.size() && i < 8; i++)
			CHECK_STR(skills[i].Key, keys[i]);
		const std::vector<const char*>& needs = NeedNames();
		CHECK(needs.size() == 6);
		CHECK_STR(needs[0], "수면");
		CHECK_STR(needs[1], "음식");
		CHECK_STR(needs[5], "돌봄");
	});

	Test("인물: 명령의 낱말과 그것이 받는 것", [] {
		for (const char* word : { "skill_set", "skill_add", "skills_max", "need_set", "needs_fill", "age_set", "happy", "cure", "trait_add", "trait_remove" })
		{
			PersonAct act = PersonAct::Cure;
			CHECK(ParsePersonAct(word, act));
			CHECK_STR(PersonActWord(act), word);
		}
		PersonAct act = PersonAct::Cure;
		CHECK(!ParsePersonAct("kill", act) && !ParsePersonAct("", act));
		CHECK(NeedsIndex(PersonAct::SkillSet) && NeedsIndex(PersonAct::SkillAdd) && NeedsIndex(PersonAct::NeedSet));
		CHECK(!NeedsIndex(PersonAct::SkillsMax) && !NeedsIndex(PersonAct::AgeSet) && !NeedsIndex(PersonAct::Happy));
		CHECK(NeedsAmount(PersonAct::SkillSet) && NeedsAmount(PersonAct::NeedSet) && NeedsAmount(PersonAct::AgeSet) && !NeedsAmount(PersonAct::Cure));
		CHECK(NeedsText(PersonAct::TraitAdd) && NeedsText(PersonAct::TraitRemove) && !NeedsText(PersonAct::Happy));
	});

	Test("인물: 온전하지 않은 명령은 하지 않는다", [] {
		std::string why;
		PersonCommand c;
		c.Who = "25556c3312bce178";
		c.Act = PersonAct::SkillSet;
		c.Index = 4;
		c.Amount = 12;
		CHECK(CheckPersonCommand(c, why));
		c.Index = 8;		// 능력치는 여덟이다
		CHECK(!CheckPersonCommand(c, why) && !why.empty());
		c.Index = -1;
		CHECK(!CheckPersonCommand(c, why));
		c.Index = 0;
		c.Amount = std::numeric_limits<double>::quiet_NaN();
		CHECK(!CheckPersonCommand(c, why));

		c.Act = PersonAct::NeedSet;
		c.Amount = 100;
		c.Index = 5;
		CHECK(CheckPersonCommand(c, why));
		c.Index = 6;		// 욕구는 여섯이다
		CHECK(!CheckPersonCommand(c, why));

		c.Act = PersonAct::TraitAdd;
		c.Text = "brave";
		CHECK(CheckPersonCommand(c, why));
		c.Text = "Brave!";
		CHECK(!CheckPersonCommand(c, why));
		c.Text.clear();
		CHECK(!CheckPersonCommand(c, why));

		c.Act = PersonAct::Happy;
		CHECK(CheckPersonCommand(c, why));
		c.Who.clear();		// 누구인지 없다
		CHECK(!CheckPersonCommand(c, why));
	});

	Test("인물: 일괄 명령은 플레이어의 산 사람에게만 간다", [] {
		// 영주 A, 주민 B, 손님 C(상인의 진영은 unique_guests 였다), 죽은 영주 D, 다른 왕국의 영주 E
		std::vector<PersonRow> people(5);
		people[0] = { "aaaa", "A", "player", true, 0, 3, false };
		people[1] = { "bbbb", "B", "player", false, 0, 1, false };
		people[2] = { "cccc", "C", "unique_guests", true, 1, 3, false };
		people[3] = { "dddd", "D", "player", true, 2, 3, true };
		people[4] = { "eeee", "E", "kingdom_7", true, 3, 3, false };
		CHECK(IsPlayers(people[0]) && IsPlayers(people[1]) && !IsPlayers(people[2]) && !IsPlayers(people[3]) && !IsPlayers(people[4]));

		CHECK(PickTargets(people, "lords") == std::vector<size_t>{ 0 });
		CHECK((PickTargets(people, "people") == std::vector<size_t>{ 0, 1 }));
		// 하나를 짚어 고른 것은 손님이어도 된다(사용자가 골랐다). 죽은 사람과 없는 사람은 안 된다.
		CHECK(PickTargets(people, "cccc") == std::vector<size_t>{ 2 });
		CHECK(PickTargets(people, "dddd").empty());
		CHECK(PickTargets(people, "ffff").empty() && PickTargets(people, "").empty());
	});

	Test("인물: 쓰는 수는 범위 안의 수로 다듬는다", [] {
		double out = -1;
		CHECK(SkillValue(12, out) && out == 12);
		CHECK(SkillValue(25, out) && out == 20);			// 게임의 최고 등급(get_max_level 이 20 을 돌려줬다)
		CHECK(SkillValue(-3, out) && out == 0);
		CHECK(SkillValue(7.6, out) && out == 8);			// 등급은 정수다
		CHECK(!SkillValue(std::numeric_limits<double>::infinity(), out));
		CHECK(SkillAfterAdd(19, 5, out) && out == 20);
		CHECK(SkillAfterAdd(2, -5, out) && out == 0);
		CHECK(!SkillAfterAdd(std::numeric_limits<double>::quiet_NaN(), 1, out));

		CHECK(NeedValue(150, 100, out) && out == 100);
		CHECK(NeedValue(-1, 100, out) && out == 0);
		CHECK(NeedValue(40.5, 100, out) && out == 40.5);
		CHECK(NeedValue(90, 60, out) && out == 60);			// 상한이 낮아진 욕구는 그 상한까지만
		CHECK(!NeedValue(50, std::numeric_limits<double>::quiet_NaN(), out) && !NeedValue(50, 0, out));

		CHECK(AgeValue(30.4, out) && out == 30);
		CHECK(AgeValue(500, out) && out == 120);
		CHECK(AgeValue(-4, out) && out == 1);
		CHECK(!AgeValue(std::numeric_limits<double>::quiet_NaN(), out));
	});

	Test("인물: 특성 이름의 꼴과 치료가 떼는 부상", [] {
		CHECK(GoodTraitName("brave") && GoodTraitName("pneumonia_st_1") && GoodTraitName("__wolves_wont_attack__"));
		CHECK(!GoodTraitName("") && !GoodTraitName("a b") && !GoodTraitName("Brave") && !GoodTraitName("brave;x") && !GoodTraitName(std::string(65, 'a')));

		const std::vector<const char*>& wounds = WoundTraits();
		const auto has = [&](const char* name) {
			for (const char* wound : wounds)
				if (std::string(wound) == name)
					return true;
			return false;
		};
		CHECK(has("bruise_light") && has("cut") && has("wound_deep") && has("burn_heavy"));
		CHECK(!has("human") && !has("brave") && !has("kid") && !has("lost_head") && !has("dead"));
		for (const char* wound : wounds)
			CHECK(GoodTraitName(wound));
	});

	Test("인물: 죽음은 게임의 캐시에서 읽고, 비어 있으면 모른다고 답한다", [] {
		// c_status.__is_dead 는 true/false 이거나, 아직 셈하지 않았으면 -4 다. 특성을 바꾸면 게임이 캐시를 비운다(research/11).
		// -4 를 "죽었다"로 읽으면 특성을 붙인 바로 뒤의 명령이 그 사람을 놓친다(확인 실행에서 그랬다).
		CHECK(AliveFromDeadCache(true, 0) == Alive::Yes);
		CHECK(AliveFromDeadCache(true, 1) == Alive::No);
		CHECK(AliveFromDeadCache(true, -4) == Alive::Unknown);
		CHECK(AliveFromDeadCache(false, 0) == Alive::Unknown);		// 읽지 못했다
		CHECK(AliveFromDeadCache(true, std::numeric_limits<double>::quiet_NaN()) == Alive::Unknown);
	});

	Test("인물: 종과 죽음의 특성은 붙이지도 떼지도 않는다", [] {
		for (const char* name : { "human", "wolf", "pig", "dog", "dead", "delayed_dead", "dead_from_old_age", "dead_from_poison", "dying_from_old", "lost_head" })
			CHECK(IsProtectedTrait(name));
		for (const char* name : { "brave", "kid", "bruise_light", "pneumonia_st_1", "inspired", "" })
			CHECK(!IsProtectedTrait(name));

		std::string why;
		PersonCommand c;
		c.Who = "25556c3312bce178";
		c.Act = PersonAct::TraitAdd;
		c.Text = "dead";
		CHECK(!CheckPersonCommand(c, why) && !why.empty());
		c.Act = PersonAct::TraitRemove;
		c.Text = "human";
		CHECK(!CheckPersonCommand(c, why));
		c.Text = "brave";
		CHECK(CheckPersonCommand(c, why));
		CHECK(!ParseRemoteLine("person 25556c3312bce178 trait_add name=lost_head").Error.empty());
	});

	Test("인물: 여럿에게 한꺼번에 하는 것은 네 가지뿐이다", [] {
		CHECK(IsBulkWho("lords") && IsBulkWho("people") && !IsBulkWho("25556c3312bce178") && !IsBulkWho(""));
		std::string why;
		PersonCommand c;
		c.Who = "people";
		for (const PersonAct act : { PersonAct::SkillsMax, PersonAct::NeedsFill, PersonAct::Happy, PersonAct::Cure })
		{
			c.Act = act;
			CHECK(CheckPersonCommand(c, why));
		}
		// 나이·특성·능력치 하나·욕구 하나는 한 사람을 짚어서만 한다(한 줄의 실수가 모든 사람을 세이브에 남게 바꾸지 않게).
		c.Act = PersonAct::AgeSet;
		c.Amount = 1;
		CHECK(!CheckPersonCommand(c, why) && !why.empty());
		c.Act = PersonAct::TraitAdd;
		c.Text = "brave";
		CHECK(!CheckPersonCommand(c, why));
		c.Act = PersonAct::SkillSet;
		c.Index = 0;
		c.Amount = 20;
		CHECK(!CheckPersonCommand(c, why));
		c.Act = PersonAct::NeedSet;
		CHECK(!CheckPersonCommand(c, why));
		c.Who = "lords";
		c.Act = PersonAct::TraitRemove;
		CHECK(!CheckPersonCommand(c, why));
		CHECK(!ParseRemoteLine("person people age_set amount=1").Error.empty());
		CHECK(!ParseRemoteLine("person lords trait_add name=brave").Error.empty());
		CHECK(ParseRemoteLine("person lords cure").Error.empty());
	});

	Test("인물: 채우기는 상한까지, 번호는 유한한 정수만", [] {
		double out = -1;
		CHECK(NeedValue(k_FillAll, 100, out) && out == 100);		// 창의 "채우기"는 읽은 상한이 아니라 이 수를 보낸다(상한을 읽지 못했어도 0 을 쓰지 않는다)
		CHECK(NeedValue(k_FillAll, 60, out) && out == 60);
		CHECK(ShouldFillNeed(99.4, 100) && !ShouldFillNeed(99.6, 100) && !ShouldFillNeed(100, 100));	// 거의 찬 칸은 건드리지 않는다
		CHECK(!ShouldFillNeed(50, 0) && !ShouldFillNeed(std::numeric_limits<double>::quiet_NaN(), 100));

		// 행복 생각: 한 사람을 짚었으면 합을 읽지 못해도 붙인다. 여럿을 돌 때는 읽지 못한 사람을 건너뛴다(되풀이해 쌓이지 않게).
		CHECK(ShouldAttachHappy(true, 40, false) && ShouldAttachHappy(true, 40, true));
		CHECK(!ShouldAttachHappy(true, 100, false) && !ShouldAttachHappy(true, 136.5, true));
		CHECK(ShouldAttachHappy(false, 0, false) && !ShouldAttachHappy(false, 0, true));

		CHECK(!ParseRemoteLine("person lords skill_set index=inf amount=1").Error.empty());
		CHECK(!ParseRemoteLine("person 25556c3312bce178 skill_set index=1e300 amount=1").Error.empty());
		CHECK(!ParseRemoteLine("person 25556c3312bce178 happy index=inf").Error.empty());
		CHECK(!ParseRemoteLine("person 25556c3312bce178 skill_set index=1.5 amount=1").Error.empty());
		CHECK(!ParseRemoteLine("person 25556c3312bce178 age_set amount=nan").Error.empty());
		CHECK(ParseRemoteLine("person 25556c3312bce178 skill_add index=7 amount=-1").Error.empty());
	});

	Test("인구: 노화 깃발을 끈 뒤에는 처음부터 온전한 한 바퀴로 되돌린다", [] {
		const auto run = [](HoldRound& round, size_t count, bool ageless, bool skip = false) {
			// 한 틱: 계획을 받고, 묶음의 사람마다 깃발을 쓰고, 묶음을 닫는다. 돌려주는 것: 이번 틱에 깃발을 쓴 사람의 자리.
			std::vector<size_t> touched;
			const HoldPlan plan = HoldBegin(round, false, false, ageless);
			if (!plan.Work)
				return touched;
			const PeopleSlice slice = NextPeopleSlice(count, round.Cursor, 40);
			for (size_t i = slice.Begin; i < slice.End; i++)
			{
				const bool skipped = skip && i == slice.Begin;
				if (plan.WriteAge && !skipped)
					touched.push_back(i);
				HoldTouched(round, plan.WriteAge && !skipped && plan.AgeValue == 0, skipped);
			}
			HoldEnd(round, slice, ageless);
			return touched;
		};

		// (가) 바퀴 도중에 끈다: 100명, 40명씩. 80명까지 끈 뒤 항목을 끄면 처음부터 다시 돌아 100명을 모두 되돌린다.
		HoldRound round;
		std::vector<bool> off(100, false);
		for (int tick = 0; tick < 2; tick++)
			for (const size_t i : run(round, 100, true))
				off[i] = true;
		CHECK(round.AgeWritten && round.Cursor == 80);
		for (int tick = 0; tick < 3; tick++)
			for (const size_t i : run(round, 100, false))
				off[i] = false;
		CHECK(std::find(off.begin(), off.end(), true) == off.end());		// 남은 깃발이 없다
		CHECK(!round.AgeWritten);
		CHECK(run(round, 100, false).empty());								// 되돌릴 것이 없으면 돌지 않는다

		// (나) 켠 뒤 첫 바퀴가 감기기 전에 끈다: 첫 묶음만 쓴 채 꺼도 되돌린다.
		round = HoldRound();
		CHECK(run(round, 100, true).size() == 40 && round.AgeWritten);
		size_t restored = 0;
		for (int tick = 0; tick < 3; tick++)
			restored += run(round, 100, false).size();
		CHECK(restored == 100 && !round.AgeWritten);

		// (다) 되돌리는 바퀴에서 건너뛴 사람이 있으면(자리가 바뀌었다) 한 바퀴를 더 돈다.
		round = HoldRound();
		run(round, 30, true);
		CHECK(round.AgeWritten);
		run(round, 30, false, true);										// 한 사람을 건너뛰었다
		CHECK(round.AgeWritten);											// 아직 되돌릴 것이 있다고 본다
		const HoldPlan again = HoldBegin(round, false, false, false);
		CHECK(again.Work && again.Rescan && again.WriteAge && again.AgeValue == 1);
		round.Cursor = 0;
		run(round, 30, false);												// 이번에는 깨끗이 돈다
		CHECK(!round.AgeWritten);

		// 다시 켜면 되돌리던 것을 그만두고 끈다.
		round = HoldRound();
		run(round, 100, true);
		run(round, 100, false);
		const HoldPlan on = HoldBegin(round, false, false, true);
		CHECK(on.WriteAge && on.AgeValue == 0 && !round.Restoring);

		// 할 일이 없으면 일하지 않는다. 욕구나 행복만 켜져 있으면 깃발은 건드리지 않는다.
		round = HoldRound();
		CHECK(!HoldBegin(round, false, false, false).Work);
		const HoldPlan needs = HoldBegin(round, true, false, false);
		CHECK(needs.Work && !needs.WriteAge);
	});

	Test("인물: 지식·소지금·소지품을 주는 명령", [] {
		for (const char* word : { "knowledge_all", "knowledge_add", "money_add", "item_add" })
		{
			PersonAct act = PersonAct::Cure;
			CHECK(ParsePersonAct(word, act));
			CHECK_STR(PersonActWord(act), word);
		}
		CHECK(NeedsText(PersonAct::KnowledgeAdd) && !NeedsText(PersonAct::KnowledgeAll));
		CHECK(NeedsAmount(PersonAct::MoneyAdd) && NeedsAmount(PersonAct::ItemAdd) && !NeedsAmount(PersonAct::KnowledgeAll) && !NeedsAmount(PersonAct::KnowledgeAdd));
		CHECK(NeedsIndex(PersonAct::ItemAdd) && !NeedsIndex(PersonAct::MoneyAdd) && !NeedsIndex(PersonAct::KnowledgeAdd));
		CHECK(IndexLimit(PersonAct::SkillSet) == 8 && IndexLimit(PersonAct::NeedSet) == 6 && IndexLimit(PersonAct::ItemAdd) == 200 && IndexLimit(PersonAct::Happy) == 0);

		std::string why;
		PersonCommand c;
		c.Who = "25556c3312bce178";
		c.Act = PersonAct::KnowledgeAdd;
		c.Text = "building_mine";
		CHECK(CheckPersonCommand(c, why));
		c.Text = "dead";			// 지식의 이름은 특성의 막힌 이름과 무관하다(게임의 지식 목록으로 본다)
		CHECK(CheckPersonCommand(c, why));
		c.Text = "Bad Name";
		CHECK(!CheckPersonCommand(c, why));
		c.Text.clear();
		CHECK(!CheckPersonCommand(c, why));

		c.Act = PersonAct::ItemAdd;
		c.Index = 1;
		c.Amount = 50;
		CHECK(CheckPersonCommand(c, why));
		c.Index = -1;
		CHECK(!CheckPersonCommand(c, why));
		c.Index = 200;
		CHECK(!CheckPersonCommand(c, why));
		c.Index = 1;
		c.Amount = 0;				// 줄 것이 없다
		CHECK(!CheckPersonCommand(c, why));

		c.Act = PersonAct::MoneyAdd;
		c.Amount = 1000;
		CHECK(CheckPersonCommand(c, why));
		c.Amount = -200;
		CHECK(CheckPersonCommand(c, why));
		c.Amount = std::numeric_limits<double>::quiet_NaN();
		CHECK(!CheckPersonCommand(c, why));
		c.Amount = 2e9;				// 한 번에 백만까지
		CHECK(!CheckPersonCommand(c, why));

		// 모든 지식은 영주 전원에게도 된다(주민은 지식을 갖지 않는다). 소지금·소지품·지식 하나는 한 사람을 짚어서만.
		c.Act = PersonAct::KnowledgeAll;
		c.Who = "lords";
		CHECK(CheckPersonCommand(c, why));
		c.Who = "people";
		CHECK(!CheckPersonCommand(c, why));
		c.Who = "lords";
		c.Act = PersonAct::MoneyAdd;
		c.Amount = 100;
		CHECK(!CheckPersonCommand(c, why));
		c.Act = PersonAct::Cure;
		c.Who = "people";
		CHECK(CheckPersonCommand(c, why));
		CHECK(BulkAllowed("lords", PersonAct::KnowledgeAll) && !BulkAllowed("people", PersonAct::KnowledgeAll) && BulkAllowed("people", PersonAct::Happy)
			&& !BulkAllowed("lords", PersonAct::ItemAdd) && BulkAllowed("25556c3312bce178", PersonAct::ItemAdd));
	});

	Test("인물: 소지품의 번호는 1부터, 착용 중인 장비는 빼지 않는다", [] {
		std::string why;
		PersonCommand c;
		c.Who = "25556c3312bce178";
		c.Act = PersonAct::ItemAdd;
		c.Amount = 5;
		c.Index = 0;				// 0 번(룬)은 창도 내지 않는다. 원격으로도 건드리지 않는다
		CHECK(!CheckPersonCommand(c, why) && !why.empty());
		c.Index = 1;
		CHECK(CheckPersonCommand(c, why));
		c.Index = 199;
		CHECK(CheckPersonCommand(c, why));
		CHECK(!ParseRemoteLine("person 25556c3312bce178 item_add index=0 amount=5").Error.empty());

		// "0 으로" 단추는 -k_GiftMax 를 보낸다(가진 것까지만 빠진다). 그 수는 받아야 하고, 그것을 넘는 수는 받지 않는다.
		c.Index = 1;
		c.Amount = -k_GiftMax;
		CHECK(CheckPersonCommand(c, why));
		c.Amount = k_GiftMax;
		CHECK(CheckPersonCommand(c, why));
		c.Amount = k_GiftMax + 1;
		CHECK(!CheckPersonCommand(c, why));
		c.Amount = -(k_GiftMax + 1);
		CHECK(!CheckPersonCommand(c, why));

		// 착용 중인 장비의 자원 번호(__equipment.__cached_armor·first_arm·second_arm 의 __resource. 없으면 -1 이나 읽지 못한 수).
		// 그 칸은 소지품에서 빼지 않는다: 수만 줄고 착용은 그대로라 어긋난다(게임이 어떻게 받는지 재지 않았다).
		const std::vector<double> equipped = { 6, 10, -1 };
		CHECK(IsEquipped(6, equipped) && IsEquipped(10, equipped));
		CHECK(!IsEquipped(1, equipped) && !IsEquipped(-1, equipped) && !IsEquipped(0, {}));
		CHECK(!IsEquipped(6, { -1e9, std::numeric_limits<double>::quiet_NaN() }));
	});

	Test("인물: 주는 수는 정수로, 가진 것보다 많이 빼지 않는다", [] {
		double delta = 0;
		CHECK(GiftDelta(493, 1000, delta) && delta == 1000);
		CHECK(GiftDelta(493, -1000, delta) && delta == -493);		// 0 아래로 내려가지 않는다
		CHECK(GiftDelta(10, 2.6, delta) && delta == 3);
		CHECK(!GiftDelta(0, -5, delta));							// 뺄 것이 없다
		CHECK(!GiftDelta(5, 0.2, delta));							// 반올림하면 0 이다
		CHECK(!GiftDelta(std::numeric_limits<double>::quiet_NaN(), 5, delta) && !GiftDelta(5, std::numeric_limits<double>::infinity(), delta));
		CHECK(GiftDelta(-3, 10, delta) && delta == 10);			// 읽은 수가 음수여도 더하는 것은 된다
	});

	Test("원격 명령: 지식·소지금·소지품의 줄을 읽는다", [] {
		CHECK(ParseRemoteLine("person 25556c3312bce178 knowledge_all").Error.empty());
		CHECK(ParseRemoteLine("person lords knowledge_all").Error.empty());
		CHECK(!ParseRemoteLine("person people knowledge_all").Error.empty());
		RemoteCommand c = ParseRemoteLine("person 25556c3312bce178 knowledge_add name=building_mine");
		CHECK(c.Error.empty() && c.Options.at("name") == "building_mine");
		CHECK(!ParseRemoteLine("person 25556c3312bce178 knowledge_add").Error.empty());
		c = ParseRemoteLine("person 25556c3312bce178 money_add amount=1000");
		CHECK(c.Error.empty() && c.Number == 1000);
		c = ParseRemoteLine("person 25556c3312bce178 item_add index=1 amount=50");
		CHECK(c.Error.empty() && c.Options.at("index") == "1" && c.Number == 50);
		CHECK(!ParseRemoteLine("person 25556c3312bce178 item_add amount=50").Error.empty());
		CHECK(!ParseRemoteLine("person 25556c3312bce178 item_add index=500 amount=50").Error.empty());
		CHECK(!ParseRemoteLine("person lords money_add amount=5").Error.empty());
		CHECK(!ParseRemoteLine("person 25556c3312bce178 money_add amount=0").Error.empty());
	});

	Test("프리셋: 표의 확인된 항목만, 범위 안의 수로", [] {
		CHECK(Presets().size() == 4);
		std::vector<std::string> keys;
		for (const Preset& preset : Presets())
		{
			std::string why;
			CHECK(CheckPreset(preset, why));
			CHECK(std::find(keys.begin(), keys.end(), preset.Key) == keys.end());
			keys.push_back(preset.Key);
			CHECK(FindPreset(preset.Key) == &preset);
		}
		CHECK(FindPreset("normal") && FindPreset("normal")->Items.empty());		// 기본: 모두 끈다
		CHECK(FindPreset("easy") && FindPreset("sandbox") && FindPreset("god"));
		CHECK(!FindPreset("nope") && !FindPreset(""));

		// 확인 전의 항목, 없는 항목, 범위 밖의 수, 수가 없는 항목에 준 수, 겹친 항목, 값을 써 넣는 항목은 거부한다
		std::string why;
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "ally_invincible", 0 } } }, why));
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "no_such_cheat", 0 } } }, why));
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "production_time", 5 } } }, why));
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "production_time", 0 } } }, why));			// 배율 항목은 배율을 준다
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "build_free", 2 } } }, why));
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "build_free", 0 }, { "build_free", 0 } } }, why));
		CHECK(!CheckPreset(Preset{ "x", "x", "", { { "daily_migrants", 5 } } }, why));			// 세이브에 남는 값을 쓰는 항목은 묶음에 넣지 않는다
		CHECK(CheckPreset(Preset{ "x", "x", "", { { "build_free", 0 }, { "production_time", 0.5 } } }, why));

		// god 는 sandbox 가 켜는 것을 모두 켠다
		for (const PresetItem& item : FindPreset("sandbox")->Items)
		{
			bool found = false;
			for (const PresetItem& other : FindPreset("god")->Items)
				found = found || std::string(other.Id) == item.Id;
			CHECK(found);
		}

		RemoteCommand c = ParseRemoteLine("preset god");
		CHECK(c.Error.empty() && c.Verb == "preset" && c.Target == "god");
		CHECK(!ParseRemoteLine("preset").Error.empty() && !ParseRemoteLine("preset nope").Error.empty() && !ParseRemoteLine("preset god now").Error.empty());
		CHECK(ParseRemoteLine("time pause").Error.empty() && ParseRemoteLine("time resume").Target == "resume");
		CHECK(!ParseRemoteLine("time stop").Error.empty() && !ParseRemoteLine("time").Error.empty() && !ParseRemoteLine("time pause now").Error.empty());
	});

	Test("월드: 한 번 하는 일과 원격 명령", [] {
		WorldAct act = WorldAct::CooldownsClear;
		CHECK(ParseWorldAct("bishop", act) && act == WorldAct::BishopSend);
		CHECK(ParseWorldAct("cooldowns_clear", act) && act == WorldAct::CooldownsClear);
		CHECK(!ParseWorldAct("ambush", act) && !ParseWorldAct("", act));		// 궁수 매복은 불러서 게임이 끝났다(research/14). 넣지 않는다
		CHECK(std::string(WorldActWord(WorldAct::BishopSend)) == "bishop" && std::string(WorldActWord(WorldAct::CooldownsClear)) == "cooldowns_clear");

		// 이벤트 쿨다운 한 칸: 0 보다 큰 수에만 0 을 쓴다
		CHECK(ShouldClearCooldown(true, 19) && ShouldClearCooldown(true, 0.5));
		CHECK(!ShouldClearCooldown(true, 0) && !ShouldClearCooldown(true, -4) && !ShouldClearCooldown(false, 19));
		CHECK(!ShouldClearCooldown(true, std::numeric_limits<double>::quiet_NaN()));

		// 주교: 있는지 읽지 못했거나 이미 있으면 부르지 않는다
		CHECK(ChooseBishopStep(true, false) == BishopStep::Call);
		CHECK(ChooseBishopStep(true, true) == BishopStep::AlreadyHere);
		CHECK(ChooseBishopStep(false, false) == BishopStep::Unknown && ChooseBishopStep(false, true) == BishopStep::Unknown);

		// 쿨다운 지우기의 보고: 쓴 것, 쓰지 못한 것, 열지 못한 것을 그대로 적는다(검토의 지적)
		const ClearResult none;									// 열지 못했다
		const ClearResult two{ true, 2, 0 }, one{ true, 1, 0 }, empty{ true, 0, 0 }, part{ true, 1, 2 };
		CHECK(CooldownReport(two, one) == "이벤트 쿨다운 2개와 묶음 쿨다운 1개를 0 으로 썼습니다");
		CHECK(CooldownReport(empty, empty) == "지울 쿨다운이 없습니다 (0 보다 큰 칸이 없습니다)");
		CHECK(CooldownReport(part, one) == "이벤트 쿨다운 1개와 묶음 쿨다운 1개를 0 으로 썼습니다. 쓰지 못한 칸: 이벤트 2, 묶음 0");
		CHECK(CooldownReport(two, none) == "이벤트 쿨다운 2개를 0 으로 썼습니다. 묶음 쿨다운은 읽지 못했습니다");		// 앞의 쓰기를 숨기지 않는다
		CHECK(CooldownReport(none, one) == "묶음 쿨다운 1개를 0 으로 썼습니다. 이벤트 쿨다운은 읽지 못했습니다");
		CHECK(CooldownReport(none, none) == "이벤트 쿨다운을 읽지 못했습니다");
		CHECK(CooldownTouched(two, none) && CooldownTouched(none, part) && CooldownTouched(ClearResult{ true, 0, 3 }, empty));
		CHECK(!CooldownTouched(empty, empty) && !CooldownTouched(none, none));

		RemoteCommand c = ParseRemoteLine("world bishop");
		CHECK(c.Error.empty() && c.Verb == "world" && c.Target == "bishop");
		CHECK(ParseRemoteLine("world cooldowns_clear").Error.empty());
		CHECK(!ParseRemoteLine("world").Error.empty() && !ParseRemoteLine("world ambush").Error.empty() && !ParseRemoteLine("world bishop now").Error.empty());
	});

	Test("훅: 누구의 호출에 걸지와 self 의 묶음", [] {
		CHECK(HookApplies('a', true) && HookApplies('a', false));
		CHECK(HookApplies('p', true) && !HookApplies('p', false));
		CHECK(!HookApplies('o', true) && HookApplies('o', false));
		SelfSet set;
		CHECK(set.Size() == 0 && !set.Has(0x1000));
		set.Replace({ 0x3000, 0x1000, 0, 0x2000, 0x1000 });		// 0 은 버리고 겹친 것은 하나로
		CHECK(set.Size() == 3 && set.Has(0x1000) && set.Has(0x2000) && set.Has(0x3000));
		CHECK(!set.Has(0) && !set.Has(0x1800) && !set.Has(0x4000));
		set.Replace({});
		CHECK(set.Size() == 0 && !set.Has(0x1000));
	});

	Test("군대: 디버그 소환기의 종류와 원격 명령", [] {
		SpawnKind kind = SpawnKind::Soldier;
		CHECK(ParseSpawnKind("knight", kind) && kind == SpawnKind::Knight);
		CHECK(ParseSpawnKind("peasant", kind) && kind == SpawnKind::Peasant);
		CHECK(ParseSpawnKind("slave", kind) && ParseSpawnKind("lord", kind) && ParseSpawnKind("soldier", kind));
		CHECK(!ParseSpawnKind("bandit", kind) && !ParseSpawnKind("wolf", kind) && !ParseSpawnKind("", kind));		// 플레이어의 사람만 만든다
		CHECK(std::string(SpawnMethod(SpawnKind::Soldier)) == "__spawn_soldier" && std::string(SpawnMethod(SpawnKind::Lord)) == "__spawn_lord");
		CHECK(std::string(SpawnMethod(SpawnKind::Slave)) == "__spawn_slave");		// 소환기의 목록에는 "slaves" 지만 메서드는 __spawn_slave 다(research/13)
		CHECK(std::string(SpawnWord(SpawnKind::Peasant)) == "peasant");

		RemoteCommand c = ParseRemoteLine("person spawn knight");
		CHECK(c.Error.empty() && c.Verb == "person" && c.Target == "spawn" && c.Options.at("kind") == "knight");
		CHECK(!ParseRemoteLine("person spawn").Error.empty());
		CHECK(!ParseRemoteLine("person spawn bandit").Error.empty());
		CHECK(!ParseRemoteLine("person spawn knight 2").Error.empty());
	});

	Test("군대: 한 번에 만드는 병사의 수와 원격 명령", [] {
		int count = 0;
		CHECK(SoldierBatch(3, count) && count == 3);
		CHECK(SoldierBatch(1, count) && count == 1);
		CHECK(SoldierBatch(25, count) && count == 20);			// 한 번에 스무 명까지
		CHECK(SoldierBatch(2.6, count) && count == 3);
		CHECK(!SoldierBatch(0, count) && !SoldierBatch(-4, count) && !SoldierBatch(0.3, count));
		CHECK(!SoldierBatch(std::numeric_limits<double>::quiet_NaN(), count) && !SoldierBatch(std::numeric_limits<double>::infinity(), count));

		RemoteCommand c = ParseRemoteLine("person spawn_soldier amount=3");
		CHECK(c.Error.empty() && c.Verb == "person" && c.Target == "spawn_soldier" && c.Number == 3);
		CHECK(!ParseRemoteLine("person spawn_soldier").Error.empty());
		CHECK(!ParseRemoteLine("person spawn_soldier amount=0").Error.empty());
		CHECK(!ParseRemoteLine("person spawn_soldier amount=nan").Error.empty());
		CHECK(!ParseRemoteLine("person spawn_soldier amount=3 extra").Error.empty());
	});

	Test("인구: 켠 항목이 채워 둘 욕구의 번호", [] {
		CHECK(NeedsToHold(false, false, false).empty());
		CHECK(NeedsToHold(true, false, false) == std::vector<int>{ 1 });				// 음식
		CHECK((NeedsToHold(false, true, false) == std::vector<int>{ 0, 2 }));			// 수면, 휴식
		CHECK((NeedsToHold(true, true, false) == std::vector<int>{ 0, 1, 2 }));
		CHECK((NeedsToHold(false, false, true) == std::vector<int>{ 0, 1, 2, 3, 4, 5 }));
		CHECK((NeedsToHold(true, true, true) == std::vector<int>{ 0, 1, 2, 3, 4, 5 }));
	});

	Test("인구: 한 틱에 다루는 사람의 수를 묶는다", [] {
		// 300명을 40명씩: 여덟 틱에 한 바퀴. 끝에 닿으면 다음은 처음부터다.
		PeopleSlice slice = NextPeopleSlice(300, 0, 40);
		CHECK(slice.Begin == 0 && slice.End == 40 && slice.Next == 40 && !slice.Wrapped);
		slice = NextPeopleSlice(300, 280, 40);
		CHECK(slice.Begin == 280 && slice.End == 300 && slice.Next == 0 && slice.Wrapped);
		// 사람이 줄어 자리가 끝을 넘었으면 처음부터 다시 한다.
		slice = NextPeopleSlice(20, 280, 40);
		CHECK(slice.Begin == 0 && slice.End == 20 && slice.Next == 0 && slice.Wrapped);
		slice = NextPeopleSlice(0, 5, 40);
		CHECK(slice.Begin == 0 && slice.End == 0 && slice.Next == 0 && slice.Wrapped);
		slice = NextPeopleSlice(10, 0, 0);		// 묶음이 0 이어도 멈추지 않는다(한 명씩)
		CHECK(slice.Begin == 0 && slice.End == 1);
	});

	Test("원격 명령: 인물의 줄을 읽는다", [] {
		RemoteCommand c = ParseRemoteLine("person list");
		CHECK(c.Error.empty() && c.Verb == "person" && c.Target == "list");
		c = ParseRemoteLine("person list all=1");
		CHECK(c.Error.empty() && c.Options.at("all") == "1");
		c = ParseRemoteLine("person show 25556c3312bce178");
		CHECK(c.Error.empty() && c.Target == "show" && c.Options.at("who") == "25556c3312bce178");

		c = ParseRemoteLine("person 25556c3312bce178 skill_set index=4 amount=12");
		CHECK(c.Error.empty() && c.Target == "25556c3312bce178" && c.Options.at("act") == "skill_set" && c.Options.at("index") == "4" && c.Number == 12);
		c = ParseRemoteLine("person lords skills_max");
		CHECK(c.Error.empty() && c.Target == "lords" && c.Options.at("act") == "skills_max");
		c = ParseRemoteLine("person people needs_fill");
		CHECK(c.Error.empty() && c.Target == "people");
		c = ParseRemoteLine("person 25556c3312bce178 trait_add name=brave");
		CHECK(c.Error.empty() && c.Options.at("name") == "brave");
		c = ParseRemoteLine("person 25556c3312bce178 age_set amount=30");
		CHECK(c.Error.empty() && c.Number == 30);

		CHECK(!ParseRemoteLine("person").Error.empty());
		CHECK(!ParseRemoteLine("person lords").Error.empty());										// 무엇을 할지 없다
		CHECK(!ParseRemoteLine("person lords explode").Error.empty());
		CHECK(!ParseRemoteLine("person 25556c3312bce178 skill_set amount=12").Error.empty());			// 번호가 없다
		CHECK(!ParseRemoteLine("person 25556c3312bce178 skill_set index=9 amount=12").Error.empty());	// 능력치는 여덟이다
		CHECK(!ParseRemoteLine("person 25556c3312bce178 skill_set index=1").Error.empty());			// 수가 없다
		CHECK(!ParseRemoteLine("person 25556c3312bce178 trait_add").Error.empty());
		CHECK(!ParseRemoteLine("person 25556c3312bce178 trait_add name=Bad!").Error.empty());
		CHECK(!ParseRemoteLine("person show").Error.empty());
		CHECK(!ParseRemoteLine("person bad/who happy").Error.empty());								// 누구: 글자·숫자·밑줄만
	});

	Test("tools/probes 의 요청 파일은 모두 오류 없이 읽힌다", [] {
		int files = 0;
		for (const auto& entry : std::filesystem::directory_iterator(g_ProbesDir))
		{
			if (entry.path().extension() != ".txt")
				continue;
			std::ifstream in(entry.path());
			const Request r = ParseRequest(in);
			files++;
			for (const std::string& error : r.Errors)
			{
				std::printf("  FAIL %s: %s\n", entry.path().filename().string().c_str(), error.c_str());
				g_Failed++;
			}
		}
		CHECK(files > 0);
	});

	if (g_Failed)
	{
		std::printf("core tests: %d FAILED\n", g_Failed);
		return 1;
	}
	std::printf("core tests: %d passed\n", g_Passed);
	return 0;
}
