#include "RemoteCommand.hpp"

#include "AskPath.hpp"
#include "CheatTable.hpp"
#include "DiplomacyPlan.hpp"
#include "EconomyPlan.hpp"
#include "PeoplePlan.hpp"
#include "Presets.hpp"
#include "WorldPlan.hpp"
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
			// override <스크립트 이름|메서드의 주소> x:<배율> [whole]      원래 함수가 돌려준 수에 곱한다. whole: 정수는 정수로 남긴다
			RemoteArg value;
			if (count >= 3 && tokens[2].rfind("x:", 0) == 0)
			{
				value.Kind = 'x';
				if (count > 4 || !ParseNumber(tokens[2].substr(2), value.Number) || !std::isfinite(value.Number) || !(value.Number > 0))
					return fail("override x: needs a finite factor above 0");
				if (count == 4 && tokens[3] != "whole")
					return fail("override x: takes only 'whole' after the factor");
				command.Target = tokens[1];
				command.Args.push_back(value);
				if (count == 4)
					command.Options["whole"] = "1";
				return command;
			}
			if (count < 3 || count > 4 || !ParseArg(tokens[2], value) || (value.Kind != 'n' && value.Kind != 'b' && value.Kind != 'u')
				|| (value.Kind == 'n' && !std::isfinite(value.Number)))
				return fail("override needs a target and a value (n:<finite number>, b:0|1, u or x:<factor>)");
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
				return fail("economy needs gold_add, gold_set, add, set, all, floor or gold_floor");
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
		else if (verb == "person")
		{
			// person list [all=1]                                       플레이어의 영주들(all 이면 주민과 손님까지)
			// person show <uuid>                                        그 사람의 값들
			// person <uuid|lords|people> <할 일> [index=N] [amount=N] [name=글]
			if (count >= 2 && tokens[1] == "list")
			{
				command.Target = "list";
				options(2);
				return command;
			}
			if (count >= 2 && tokens[1] == "show")
			{
				if (count != 3 || !GoodWho(tokens[2]))
					return fail("person show needs one uuid");
				command.Target = "show";
				command.Options["who"] = tokens[2];
				return command;
			}
			if (count >= 2 && tokens[1] == "spawn")
			{
				// person spawn <soldier|knight|peasant|slave|lord>: 게임의 디버그 소환기로 플레이어의 사람 하나를 마우스 자리에 만든다(research/13)
				SpawnKind kind = SpawnKind::Soldier;
				if (count != 3 || !ParseSpawnKind(tokens[2], kind))
					return fail("person spawn needs soldier, knight, peasant, slave or lord");
				command.Target = "spawn";
				command.Options["kind"] = tokens[2];
				return command;
			}
			if (count >= 2 && tokens[1] == "spawn_soldier")
			{
				// person spawn_soldier amount=<1..20>: 플레이어의 병사를 그만큼 만든다(게임의 디버그 함수. research/13)
				int soldiers = 0;
				if (count != 3 || !options(2))
					return fail("person spawn_soldier needs amount=<1..20>");
				const auto wanted = command.Options.find("amount");
				if (wanted == command.Options.end() || !ParseNumber(wanted->second, command.Number) || !SoldierBatch(command.Number, soldiers))
					return fail("person spawn_soldier needs amount=<1..20>");
				command.Target = "spawn_soldier";
				return command;
			}
			PersonCommand person;
			if (count < 3 || !GoodWho(tokens[1]) || !ParsePersonAct(tokens[2], person.Act))
				return fail("person needs who (a uuid, lords or people) and what to do (skill_set, skill_add, skills_max, need_set, needs_fill, age_set, happy, cure, "
					"trait_add, trait_remove, knowledge_all, knowledge_add, money_add, item_add)");
			command.Target = tokens[1];
			if (!options(3))
				return command;
			command.Options["act"] = tokens[2];
			person.Who = tokens[1];
			double index = -1;
			const auto given = command.Options.find("index");
			// 정수로 바꾸기 전에 범위를 본다(inf 나 1e300 을 int 로 바꾸는 것은 정의되지 않은 동작이다).
			if (given != command.Options.end() && (!ParseNumber(given->second, index) || !std::isfinite(index) || index != std::floor(index) || index < 0 || index >= 1000))
				return fail("person index= needs a whole number");
			person.Index = static_cast<int>(index);
			const auto amount = command.Options.find("amount");
			if (NeedsAmount(person.Act) && (amount == command.Options.end() || !ParseNumber(amount->second, command.Number)))
				return fail(std::string("person ") + tokens[2] + " needs amount=<number>");
			person.Amount = command.Number;
			const auto name = command.Options.find("name");
			if (name != command.Options.end())
				person.Text = name->second;
			if (NeedsIndex(person.Act) && (person.Index < 0 || person.Index >= IndexLimit(person.Act)))
				return fail(std::string("person ") + tokens[2] + " needs index=<0.." + std::to_string(IndexLimit(person.Act) - 1) + ">");
			if (NeedsText(person.Act) && !GoodTraitName(person.Text))
				return fail(std::string("person ") + tokens[2] + " needs name=<name>");
			const bool trait = person.Act == PersonAct::TraitAdd || person.Act == PersonAct::TraitRemove;
			if (trait && IsProtectedTrait(person.Text))
				return fail(std::string("person ") + tokens[2] + " does not take that trait (species and death states are protected)");
			if (!BulkAllowed(person.Who, person.Act))
				return fail(std::string("person ") + tokens[1] + " takes only skills_max, needs_fill, happy or cure, lords also knowledge_all, people also equip (name one person for the rest)");
			if (person.Act == PersonAct::Equip && !FindLoadout(person.Text))
				return fail("person equip needs name=<h_swordman|h_axeman|h_spearman|h_hammerhead|any>");
			std::string why;
			if (!CheckPersonCommand(person, why))
				return fail(std::string("person ") + tokens[2] + " cannot be read");
		}
		else if (verb == "preset")
		{
			// preset <normal|easy|sandbox|god>      치트 표의 확인된 항목의 묶음을 건다(core/Presets)
			if (count != 2 || !FindPreset(tokens[1]))
				return fail("preset needs normal, easy, sandbox or god");
			command.Target = tokens[1];
		}
		else if (verb == "time")
		{
			// time <pause|resume>                   게임의 시간을 멈춘다, 다시 흐르게 한다
			if (count != 2 || (tokens[1] != "pause" && tokens[1] != "resume"))
				return fail("time needs pause or resume");
			command.Target = tokens[1];
		}
		else if (verb == "world")
		{
			// world <cooldowns_clear|bishop>      한 번 하는 일: 이벤트 쿨다운 지우기, 주교 부르기(core/WorldPlan)
			WorldAct act = WorldAct::CooldownsClear;
			if (count != 2 || !ParseWorldAct(tokens[1], act))
				return fail("world needs cooldowns_clear or bishop");
			command.Target = tokens[1];
		}
		else if (verb == "diplomacy")
		{
			// diplomacy list                                                  왕국들과 지금의 관계
			// diplomacy <uuid|all> <friends|neutral|hostile> [side=them|us|both]  바라는 관계가 될 때까지 평판을 붙인다(core/DiplomacyPlan)
			// diplomacy <uuid> opinion amount=<수> [side=them|us|both]         평판을 그만큼 움직인다(5 의 배수로)
			if (count >= 2 && tokens[1] == "list")
			{
				if (count != 2)
					return fail("diplomacy list takes nothing");
				command.Target = "list";
				return command;
			}
			DiplomacyCommand diplomacy;
			if (count < 3 || !GoodFactionWho(tokens[1]) || !ParseDiplomacyGoal(tokens[2], diplomacy.Goal))
				return fail("diplomacy needs list, or who (a faction uuid or all) and friends, neutral, hostile, opinion or pact");
			command.Target = tokens[1];
			if (!options(3))
				return command;
			command.Options["goal"] = tokens[2];
			diplomacy.Who = tokens[1];
			const auto side = command.Options.find("side");
			if (side != command.Options.end() && !ParseDiplomacySide(side->second, diplomacy.Side))
				return fail("diplomacy side= needs them, us or both");
			const auto amount = command.Options.find("amount");
			if (diplomacy.Goal == DiplomacyGoal::Pact)
			{
				// diplomacy <uuid> pact name=<peace|trade|defence>      협정을 맺는다(양쪽에 쓰인다. side 를 받지 않는다)
				const auto name = command.Options.find("name");
				if (name == command.Options.end() || !ParseDiplomacyPact(name->second, diplomacy.Pact))
					return fail("diplomacy pact needs name=<peace|trade|defence>");
				if (side != command.Options.end() || amount != command.Options.end())
					return fail("diplomacy pact takes only name=");
				if (diplomacy.Who == "all")
					return fail("diplomacy pact needs one kingdom (a faction uuid)");
			}
			else if (diplomacy.Goal == DiplomacyGoal::Opinion)
			{
				if (amount == command.Options.end() || !ParseNumber(amount->second, command.Number) || OpinionSteps(command.Number) == 0)
					return fail("diplomacy opinion needs amount=<how many opinions to attach: a whole number from -40 to 40, not 0>");
				diplomacy.Amount = command.Number;
			}
			else if (amount != command.Options.end())
				return fail(std::string("diplomacy ") + tokens[2] + " takes no amount");
			std::string why;
			if (!CheckDiplomacy(diplomacy, why))
				return fail("diplomacy all takes only friends or neutral (name one kingdom for hostile and opinion)");
		}
		else if (verb == "cheat")
		{
			// cheat <치트의 Id> on|off          수가 있는 항목을 on 으로 켜면 표가 내놓는 수로 켠다
			// cheat <치트의 Id> <수>            수가 있는 항목(값, 배율)을 그 수로 켠다
			const Cheat* cheat = count == 3 ? FindCheat(tokens[1]) : nullptr;
			if (!cheat)
				return fail("cheat needs the id of a cheat and on, off or a number");
			command.Target = tokens[1];
			if (tokens[2] == "on" || tokens[2] == "off")
				command.Number = tokens[2] == "on" ? 1 : 0;
			else
			{
				RemoteArg value;
				value.Kind = 'n';
				if (!HasNumber(cheat->Kind) || !ParseNumber(tokens[2], value.Number) || !std::isfinite(value.Number))
					return fail(HasNumber(cheat->Kind) ? "cheat needs on, off or a finite number" : "cheat " + tokens[1] + " takes only on or off");
				command.Number = 1;
				command.Args.push_back(value);
			}
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
