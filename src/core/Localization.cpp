#include "Localization.hpp"

#include <algorithm>

namespace NlCore
{
	namespace
	{
		// CSV 의 한 줄(레코드)을 칸들로 읽는다. At 은 다음 줄의 처음으로 옮긴다. 닫히지 않은 따옴표로 글이 끝나면 거짓(그 줄은 버린다).
		bool ReadRecord(std::string_view Csv, size_t& At, std::vector<std::string>& Fields)
		{
			Fields.clear();
			std::string field;
			bool quoted = false;
			while (At < Csv.size())
			{
				const char c = Csv[At];
				if (quoted)
				{
					if (c == '"')
					{
						if (At + 1 < Csv.size() && Csv[At + 1] == '"')
						{
							field += '"';
							At += 2;
							continue;
						}
						quoted = false;
						At++;
						continue;
					}
					// 따옴표 안의 CRLF 는 줄바꿈 하나로 둔다.
					if (c == '\r' && At + 1 < Csv.size() && Csv[At + 1] == '\n')
					{
						At++;
						continue;
					}
					field += c;
					At++;
					continue;
				}
				if (c == '"' && field.empty())
				{
					quoted = true;
					At++;
				}
				else if (c == ',')
				{
					Fields.push_back(std::move(field));
					field.clear();
					At++;
				}
				else if (c == '\n' || c == '\r')
				{
					At += c == '\r' && At + 1 < Csv.size() && Csv[At + 1] == '\n' ? 2 : 1;
					Fields.push_back(std::move(field));
					return true;
				}
				else
				{
					field += c;
					At++;
				}
			}
			if (quoted)
				return false;
			Fields.push_back(std::move(field));
			return true;
		}

		bool IsBlank(std::string_view Text)
		{
			return std::all_of(Text.begin(), Text.end(), [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; });
		}

		std::string_view TrimView(std::string_view Text)
		{
			while (!Text.empty() && (Text.front() == ' ' || Text.front() == '\t' || Text.front() == '\r' || Text.front() == '\n'))
				Text.remove_prefix(1);
			while (!Text.empty() && (Text.back() == ' ' || Text.back() == '\t' || Text.back() == '\r' || Text.back() == '\n'))
				Text.remove_suffix(1);
			return Text;
		}

		bool IsWordChar(char c)
		{
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
		}

		bool IsLetter(char c)
		{
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
		}

		char Lower(char c)
		{
			return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
		}
	}

	bool ReadLocalization(std::string_view Csv, std::string_view Prefix, const std::vector<std::string>& Languages,
		std::unordered_map<std::string, std::string>& Out, std::string& Why)
	{
		Why.clear();
		if (Csv.size() >= 3 && Csv.substr(0, 3) == "\xEF\xBB\xBF")
			Csv.remove_prefix(3);
		size_t at = 0;
		std::vector<std::string> fields;
		if (Csv.empty() || !ReadRecord(Csv, at, fields) || fields.size() < 2)
		{
			Why = "no header row";
			return false;
		}
		// 값을 꺼낼 칸들(Languages 의 차례).
		std::vector<size_t> columns;
		for (const std::string& language : Languages)
			for (size_t i = 1; i < fields.size(); i++)
				if (fields[i] == language)
				{
					columns.push_back(i);
					break;
				}
		if (columns.empty())
		{
			Why = "the header has none of the wanted languages";
			return false;
		}

		while (at < Csv.size())
		{
			if (!ReadRecord(Csv, at, fields))
				break;		// 닫히지 않은 따옴표로 끝났다. 그 줄은 버린다
			if (fields.empty() || fields[0].empty() || fields[0].compare(0, Prefix.size(), Prefix) != 0)
				continue;
			for (const size_t column : columns)
				if (column < fields.size() && !IsBlank(fields[column]))
				{
					Out.emplace(fields[0], fields[column]);		// 같은 열쇠가 또 나오면 앞의 것을 둔다
					break;
				}
		}
		return true;
	}

	std::string TraitCaptionKey(const std::string& Name)
	{
		return "trait." + Name;
	}

	std::string PlainHint(std::string_view Raw, std::string_view Caption)
	{
		// 첫 줄이 이름과 같으면 뗀다.
		Raw = TrimView(Raw);
		const size_t first_end = Raw.find('\n');
		const std::string_view first = TrimView(Raw.substr(0, first_end));
		if (!Caption.empty() && first == TrimView(Caption))
			Raw = first_end == std::string_view::npos ? std::string_view() : Raw.substr(first_end + 1);

		std::string text;
		for (size_t i = 0; i < Raw.size();)
		{
			const char c = Raw[i];
			if (c == '<')
			{
				// <이름>, </이름>, <이름=…> 꼴의 표식만 지운다("a < b" 는 둔다).
				const size_t close = Raw.find('>', i);
				const size_t name = i + 1 + (i + 1 < Raw.size() && Raw[i + 1] == '/' ? 1 : 0);
				if (close != std::string_view::npos && name < close && IsLetter(Raw[name]) && Raw.substr(i, close - i).find('\n') == std::string_view::npos)
				{
					if (Raw.substr(i, close - i + 1) == "<nbsp>")
						text += ' ';
					i = close + 1;
					continue;
				}
			}
			else if (c == '{')
			{
				// {이름}: 게임이 값으로 채우는 자리.
				size_t j = i + 1;
				while (j < Raw.size() && IsWordChar(Raw[j]))
					j++;
				if (j > i + 1 && j < Raw.size() && Raw[j] == '}')
				{
					text += "(값)";
					i = j + 1;
					continue;
				}
			}
			else if (c == '[')
			{
				// [hint_이름]: 다른 힌트를 끼우는 자리. 글자로 시작하고 밑줄이 든 한 낱말만 지운다("[1]", "[두 낱말]" 은 둔다).
				size_t j = i + 1;
				bool underscore = false;
				for (; j < Raw.size() && IsWordChar(Raw[j]); j++)
					underscore = underscore || Raw[j] == '_';
				if (j > i + 1 && IsLetter(Raw[i + 1]) && underscore && j < Raw.size() && Raw[j] == ']')
				{
					i = j + 1;
					continue;
				}
			}
			else if (c == '\xE2' && Raw.substr(i, 3) == "\xE2\x80\x94")
			{
				text += '-';
				i += 3;
				continue;
			}
			else if (c == '\r')
			{
				i++;
				continue;
			}
			text += c;
			i++;
		}

		// 줄 끝의 빈칸을 떼고, 빈 줄은 하나까지만 둔다.
		std::string out;
		int blanks = 0;
		size_t start = 0;
		while (start <= text.size())
		{
			const size_t end = std::min(text.find('\n', start), text.size());
			std::string_view line(text.data() + start, end - start);
			while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
				line.remove_suffix(1);
			if (IsBlank(line))
				blanks++;
			else
			{
				if (!out.empty())
					out += blanks > 0 ? "\n\n" : "\n";
				out += line;
				blanks = 0;
			}
			start = end + 1;
		}
		return std::string(TrimView(out));
	}

	HintText SplitHint(std::string_view Raw)
	{
		HintText out;
		Raw = TrimView(Raw);
		const size_t end = Raw.find('\n');
		out.Title = PlainHint(Raw.substr(0, end), "");
		if (end != std::string_view::npos)
			out.Body = PlainHint(Raw.substr(end + 1), "");
		return out;
	}

	bool TraitLayoutOk(const std::string& Name, const std::string& Property0, const std::string& Property1)
	{
		return !Name.empty() && Property0 == Name && Property1 == TraitCaptionKey(Name);
	}

	bool TraitBefore(const std::string& NameA, const std::string& CaptionA, const std::string& NameB, const std::string& CaptionB)
	{
		if (CaptionA.empty() != CaptionB.empty())
			return !CaptionA.empty();
		if (CaptionA != CaptionB)
			return CaptionA < CaptionB;
		return NameA < NameB;
	}

	bool TraitMatches(std::string_view Filter, std::string_view Name, std::string_view Caption)
	{
		if (Filter.empty())
			return true;
		const auto contains = [&](std::string_view In) {
			if (In.size() < Filter.size())
				return false;
			for (size_t i = 0; i + Filter.size() <= In.size(); i++)
			{
				size_t k = 0;
				while (k < Filter.size() && Lower(In[i + k]) == Lower(Filter[k]))
					k++;
				if (k == Filter.size())
					return true;
			}
			return false;
		};
		return contains(Name) || contains(Caption);
	}
}
