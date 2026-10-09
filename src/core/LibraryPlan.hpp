#pragma once
// 지식 탭의 "도서관의 책"(src/Library.cpp)의 판단 가운데 러너에 기대지 않는 것. 잰 것은 research/33.
// 게임의 도서관(LibraryManager)은 책을 ds_map(지식 구조체 → 권수)으로 든다. 책을 넣고 빼는 것은 게임의 change_books(지식 구조체, 1 | -1)다.

#include "PlaceKey.hpp"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace NlCore
{
	// 게임의 지식 한 가지와 도서관에 있는 그 책의 권수. 틱이 읽어 글과 수로 둔다.
	struct Book
	{
		std::string Name;		// 게임의 이름(__name). 읽지 못한 칸은 빈 글이다(자리를 지킨다)
		std::string Caption;	// 화면 이름(__caption_replaced. 게임의 글)
		std::string Category;	// 갈래(__category. 게임의 이름)
		double Copies = 0;		// 도서관에 있는 권수(없으면 0)
	};

	// 갈래의 말(모드가 붙인 말이다): economic 경제, textbooks 교과서, cultural_knowledge 문화. 모르는 갈래는 게임의 이름 그대로, 빈 갈래는 줄표.
	std::string BookCategoryLabel(const std::string& Category);
	// 보일 이름: 화면 이름, 없으면 게임의 이름.
	std::string BookLabel(const Book& One);

	// 지식의 이름으로 받을 글인가: 소문자, 숫자, 밑줄, 느낌표. 1~64자(게임의 이름 둘에 느낌표가 있다: 41!building_wooden_wall_section).
	bool GoodKnowledgeName(const std::string& Name);

	// 창의 단추와 원격 명령이 같은 길을 탄다.
	//   library list [find=<글>]     요약과 도서관에 있는 책들. find 를 주면 이름이나 화면 이름에 그 글이 든 지식 모두(없는 책도)
	//   library add name=<지식>      그 책을 한 권 넣는다(도서관에 없을 때만)
	//   library add_all              도서관에 없는 책을 모두 한 권씩 넣는다
	//   library remove name=<지식>   그 책을 한 권 뺀다
	//   library undo                 이 실행에서 모듈이 넣은 책을 뺀다
	enum class LibraryAct { List, Add, AddAll, Remove, Undo };
	struct LibraryCommand
	{
		LibraryAct Act = LibraryAct::List;
		std::string Name;		// Add, Remove: 지식의 이름
		std::string Find;		// List: 찾는 글(없으면 빈 글)
	};
	// library 뒤의 낱말들을 읽는다. 틀리면 거짓이고 Why 에 까닭(Out 은 그대로다).
	bool ParseLibraryCommand(const std::vector<std::string>& Words, LibraryCommand& Out, std::string& Why);
	const char* LibraryActWord(LibraryAct Act);

	// 요약의 한 줄: 도서관에 있는 책의 종류와 권수, 게임의 지식의 수. 이름을 읽지 못한 칸은 따로 적는다.
	std::string LibrarySummary(const std::vector<Book>& Books);
	// 책 한 줄(원격 list): 게임의 이름, 권수, 화면 이름, 갈래.
	std::string BookLine(const Book& One);
	// 표에 보일 줄인가. Find: 이름이나 화면 이름에 들어 있어야 하는 글(빈 글은 모두). OnlyShelved: 도서관에 있는 책만.
	bool BookShown(const Book& One, const std::string& Find, bool OnlyShelved);

	// "모든 책 넣기"가 넣을 책들: 권수가 0 이고 이름이 있는 것(받은 차례).
	std::vector<std::string> BooksToAdd(const std::vector<Book>& Books);

	// 부른 뒤의 판정. 권수를 앞뒤로 읽어 정확히 Delta(+1, -1)만큼 달라졌으면 Done, 그대로면 Same. 읽지 못했거나 다른 만큼 달라졌으면 Unknown(됐다고 적지 않는다).
	enum class BookOutcome { Done, Same, Unknown };
	BookOutcome AfterBookChange(bool ReadBefore, double Before, bool ReadAfter, double After, int Delta);

	// 이 실행에서 모듈이 넣은 책의 기억(되돌리기가 쓴다: 플레이어가 쓴 책은 건드리지 않는다). 그 자리(도서관 관리자와 지도. core/PlaceKey)의 것이다.
	class LibraryMemory
	{
	public:
		// 그 자리에 들어선다. 자리가 달라졌고 기억한 것이 있었으면 버리고 참(다른 세이브를 불러온 뒤 앞의 게임의 기억으로 빼지 않는다).
		bool Enter(const PlaceKey& Place);
		void Added(const std::string& Name);		// 한 권 넣었다
		void Removed(const std::string& Name);		// 한 권 뺐다(기억에 있을 때만 줄인다)
		// 지금의 표에 맞춘다: 기억한 권수가 도서관의 권수보다 많으면 줄이고, 도서관에 없는 책은 잊는다(그사이 다른 길로 빠졌다). 표를 읽었을 때만 부른다.
		void Clamp(const std::vector<Book>& Books);
		// 되돌릴 책들: 이름과 뺄 권수. 이름순.
		std::vector<std::pair<std::string, int>> ToUndo() const;
		int Count() const;		// 넣은 권수의 합

	private:
		PlaceKey m_Place;
		std::map<std::string, int> m_Added;
	};

	// 한 권의 결과. Add: 넣기인가(아니면 빼기). CopiesAfter: 부른 뒤의 권수.
	std::string OneBookReport(bool Add, const std::string& Label, BookOutcome Outcome, double CopiesAfter);
	// 여럿의 결과(모든 책 넣기, 되돌리기). Asked: 하려던 권수, Done: 된 권수, Unsure: 부른 뒤 확인하지 못한 권수. 나머지는 못 한 것이다. Why: 그 까닭(마지막 것).
	std::string BooksReport(bool Add, int Asked, int Done, int Unsure, const std::string& Why);
}
