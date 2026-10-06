#include "common.hpp"

void RunLocalizationTests()
{
	Test("현지화 CSV 와 특성의 글", [] {
		// 지어낸 글이다(게임 파일의 글이 아니다). 꼴은 게임의 localization/main.csv 와 hints.csv 에서 본 것: 첫 줄이 머리, 쉼표, 따옴표 안의 쉼표·줄바꿈, "" 는 따옴표 하나.
		const std::string csv =
			"Key,Russian,English,Comments,Korean\r\n"
			"trait.brave,R1,Brave,,용감\r\n"
			"trait.quoted,\"a, b\",\"He said \"\"hi\"\"\",,\"첫 줄\n둘째 줄, 쉼표 \"\"따옴표\"\"\"\r\n"
			"trait.noko,X,OnlyEnglish,,\r\n"
			"other.key,a,b,,다른 것\r\n"
			"trait.brave,dup,Dup,,중복\r\n"
			"trait.short,only\r\n"
			"trait.last,R,Last,,마지막";
		std::unordered_map<std::string, std::string> out;
		std::string why;
		CHECK(ReadLocalization(csv, "trait.", { "Korean", "English" }, out, why) && why.empty());
		CHECK(out.size() == 4);
		CHECK_STR(out["trait.brave"], "용감");			// 같은 열쇠가 또 나오면 앞의 것을 둔다
		CHECK_STR(out["trait.quoted"], "첫 줄\n둘째 줄, 쉼표 \"따옴표\"");
		CHECK_STR(out["trait.noko"], "OnlyEnglish");	// 한국어 칸이 비면 다음 언어
		CHECK_STR(out["trait.last"], "마지막");			// 끝에 줄바꿈이 없는 마지막 줄
		CHECK(out.count("other.key") == 0 && out.count("trait.short") == 0);

		// BOM, LF 만 쓰는 파일, 열쇠의 앞머리가 빈 글이면 모두 받는다
		out.clear();
		CHECK(ReadLocalization("\xEF\xBB\xBF" "Code,English,Korean\nhint_a,A,가\nhint_b,B,\n", "", { "Korean" }, out, why) && out.size() == 1 && out["hint_a"] == "가");
		// 머리에 그 언어가 없다, 빈 글
		out.clear();
		CHECK(!ReadLocalization("Key,Russian,English\ntrait.brave,R,Brave\n", "trait.", { "Korean" }, out, why) && !why.empty() && out.empty());
		CHECK(!ReadLocalization("", "trait.", { "Korean" }, out, why) && !why.empty());
		// 닫히지 않은 따옴표로 끝난 파일: 그 앞의 줄들만 받는다(죽지 않는다)
		out.clear();
		CHECK(ReadLocalization("Key,Korean\ntrait.a,가\ntrait.b,\"나", "trait.", { "Korean" }, out, why) && out.size() == 1 && out["trait.a"] == "가");
		// 따옴표 안의 CRLF 는 줄바꿈 하나로
		out.clear();
		CHECK(ReadLocalization("Key,Korean\r\ntrait.a,\"가\r\n나\"\r\n", "trait.", { "Korean" }, out, why) && out["trait.a"] == "가\n나");

		// 머리에 Korean 이 없고 English 만 있는 파일: 다음 언어로 읽는다
		out.clear();
		CHECK(ReadLocalization("Key,Russian,English\ntrait.brave,R,Brave\n", "trait.", { "Korean", "English" }, out, why) && out["trait.brave"] == "Brave");
		// 머리 줄이 닫히지 않은 따옴표로 끝난다
		out.clear();
		CHECK(!ReadLocalization("Key,\"Korean\ntrait.a,가\n", "trait.", { "Korean" }, out, why) && !why.empty() && out.empty());

		CHECK_STR(TraitCaptionKey("sex_desire_weak"), "trait.sex_desire_weak");

		// 힌트의 글을 창에 보일 글로: 첫 줄이 이름과 같으면 떼고, 꺾쇠 표식은 지우고 안의 글은 두고, {자리}는 "(값)", [다른 힌트]는 지운다.
		CHECK_STR(PlainHint("용감\n싸움에서 <b>물러서지</b> 않습니다.", "용감"), "싸움에서 물러서지 않습니다.");
		CHECK_STR(PlainHint("<hint=hint_x>관심</hint>이 {beauty} 늘어납니다.", "매력"), "관심이 (값) 늘어납니다.");
		CHECK_STR(PlainHint("상처\n아픕니다.\n[hint_injury_remain_time]\n\n\n[hint_other]\n끝", "상처"), "아픕니다.\n\n끝");
		CHECK_STR(PlainHint("a<nbsp>b \xE2\x80\x94 c", ""), "a b - c");			// U+2014 는 글꼴에 없다
		CHECK_STR(PlainHint("이름이 다르면\n첫 줄을 둔다", "용감"), "이름이 다르면\n첫 줄을 둔다");
		CHECK_STR(PlainHint("a < b 이고 {닫히지 않음", ""), "a < b 이고 {닫히지 않음");
		CHECK_STR(PlainHint("[1] 과 [두 낱말] 은 둔다", ""), "[1] 과 [두 낱말] 은 둔다");
		CHECK_STR(PlainHint("  \n 앞뒤의 빈 줄 \n\n", ""), "앞뒤의 빈 줄");
		CHECK_STR(PlainHint("용감", "용감"), "");
		CHECK_STR(PlainHint("<img=spr_x></img>그림 뒤", ""), "그림 뒤");
		// 0xE2 로 시작하지만 긴 줄표가 아닌 글자는 그대로 둔다. 표식이 닫히지 않은 채 끝나는 글도 끝난다
		// (자리가 나아가지 않는 가지가 없다. 한 번 그런 가지로 시험이 멈췄다: [이름_ 을 읽던 줄).
		CHECK_STR(PlainHint("a\xE2\x80\xA6" "b", ""), "a\xE2\x80\xA6" "b");
		CHECK_STR(PlainHint("\xE2", ""), "\xE2");
		CHECK_STR(PlainHint("끝이 <", ""), "끝이 <");
		CHECK_STR(PlainHint("끝이 </", ""), "끝이 </");
		CHECK_STR(PlainHint("끝이 <b", ""), "끝이 <b");
		CHECK_STR(PlainHint("끝이 [ab_", ""), "끝이 [ab_");
		CHECK_STR(PlainHint("끝이 [a_b_c_d", ""), "끝이 [a_b_c_d");
		CHECK_STR(PlainHint("끝이 {", ""), "끝이 {");
		CHECK_STR(PlainHint("끝이 {ab", ""), "끝이 {ab");

		// 힌트의 글을 제목(첫 줄)과 본문으로 가른다: 특성의 힌트에서 첫 줄은 언제나 짧은 제목이었다(research/20). 둘 다 다듬는다.
		HintText split = SplitHint("용감함\n싸움에서 <b>물러서지</b> 않습니다.\n\n{time} 동안 이어집니다.");
		CHECK_STR(split.Title, "용감함");
		CHECK_STR(split.Body, "싸움에서 물러서지 않습니다.\n\n(값) 동안 이어집니다.");
		split = SplitHint("\n  <b>제목</b>  \n본문");
		CHECK_STR(split.Title, "제목");
		CHECK_STR(split.Body, "본문");
		split = SplitHint("제목뿐");
		CHECK(split.Title == "제목뿐" && split.Body.empty());
		split = SplitHint("");
		CHECK(split.Title.empty() && split.Body.empty());
		split = SplitHint("제목\r\n본문 첫 줄\r\n본문 둘째 줄");
		CHECK(split.Title == "제목" && split.Body == "본문 첫 줄\n본문 둘째 줄");

		// 게임의 속성 함수(trait_property_get(이름, 번호))의 배치가 잰 것과 같은가: 0번이 이름, 1번이 화면 이름의 열쇠("trait.<이름>").
		// 아니면(게임이 갱신돼 번호가 밀렸다) 설명의 열쇠(21번)를 믿지 않는다.
		CHECK(TraitLayoutOk("brave", "brave", "trait.brave"));
		// 1번(화면 이름의 열쇠)은 "trait.<이름>"이 아닐 수 있다: aging 의 1번은 "trait.oldman"이었다(research/20 의 실행 3). "trait."로 시작하면 배치가 맞는 것으로 본다.
		CHECK(TraitLayoutOk("aging", "aging", "trait.oldman") && !TraitLayoutOk("aging", "aging", "hint_oldman") && !TraitLayoutOk("aging", "aging", "trait."));
		// 화면 이름의 줄을 찾을 열쇠(main.csv 의 "trait." 뒤의 글): 게임이 1번으로 알려 준 열쇠를 쓴다. 묻지 못했으면 그 이름으로 찾는다.
		// 게임이 빈 글을 줬으면(안쪽 특성 "__…__") 이름의 줄이 없는 것이다.
		CHECK_STR(TraitCaptionRow("aging", true, "trait.oldman"), "oldman");
		CHECK_STR(TraitCaptionRow("brave", true, "trait.brave"), "brave");
		CHECK_STR(TraitCaptionRow("__criminal_surrender__", true, ""), "");
		CHECK_STR(TraitCaptionRow("brave", false, ""), "brave");
		CHECK_STR(TraitCaptionRow("brave", false, "trait.other"), "brave");
		CHECK_STR(TraitCaptionRow("brave", true, "something_else"), "");		// "trait."로 시작하지 않는 답은 화면 이름의 열쇠가 아니다
		CHECK_STR(TraitCaptionRow("brave", true, "trait."), "");
		CHECK(!TraitLayoutOk("brave", "trait.brave", "brave") && !TraitLayoutOk("brave", "brave", "") && !TraitLayoutOk("brave", "", "trait.brave")
			&& !TraitLayoutOk("brave", "calm", "trait.calm") && !TraitLayoutOk("", "", "trait."));
		// 배치를 확인할 특성: 화면 이름의 줄(trait.<이름>)이 있는 이름 가운데서 고른다. 이름순의 앞쪽은 "__…__" 꼴의 안쪽 특성이고
		// 그것들의 1번은 잰 적이 없다(게임의 이름 함수는 그런 이름에 빈 글을 돌려줬다. research/20).
		{
			const std::vector<std::string> names = { "__criminal_surrender__", "__fire_immunity__", "accurate_archer", "aging", "bald", "brave", "calm" };
			const std::unordered_map<std::string, std::string> captions = { { "accurate_archer", "가" }, { "bald", "나" }, { "brave", "다" }, { "calm", "라" }, { "ghost", "마" } };
			const std::vector<std::string> probes = TraitLayoutProbes(names, captions, 3);
			CHECK(probes.size() == 3 && probes[0] == "accurate_archer" && probes[1] == "bald" && probes[2] == "brave");
			CHECK(TraitLayoutProbes(names, captions, 10).size() == 4);		// 줄이 있는 것은 넷뿐이다(aging 과 __ 들은 없다)
			CHECK(TraitLayoutProbes({ "__a__", "__b__" }, captions, 3).empty() && TraitLayoutProbes(names, {}, 3).empty() && TraitLayoutProbes(names, captions, 0).empty());
			// 화면 이름이 빈 줄은 줄이 없는 것으로 친다
			CHECK(TraitLayoutProbes({ "x" }, { { "x", "" } }, 3).empty());
		}
		// 21번이 힌트의 열쇠가 맞는가의 양성 대조: 게임이 준 열쇠의 대부분이 힌트 파일에 있어야 한다(이 빌드: 246개 가운데 230개).
		// 절반도 없으면 번호가 밀린 것으로 보고 설명을 붙이지 않는다. 열쇠가 적으면(10개 미만) 판정하지 않는다.
		CHECK(HintKeysPlausible(246, 230) && HintKeysPlausible(246, 123) && !HintKeysPlausible(246, 122) && !HintKeysPlausible(246, 0));
		CHECK(HintKeysPlausible(0, 0) && HintKeysPlausible(9, 0) && !HintKeysPlausible(10, 4) && HintKeysPlausible(10, 5));
		// 힌트의 제목을 명칭으로 써도 되는가: 본문이 있고(한 줄뿐인 힌트의 글은 제목이 아니다) 제목이 짧다(이 빌드의 제목은 18자 이하. 한글 20자 = 60바이트까지).
		CHECK(GoodHintTitle({ "출혈", "피가 납니다." }) && !GoodHintTitle({ "출혈", "" }) && !GoodHintTitle({ "", "본문" }));
		CHECK(GoodHintTitle({ std::string(60, 'a'), "b" }) && !GoodHintTitle({ std::string(61, 'a'), "b" }));
		// 첫 줄이 표식뿐이라 다듬으면 비는 힌트: 다음 줄이 제목이다
		split = SplitHint("<img=spr_x></img>\n[hint_other_thing]\n진짜 제목\n본문");
		CHECK_STR(split.Title, "진짜 제목");
		CHECK_STR(split.Body, "본문");
		split = SplitHint("<b></b>\n \n");
		CHECK(split.Title.empty() && split.Body.empty());

		// 목록의 차례: 화면 이름이 있는 것을 그 이름의 차례로 먼저, 없는 것을 게임의 이름의 차례로 뒤에.
		CHECK(TraitBefore("zeal", "가", "ant", "나") && !TraitBefore("ant", "나", "zeal", "가"));
		CHECK(TraitBefore("zeal", "가", "ant", "") && !TraitBefore("ant", "", "zeal", "가"));
		CHECK(TraitBefore("ant", "", "bee", "") && !TraitBefore("bee", "", "ant", ""));
		CHECK(TraitBefore("ant", "같음", "bee", "같음") && !TraitBefore("ant", "가", "ant", "가"));

		// 찾기: 게임의 이름이나 화면 이름에 들어 있다(영문은 대소문자를 가리지 않는다)
		CHECK(TraitMatches("", "brave", "용감") && TraitMatches("brav", "brave", "용감") && TraitMatches("BRAV", "brave", "용감") && TraitMatches("용", "brave", "용감"));
		CHECK(!TraitMatches("x", "brave", "용감") && !TraitMatches("감용", "brave", "용감"));

		// 원격 명령
		const RemoteCommand traits = ParseRemoteLine("traits");
		CHECK(traits.Error.empty() && traits.Verb == "traits");
		const RemoteCommand found = ParseRemoteLine("traits find=brave max=5");
		CHECK(found.Error.empty() && found.Options.at("find") == "brave" && found.Options.at("max") == "5");
		CHECK(!ParseRemoteLine("traits fnd=x").Error.empty() && !ParseRemoteLine("traits max=0").Error.empty() && !ParseRemoteLine("traits max=x").Error.empty());
		for (const char* bad : { "traits max=inf", "traits max=1e30", "traits max=2.5", "traits max=-3", "traits max=100001" })
			CHECK(!ParseRemoteLine(bad).Error.empty());
		CHECK(ParseRemoteLine("traits max=100000").Error.empty());
	});

	Test("현지화: 게임 폴더가 주어지면 진짜 파일을 읽어 본다 (NLTOYBOX_TEST_GAME_DIR 이 없으면 건너뛴다)", [] {
		// 게임 파일의 글을 시험에 싣지 않는다: 여기서는 꼴만 본다(읽히는가, 수가 맞는가, 다듬은 글에 표식이 남지 않는가, 끝나는가).
		// 게임이 갱신된 뒤 다시 돌려 본다: $env:NLTOYBOX_TEST_GAME_DIR = <게임 폴더>; build\\nlcore_tests.exe tools\\probes
		// 일부러 도구들의 NORLAND_GAME_DIR 과 다른 이름을 쓴다: 그 변수를 늘 켜 둔 사람의 평소 시험이 게임 파일에 기대지 않게.
#pragma warning(suppress: 4996)		// getenv: 읽기만 한다
		const char* dir = std::getenv("NLTOYBOX_TEST_GAME_DIR");
		if (!dir || !*dir)
		{
			std::printf("  skip: NLTOYBOX_TEST_GAME_DIR 이 없다(평소 시험은 게임 파일에 기대지 않는다)\n");		// 돌았는지 안 돌았는지 보이게(2026-10-07 리뷰 R19)
			return;
		}
		const std::filesystem::path root = std::filesystem::path(dir) / "localization";
		const auto slurp = [&](const char* name, std::string& out) {
			std::ifstream in(root / name, std::ios::binary);
			if (!in)
				return false;
			out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
			return true;
		};
		std::string text, why;
		std::unordered_map<std::string, std::string> captions, hints;
		CHECK(slurp("main.csv", text) && ReadLocalization(text, "trait.", { "Korean", "English" }, captions, why));
		CHECK(captions.size() >= 200);
		size_t files = 0;
		for (const char* name : { "hints_tutorial.csv", "hints_with_icons.csv", "hints.csv" })
			if (slurp(name, text) && ReadLocalization(text, "", { "Korean", "English" }, hints, why))
				files++;
		CHECK(files == 3 && hints.size() >= 3000);
		size_t titled = 0, bodies = 0, tagged = 0, longest = 0;
		for (const auto& [key, raw] : hints)
		{
			const HintText split = SplitHint(raw);
			titled += split.Title.empty() ? 0 : 1;
			bodies += split.Body.empty() ? 0 : 1;
			longest = (std::max)(longest, split.Body.size());
			// 다듬은 글에 게임의 표식이 남지 않는다
			if (split.Body.find("<hint=") != std::string::npos || split.Body.find("</hint>") != std::string::npos || split.Body.find("<b>") != std::string::npos
				|| split.Title.find('<') != std::string::npos)
				tagged++;
		}
		CHECK(titled >= 3000 && bodies >= 2500 && tagged == 0);
		std::printf("  real files: %zu trait captions, %zu hints (%zu titled, %zu with a body, longest body %zu bytes)\n", captions.size(), hints.size(), titled, bodies, longest);
	});
}
