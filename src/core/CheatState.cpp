#include "CheatState.hpp"

#include "AskPath.hpp"
#include "EconomyPlan.hpp"
#include "Text.hpp"

#include <algorithm>
#include <cmath>

namespace NlCore
{
	namespace
	{
		// "이름=수" 를 마지막 '=' 에서 가른다(주소의 중괄호 안에 '=' 가 있을 수 있다).
		bool SplitNumber(const std::string& Text, std::string& Name, double& Value)
		{
			const size_t eq = Text.rfind('=');
			if (eq == std::string::npos)
				return false;
			Name = Trim(Text.substr(0, eq));
			return !Name.empty() && ParseNumber(Trim(Text.substr(eq + 1)), Value) && std::isfinite(Value);
		}
	}

	CheatState ParseCheatState(std::istream& In)
	{
		CheatState state;
		std::string line;
		while (std::getline(In, line))
		{
			line = Trim(line);
			const size_t space = line.find(' ');
			if (line.empty() || line[0] == '#' || space == std::string::npos)
				continue;

			const std::string word = line.substr(0, space);
			const std::string rest = Trim(line.substr(space + 1));
			std::string name;
			double value = 0;
			if (word == "on")
			{
				if (!rest.empty() && rest.find_first_of(" =") == std::string::npos)
					state.On.insert(rest);
			}
			else if (word == "num")
			{
				if (SplitNumber(rest, name, value) && name.find(' ') == std::string::npos)
					state.Numbers[name] = value;
			}
			else if (word == "pin")
			{
				if (GoodPath(rest) && std::find(state.Pins.begin(), state.Pins.end(), rest) == state.Pins.end())
					state.Pins.push_back(rest);
			}
			else if (word == "lock" && SplitNumber(rest, name, value) && GoodPath(name))
			{
				const auto it = std::find_if(state.Locks.begin(), state.Locks.end(), [&](const LockLine& lock) { return lock.Path == name; });
				if (it == state.Locks.end())
					state.Locks.push_back({ name, value });
				else
					it->Value = value;
			}
			else if (word == "floor" && SplitNumber(rest, name, value) && GoodFloorKey(name) && FloorValue(value) > 0)
				state.Floors[name] = FloorValue(value);
		}
		return state;
	}

	std::string FormatCheatState(const CheatState& State)
	{
		std::string text = "# NlToyBox 의 치트 상태. 모드창(F8)에서 바꾸면 여기에 저장된다.\n";
		for (const std::string& id : State.On)
			text += "on " + id + "\n";
		for (const auto& [id, value] : State.Numbers)
			text += "num " + id + "=" + Shortest(value) + "\n";
		for (const std::string& path : State.Pins)
			text += "pin " + path + "\n";
		for (const LockLine& lock : State.Locks)
			text += "lock " + lock.Path + "=" + Shortest(lock.Value) + "\n";
		for (const auto& [key, value] : State.Floors)
			if (GoodFloorKey(key) && FloorValue(value) > 0)
				text += "floor " + key + "=" + Shortest(FloorValue(value)) + "\n";
		return text;
	}
}
