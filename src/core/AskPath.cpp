#include "AskPath.hpp"

#include "Text.hpp"

#include <cmath>
#include <utility>

namespace NlCore
{
	namespace
	{
		constexpr const char* k_Marks = ".[@#";

		// 0 이상의 정수만 받는다.
		bool ParseIndex(const std::string& Text, double& Out)
		{
			double value = 0;
			if (!ParseNumber(Text, value) || value < 0 || value != std::floor(value))
				return false;
			Out = value;
			return true;
		}

		AskPath Fail(AskPath Path, const std::string& Why)
		{
			Path.Error = Why;
			return Path;
		}
	}

	bool GoodPath(const std::string& Text, bool NeedSteps)
	{
		const AskPath path = ParseAskPath(Text);
		return path.Error.empty() && (!NeedSteps || !path.Steps.empty());
	}

	AskPath ParseAskPath(const std::string& Text)
	{
		AskPath path;
		const size_t root_end = Text.find_first_of(k_Marks);
		const std::string root = Text.substr(0, root_end);

		if (root == "global")
			path.Root = "global";
		else if (root.rfind("inst:", 0) == 0)
		{
			path.Root = "inst";
			const size_t colon = root.find(':', 5);
			path.Object = root.substr(5, colon == std::string::npos ? std::string::npos : colon - 5);
			if (path.Object.empty())
				return Fail(path, "inst needs an object name");
			if (colon != std::string::npos && !ParseIndex(root.substr(colon + 1), path.Number))
				return Fail(path, "inst:<object>:<n> needs a whole number");
		}
		else if (root.rfind("map:", 0) == 0 || root.rfind("list:", 0) == 0)
		{
			const size_t colon = root.find(':');
			path.Root = root.substr(0, colon);
			if (!ParseIndex(root.substr(colon + 1), path.Number))
				return Fail(path, path.Root + ":<id> needs a whole number");
		}
		else
			return Fail(path, "path must start with global, inst:<object>, map:<id> or list:<id>");

		size_t at = root_end;
		while (at != std::string::npos && at < Text.size())
		{
			PathStep step;
			step.Kind = Text[at];
			at++;

			if (step.Kind == '[')
			{
				const size_t close = Text.find(']', at);
				if (close == std::string::npos || !ParseIndex(Text.substr(at, close - at), step.Index))
					return Fail(path, "[n] needs a whole number and a closing bracket");
				at = close + 1;
			}
			else if (step.Kind == '@' && at < Text.size() && Text[at] == '{')
			{
				const size_t close = Text.find('}', at);
				if (close == std::string::npos)
					return Fail(path, "@{key} needs a closing brace");
				step.Name = Text.substr(at + 1, close - at - 1);
				at = close + 1;
			}
			else
			{
				const size_t end = Text.find_first_of(k_Marks, at);
				const std::string word = Text.substr(at, end == std::string::npos ? std::string::npos : end - at);
				if (word.empty())
					return Fail(path, std::string("empty name after '") + step.Kind + "'");
				if (step.Kind == '#')
				{
					if (!ParseIndex(word, step.Index))
						return Fail(path, "#n needs a whole number");
				}
				else
					step.Name = word;
				at = end;
			}

			if (at != std::string::npos && at < Text.size() && std::string(k_Marks).find(Text[at]) == std::string::npos)
				return Fail(path, "unexpected text after a step");
			path.Steps.push_back(std::move(step));
		}
		return path;
	}

	std::string FormatStep(const PathStep& Step)
	{
		switch (Step.Kind)
		{
		case '[': return "[" + Number(Step.Index) + "]";
		case '#': return "#" + Number(Step.Index);
		case '@':
			if (Step.Name.empty() || Step.Name.find_first_of(".[@#{}") != std::string::npos)
				return "@{" + Step.Name + "}";
			return "@" + Step.Name;
		default: return "." + Step.Name;
		}
	}

	std::string FormatAskPath(const AskPath& Path)
	{
		if (!Path.Error.empty() || Path.Root.empty())
			return "";

		std::string text = Path.Root;
		if (Path.Root == "inst")
			text += ":" + Path.Object + (Path.Number > 0 ? ":" + Number(Path.Number) : "");
		else if (Path.Root != "global")
			text += ":" + Number(Path.Number);
		for (const PathStep& step : Path.Steps)
			text += FormatStep(step);
		return text;
	}

	AskPath ParentPath(AskPath Path)
	{
		if (!Path.Steps.empty())
			Path.Steps.pop_back();
		return Path;
	}

	AskPath ChildPath(AskPath Path, PathStep Step)
	{
		Path.Steps.push_back(std::move(Step));
		return Path;
	}
}
