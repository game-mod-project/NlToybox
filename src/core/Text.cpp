#include "Text.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>

namespace NlCore
{
	namespace
	{
		// printf 는 로캘에 따라 소수점을 쉼표로 쓸 수 있다. JSON 과 로그는 항상 점이어야 한다.
		std::string Dotted(std::string Text)
		{
			for (char& c : Text)
				if (c == ',')
					c = '.';
			return Text;
		}
	}

	std::string Trim(const std::string& Text)
	{
		const size_t begin = Text.find_first_not_of(" \t\r\n");
		if (begin == std::string::npos)
			return "";
		return Text.substr(begin, Text.find_last_not_of(" \t\r\n") - begin + 1);
	}

	std::string Quote(std::string Text, size_t MaxBytes)
	{
		if (Text.size() > MaxBytes)
		{
			size_t cut = MaxBytes;
			while (cut > 0 && (static_cast<unsigned char>(Text[cut]) & 0xC0) == 0x80)
				cut--;
			Text.resize(cut);
			Text += "...";
		}

		std::string out = "\"";
		for (size_t i = 0; i < Text.size();)
		{
			const unsigned char c = static_cast<unsigned char>(Text[i]);
			const size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
			bool valid = len != 0 && i + len <= Text.size();
			for (size_t k = 1; valid && k < len; k++)
				valid = (static_cast<unsigned char>(Text[i + k]) & 0xC0) == 0x80;

			if (!valid) { out += '?'; i++; continue; }
			if (len > 1) { out.append(Text, i, len); i += len; continue; }

			switch (c)
			{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20) { char buf[8]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
				else out += static_cast<char>(c);
			}
			i++;
		}
		return out + "\"";
	}

	std::string Number(double Value)
	{
		if (!std::isfinite(Value))
			return Value != Value ? "\"nan\"" : (Value > 0 ? "\"inf\"" : "\"-inf\"");
		char buf[40];
		snprintf(buf, sizeof(buf), "%.17g", Value);
		return Dotted(buf);
	}

	std::string Fixed(double Value, int Digits)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "%.*f", Digits, Value);
		return Dotted(buf);
	}

	bool ParseNumber(const std::string& Text, double& Out)
	{
		if (Text.empty())
			return false;
		double parsed = 0;
		const char* end = Text.data() + Text.size();
		const std::from_chars_result result = std::from_chars(Text.data(), end, parsed);
		if (result.ec != std::errc() || result.ptr != end)
			return false;
		Out = parsed;
		return true;
	}

	bool Contains(const std::string& Name, const std::string& Part)
	{
		return !Part.empty() && Name.find(Part) != std::string::npos;
	}
}
