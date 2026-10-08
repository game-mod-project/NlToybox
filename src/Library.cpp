#include "Library.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/Guard.hpp"
#include "core/PlaceKey.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <deque>
#include <map>
#include <mutex>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::Book;
using NlCore::BookOutcome;
using NlCore::LibraryAct;
using NlCore::LibraryCommand;

namespace
{
	// 도서관(research/33). 관리자(LibraryManager)는 지도 관리 인스턴스에 있고, 책은 ds_map(지식 구조체 → 권수)이다(__books 가 그 번호).
	// 게임은 책 사본이 만들어질 때 change_books(지식 구조체, 1) -> undefined 를 부른다(기록으로 봤다). 같은 꼴로 1 과 -1 을 불러 권수가 그만큼 바뀌는 것을 봤다.
	// 지식 구조체는 게임의 지식 목록의 것이다(주소로 맞춰 봤다). 인물 모듈의 지식 주기도 그 목록에서 구조체를 얻는다(PeopleInternal.hpp).
	constexpr const char* k_Library = "inst:o_game_map_controller.__library_manager";
	constexpr const char* k_KnowledgeList = "inst:o_data.__knowledge_data.__knowledge_list";
	constexpr size_t k_MaxQueue = 8;		// 창이 쌓아 둘 명령의 수. 넘치면 받지 않고 결과 줄에 적는다

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlLibrary::LogFn g_Log;
	bool g_Busy = false;				// 하는 중이다(여기서 부른 게임의 함수가 틱을 다시 부르면 안쪽은 아무것도 하지 않는다)

	// 틱이 읽은 것. 창은 이것만 그린다.
	struct View
	{
		bool Read = false;
		std::string Why;				// 읽지 못한 까닭
		std::vector<Book> Books;		// 게임의 지식의 차례. 권수는 도서관의 것
		int Unnamed = 0;				// 도서관의 책 가운데 이름이나 권수를 읽지 못한 것("없음"과 가른다)
		int Undoable = 0;				// 이 실행에서 모듈이 넣은 권수(되돌릴 수 있는 것)
	};
	View g_View;
	std::vector<Book> g_Knowledge;		// 게임의 지식(이름, 화면 이름, 갈래). 자리가 게임의 목록의 번호다. 처음 볼 때 한 번 읽는다
	NlCore::LibraryMemory g_Memory;		// 이 실행에서 모듈이 넣은 책
	std::deque<LibraryCommand> g_Queue;	// 창이 쌓고 틱이 한다
	std::string g_Last;					// 마지막으로 한 일
	double g_Next = 0;					// 다음에 다시 읽을 시각
	char g_Filter[48] = "";				// 창의 찾기 칸
	bool g_OnlyShelved = false;			// 창의 "도서관에 있는 책만"

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	bool FollowText(const RValue& From, const char* Member, std::string& Out)
	{
		RValue value;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Follow(From, { { '.', Member, 0 } }, value, why) || !value.IsString())
			return false;
		Out = value.ToString();
		return true;
	}

	// 게임의 지식 목록을 읽는다(121가지를 봤다). 읽지 못한 칸도 자리를 지킨다: 자리가 번호다.
	void ReadKnowledge()
	{
		RValue list;		// 이 함수 안에서만 든다
		std::string why;
		g_Knowledge.clear();
		if (!NlAccess::Read(NlCore::ParseAskPath(k_KnowledgeList), list, why) || !list.IsArray())
			return;
		NlAccess::ForEachChild(list, Holder::Array, [&](const NlCore::PathStep&, const RValue& item) {
			Book one;
			if (item.IsStruct())
			{
				FollowText(item, "__name", one.Name);
				FollowText(item, "__caption_replaced", one.Caption);
				FollowText(item, "__category", one.Category);
			}
			g_Knowledge.push_back(std::move(one));
			return true;
		});
		Log("library: " + std::to_string(g_Knowledge.size()) + " knowledge name(s)");
	}

	// 기억한 것이 어느 게임·어느 지도의 것인지(core/PlaceKey): 도서관 관리자 구조체의 주소와 지도 관리 인스턴스.
	bool PlaceOf(NlCore::PlaceKey& Out)
	{
		RValue value;		// 이 함수 안에서만 든다
		std::string why;
		int64_t instance = 0;
		if (!NlAccess::Read(NlCore::ParseAskPath(k_Library), value, why) || !value.IsStruct()
			|| !NlAccess::InstanceIdentity(NlCore::ParseAskPath("inst:o_game_map_controller"), instance))
			return false;
		Out.Struct = reinterpret_cast<std::uintptr_t>(value.m_Object);
		Out.Instance = instance;
		return true;
	}

	// 도서관의 책: ds_map 의 열쇠(지식 구조체)마다 그 이름과 권수. 열쇠의 배열에서 얻은 열쇠로 값을 찾는다(원격 list map:<번호> 가 같은 길로 "struct → number 1"을 읽었다).
	// 빈 도서관과 읽지 못한 것을 가른다: ds_map 이 있는지와 크기를 먼저 본다.
	bool ReadShelf(std::map<std::string, double>& Out, int& Unnamed, std::string& Why)
	{
		Out.clear();
		Unnamed = 0;
		double id = 0;
		if (!NlAccess::ReadNumber(std::string(k_Library) + ".__books", id) || !(NlGame::CallNumber("ds_exists", { RValue(id), RValue(NlGame::k_DsMap) }, 0) > 0))
		{
			Why = "도서관의 책의 표(ds_map)를 찾지 못했습니다";
			return false;
		}
		const double size = NlGame::CallNumber("ds_map_size", { RValue(id) }, -1);
		if (size < 0)
		{
			Why = "도서관의 책의 수를 읽지 못했습니다";
			return false;
		}
		if (size == 0)
			return true;
		RValue keys;		// 이 함수 안에서만 든다
		if (!NlGame::Call("ds_map_keys_to_array", { RValue(id) }, keys) || !keys.IsArray())
		{
			Why = "도서관의 책의 열쇠를 읽지 못했습니다";
			return false;
		}
		for (const RValue& key : keys.ToVector())
		{
			RValue copies;
			std::string name;
			if (!key.IsStruct() || !FollowText(key, "__name", name) || name.empty()
				|| !NlGame::Call("ds_map_find_value", { RValue(id), key }, copies) || !NlGame::IsRealNumber(copies))
			{
				Unnamed++;
				continue;
			}
			Out[name] += copies.ToDouble();
		}
		return true;
	}

	// 게임의 지식과 도서관의 책을 읽어 표로 만든다. 읽지 못하면 거짓이고 Out.Why 에 까닭.
	bool Scan(View& Out)
	{
		Out = View();
		if (!NlAccess::InGame())
		{
			Out.Why = "게임 화면이 아닙니다";
			return false;
		}
		if (g_Knowledge.empty())
			ReadKnowledge();
		if (g_Knowledge.empty())
		{
			Out.Why = "게임의 지식 목록을 읽지 못했습니다";
			return false;
		}
		NlCore::PlaceKey place;
		if (!PlaceOf(place))
		{
			Out.Why = "도서관(게임의 LibraryManager)을 찾지 못했습니다";
			return false;
		}
		if (g_Memory.Enter(place))		// 다른 세이브나 다른 지도다: 앞의 게임에서 넣은 책의 기억으로 빼지 않는다
			Log("library: another game or map; forgot the books this module added");
		std::map<std::string, double> shelf;
		if (!ReadShelf(shelf, Out.Unnamed, Out.Why))
			return false;
		Out.Books = g_Knowledge;
		for (Book& book : Out.Books)
		{
			const auto at = book.Name.empty() ? shelf.end() : shelf.find(book.Name);
			if (at == shelf.end())
				continue;
			book.Copies = at->second;
			shelf.erase(at);
		}
		for (const auto& [name, copies] : shelf)		// 지식의 목록에 없는 이름의 책(본 적은 없다). 버리지 않고 끝에 붙인다
			Out.Books.push_back(Book{ name, std::string(), std::string(), copies });
		g_Memory.Clamp(Out.Books);
		Out.Undoable = g_Memory.Count();
		Out.Read = true;
		return true;
	}

	const Book* FindBook(const View& Seen, const std::string& Name)
	{
		for (const Book& book : Seen.Books)
			if (!Name.empty() && book.Name == Name)
				return &book;
		return nullptr;
	}

	// 부른 뒤의 권수. 표를 읽지 못했으면 거짓(0 권과 가른다).
	bool CopiesAfter(const View& After, const std::string& Name, double& Out)
	{
		Out = 0;
		if (!After.Read)
			return false;
		const Book* book = FindBook(After, Name);
		Out = book ? book->Copies : 0;
		return true;
	}

	// 책 한 권을 넣거나(Delta 1) 뺀다(Delta -1): 게임의 change_books(지식 구조체, 수). 지식 구조체는 게임의 목록의 그 자리에서 얻고, 그 자리의 이름이 청한 이름인지 다시 본다.
	bool ChangeOne(const std::string& Name, int Delta, std::string& Why)
	{
		size_t index = 0;
		while (index < g_Knowledge.size() && g_Knowledge[index].Name != Name)
			index++;
		RValue knowledge, result;		// 이 함수 안에서만 든다
		std::string name;
		if (Name.empty() || index >= g_Knowledge.size()
			|| !NlAccess::Read(NlCore::ParseAskPath(std::string(k_KnowledgeList) + "[" + std::to_string(index) + "]"), knowledge, Why) || !knowledge.IsStruct()
			|| !FollowText(knowledge, "__name", name) || name != Name)
		{
			Why = "게임의 지식 목록에서 찾지 못했습니다";
			return false;
		}
		Log(std::string("library call change_books(") + Name + ", " + (Delta > 0 ? "1" : "-1") + ")");		// 부르기 전에 남긴다
		return NlAccess::CallMethod(NlCore::ParseAskPath(std::string(k_Library) + ".change_books"), { knowledge, RValue(Delta > 0 ? 1.0 : -1.0) }, result, Why);
	}

	// 게임이 센 책의 종류(get_books_count(): 인자 없음. 기계어로 봤고 불러서 수를 받았다). 로그와 원격의 답에 적어 표의 수와 견준다.
	std::string GameCount()
	{
		RValue result;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(std::string(k_Library) + ".get_books_count"), {}, result, why) || !NlGame::IsRealNumber(result))
			return "?";
		return NlCore::Shortest(result.ToDouble());
	}

	std::vector<std::string> ListLines(const View& Seen, const std::string& Find)
	{
		std::vector<std::string> out;
		out.push_back(NlCore::LibrarySummary(Seen.Books) + (Seen.Unnamed > 0 ? ", 읽지 못한 책 " + std::to_string(Seen.Unnamed) + "종" : std::string()));
		out.push_back("게임이 센 책의 종류 " + GameCount() + ", 이 실행에서 모듈이 넣은 책 " + std::to_string(Seen.Undoable) + "권");
		for (const Book& book : Seen.Books)
			if (NlCore::BookShown(book, Find, Find.empty()))		// 찾는 글이 없으면 도서관에 있는 책만
				out.push_back(NlCore::BookLine(book));
		return out;
	}

	// 한 권을 넣거나 뺀다. 권수를 앞뒤로 읽어 판정한다(반환값에 기대지 않는다: 게임의 함수는 undefined 를 돌려준다).
	std::string OneBook(const View& Seen, const std::string& Name, bool Add, View& After)
	{
		const Book* book = FindBook(Seen, Name);
		if (!book)
			return "게임의 지식 목록에 그런 이름이 없습니다: " + Name;
		const std::string label = NlCore::BookLabel(*book);
		const double before = book->Copies;
		if (Add && before > 0)
			return "'" + label + "': 이미 도서관에 있는 책입니다";
		if (!Add && !(before > 0))
			return "'" + label + "': 도서관에 없는 책입니다";
		std::string why;
		if (!ChangeOne(Name, Add ? 1 : -1, why))
			return "'" + label + "': 게임의 함수를 부르지 못했습니다 (" + why + ")";
		Scan(After);
		double after = 0;
		const bool read = CopiesAfter(After, Name, after);
		const BookOutcome outcome = NlCore::AfterBookChange(true, before, read, after, Add ? 1 : -1);
		if (outcome == BookOutcome::Done && Add)
			g_Memory.Added(Name);
		else if (outcome == BookOutcome::Done)
			g_Memory.Removed(Name);		// 모듈이 넣었던 책이면 기억에서도 줄인다(되돌리기가 플레이어의 책을 빼지 않게)
		Log(std::string("library: ") + (Add ? "add " : "remove ") + Name + ": copies " + NlCore::Shortest(before) + " -> " + (read ? NlCore::Shortest(after) : std::string("?"))
			+ ", game counts " + GameCount() + " kind(s)");
		return NlCore::OneBookReport(Add, label, outcome, after);
	}

	// 도서관에 없는 책을 모두 한 권씩 넣는다. 한 틱에 잇달아 부른다(110권을 한 요청으로 불러 봤다: 탈이 없었고 121가지가 모두 들어갔다. research/33).
	std::string AddAll(const View& Seen, View& After)
	{
		const std::vector<std::string> names = NlCore::BooksToAdd(Seen.Books);
		std::string why;
		for (const std::string& name : names)
		{
			std::string note;
			if (!ChangeOne(name, 1, note))
				why = note;
		}
		if (names.empty())
			return NlCore::BooksReport(true, 0, 0, 0, why);
		Scan(After);
		int done = 0, unsure = 0;
		for (const std::string& name : names)
		{
			double after = 0;
			const bool read = CopiesAfter(After, name, after);
			switch (NlCore::AfterBookChange(true, 0, read, after, 1))
			{
			case BookOutcome::Done: done++; g_Memory.Added(name); break;
			case BookOutcome::Unknown: unsure++; break;
			case BookOutcome::Same: if (why.empty()) why = "게임의 권수가 그대로입니다"; break;
			}
		}
		Log("library: add_all: asked " + std::to_string(names.size()) + ", done " + std::to_string(done) + ", unsure " + std::to_string(unsure) + (why.empty() ? "" : ", " + why)
			+ ", game counts " + GameCount() + " kind(s)");
		return NlCore::BooksReport(true, static_cast<int>(names.size()), done, unsure, why);
	}

	// 이 실행에서 모듈이 넣은 책을 뺀다(플레이어가 쓴 책은 건드리지 않는다). 기억한 권수는 Scan 이 지금의 권수에 맞춰 두었다.
	std::string Undo(const View& Seen, View& After)
	{
		const std::vector<std::pair<std::string, int>> mine = g_Memory.ToUndo();
		int asked = 0;
		std::string why;
		for (const auto& [name, count] : mine)
		{
			asked += count;
			for (int i = 0; i < count; i++)
			{
				std::string note;
				if (!ChangeOne(name, -1, note))
					why = note;
			}
		}
		if (asked == 0)
			return NlCore::BooksReport(false, 0, 0, 0, why);
		Scan(After);
		int done = 0, unsure = 0;
		for (const auto& [name, count] : mine)
		{
			const Book* was = FindBook(Seen, name);
			double after = 0;
			if (!was || !CopiesAfter(After, name, after))
			{
				unsure += count;
				continue;
			}
			const double gone = was->Copies - after;
			const int removed = gone >= count ? count : gone > 0 ? static_cast<int>(gone) : 0;		// 기억한 것보다 많이 빠졌어도 모듈이 뺀 것은 count 까지다
			done += removed;
			for (int i = 0; i < removed; i++)
				g_Memory.Removed(name);
			if (removed < count && why.empty())
				why = "게임의 권수가 그대로입니다";
		}
		Log("library: undo: asked " + std::to_string(asked) + ", done " + std::to_string(done) + ", unsure " + std::to_string(unsure) + (why.empty() ? "" : ", " + why)
			+ ", game counts " + GameCount() + " kind(s)");
		return NlCore::BooksReport(false, asked, done, unsure, why);
	}

	// 한 가지 일을 한다. 바꾼 뒤의 표를 창에 보인다.
	std::vector<std::string> DoNow(const LibraryCommand& C)
	{
		View seen;
		if (!Scan(seen))
			return { seen.Why };
		if (C.Act == LibraryAct::List)
		{
			g_View = seen;
			return ListLines(seen, C.Find);
		}
		View after;
		std::string said;
		switch (C.Act)
		{
		case LibraryAct::Add: said = OneBook(seen, C.Name, true, after); break;
		case LibraryAct::Remove: said = OneBook(seen, C.Name, false, after); break;
		case LibraryAct::AddAll: said = AddAll(seen, after); break;
		case LibraryAct::Undo: said = Undo(seen, after); break;
		default: break;
		}
		g_Last = said;
		if (after.Read)
			after.Undoable = g_Memory.Count();		// 판정 뒤에 기억이 바뀌었다
		g_View = after.Read ? after : seen;
		return { said };
	}

	void Push(LibraryAct Act, const std::string& Name = std::string())
	{
		if (g_Queue.size() >= k_MaxQueue)
		{
			g_Last = NlCore::QueueFullText(k_MaxQueue);
			return;
		}
		g_Queue.push_back(LibraryCommand{ Act, Name, std::string() });
	}
}

void NlLibrary::Init(LogFn Log_)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
}

void NlLibrary::GameTick(double Now, bool Visible)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return;
	if (g_Queue.empty() && (!Visible || Now < g_Next))		// 시각부터 본다(이 틱은 오브젝트 이벤트마다 불린다)
		return;
	const NlCore::ScopedFlag busy(g_Busy);
	g_Next = Now + 1;
	if (!g_Queue.empty())
	{
		const LibraryCommand command = g_Queue.front();
		g_Queue.pop_front();
		DoNow(command);
		return;
	}
	View view;
	Scan(view);
	g_View = view;
}

void NlLibrary::Draw()
{
	std::lock_guard lock(g_Mutex);
	if (!g_View.Read)
	{
		NlUi::Hint(g_View.Why.empty() ? "읽는 중입니다" : g_View.Why.c_str());
		if (!g_Last.empty())
			NlUi::Hint(("마지막 한 일: " + g_Last).c_str());
		return;
	}
	ImGui::TextUnformatted(NlCore::LibrarySummary(g_View.Books).c_str());
	if (g_View.Unnamed > 0)
		NlUi::Hint("도서관의 책 가운데 " + std::to_string(g_View.Unnamed) + "종은 이름이나 권수를 읽지 못했습니다.");
	if (ImGui::Button("모든 책을 도서관에 넣기"))
		Push(LibraryAct::AddAll);
	ImGui::SameLine();
	ImGui::BeginDisabled(g_View.Undoable <= 0);
	if (ImGui::Button(("이 실행에서 넣은 책 빼기 (" + std::to_string(g_View.Undoable) + "권)###library_undo").c_str()))
		Push(LibraryAct::Undo);
	ImGui::EndDisabled();
	// 마지막 한 일은 표의 위에 둔다(표 아래에 두면 스크롤해야 보인다).
	if (!g_Last.empty())
		NlUi::Hint(("마지막 한 일: " + g_Last).c_str());

	ImGui::SetNextItemWidth(160);
	ImGui::InputText("찾기 (이름, 화면 이름)##library", g_Filter, sizeof(g_Filter));
	ImGui::SameLine();
	ImGui::Checkbox("도서관에 있는 책만", &g_OnlyShelved);
	if (ImGui::BeginTable("library", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter, ImVec2(0, 300)))
	{
		// 단추 칸을 맨 앞에 둔다: 이름이 길어 표가 패널 너비를 넘어도 단추는 보인다(이벤트의 표와 같다).
		ImGui::TableSetupScrollFreeze(1, 1);
		ImGui::TableSetupColumn("");
		ImGui::TableSetupColumn("권수");
		ImGui::TableSetupColumn("갈래");
		ImGui::TableSetupColumn("책");
		ImGui::TableHeadersRow();
		for (const Book& book : g_View.Books)
		{
			if (!NlCore::BookShown(book, g_Filter, g_OnlyShelved))
				continue;
			const bool shelved = book.Copies > 0;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (ImGui::SmallButton(((shelved ? "빼기##" : "넣기##") + book.Name).c_str()))
				Push(shelved ? LibraryAct::Remove : LibraryAct::Add, book.Name);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(shelved ? NlCore::Shortest(book.Copies).c_str() : "-");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(NlCore::BookCategoryLabel(book.Category).c_str());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(NlCore::BookLabel(book).c_str());
			if (!book.Caption.empty())
			{
				ImGui::SameLine();
				ImGui::TextDisabled("%s", book.Name.c_str());
			}
		}
		ImGui::EndTable();
	}
	NlUi::Hint("게임의 도서관에 있는 책입니다. '넣기'와 '빼기'는 게임이 책 사본이 만들어질 때 부르는 함수(change_books)를 같은 꼴로 부릅니다: 한 번에 한 권입니다. "
		"'모든 책을 도서관에 넣기'는 도서관에 없는 책을 모두 한 권씩 넣습니다(110권을 한꺼번에 넣어 봤습니다). 게임의 '새로 제작된 책 사본' 알림은 뜨지 않습니다. "
		"'이 실행에서 넣은 책 빼기'는 모드가 넣은 책만 뺍니다(영주가 쓴 책은 건드리지 않습니다). 게임을 끄거나 다른 세이브를 불러오면 그 기억은 없어집니다. "
		"도서관의 책은 세이브에 들어가는 자료입니다(세이브 파일의 books 에 책의 이름과 권수가 있습니다): 넣은 채 저장하면 남는 것으로 봅니다(저장해서 확인하지는 않았습니다). "
		"갈래의 말(경제, 교과서, 문화)은 모드가 붙인 것입니다.");
}

std::vector<std::string> NlLibrary::Do(const NlCore::LibraryCommand& Command)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const NlCore::ScopedFlag busy(g_Busy);
	return DoNow(Command);
}
