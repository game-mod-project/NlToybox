// 러너에 기대지 않는 코어(src/core)의 시험. 게임을 켜지 않는다.
// 사용: nlcore_tests.exe <요청 파일 폴더>     (tools/test-native.ps1 이 부른다)

#include "core/AskPath.hpp"
#include "core/CheatState.hpp"
#include "core/CheatTable.hpp"
#include "core/Knobs.hpp"
#include "core/PathTable.hpp"
#include "core/Rate.hpp"
#include "core/Request.hpp"
#include "core/Schedule.hpp"
#include "core/Text.hpp"

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
			if (!path.Error.empty() || path.Steps.empty())
			{
				std::printf("  FAIL %s: %s (%s)\n", cheat.Id, cheat.Path, path.Error.c_str());
				g_Failed++;
			}
			CHECK(path.Root == "global" || path.Root == "inst");		// 표에는 ds 번호를 적지 않는다
			CHECK(ids.insert(cheat.Id).second);
			CHECK(std::string(cheat.Id).find_first_of(" =") == std::string::npos);
			CHECK(cheat.Label[0] != 0 && cheat.Help[0] != 0);
			if (cheat.Kind == CheatKind::Toggle)
				CHECK(cheat.On != cheat.Off);
			else
				CHECK(cheat.Min < cheat.Max);
		}
		CHECK(Cheats().size() == 31);
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
		CHECK(Areas().size() == 17);
		CHECK(FindArea("nope") == nullptr);
		CHECK_STR(GetArea(Area::Time).Key, "time");
		for (const Cheat& cheat : Cheats())
			CHECK(FindArea(GetArea(cheat.Where).Key) != nullptr);
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

	// 요청 파일의 오타로 게임 실행 한 번을 버리지 않는다.
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
