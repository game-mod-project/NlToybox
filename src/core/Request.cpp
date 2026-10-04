#include "Request.hpp"

#include "Text.hpp"

#include <cmath>

namespace NlCore
{
	namespace
	{
		std::vector<std::string> Split(const std::string& Text, char Separator)
		{
			std::vector<std::string> parts;
			size_t begin = 0;
			while (true)
			{
				const size_t end = Text.find(Separator, begin);
				parts.push_back(Trim(Text.substr(begin, end == std::string::npos ? std::string::npos : end - begin)));
				if (end == std::string::npos)
					return parts;
				begin = end + 1;
			}
		}

		bool ParseSeconds(const std::string& Value, double& Out)
		{
			double parsed = 0;
			if (!ParseNumber(Value, parsed) || parsed < 0)
				return false;
			Out = parsed;
			return true;
		}
	}

	Request ParseRequest(std::istream& In)
	{
		Request request;
		std::string line;
		int number = 0;
		while (std::getline(In, line))
		{
			number++;
			line = Trim(line);
			if (line.empty() || line[0] == '#')
				continue;

			const std::string where = "line " + std::to_string(number) + ": ";
			const size_t eq = line.find('=');
			if (eq == std::string::npos)
			{
				request.Errors.push_back(where + "no '=' in '" + line + "'");
				continue;
			}

			const std::string key = Trim(line.substr(0, eq));
			const std::string value = Trim(line.substr(eq + 1));
			double parsed = 0;

			if (key == "delay_seconds")
			{
				if (ParseSeconds(value, parsed))
					request.DelaySeconds = static_cast<int>(parsed);
				else
					request.Errors.push_back(where + "delay_seconds needs a number >= 0");
			}
			else if (key == "repeat_seconds")
			{
				// 너무 짧으면 덤프가 이어 붙어 게임이 멈춘 채로 있게 된다.
				if (ParseSeconds(value, parsed) && (parsed == 0 || parsed >= 10))
					request.RepeatSeconds = parsed;
				else
					request.Errors.push_back(where + "repeat_seconds needs 0 or a number >= 10");
			}
			else if (key == "keep_last")
			{
				if (ParseNumber(value, parsed) && parsed == std::floor(parsed) && parsed >= 1 && parsed <= 9)
					request.KeepLast = static_cast<int>(parsed);
				else
					request.Errors.push_back(where + "keep_last needs a whole number from 1 to 9");
			}
			else if (key == "trace_events")
			{
				if (value == "1" || value == "0")
					request.TraceEvents = value == "1";
				else
					request.Errors.push_back(where + "trace_events needs 0 or 1");
			}
			else if (key == "skip")
			{
				if (value == "ds" || value == "instances" || value == "room")
					request.Skip.push_back(value);
				else
					request.Errors.push_back(where + "skip needs ds, instances or room");
			}
			else if (key == "find")
			{
				if (ParseNumber(value, parsed))
					request.FindValues.push_back(parsed);
				else
					request.Errors.push_back(where + "find needs a number");
			}
			else if (key == "find_name")
			{
				if (!value.empty())
					request.FindNames.push_back(value);
				else
					request.Errors.push_back(where + "find_name is empty");
			}
			else if (key == "watch")
			{
				const bool ok = value.rfind("global.", 0) == 0 && value.size() > 7
					&& value.back() != '.' && value.find("..") == std::string::npos;
				if (ok)
					request.Watches.push_back(value);
				else
					request.Errors.push_back(where + "watch must look like global.a.b");
			}
			else if (key == "script")
			{
				// 이름|인자|인자…
				const std::vector<std::string> parts = Split(value, '|');
				if (parts[0].empty())
					request.Errors.push_back(where + "script needs a name");
				else
				{
					ScriptCall call;
					call.Name = parts[0];
					call.Args.assign(parts.begin() + 1, parts.end());
					request.Scripts.push_back(std::move(call));
				}
			}
			else
				request.Errors.push_back(where + "unknown key '" + key + "'");
		}

		// 되풀이 덤프마다 같은 스크립트를 부르지 않는다. 스크립트는 값을 바꿀 수도, 게임을 끝낼 수도 있다.
		if (request.RepeatSeconds > 0 && !request.Scripts.empty())
			request.Errors.push_back("script cannot be used with repeat_seconds");

		return request;
	}
}
