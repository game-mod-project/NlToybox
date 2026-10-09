#include "common.hpp"

namespace
{
	Request Parse(const std::string& Text)
	{
		std::istringstream in(Text);
		return ParseRequest(in);
	}
}

void RunRequestTests()
{
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
}
