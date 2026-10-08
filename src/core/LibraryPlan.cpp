#include "LibraryPlan.hpp"

#include "Text.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace NlCore
{
	namespace
	{
		constexpr size_t k_FindMax = 48;		// 찾는 글의 길이(창의 찾기 칸과 같다)

		struct ActInfo
		{
			LibraryAct Act;
			const char* Word;
			char Takes;		// 'n' 아무것도, 'k' name=<지식>, 'f' find=<글>(없어도 된다)
		};
		constexpr ActInfo k_Acts[] = {
			{ LibraryAct::List, "list", 'f' },
			{ LibraryAct::Add, "add", 'k' },
			{ LibraryAct::AddAll, "add_all", 'n' },
			{ LibraryAct::Remove, "remove", 'k' },
			{ LibraryAct::Undo, "undo", 'n' },
		};

		std::string Copies(double Count)
		{
			return Shortest(Count) + "권";
		}

		// "key=값" 의 값. 그 열쇠가 아니거나 값이 비었으면 거짓.
		bool OptionValue(const std::string& Word, const char* Key, std::string& Out)
		{
			const std::string prefix = std::string(Key) + "=";
			if (Word.size() <= prefix.size() || Word.compare(0, prefix.size(), prefix) != 0)
				return false;
			Out = Word.substr(prefix.size());
			return true;
		}
	}

	std::string BookCategoryLabel(const std::string& Category)
	{
		if (Category == "economic")
			return "경제";
		if (Category == "textbooks")
			return "교과서";
		if (Category == "cultural_knowledge")
			return "문화";
		return Category.empty() ? "-" : Category;
	}

	std::string BookLabel(const Book& One)
	{
		return One.Caption.empty() ? One.Name : One.Caption;
	}

	bool GoodKnowledgeName(const std::string& Name)
	{
		if (Name.empty() || Name.size() > 64)
			return false;
		return std::all_of(Name.begin(), Name.end(), [](unsigned char c) { return std::islower(c) || std::isdigit(c) || c == '_' || c == '!'; });
	}

	bool ParseLibraryCommand(const std::vector<std::string>& Words, LibraryCommand& Out, std::string& Why)
	{
		static const char* k_Usage = "library needs list [find=<text>], add name=<knowledge>, add_all, remove name=<knowledge> or undo";
		if (Words.empty())
		{
			Why = k_Usage;
			return false;
		}
		for (const ActInfo& act : k_Acts)
		{
			if (Words[0] != act.Word)
				continue;
			LibraryCommand command;
			command.Act = act.Act;
			switch (act.Takes)
			{
			case 'n':
				if (Words.size() != 1)
				{
					Why = std::string("library ") + act.Word + " takes nothing";
					return false;
				}
				break;
			case 'k':
				if (Words.size() != 2 || !OptionValue(Words[1], "name", command.Name) || !GoodKnowledgeName(command.Name))
				{
					Why = std::string("library ") + act.Word + " needs name=<the knowledge's name: lowercase letters, digits, underscores, !>";
					return false;
				}
				break;
			default:
				if (Words.size() > 2 || (Words.size() == 2 && (!OptionValue(Words[1], "find", command.Find) || command.Find.size() > k_FindMax)))
				{
					Why = "library list takes only find=<text, up to 48 bytes>";
					return false;
				}
				break;
			}
			Out = command;
			return true;
		}
		Why = k_Usage;
		return false;
	}

	const char* LibraryActWord(LibraryAct Act)
	{
		for (const ActInfo& act : k_Acts)
			if (act.Act == Act)
				return act.Word;
		return "";
	}

	std::string LibrarySummary(const std::vector<Book>& Books)
	{
		int kinds = 0, known = 0, unread = 0;
		double copies = 0;
		for (const Book& book : Books)
		{
			if (book.Name.empty())
			{
				unread++;
				continue;
			}
			known++;
			if (book.Copies > 0)
			{
				kinds++;
				copies += book.Copies;
			}
		}
		return "도서관의 책 " + std::to_string(kinds) + "종 (" + Copies(copies) + ") / 게임의 지식 " + std::to_string(known) + "가지"
			+ (unread > 0 ? ", 이름을 읽지 못한 지식 " + std::to_string(unread) + "가지" : std::string());
	}

	std::string BookLine(const Book& One)
	{
		return One.Name + "  " + (One.Copies > 0 ? Copies(One.Copies) : std::string("없음")) + "  " + (One.Caption.empty() ? std::string("-") : One.Caption)
			+ "  (" + BookCategoryLabel(One.Category) + ")";
	}

	bool BookShown(const Book& One, const std::string& Find, bool OnlyShelved)
	{
		if (One.Name.empty() || (OnlyShelved && !(One.Copies > 0)))
			return false;
		return Find.empty() || Contains(One.Name, Find) || Contains(One.Caption, Find);
	}

	std::vector<std::string> BooksToAdd(const std::vector<Book>& Books)
	{
		std::vector<std::string> out;
		for (const Book& book : Books)
			if (!book.Name.empty() && !(book.Copies > 0))
				out.push_back(book.Name);
		return out;
	}

	BookOutcome AfterBookChange(bool ReadBefore, double Before, bool ReadAfter, double After, int Delta)
	{
		if (!ReadBefore || !ReadAfter || !std::isfinite(Before) || !std::isfinite(After))
			return BookOutcome::Unknown;
		if (After == Before)
			return BookOutcome::Same;
		return After - Before == Delta ? BookOutcome::Done : BookOutcome::Unknown;
	}

	bool LibraryMemory::Enter(const PlaceKey& Place)
	{
		if (Place == m_Place)
			return false;
		const bool dropped = !m_Added.empty();
		m_Added.clear();
		m_Place = Place;
		return dropped;
	}

	void LibraryMemory::Added(const std::string& Name)
	{
		m_Added[Name]++;
	}

	void LibraryMemory::Removed(const std::string& Name)
	{
		const auto at = m_Added.find(Name);
		if (at != m_Added.end() && --at->second <= 0)
			m_Added.erase(at);
	}

	void LibraryMemory::Clamp(const std::vector<Book>& Books)
	{
		for (auto at = m_Added.begin(); at != m_Added.end();)
		{
			double copies = 0;
			for (const Book& book : Books)
				if (book.Name == at->first && std::isfinite(book.Copies))
					copies = book.Copies;
			const int shelved = copies >= 1 ? static_cast<int>((std::min)(std::floor(copies), 1000000.0)) : 0;
			at->second = (std::min)(at->second, shelved);
			at = at->second <= 0 ? m_Added.erase(at) : std::next(at);
		}
	}

	std::vector<std::pair<std::string, int>> LibraryMemory::ToUndo() const
	{
		return std::vector<std::pair<std::string, int>>(m_Added.begin(), m_Added.end());		// std::map 이라 이름순이다
	}

	int LibraryMemory::Count() const
	{
		int sum = 0;
		for (const auto& [name, count] : m_Added)
			sum += count;
		return sum;
	}

	std::string OneBookReport(bool Add, const std::string& Label, BookOutcome Outcome, double CopiesAfter)
	{
		const std::string who = "'" + Label + "'";
		switch (Outcome)
		{
		case BookOutcome::Done:
			if (Add)
				return who + " 책을 도서관에 넣었습니다 (지금 " + Copies(CopiesAfter) + ")";
			return CopiesAfter > 0 ? who + " 책을 한 권 뺐습니다 (남은 " + Copies(CopiesAfter) + ")" : who + " 책을 도서관에서 뺐습니다";
		case BookOutcome::Same:
			return who + (Add ? ": 넣지 못했습니다" : ": 빼지 못했습니다") + " (게임의 권수가 그대로입니다)";
		default:
			return who + ": 부른 뒤 권수를 확인하지 못했습니다";
		}
	}

	std::string BooksReport(bool Add, int Asked, int Done, int Unsure, const std::string& Why)
	{
		if (Asked <= 0)
			return Add ? "넣을 책이 없습니다 (도서관에 모든 책이 있습니다)" : "뺄 책이 없습니다 (이 실행에서 모듈이 넣은 책이 없습니다)";
		const std::string what = Add ? "책 " : "이 실행에서 넣은 책 ";
		const char* did = Add ? "을 도서관에 넣었습니다" : "을 도서관에서 뺐습니다";
		if (Done == Asked)
			return what + std::to_string(Asked) + "권" + did;
		std::string notes;
		const auto note = [&notes](const std::string& text) { notes += std::string(notes.empty() ? "" : ", ") + text; };
		const int failed = Asked - Done - Unsure;
		if (Unsure > 0)
			note(std::to_string(Unsure) + "권은 부른 뒤 확인하지 못함");
		if (failed > 0)
			note(std::to_string(failed) + "권은 못 함" + (Why.empty() ? "" : ": " + Why));
		return what + std::to_string(Asked) + "권 가운데 " + std::to_string(Done) + "권" + did + " (" + notes + ")";
	}
}
