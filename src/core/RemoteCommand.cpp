#include "RemoteCommand.hpp"

#include "AskPath.hpp"
#include "CheatTable.hpp"
#include "EconomyPlan.hpp"
#include "Text.hpp"

#include <cmath>
#include <fstream>

namespace NlCore
{
	namespace
	{
		// 공백에서 가른다. 중괄호 안의 공백은 가르지 않는다(주소의 @{키}, s:{글}).
		std::vector<std::string> Split(const std::string& Line)
		{
			std::vector<std::string> tokens;
			std::string token;
			int depth = 0;
			for (const char c : Line)
			{
				if (c == '{')
					depth++;
				else if (c == '}' && depth > 0)
					depth--;
				if ((c == ' ' || c == '\t') && depth == 0)
				{
					if (!token.empty())
						tokens.push_back(token);
					token.clear();
					continue;
				}
				token += c;
			}
			if (!token.empty())
				tokens.push_back(token);
			return tokens;
		}

		bool GoodPath(const std::string& Text, bool NeedSteps)
		{
			const AskPath path = ParseAskPath(Text);
			return path.Error.empty() && (!NeedSteps || !path.Steps.empty());
		}

		// 파일 이름이 되는 글: 영문, 숫자, '_', '-' 만.
		bool GoodName(const std::string& Text)
		{
			if (Text.empty() || Text.size() > 60)
				return false;
			for (const char c : Text)
				if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
					return false;
			return true;
		}

		bool ParseArg(const std::string& Token, RemoteArg& Out)
		{
			if (Token == "u")
			{
				Out.Kind = 'u';
				return true;
			}
			if (Token.size() < 2 || Token[1] != ':')
				return false;
			const std::string rest = Token.substr(2);
			Out.Kind = Token[0];
			switch (Token[0])
			{
			case 'n':
				return ParseNumber(rest, Out.Number);
			case 'b':
				Out.Number = rest == "1" ? 1 : 0;
				return rest == "0" || rest == "1";
			case 's':
				Out.Text = rest.size() >= 2 && rest.front() == '{' && rest.back() == '}' ? rest.substr(1, rest.size() - 2) : rest;
				return true;
			case 'p':
				Out.Text = rest;
				return GoodPath(rest, false);
			default:
				return false;
			}
		}
	}

	bool TakeRemoteRequest(const std::filesystem::path& Ask, const std::filesystem::path& Taken, std::vector<std::string>& Lines)
	{
		std::error_code ec;
		Lines.clear();
		std::filesystem::remove(Taken, ec);
		std::filesystem::rename(Ask, Taken, ec);
		if (ec)
			return false;

		bool opened = false;
		{
			std::ifstream in(Taken);
			opened = static_cast<bool>(in);
			std::string line;
			while (opened && std::getline(in, line))
				Lines.push_back(line);
		}
		std::filesystem::remove(Taken, ec);
		return opened;
	}

	void DropStaleRemoteRequest(const std::filesystem::path& Ask, const std::filesystem::path& Taken)
	{
		std::error_code ec;
		std::filesystem::remove(Ask, ec);
		std::filesystem::remove(Taken, ec);
	}

	double OptionNumber(const RemoteCommand& Command, const std::string& Key, double Fallback)
	{
		const auto it = Command.Options.find(Key);
		double value = 0;
		return it != Command.Options.end() && ParseNumber(it->second, value) ? value : Fallback;
	}

	RemoteCommand ParseRemoteLine(const std::string& Line)
	{
		RemoteCommand command;
		const std::string line = Trim(Line);
		if (line.empty() || line[0] == '#')
			return command;

		const std::vector<std::string> tokens = Split(line);
		const size_t count = tokens.size();
		command.Verb = tokens[0];
		const std::string& verb = command.Verb;

		const auto fail = [&](const std::string& why) {
			command.Error = why;
			return command;
		};
		// tokens[From] 부터를 key=value 로 읽는다.
		const auto options = [&](size_t From) {
			for (size_t i = From; i < count; i++)
			{
				const size_t eq = tokens[i].find('=');
				if (eq == std::string::npos || eq == 0)
				{
					command.Error = "expected key=value: " + tokens[i];
					return false;
				}
				command.Options[tokens[i].substr(0, eq)] = tokens[i].substr(eq + 1);
			}
			return true;
		};

		if (verb == "state")
		{
			if (count != 1)
				return fail("state takes nothing");
		}
		else if (verb == "ask" || verb == "about" || verb == "list" || verb == "tree" || verb == "statics")
		{
			if (count < 2 || !GoodPath(tokens[1], false))
				return fail(verb + " needs a path");
			if ((verb == "ask" || verb == "about") && count != 2)
				return fail(verb + " takes one path");
			command.Target = tokens[1];
			options(2);
		}
		else if (verb == "find")
		{
			if (!options(1))
				return command;
			double value = 0;
			const auto given = command.Options.find("value");
			if (given != command.Options.end() && !ParseNumber(given->second, value))
				return fail("find value= needs a number");
			if (!command.Options.count("name") && given == command.Options.end())
				return fail("find needs name= or value=");
		}
		else if (verb == "refine")
		{
			if (!options(1))
				return command;
			const auto given = command.Options.find("value");
			if (given == command.Options.end() || !ParseNumber(given->second, command.Number))
				return fail("refine needs value=<number>");
		}
		else if (verb == "write" || verb == "poke")
		{
			const size_t eq = count == 2 ? tokens[1].rfind('=') : std::string::npos;		// 주소의 중괄호 안에 '=' 가 있을 수 있다
			if (eq == std::string::npos || !ParseNumber(tokens[1].substr(eq + 1), command.Number) || !GoodPath(tokens[1].substr(0, eq), true))
				return fail(verb + " needs <path>=<number>");
			command.Target = tokens[1].substr(0, eq);
		}
		else if (verb == "record" || verb == "unrecord")
		{
			if (count != 2)
				return fail(verb + " needs one name");
			command.Target = tokens[1];
		}
		else if (verb == "records")
		{
			if (count > 2)
				return fail("records takes at most one name");
			if (count == 2)
				command.Target = tokens[1];
		}
		else if (verb == "shot")
		{
			if (count != 2 || !GoodName(tokens[1]))
				return fail("shot needs a name of letters, digits, '_' and '-'");
			command.Target = tokens[1];
		}
		else if (verb == "window")
		{
			if (count != 2 || (tokens[1] != "open" && tokens[1] != "close"))
				return fail("window needs open or close");
			command.Target = tokens[1];
		}
		else if (verb == "treecall")
		{
			// treecall <인자 없는 스크립트의 이름> [depth=N] [max=N]
			if (count < 2 || tokens[1].find('=') != std::string::npos)
				return fail("treecall needs a script name");
			command.Target = tokens[1];
			if (!options(2))
				return command;
		}
		else if (verb == "override")
		{
			// override <스크립트 이름|메서드의 주소> <n:수|b:0|1|u> [skip]
			RemoteArg value;
			if (count < 3 || count > 4 || !ParseArg(tokens[2], value) || (value.Kind != 'n' && value.Kind != 'b' && value.Kind != 'u')
				|| (value.Kind == 'n' && !std::isfinite(value.Number)))
				return fail("override needs a target and a value (n:<finite number>, b:0|1 or u)");
			if (count == 4 && tokens[3] != "skip")
				return fail("override takes only 'skip' after the value");
			command.Target = tokens[1];
			command.Args.push_back(value);
			if (count == 4)
				command.Options["skip"] = "1";
		}
		else if (verb == "unoverride")
		{
			if (count != 2)
				return fail("unoverride needs one name or 'all'");
			command.Target = tokens[1];
		}
		else if (verb == "economy")
		{
			EconomyAct act = EconomyAct::GoldAdd;
			if (count < 2 || !ParseEconomyAct(tokens[1], act))
				return fail("economy needs gold_add, gold_set, add, set or all");
			command.Target = tokens[1];
			if (!options(2))
				return command;
			const auto amount = command.Options.find("amount");
			if (amount == command.Options.end() || !ParseNumber(amount->second, command.Number))
				return fail("economy needs amount=<number>");
			double resource = 0;
			const auto given = command.Options.find("resource");
			if (NeedsResource(act) && (given == command.Options.end() || !ParseNumber(given->second, resource)
				|| !(resource >= 0 && resource < 1000) || resource != std::floor(resource)))
				return fail("economy " + tokens[1] + " needs resource=<index>");
			if (!NeedsResource(act) && given != command.Options.end())
				return fail("economy " + tokens[1] + " takes no resource");
		}
		else if (verb == "cheat")
		{
			// cheat <치트의 Id> on|off
			if (count != 3 || !FindCheat(tokens[1]) || (tokens[2] != "on" && tokens[2] != "off"))
				return fail("cheat needs the id of a cheat and on or off");
			command.Target = tokens[1];
			command.Number = tokens[2] == "on" ? 1 : 0;
		}
		else if (verb == "page")
		{
			if (count != 2 || !FindArea(tokens[1]))
				return fail("page needs the key of an area (explorer, economy, build, ...)");
			command.Target = tokens[1];
		}
		else if (verb == "call" || verb == "method")
		{
			if (count < 2 || (verb == "method" && !GoodPath(tokens[1], true)))
				return fail(verb == "call" ? "call needs a script name" : "method needs the path of a method");
			command.Target = tokens[1];
			for (size_t i = 2; i < count; i++)
			{
				RemoteArg arg;
				if (!ParseArg(tokens[i], arg))
					return fail("cannot read the argument: " + tokens[i]);
				command.Args.push_back(arg);
			}
		}
		else
			return fail("unknown command: " + verb);
		return command;
	}
}
