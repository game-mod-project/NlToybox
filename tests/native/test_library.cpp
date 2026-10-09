#include "common.hpp"

void RunLibraryTests()
{
	Test("도서관: 지식의 이름, 명령, 원격 줄", [] {
		// 지식의 이름: 소문자·숫자·밑줄, 그리고 느낌표(게임의 이름 둘에 있다: 41!building_wooden_wall_section. research/33).
		CHECK(GoodKnowledgeName("tech_clay") && GoodKnowledgeName("skill_oratory_3") && GoodKnowledgeName("41!building_wooden_wall_section") && GoodKnowledgeName(std::string(64, 'a')));
		CHECK(!GoodKnowledgeName("") && !GoodKnowledgeName("Tech_clay") && !GoodKnowledgeName("a b") && !GoodKnowledgeName("a-b") && !GoodKnowledgeName("a.b") && !GoodKnowledgeName("a=b")
			&& !GoodKnowledgeName(std::string(65, 'a')));

		LibraryCommand command;
		std::string why;
		CHECK(ParseLibraryCommand({ "list" }, command, why) && command.Act == LibraryAct::List && command.Name.empty() && command.Find.empty());
		CHECK(ParseLibraryCommand({ "list", "find=mine" }, command, why) && command.Act == LibraryAct::List && command.Find == "mine" && command.Name.empty());
		CHECK(ParseLibraryCommand({ "add", "name=tech_clay" }, command, why) && command.Act == LibraryAct::Add && command.Name == "tech_clay" && command.Find.empty());
		CHECK(ParseLibraryCommand({ "remove", "name=42!building_fortification_tower" }, command, why) && command.Act == LibraryAct::Remove && command.Name == "42!building_fortification_tower");
		CHECK(ParseLibraryCommand({ "add_all" }, command, why) && command.Act == LibraryAct::AddAll && command.Name.empty());
		CHECK(ParseLibraryCommand({ "undo" }, command, why) && command.Act == LibraryAct::Undo);
		// 틀린 꼴은 받지 않고 까닭을 적는다. 받지 않은 명령은 Out 을 바꾸지 않는다.
		command = LibraryCommand{ LibraryAct::Undo, "kept", "" };
		for (const std::vector<std::string>& words : std::vector<std::vector<std::string>>{ {}, { "burn" }, { "add" }, { "add", "tech_clay" }, { "add", "name=" }, { "add", "name=A" },
			{ "add", "name=a", "name=b" }, { "remove" }, { "remove", "find=a" }, { "add_all", "name=a" }, { "undo", "now" }, { "list", "name=a" }, { "list", "find=" }, { "list", "find=a", "x" },
			{ "list", "find=" + std::string(49, 'a') } })
		{
			why.clear();
			CHECK(!ParseLibraryCommand(words, command, why) && !why.empty());
		}
		CHECK(command.Act == LibraryAct::Undo && command.Name == "kept");
		CHECK_STR(LibraryActWord(LibraryAct::List), "list");
		CHECK_STR(LibraryActWord(LibraryAct::Add), "add");
		CHECK_STR(LibraryActWord(LibraryAct::AddAll), "add_all");
		CHECK_STR(LibraryActWord(LibraryAct::Remove), "remove");
		CHECK_STR(LibraryActWord(LibraryAct::Undo), "undo");

		// 원격 줄: 할 일은 Target 에, 이름과 찾는 글은 옵션에(src/Remote.cpp 의 DoLibrary 가 다시 읽는다).
		const auto option = [](const RemoteCommand& c, const char* key) {
			const auto at = c.Options.find(key);
			return at == c.Options.end() ? std::string() : at->second;
		};
		const RemoteCommand add = ParseRemoteLine("library add name=tech_clay");
		CHECK(add.Error.empty() && add.Verb == "library" && add.Target == "add" && option(add, "name") == "tech_clay");
		CHECK(ParseRemoteLine("library list").Error.empty() && ParseRemoteLine("library list").Target == "list" && option(ParseRemoteLine("library list find=mine"), "find") == "mine");
		CHECK(ParseRemoteLine("library add_all").Error.empty() && ParseRemoteLine("library add_all").Target == "add_all" && ParseRemoteLine("library undo").Error.empty());
		CHECK(option(ParseRemoteLine("library remove name=41!building_wooden_wall_section"), "name") == "41!building_wooden_wall_section");
		CHECK(!ParseRemoteLine("library").Error.empty() && !ParseRemoteLine("library add").Error.empty() && !ParseRemoteLine("library add_all now").Error.empty()
			&& !ParseRemoteLine("library burn").Error.empty() && !ParseRemoteLine("library add name=Tech").Error.empty());
	});

	Test("도서관: 책의 표, 넣을 책, 판정, 결과의 글", [] {
		// 책의 표. 이름과 글은 지어낸 것이다. 셋째는 이름을 읽지 못한 칸(자리를 지킨다).
		const std::vector<Book> books = {
			{ "alpha", "알파", "economic", 1 }, { "beta", "베타", "textbooks", 0 }, { "", "", "", 0 }, { "gamma", "감마", "cultural_knowledge", 2 }, { "delta", "", "mystery", 0 },
		};
		// 갈래의 말은 모드가 붙인 것이다. 모르는 갈래는 게임의 이름 그대로, 빈 갈래는 줄표.
		CHECK_STR(BookCategoryLabel("economic"), "경제");
		CHECK_STR(BookCategoryLabel("textbooks"), "교과서");
		CHECK_STR(BookCategoryLabel("cultural_knowledge"), "문화");
		CHECK_STR(BookCategoryLabel("mystery"), "mystery");
		CHECK_STR(BookCategoryLabel(""), "-");
		// 보일 이름: 화면 이름, 없으면 게임의 이름.
		CHECK_STR(BookLabel(books[0]), "알파");
		CHECK_STR(BookLabel(books[4]), "delta");

		// "모든 책 넣기"가 넣을 것: 권수가 0 이고 이름이 있는 것만(받은 차례).
		CHECK(BooksToAdd(books) == (std::vector<std::string>{ "beta", "delta" }));
		CHECK(BooksToAdd({}).empty() && BooksToAdd({ { "alpha", "", "", 3 } }).empty());
		// 요약: 도서관에 있는 종류와 권수, 게임의 지식의 수. 이름을 읽지 못한 칸은 따로 적는다("없음"과 가른다).
		CHECK_STR(LibrarySummary(books), "도서관의 책 2종 (3권) / 게임의 지식 4가지, 이름을 읽지 못한 지식 1가지");
		CHECK_STR(LibrarySummary({ { "alpha", "", "", 0 } }), "도서관의 책 0종 (0권) / 게임의 지식 1가지");
		CHECK_STR(LibrarySummary({}), "도서관의 책 0종 (0권) / 게임의 지식 0가지");
		// 한 줄(원격 list): 게임의 이름, 권수, 화면 이름, 갈래.
		CHECK_STR(BookLine(books[0]), "alpha  1권  알파  (경제)");
		CHECK_STR(BookLine(books[1]), "beta  없음  베타  (교과서)");
		CHECK_STR(BookLine(books[4]), "delta  없음  -  (mystery)");
		// 표에 보일 줄: 찾는 글이 이름이나 화면 이름에 들어 있다(빈 글은 모두). "도서관에 있는 책만"이면 권수가 있는 것만. 이름을 읽지 못한 칸은 보이지 않는다.
		CHECK(BookShown(books[0], "", false) && BookShown(books[1], "", false) && !BookShown(books[2], "", false));
		CHECK(BookShown(books[0], "", true) && !BookShown(books[1], "", true) && BookShown(books[3], "", true));
		CHECK(BookShown(books[0], "alp", false) && BookShown(books[0], "알", false) && !BookShown(books[0], "bet", false) && BookShown(books[4], "elt", false));
		CHECK(!BookShown(books[1], "bet", true) && BookShown(books[3], "감마", true));

		// 부른 뒤의 판정: 권수를 앞뒤로 읽어 정확히 그만큼 달라졌으면 됐다. 그대로면 안 된 것. 읽지 못했거나 다른 만큼 달라졌으면 모른다(됐다고 적지 않는다).
		CHECK(AfterBookChange(true, 0, true, 1, 1) == BookOutcome::Done && AfterBookChange(true, 2, true, 1, -1) == BookOutcome::Done && AfterBookChange(true, 1, true, 0, -1) == BookOutcome::Done);
		CHECK(AfterBookChange(true, 0, true, 0, 1) == BookOutcome::Same && AfterBookChange(true, 2, true, 2, -1) == BookOutcome::Same);
		CHECK(AfterBookChange(false, 0, true, 1, 1) == BookOutcome::Unknown && AfterBookChange(true, 0, false, 1, 1) == BookOutcome::Unknown);
		CHECK(AfterBookChange(true, 0, true, 2, 1) == BookOutcome::Unknown && AfterBookChange(true, 1, true, 2, -1) == BookOutcome::Unknown
			&& AfterBookChange(true, 0, true, std::nan(""), 1) == BookOutcome::Unknown);

		// 한 권의 결과.
		CHECK_STR(OneBookReport(true, "알파", BookOutcome::Done, 1), "'알파' 책을 도서관에 넣었습니다 (지금 1권)");
		CHECK_STR(OneBookReport(false, "알파", BookOutcome::Done, 1), "'알파' 책을 한 권 뺐습니다 (남은 1권)");
		CHECK_STR(OneBookReport(false, "알파", BookOutcome::Done, 0), "'알파' 책을 도서관에서 뺐습니다");
		CHECK_STR(OneBookReport(true, "알파", BookOutcome::Same, 0), "'알파': 넣지 못했습니다 (게임의 권수가 그대로입니다)");
		CHECK_STR(OneBookReport(false, "알파", BookOutcome::Same, 2), "'알파': 빼지 못했습니다 (게임의 권수가 그대로입니다)");
		CHECK_STR(OneBookReport(true, "알파", BookOutcome::Unknown, 0), "'알파': 부른 뒤 권수를 확인하지 못했습니다");
		// 여럿의 결과: 하려던 권수, 된 권수, 부른 뒤 확인하지 못한 권수. 나머지는 못 한 것이고 까닭을 적는다.
		CHECK_STR(BooksReport(true, 110, 110, 0, ""), "책 110권을 도서관에 넣었습니다");
		CHECK_STR(BooksReport(false, 110, 110, 0, ""), "이 실행에서 넣은 책 110권을 도서관에서 뺐습니다");
		CHECK_STR(BooksReport(true, 0, 0, 0, ""), "넣을 책이 없습니다 (도서관에 모든 책이 있습니다)");
		CHECK_STR(BooksReport(false, 0, 0, 0, ""), "뺄 책이 없습니다 (이 실행에서 모듈이 넣은 책이 없습니다)");
		CHECK_STR(BooksReport(true, 110, 107, 1, "no such method"), "책 110권 가운데 107권을 도서관에 넣었습니다 (1권은 부른 뒤 확인하지 못함, 2권은 못 함: no such method)");
		CHECK_STR(BooksReport(false, 5, 3, 0, ""), "이 실행에서 넣은 책 5권 가운데 3권을 도서관에서 뺐습니다 (2권은 못 함)");
		CHECK_STR(BooksReport(true, 4, 0, 4, ""), "책 4권 가운데 0권을 도서관에 넣었습니다 (4권은 부른 뒤 확인하지 못함)");
	});

	Test("도서관: 이 실행에서 넣은 책의 기억", [] {
		// 모듈이 넣은 책만 되돌린다(플레이어가 쓴 책은 건드리지 않는다). 기억은 그 자리(도서관 관리자와 지도)의 것이다.
		LibraryMemory memory;
		const PlaceKey here{ 0x1000, 7 }, there{ 0x2000, 7 }, other_map{ 0x1000, 8 };
		CHECK(!memory.Enter(here) && memory.Count() == 0 && memory.ToUndo().empty());		// 처음 들어선 것은 버린 것이 아니다
		memory.Added("beta");
		memory.Added("alpha");
		memory.Added("beta");
		CHECK(memory.Count() == 3 && memory.ToUndo() == (std::vector<std::pair<std::string, int>>{ { "alpha", 1 }, { "beta", 2 } }));		// 이름순
		// 뺀 것은 줄인다. 기억에 없는 책을 빼도(플레이어의 책) 달라지지 않는다.
		memory.Removed("beta");
		memory.Removed("gamma");
		CHECK(memory.Count() == 2 && memory.ToUndo() == (std::vector<std::pair<std::string, int>>{ { "alpha", 1 }, { "beta", 1 } }));
		memory.Removed("alpha");
		memory.Removed("alpha");
		CHECK(memory.Count() == 1 && memory.ToUndo() == (std::vector<std::pair<std::string, int>>{ { "beta", 1 } }));
		// 같은 자리로 다시 들어서면 그대로다. 관리자나 지도가 달라지면 버린다(다른 세이브를 불러온 뒤 앞의 게임의 기억으로 빼지 않는다).
		CHECK(!memory.Enter(here) && memory.Count() == 1);
		CHECK(memory.Enter(there) && memory.Count() == 0 && memory.ToUndo().empty());
		CHECK(!memory.Enter(there));			// 버릴 것이 없으면 거짓
		memory.Added("alpha");
		CHECK(memory.Enter(PlaceKey{ 0x2000, 8 }) && memory.Count() == 0);
		CHECK(!memory.Enter(other_map));		// 빈 기억은 자리가 달라져도 "버렸다"고 하지 않는다

		// 지금의 표에 맞춘다: 기억한 권수가 도서관의 권수보다 많으면 줄이고, 도서관에 없는 책은 잊는다(그사이 다른 길로 빠졌다).
		memory.Added("alpha");
		memory.Added("alpha");
		memory.Added("beta");
		memory.Added("gamma");
		memory.Clamp({ { "alpha", "", "", 1 }, { "beta", "", "", 0 }, { "gamma", "", "", 5 } });
		CHECK(memory.Count() == 2 && memory.ToUndo() == (std::vector<std::pair<std::string, int>>{ { "alpha", 1 }, { "gamma", 1 } }));
		memory.Clamp({ { "alpha", "", "", 1 } });		// 표에 없는 이름도 잊는다
		CHECK(memory.Count() == 1 && memory.ToUndo() == (std::vector<std::pair<std::string, int>>{ { "alpha", 1 } }));
	});
}
