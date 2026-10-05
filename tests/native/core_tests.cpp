// 러너에 기대지 않는 코어(src/core)의 시험. 게임을 켜지 않는다.
// 사용: nlcore_tests.exe <요청 파일 폴더>     (tools/test-native.ps1 이 부른다)

#include "core/AskPath.hpp"
#include "core/Binding.hpp"
#include "core/CallLog.hpp"
#include "core/CheatState.hpp"
#include "core/CheatTable.hpp"
#include "core/EconomyPlan.hpp"
#include "core/Knobs.hpp"
#include "core/PathTable.hpp"
#include "core/Rate.hpp"
#include "core/RemoteCommand.hpp"
#include "core/Request.hpp"
#include "core/Schedule.hpp"
#include "core/SpeedControl.hpp"
#include "core/SpeedTrial.hpp"
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

	// 가짜 게임: 손잡이 넷 가운데 Live 번이 속도를 정한다(값이 0 보다 클 때. 아니면 배율 1). 흐름 = 57.4 × 배율.
	struct FakeGame
	{
		double Values[4] = { 1, 1, -4, 1 };
		int Live = 0;					// -1 이면 아무 손잡이도 듣지 않는다
		bool Overwrites = false;		// 쓰지 않는 틱마다 게임이 Live 손잡이를 Reset 으로 되돌린다
		double Reset = 1;
		bool Unreadable[4] = {};
		int PauseAtTick = -1;			// 이 틱부터 흐름이 0 이다

		double Flow(int Tick) const
		{
			if (PauseAtTick >= 0 && Tick >= PauseAtTick)
				return 0;
			return 57.4 * (Live >= 0 && Values[Live] > 0 ? Values[Live] : 1);
		}
	};

	// 시험을 끝까지 돌린다(0.1초 간격). 바깥이 할 일을 가짜 게임에 그대로 한다.
	void RunTrial(SpeedTrial& Trial, FakeGame& Game, double Warp = 1)
	{
		if (!Trial.Start(Warp))
			return;
		double now = 10;
		for (int tick = 1; tick <= 400 && Trial.Running(); tick++)
		{
			now += 0.1;
			const int candidate = Trial.Candidate();
			const SpeedStep step = Trial.Tick(now, !Game.Unreadable[candidate], Game.Values[candidate], Game.Flow(tick));
			if (step.What != SpeedStep::Kind::None)
				Game.Values[step.Candidate] = step.Value;
			else if (Game.Overwrites && Game.Live >= 0)
				Game.Values[Game.Live] = Game.Reset;
		}
	}

	// 가짜 세계: 주소 → 값. 게임 시간 "t" 는 Advance 가 흘린다. 속도는 Live 주소의 값이 정한다(0 보다 클 때. 아니면 1).
	struct FakeWorld
	{
		std::map<std::string, double> Values;
		std::string Live = "new";			// "variants" 면 variants[index]. "none" 이면 아무 값도 듣지 않는다
		bool Paused = false;
		double OverwriteEvery = 0;			// 0 보다 크면 게임이 이 간격으로 Live 의 값을 1 로 되돌린다
		double SinceOverwrite = 0;
		int Writes = 0;
		std::vector<std::string> Lines;

		FakeWorld() { Create(); }

		// 시간 컨트롤러가 (다시) 만들어졌다.
		void Create()
		{
			Values = { { "t", 1000 }, { "warp", 1 }, { "new", 1 }, { "custom", -4 }, { "index", 0 },
				{ "variants[0]", 1 }, { "variants[1]", 3 }, { "variants[2]", 6 }, { "variants[3]", 12 } };
		}

		std::string LiveKey() const
		{
			if (Live != "variants")
				return Live;
			const auto index = Values.find("index");
			return index == Values.end() ? "none" : "variants[" + Shortest(index->second) + "]";
		}

		void Advance(double Seconds)
		{
			const auto live = Values.find(LiveKey());
			const double factor = live != Values.end() && live->second > 0 ? live->second : 1;
			if (!Paused && Values.count("t"))
				Values["t"] += 57.4 * factor * Seconds;
			SinceOverwrite += Seconds;
			if (OverwriteEvery > 0 && SinceOverwrite >= OverwriteEvery && live != Values.end())
			{
				SinceOverwrite = 0;
				live->second = 1;
			}
		}

		SpeedIo Io()
		{
			return {
				[this](const std::string& path, double& out) {
					const auto it = Values.find(path);
					if (it == Values.end())
						return false;
					out = it->second;
					return true;
				},
				[this](const std::string& path, double value, std::string& why) {
					const auto it = Values.find(path);
					if (it == Values.end())
					{
						why = "no such value";
						return false;
					}
					it->second = value;
					Writes++;
					return true;
				},
				[this](const std::string& line) { Lines.push_back(line); },
			};
		}
	};

	SpeedPaths FakePaths()
	{
		return { "t", "warp", { { "new", "new", "", "" }, { "warp", "warp", "", "" }, { "custom", "custom", "", "" },
			{ "variants", "", "variants", "index" } } };
	}

	// 세계를 Seconds 초 흘린다(5ms 간격). 그 사이 속도 조절의 틱을 돌린다.
	void RunWorld(FakeWorld& World, SpeedControl& Control, double& Now, double Seconds, bool Watching = true)
	{
		const int steps = static_cast<int>(Seconds / 0.005 + 0.5);
		for (int i = 0; i < steps; i++)
		{
			Now += 0.005;
			World.Advance(0.005);
			Control.Tick(Now, Watching);
		}
	}

	// 가짜 세계의 손잡이들이 처음 값 그대로인가.
	bool Untouched(const FakeWorld& World)
	{
		FakeWorld fresh;
		for (const char* key : { "warp", "new", "custom", "variants[0]", "variants[1]", "variants[2]", "variants[3]" })
			if (World.Values.at(key) != fresh.Values.at(key))
				return false;
		return true;
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
		CHECK_STR(CategoryLabel("unknown"), "unknown");
	});

	Test("경제: 명령을 변화량으로 푼다", [] {
		const std::vector<double> counts = { 0, 300, 0, 5 };
		const std::vector<int> stocked = { 1, 2, 3 };
		const auto one = [&](EconomyAct act, int resource, double amount, double gold = 3000) {
			return PlanEconomy({ act, resource, amount }, gold, counts, stocked);
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
	});

	Test("경제: 원격 명령의 낱말", [] {
		EconomyAct act = EconomyAct::GoldAdd;
		CHECK(ParseEconomyAct("gold_set", act) && act == EconomyAct::GoldSet && !NeedsResource(act));
		CHECK(ParseEconomyAct("add", act) && act == EconomyAct::ResourceAdd && NeedsResource(act));
		CHECK(ParseEconomyAct("set", act) && act == EconomyAct::ResourceSet && NeedsResource(act));
		CHECK(ParseEconomyAct("all", act) && act == EconomyAct::AllAdd && !NeedsResource(act));
		CHECK(ParseEconomyAct("gold_add", act) && act == EconomyAct::GoldAdd && !ParseEconomyAct("gold", act) && !ParseEconomyAct("", act));
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

	Test("속도 시험: 첫 후보가 들으면 고르고, 쓰기를 멈춰도 남으면 한 번 쓰기다", [] {
		FakeGame game;
		SpeedTrial trial(4);
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Done && trial.Candidate() == 0 && trial.Sticky());
		CHECK(trial.SavedOld() == 1 && trial.ProbeWarp() == 2 && std::abs(trial.UnitRate() - 57.4) < 1e-9);
		CHECK(trial.Attempts().size() == 1 && trial.Attempts()[0].Matched);
	});

	Test("속도 시험: 듣지 않는 후보는 원래 값으로 되돌리고 다음으로 간다", [] {
		FakeGame game;
		game.Live = 2;					// __debug_custom_wrap 의 자리. 원래 값은 -4 다
		SpeedTrial trial(4);
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Done && trial.Candidate() == 2 && trial.SavedOld() == -4);
		CHECK(game.Values[0] == 1 && game.Values[1] == 1 && game.Values[3] == 1);
		CHECK(trial.Attempts().size() == 3 && !trial.Attempts()[0].Matched && !trial.Attempts()[1].Matched && trial.Attempts()[2].Matched);
		CHECK(trial.Attempts()[0].Readable && trial.Attempts()[0].Old == 1);
	});

	Test("속도 시험: 게임이 값을 되돌리면 계속 쓰기다", [] {
		FakeGame game;
		game.Live = 1;
		game.Overwrites = true;
		SpeedTrial trial(4);
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Done && trial.Candidate() == 1 && !trial.Sticky());
	});

	Test("속도 시험: 아무 후보도 듣지 않으면 실패하고 모두 원래 값이다", [] {
		FakeGame game;
		game.Live = -1;
		SpeedTrial trial(4);
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Failed && trial.Candidate() == -1 && !trial.Running());
		CHECK(game.Values[0] == 1 && game.Values[1] == 1 && game.Values[2] == -4 && game.Values[3] == 1);
		CHECK(trial.Attempts().size() == 4);
	});

	Test("속도 시험: 읽을 수 없는 후보는 건드리지 않는다", [] {
		FakeGame game;
		game.Live = 1;
		game.Unreadable[0] = true;
		game.Values[0] = 123;			// 건드렸다면 바뀐다
		SpeedTrial trial(4);
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Done && trial.Candidate() == 1);
		CHECK(game.Values[0] == 123 && !trial.Attempts()[0].Readable && !trial.Attempts()[0].Matched);
	});

	Test("속도 시험: 지금 속도가 1 이 아니어도 고른다", [] {
		FakeGame game;
		game.Values[0] = 2;				// 게임이 2배속이다
		game.Values[1] = 2;
		SpeedTrial trial(4);
		RunTrial(trial, game, 2);
		CHECK(trial.State() == SpeedTrial::Phase::Done && trial.Candidate() == 0 && trial.SavedOld() == 2);
		CHECK(trial.ProbeWarp() == 4 && std::abs(trial.UnitRate() - 57.4) < 1e-9);
	});

	Test("속도 시험: 시험 중에 멈추면 그만두고 원래 값을 되돌린다", [] {
		FakeGame game;
		game.PauseAtTick = 25;			// 첫 후보를 쓰는 도중에 일시정지(기준을 재는 1.2초가 지난 뒤)
		SpeedTrial trial(4);
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Aborted && !trial.Running());
		CHECK(game.Values[0] == 1 && trial.Attempts().empty());

		game.PauseAtTick = -1;			// 풀고 다시 누르면 된다
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Done && trial.Candidate() == 0);
	});

	Test("속도 시험: 기준은 시작한 뒤에 잰다. 그동안 멈춰 있으면 아무것도 쓰지 않고 그만둔다", [] {
		FakeGame game;
		game.PauseAtTick = 0;			// 처음부터 멈춰 있다
		SpeedTrial trial(4);
		RunTrial(trial, game);
		CHECK(trial.State() == SpeedTrial::Phase::Aborted && trial.Attempts().empty());
		CHECK(game.Values[0] == 1 && game.Values[1] == 1 && game.Values[2] == -4 && game.Values[3] == 1);

		SpeedTrial fresh(4);
		CHECK(fresh.UnitRate() == 0);				// 재기 전에는 기준이 없다
	});

	Test("속도 시험: 배율이 0 이하이면 시작하지 않고, 그만두면 처음으로 돌아간다", [] {
		SpeedTrial trial(4);
		CHECK(!trial.Start(0) && !trial.Start(-1) && trial.State() == SpeedTrial::Phase::Idle);
		CHECK(trial.Start(1) && trial.Running() && !trial.Start(1));		// 시험 중에는 다시 시작하지 않는다
		trial.Cancel();
		CHECK(!trial.Running() && trial.State() == SpeedTrial::Phase::Idle && trial.Candidate() == -1);
		CHECK(trial.Start(1));
	});

	Test("속도: 첫 손잡이가 들으면 고르고 누른 배율을 건다", [] {
		FakeWorld world;
		SpeedControl control(FakePaths(), world.Io());
		double now = 0;
		RunWorld(world, control, now, 1);
		control.Press(5);
		RunWorld(world, control, now, 8);
		CHECK(control.Chosen() && control.Sticky() && !control.Busy() && control.Wanted() == 5);
		CHECK_STR(control.ChosenLabel(), "new");
		CHECK(world.Values["new"] == 5 && world.Values["warp"] == 1 && world.Values["custom"] == -4 && world.Values["variants[0]"] == 1);
		CHECK(std::abs(control.UnitRate() - 57.4) < 1 && std::abs(control.Flow() - 57.4 * 5) < 10);
	});

	Test("속도: 일시정지를 푼 직후에 눌러도 고른다", [] {
		FakeWorld world;
		SpeedControl control(FakePaths(), world.Io());
		double now = 0;
		world.Paused = true;
		RunWorld(world, control, now, 3);
		world.Paused = false;			// 흐름의 창에는 멈춰 있던 표본이 남아 있다
		control.Press(5);
		RunWorld(world, control, now, 8);
		CHECK(control.Chosen() && control.Wanted() == 5 && world.Values["new"] == 5);
		CHECK(std::abs(control.UnitRate() - 57.4) < 1);
	});

	Test("속도: 창을 닫아 두었다가 열고 바로 눌러도 고른다", [] {
		FakeWorld world;
		SpeedControl control(FakePaths(), world.Io());
		double now = 0;
		RunWorld(world, control, now, 1);
		world.Paused = true;
		RunWorld(world, control, now, 30, false);		// 창이 닫혀 있다: 흐름을 재지 않는다
		world.Paused = false;
		RunWorld(world, control, now, 0.2);
		control.Press(2);
		RunWorld(world, control, now, 8);
		CHECK(control.Chosen() && control.Wanted() == 2 && world.Values["new"] == 2);
		CHECK(std::abs(control.UnitRate() - 57.4) < 1);
	});

	Test("속도: 멈춰 있으면 아무것도 쓰지 않고 알린다", [] {
		FakeWorld world;
		SpeedControl control(FakePaths(), world.Io());
		double now = 0;
		world.Paused = true;
		world.Values["warp"] = 0;		// 메뉴: 배율이 0 이다
		control.Press(5);
		RunWorld(world, control, now, 2);
		CHECK(!control.Busy() && !control.Chosen() && control.Wanted() == 0 && world.Writes == 0 && !control.Note().empty());

		world.Values["warp"] = 1;		// 게임 화면의 일시정지: 배율은 1 인데 시간이 흐르지 않는다
		control.Press(5);
		RunWorld(world, control, now, 3);
		CHECK(!control.Busy() && !control.Chosen() && control.Wanted() == 0 && world.Writes == 0 && !control.Note().empty());
	});

	Test("속도: 게임에 맡기면 원래 값으로 되돌리고, 다시 누르면 시험 없이 건다", [] {
		FakeWorld world;
		SpeedControl control(FakePaths(), world.Io());
		double now = 0;
		control.Press(5);
		RunWorld(world, control, now, 8);
		control.Release();
		RunWorld(world, control, now, 0.5);
		CHECK(control.Wanted() == 0 && Untouched(world) && control.Chosen());

		control.Press(2);
		RunWorld(world, control, now, 0.3);
		CHECK(!control.Busy() && control.Wanted() == 2 && world.Values["new"] == 2);
	});

	Test("속도: 자리가 바뀌는 손잡이는 쓴 자리를 되돌리고 새 자리에 건다", [] {
		FakeWorld world;
		world.Live = "variants";
		SpeedControl control(FakePaths(), world.Io());
		double now = 0;
		control.Press(5);
		RunWorld(world, control, now, 12);
		CHECK(control.Chosen() && world.Values["variants[0]"] == 5);
		CHECK_STR(control.ChosenLabel(), "variants");
		CHECK(world.Values["new"] == 1 && world.Values["warp"] == 1 && world.Values["custom"] == -4);		// 앞의 셋은 되돌렸다

		world.Values["index"] = 2;		// 사용자가 게임의 속도 단추를 눌렀다
		RunWorld(world, control, now, 0.5);
		CHECK(world.Values["variants[0]"] == 1 && world.Values["variants[2]"] == 5);

		control.Release();
		RunWorld(world, control, now, 0.5);
		CHECK(Untouched(world));
	});

	Test("속도: 시험 중에 끄면 그만두고 되돌린다. 나중에 걸리지 않는다", [] {
		FakeWorld world;
		SpeedControl control(FakePaths(), world.Io());
		double now = 0;
		control.Press(5);
		RunWorld(world, control, now, 2);
		CHECK(control.Busy() && world.Values["new"] == 2);		// 첫 후보에 시험 값을 쓰는 중이다
		control.Release();
		RunWorld(world, control, now, 0.5);
		CHECK(!control.Busy() && control.Wanted() == 0 && Untouched(world));
		RunWorld(world, control, now, 10);
		CHECK(control.Wanted() == 0 && Untouched(world) && !control.Chosen());
	});

	Test("속도: 시간 컨트롤러가 사라졌다 돌아오면 다시 건다", [] {
		FakeWorld world;
		SpeedControl control(FakePaths(), world.Io());
		double now = 0;
		control.Press(5);
		RunWorld(world, control, now, 8);
		CHECK(world.Values["new"] == 5);

		world.Values.clear();			// 새 게임을 만드는 중: 컨트롤러가 없다
		RunWorld(world, control, now, 1);
		CHECK(!control.TimeFound() && control.Wanted() == 5);
		world.Create();
		RunWorld(world, control, now, 1);
		CHECK(control.TimeFound() && world.Values["new"] == 5 && control.Wanted() == 5);

		control.Release();				// 돌아온 뒤의 원래 값(1)으로 되돌린다
		RunWorld(world, control, now, 0.5);
		CHECK(Untouched(world));
	});

	Test("속도: 게임이 값을 되돌리면 계속 써 넣는다", [] {
		FakeWorld world;
		world.OverwriteEvery = 0.1;
		SpeedControl control(FakePaths(), world.Io());
		double now = 0;
		control.Press(5);
		RunWorld(world, control, now, 9);
		CHECK(control.Chosen() && !control.Sticky() && control.Wanted() == 5);
		RunWorld(world, control, now, 2);
		CHECK(control.Flow() > 57.4 * 4);		// 0.1초마다 되돌려져도 대부분의 시간은 x5 다
	});

	Test("속도: 아무 손잡이도 듣지 않으면 실패하고 모두 원래 값이다", [] {
		FakeWorld world;
		world.Live = "none";
		SpeedControl control(FakePaths(), world.Io());
		double now = 0;
		control.Press(5);
		RunWorld(world, control, now, 14);
		CHECK(!control.Busy() && !control.Chosen() && control.Wanted() == 0 && Untouched(world) && !control.Note().empty());
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
			"economy set resource=-1 amount=5", "economy add resource=1e300 amount=5", "page", "page nowhere", "page economy now" })
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
