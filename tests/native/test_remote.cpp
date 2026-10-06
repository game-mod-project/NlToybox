#include "common.hpp"

void RunRemoteTests()
{
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

	Test("원격: 모드창에 입력을 넣는 줄(ui click, type, key)", [] {
		// 창의 입력 칸을 시험하려고 둔다: Dear ImGui 의 입력 큐에 넣을 뿐 게임 창과 진짜 마우스·키보드는 건드리지 않는다.
		RemoteCommand c = ParseRemoteLine("ui click x=640 y=355.5");
		CHECK(c.Error.empty() && c.Verb == "ui" && c.Target == "click" && c.Options.at("x") == "640" && c.Options.at("y") == "355.5");
		c = ParseRemoteLine("ui type text=200");
		CHECK(c.Error.empty() && c.Verb == "ui" && c.Target == "type" && c.Options.at("text") == "200");
		c = ParseRemoteLine("ui key name=enter");
		CHECK(c.Error.empty() && c.Verb == "ui" && c.Target == "key" && c.Options.at("name") == "enter");
		for (const char* name : { "tab", "escape", "backspace" })
			CHECK(ParseRemoteLine(std::string("ui key name=") + name).Error.empty());
		for (const char* bad : { "ui", "ui click", "ui click x=1", "ui click x=a y=2", "ui click x=-5 y=2", "ui click x=1 y=99999", "ui click x=1 y=2 z=3",
				"ui type", "ui type text=", "ui type text=<b>", "ui type text=1 x=2", "ui key", "ui key name=f8", "ui key name=enter extra=1", "ui hover x=1 y=2" })
			CHECK(!ParseRemoteLine(bad).Error.empty());
		CHECK(!ParseRemoteLine("ui type text=" + std::string(40, '1')).Error.empty());		// 길이의 한도(32자)
	});
}
