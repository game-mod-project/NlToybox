#include "RemoteCommand.hpp"

#include "AskPath.hpp"
#include "CheatTable.hpp"
#include "CourtPlan.hpp"
#include "DiplomacyPlan.hpp"
#include "EconomyPlan.hpp"
#include "PeoplePlan.hpp"
#include "FamilyPlan.hpp"
#include "RolePlan.hpp"
#include "Presets.hpp"
#include "CrimePlan.hpp"
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

	namespace
	{
		// 동사마다의 읽기(2026-10-07 리뷰 R5 로 나눴다). 읽지 못하면 command.Error 를 적는다. 쓰는 꼴: return Fail(command, "…");
		void Fail(RemoteCommand& command, const std::string& why)
		{
			command.Error = why;
		}

		// tokens[From] 부터를 key=value 로 읽는다. 아니면 오류를 적고 거짓.
		bool Options(const std::vector<std::string>& tokens, size_t From, RemoteCommand& command)
		{
			for (size_t i = From; i < tokens.size(); i++)
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
		}

		void ParseStateLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			if (count != 1)
				return Fail(command, "state takes nothing");
		}

		void ParseQueryLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			const std::string& verb = command.Verb;
			if (count < 2 || !GoodPath(tokens[1], false))
				return Fail(command, verb + " needs a path");
			if ((verb == "ask" || verb == "about") && count != 2)
				return Fail(command, verb + " takes one path");
			command.Target = tokens[1];
			Options(tokens, 2, command);
		}

		void ParseFindLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			if (!Options(tokens, 1, command))
				return;
			double value = 0;
			const auto given = command.Options.find("value");
			if (given != command.Options.end() && !ParseNumber(given->second, value))
				return Fail(command, "find value= needs a number");
			if (!command.Options.count("name") && given == command.Options.end())
				return Fail(command, "find needs name= or value=");
		}

		void ParseRefineLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			if (!Options(tokens, 1, command))
				return;
			const auto given = command.Options.find("value");
			if (given == command.Options.end() || !ParseNumber(given->second, command.Number))
				return Fail(command, "refine needs value=<number>");
		}

		void ParseWriteLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			const std::string& verb = command.Verb;
			// write <path>=<수> 는 수를, write <path>=s:<글> 은 글(낱말 하나. 2026-10-07: 이벤트 감독의 __debug_forced_event 가 글을 받는 것으로 보인다)을 쓴다.
			const size_t eq = count == 2 ? tokens[1].rfind('=') : std::string::npos;		// 주소의 중괄호 안에 '=' 가 있을 수 있다
			if (eq == std::string::npos || !GoodPath(tokens[1].substr(0, eq), true))
				return Fail(command, verb + " needs <path>=<number> or <path>=s:<text>");
			const std::string value = tokens[1].substr(eq + 1);
			if (value.rfind("s:", 0) == 0)
			{
				if (value.size() <= 2)
					return Fail(command, verb + " needs text after s:");
				command.Options["kind"] = "s";
				command.Options["text"] = value.substr(2);
			}
			else if (!ParseNumber(value, command.Number))
				return Fail(command, verb + " needs <path>=<number> or <path>=s:<text>");
			command.Target = tokens[1].substr(0, eq);
		}

		void ParseRecordLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			const std::string& verb = command.Verb;
			if (count != 2)
				return Fail(command, verb + " needs one name");
			command.Target = tokens[1];
		}

		void ParseRecordsLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			if (count > 2)
				return Fail(command, "records takes at most one name");
			if (count == 2)
				command.Target = tokens[1];
		}

		void ParseShotLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			if (count != 2 || !GoodName(tokens[1]))
				return Fail(command, "shot needs a name of letters, digits, '_' and '-'");
			command.Target = tokens[1];
		}

		void ParseWindowLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			if (count != 2 || (tokens[1] != "open" && tokens[1] != "close"))
				return Fail(command, "window needs open or close");
			command.Target = tokens[1];
		}

		void ParseUiLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			// ui click x=<수> y=<수> | ui type text=<글> | ui key name=<enter|tab|escape|backspace>
			// 모드창(Dear ImGui)의 입력 큐에 넣는다. 게임 창과 진짜 마우스·키보드는 건드리지 않는다(창의 입력 칸을 시험하려고 둔다).
			if (count < 2 || (tokens[1] != "click" && tokens[1] != "type" && tokens[1] != "key"))
				return Fail(command, "ui needs click, type or key");
			command.Target = tokens[1];
			if (!Options(tokens, 2, command))
				return;
			const auto only = [&](std::initializer_list<const char*> keys) {
				if (command.Options.size() != keys.size())
					return false;
				for (const char* key : keys)
					if (!command.Options.count(key))
						return false;
				return true;
			};
			if (command.Target == "click")
			{
				double x = 0, y = 0;
				if (!only({ "x", "y" }) || !ParseNumber(command.Options["x"], x) || !ParseNumber(command.Options["y"], y)
					|| !std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 || x > 10000 || y > 10000)
					return Fail(command, "ui click needs x=<0..10000> y=<0..10000>");
			}
			else if (command.Target == "type")
			{
				const std::string text = only({ "text" }) ? command.Options["text"] : std::string();
				bool good = !text.empty() && text.size() <= 32;
				for (const char c : text)
					good = good && ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == '_');
				if (!good)
					return Fail(command, "ui type needs text=<1..32 letters, digits, '.', '-', '_'>");
			}
			else
			{
				const std::string name = only({ "name" }) ? command.Options["name"] : std::string();
				if (name != "enter" && name != "tab" && name != "escape" && name != "backspace")
					return Fail(command, "ui key needs name=<enter|tab|escape|backspace>");
			}
		}

		void ParseTreeCallLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			// treecall <인자 없는 스크립트의 이름> [depth=N] [max=N]
			if (count < 2 || tokens[1].find('=') != std::string::npos)
				return Fail(command, "treecall needs a script name");
			command.Target = tokens[1];
			if (!Options(tokens, 2, command))
				return;
		}

		void ParseOverrideLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			// override <스크립트 이름|메서드의 주소> <n:수|b:0|1|u> [skip]
			// override <스크립트 이름|메서드의 주소> x:<배율> [whole]      원래 함수가 돌려준 수에 곱한다. whole: 정수는 정수로 남긴다
			RemoteArg value;
			if (count >= 3 && tokens[2].rfind("x:", 0) == 0)
			{
				value.Kind = 'x';
				if (count > 4 || !ParseNumber(tokens[2].substr(2), value.Number) || !std::isfinite(value.Number) || !(value.Number > 0))
					return Fail(command, "override x: needs a finite factor above 0");
				if (count == 4 && tokens[3] != "whole")
					return Fail(command, "override x: takes only 'whole' after the factor");
				command.Target = tokens[1];
				command.Args.push_back(value);
				if (count == 4)
					command.Options["whole"] = "1";
				return;
			}
			if (count < 3 || count > 4 || !ParseArg(tokens[2], value) || (value.Kind != 'n' && value.Kind != 'b' && value.Kind != 'u')
				|| (value.Kind == 'n' && !std::isfinite(value.Number)))
				return Fail(command, "override needs a target and a value (n:<finite number>, b:0|1, u or x:<factor>)");
			if (count == 4 && tokens[3] != "skip")
				return Fail(command, "override takes only 'skip' after the value");
			command.Target = tokens[1];
			command.Args.push_back(value);
			if (count == 4)
				command.Options["skip"] = "1";
		}

		void ParseUnoverrideLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			if (count != 2)
				return Fail(command, "unoverride needs one name or 'all'");
			command.Target = tokens[1];
		}

		void ParseEconomyLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			EconomyAct act = EconomyAct::GoldAdd;
			if (count < 2 || !ParseEconomyAct(tokens[1], act))
				return Fail(command, "economy needs gold_add, gold_set, add, set, all, floor or gold_floor");
			command.Target = tokens[1];
			if (!Options(tokens, 2, command))
				return;
			const auto amount = command.Options.find("amount");
			if (amount == command.Options.end() || !ParseNumber(amount->second, command.Number))
				return Fail(command, "economy needs amount=<number>");
			double resource = 0;
			const auto given = command.Options.find("resource");
			if (NeedsResource(act) && (given == command.Options.end() || !ParseNumber(given->second, resource)
				|| !(resource >= 0 && resource < 1000) || resource != std::floor(resource)))
				return Fail(command, "economy " + tokens[1] + " needs resource=<index>");
			if (!NeedsResource(act) && given != command.Options.end())
				return Fail(command, "economy " + tokens[1] + " takes no resource");
		}

		void ParsePersonLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			// person list [all=1]                                       플레이어의 영주들(all 이면 주민과 손님까지)
			// person show <uuid>                                        그 사람의 값들
			// person <uuid|lords|people> <할 일> [index=N] [amount=N] [name=글]
			if (count >= 2 && tokens[1] == "list")
			{
				command.Target = "list";
				Options(tokens, 2, command);
				return;
			}
			if (count >= 2 && tokens[1] == "show")
			{
				if (count != 3 || !GoodWho(tokens[2]))
					return Fail(command, "person show needs one uuid");
				command.Target = "show";
				command.Options["who"] = tokens[2];
				return;
			}
			if (count >= 2 && tokens[1] == "spawn")
			{
				// person spawn <soldier|knight|peasant|slave|lord>: 게임의 디버그 소환기로 플레이어의 사람 하나를 마우스 자리에 만든다(research/13)
				SpawnKind kind = SpawnKind::Soldier;
				if (count != 3 || !ParseSpawnKind(tokens[2], kind))
					return Fail(command, "person spawn needs soldier, knight, peasant, slave or lord");
				command.Target = "spawn";
				command.Options["kind"] = tokens[2];
				return;
			}
			if (count >= 2 && tokens[1] == "spawn_soldier")
			{
				// person spawn_soldier amount=<1..20>: 플레이어의 병사를 그만큼 만든다(게임의 디버그 함수. research/13)
				int soldiers = 0;
				if (count != 3 || !Options(tokens, 2, command))
					return Fail(command, "person spawn_soldier needs amount=<1..20>");
				const auto wanted = command.Options.find("amount");
				if (wanted == command.Options.end() || !ParseNumber(wanted->second, command.Number) || !SoldierBatch(command.Number, soldiers))
					return Fail(command, "person spawn_soldier needs amount=<1..20>");
				command.Target = "spawn_soldier";
				return;
			}
			PersonCommand person;
			if (count < 3 || !GoodWho(tokens[1]) || !ParsePersonAct(tokens[2], person.Act))
				return Fail(command, "person needs who (a uuid, lords or people) and what to do (skill_set, skill_add, skills_max, need_set, needs_fill, age_set, happy, cure, "
					"trait_add, trait_remove, knowledge_all, knowledge_add, money_add, item_add, equip, role, role_undo, pregnancy_next, birth, grow_up, conceive)");
			command.Target = tokens[1];
			if (!Options(tokens, 3, command))
				return;
			command.Options["act"] = tokens[2];
			person.Who = tokens[1];
			double index = -1;
			const auto given = command.Options.find("index");
			// 정수로 바꾸기 전에 범위를 본다(inf 나 1e300 을 int 로 바꾸는 것은 정의되지 않은 동작이다).
			if (given != command.Options.end() && (!ParseNumber(given->second, index) || !std::isfinite(index) || index != std::floor(index) || index < 0 || index >= 1000))
				return Fail(command, "person index= needs a whole number");
			person.Index = static_cast<int>(index);
			const auto amount = command.Options.find("amount");
			if (NeedsAmount(person.Act) && (amount == command.Options.end() || !ParseNumber(amount->second, command.Number)))
				return Fail(command, std::string("person ") + tokens[2] + " needs amount=<number>");
			person.Amount = command.Number;
			const auto name = command.Options.find("name");
			if (name != command.Options.end())
				person.Text = name->second;
			if (NeedsIndex(person.Act) && (person.Index < 0 || person.Index >= IndexLimit(person.Act)))
				return Fail(command, std::string("person ") + tokens[2] + " needs index=<0.." + std::to_string(IndexLimit(person.Act) - 1) + ">");
			if (NeedsText(person.Act) && !GoodTraitName(person.Text))
				return Fail(command, std::string("person ") + tokens[2] + " needs name=<name>");
			const bool trait = person.Act == PersonAct::TraitAdd || person.Act == PersonAct::TraitRemove;
			if (trait && IsProtectedTrait(person.Text))
				return Fail(command, std::string("person ") + tokens[2] + " does not take that trait (species and death states are protected)");
			if (!BulkAllowed(person.Who, person.Act))
				return Fail(command, std::string("person ") + tokens[1] + " takes only skills_max, needs_fill, happy or cure, lords also knowledge_all and birth, people also equip (name one person for the rest)");
			if (person.Act == PersonAct::Equip && !FindLoadout(person.Text))
				return Fail(command, "person equip needs name=<h_swordman|h_axeman|h_spearman|h_hammerhead|any>");
			if (person.Act == PersonAct::Conceive && (!IsUuid(person.Text) || person.Text == person.Who))
				return Fail(command, "person conceive needs name=<the father's uuid: 16 hex digits, not the same person>");
			if (person.Act == PersonAct::Role && !FindRole(person.Text))
			{
				std::string ids;
				for (const RolePreset& role : RolePresets())
					ids += std::string(ids.empty() ? "" : "|") + role.Id;
				return Fail(command, "person role needs name=<" + ids + ">");
			}
			std::string why;
			if (!CheckPersonCommand(person, why))
				return Fail(command, std::string("person ") + tokens[2] + " cannot be read");
		}

		void ParsePresetLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			// preset <normal|easy|sandbox|god>      치트 표의 확인된 항목의 묶음을 건다(core/Presets)
			if (count != 2 || !FindPreset(tokens[1]))
				return Fail(command, "preset needs normal, easy, sandbox or god");
			command.Target = tokens[1];
		}

		void ParseTimeLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			// time <pause|resume>                   게임의 시간을 멈춘다, 다시 흐르게 한다
			if (count != 2 || (tokens[1] != "pause" && tokens[1] != "resume"))
				return Fail(command, "time needs pause or resume");
			command.Target = tokens[1];
		}

		void ParseCrimeLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			// crime list | clear <uuid|all> | return_stolen <uuid|all> | absolve <uuid|lords> | acquit <uuid|lords>      범죄 패널의 단추와 같은 길(core/CrimePlan)
			CrimeCommand crime;
			std::string why;
			if (!ParseCrimeCommand(std::vector<std::string>(tokens.begin() + 1, tokens.end()), crime, why))
				return Fail(command, why);
			command.Target = CrimeActWord(crime.Act);
			if (!crime.Who.empty())
				command.Options["who"] = crime.Who;
		}

		void ParseWorldLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			// world <cooldowns_clear|bishop|season|season_delay|season_end>      한 번 하는 일(core/WorldPlan): 이벤트 쿨다운 지우기, 주교 부르기,
			// 계절 보기·가혹한 계절 하루 미루기·지금 단계 끝내기
			// world save 는 지금 저장, world event name=<이벤트의 시스템 이름> 은 그 이벤트를 감독의 강제 이벤트로 써 둔다(2026-10-07. research/28).
			WorldAct act = WorldAct::CooldownsClear;
			if (count < 2 || !ParseWorldAct(tokens[1], act))
				return Fail(command, "world needs one of: " + WorldActWords());
			command.Target = tokens[1];
			if (!Options(tokens, 2, command))
				return;
			const auto name = command.Options.find("name");
			if (WorldActNeedsName(act))
			{
				if (name == command.Options.end() || !GoodEventName(name->second))
					return Fail(command, "world event needs name=<the event's system name: letters, digits, underscores>");
			}
			else if (count != 2)
				return Fail(command, std::string("world ") + tokens[1] + " takes nothing");
		}

		void ParseDiplomacyLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			// diplomacy list                                                  왕국들과 지금의 관계
			// diplomacy <uuid|all> <friends|neutral|hostile> [side=them|us|both]  바라는 관계가 될 때까지 평판을 붙인다(core/DiplomacyPlan)
			// diplomacy <uuid> opinion amount=<개수> [side=them|us|both]       디버그 평판을 그 개수만큼 붙인다(-40 ~ 40)
			// diplomacy <uuid> pact name=<peace|trade|defence>                 그 왕국과 협정을 맺는다
			// diplomacy <uuid|all> clear [side=them|us|both]                   붙여 둔 디버그 평판을 뗀다(좋은 것부터, 그다음 나쁜 것)
			if (count >= 2 && tokens[1] == "list")
			{
				if (count != 2)
					return Fail(command, "diplomacy list takes nothing");
				command.Target = "list";
				return;
			}
			DiplomacyCommand diplomacy;
			if (count < 3 || !GoodFactionWho(tokens[1]) || !ParseDiplomacyGoal(tokens[2], diplomacy.Goal))
				return Fail(command, "diplomacy needs list, or who (a faction uuid or all) and friends, neutral, hostile, opinion, pact or clear");
			command.Target = tokens[1];
			if (!Options(tokens, 3, command))
				return;
			// 모르는 열쇠는 받지 않는다(sdie=them 이 조용히 양쪽을 움직이지 않게). queue 는 1 만(창의 단추처럼 쌓는다).
			for (const auto& [key, value] : command.Options)
				if (key != "side" && key != "amount" && key != "name" && key != "queue")
					return Fail(command, "diplomacy takes only side=, amount=, name= and queue=1: " + key);
			const auto queue = command.Options.find("queue");
			if (queue != command.Options.end() && queue->second != "1")
				return Fail(command, "diplomacy queue= takes only 1");
			command.Options["goal"] = tokens[2];
			diplomacy.Who = tokens[1];
			const auto side = command.Options.find("side");
			if (side != command.Options.end() && !ParseDiplomacySide(side->second, diplomacy.Side))
				return Fail(command, "diplomacy side= needs them, us or both");
			const auto amount = command.Options.find("amount");
			if (diplomacy.Goal == DiplomacyGoal::Pact)
			{
				// diplomacy <uuid> pact name=<peace|trade|defence>      협정을 맺는다(양쪽에 쓰인다. side 를 받지 않는다)
				const auto name = command.Options.find("name");
				if (name == command.Options.end() || !ParseDiplomacyPact(name->second, diplomacy.Pact))
					return Fail(command, "diplomacy pact needs name=<peace|trade|defence>");
				if (side != command.Options.end() || amount != command.Options.end())
					return Fail(command, "diplomacy pact takes only name=");
				if (diplomacy.Who == "all")
					return Fail(command, "diplomacy pact needs one kingdom (a faction uuid)");
			}
			else if (diplomacy.Goal == DiplomacyGoal::Opinion)
			{
				if (amount == command.Options.end() || !ParseNumber(amount->second, command.Number) || OpinionSteps(command.Number) == 0)
					return Fail(command, "diplomacy opinion needs amount=<how many opinions to attach: a whole number from -40 to 40, not 0>");
				diplomacy.Amount = command.Number;
			}
			else if (amount != command.Options.end())
				return Fail(command, std::string("diplomacy ") + tokens[2] + " takes no amount");
			else if (command.Options.count("name"))
				return Fail(command, std::string("diplomacy ") + tokens[2] + " takes no name= (only pact does)");
			std::string why;
			if (!CheckDiplomacy(diplomacy, why))
				return Fail(command, "diplomacy all takes only friends or neutral (name one kingdom for hostile and opinion)");
		}

		void ParseCourtLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			// court list                                                   영주들의 충성과 서로의 평판
			// court <uuid|lords> loyal [goal=<수>]                          왕을 보는 평판을 목표(기본 100)까지 올린다
			// court <uuid|lords> like about=<uuid|lords|king> [goal=<수>]   그 사람을 보는 평판을 목표까지 올린다
			// court <uuid|lords> opinion about=<…> amount=<개수>            디버그 평판을 그 개수만큼 움직인다(-40 ~ 40)
			// court <uuid|lords> clear about=<…>                            붙여 둔 디버그 평판을 모두 뗀다
			// court <uuid|lords> release                                    그 영주를 따르는 사람들의 충성 대상을 지운다
			// court bishop <like|opinion|clear> about=<uuid|lords|king> …   주교가 그 사람을 보는 평판을 같은 길로 움직인다(research/21)
			if (count >= 2 && tokens[1] == "list")
			{
				if (count != 2)
					return Fail(command, "court list takes nothing");
				command.Target = "list";
				return;
			}
			CourtCommand court;
			bool about_king = false;
			if (count < 3 || !GoodCourtWho(tokens[1]) || !ParseCourtGoal(tokens[2], court.Goal, about_king))
				return Fail(command, "court needs list, or who (lords, bishop or the uuid of a lord) and loyal, like, opinion, clear or release");
			command.Target = tokens[1];
			if (!Options(tokens, 3, command))
				return;
			// 모르는 열쇠는 받지 않는다. queue 는 1 만(창의 단추처럼 쌓는다).
			for (const auto& [key, value] : command.Options)
				if (key != "about" && key != "amount" && key != "goal" && key != "queue")
					return Fail(command, "court takes only about=, amount=, goal= and queue=1: " + key);
			const auto queue = command.Options.find("queue");
			if (queue != command.Options.end() && queue->second != "1")
				return Fail(command, "court queue= takes only 1");
			const auto about = command.Options.find("about");
			const auto amount = command.Options.find("amount");
			const auto goal = command.Options.find("goal");
			if (about_king && about != command.Options.end())
				return Fail(command, "court loyal takes no about= (it is about the king)");
			court.Who = tokens[1];
			court.About = about_king ? "king" : about != command.Options.end() ? about->second : std::string();
			court.OnlyLoyal = about_king;
			if (court.Goal == CourtGoal::Raise)
			{
				if (amount != command.Options.end())
					return Fail(command, std::string("court ") + tokens[2] + " takes goal=, not amount=");
				if (goal != command.Options.end() && (!ParseNumber(goal->second, command.Number) || command.Number == 0))
					return Fail(command, "court goal= needs a whole number from 1 to 200");
			}
			else
			{
				if (goal != command.Options.end())
					return Fail(command, std::string("court ") + tokens[2] + " takes no goal=");
				if (court.Goal == CourtGoal::Opinion)
				{
					if (amount == command.Options.end() || !ParseNumber(amount->second, command.Number))
						return Fail(command, "court opinion needs amount=<a whole number from -40 to 40, not 0>");
				}
				else if (amount != command.Options.end())
					return Fail(command, std::string("court ") + tokens[2] + " takes no amount=");
			}
			court.Amount = command.Number;
			std::string why;
			if (!CheckCourt(court, why))
				return Fail(command, "court: " + why);
			command.Options["act"] = tokens[2];
		}

		void ParseTraitsLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			// traits [find=<글>] [max=<수>]      게임의 특성들: 이름, 화면 이름, 설명의 앞부분(모듈이 게임의 현지화 파일에서 읽은 것). max 는 1 ~ 100000
			if (!Options(tokens, 1, command))
				return;
			for (const auto& [key, value] : command.Options)
				if (key != "find" && key != "max")
					return Fail(command, "traits takes only find= and max=: " + key);
			const auto max = command.Options.find("max");
			double limit = 0;
			// 범위부터 본다(유한하지 않은 수와 큰 수를 정수로 바꾸지 않는다).
			if (max != command.Options.end() && (!ParseNumber(max->second, limit) || !(limit >= 1 && limit <= 100000) || limit != std::floor(limit)))
				return Fail(command, "traits max= needs a whole number from 1 to 100000");
		}

		void ParseCheatLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			// cheat <치트의 Id> on|off          수가 있는 항목을 on 으로 켜면 표가 내놓는 수로 켠다
			// cheat <치트의 Id> <수>            수가 있는 항목(값, 배율)을 그 수로 켠다
			const Cheat* cheat = count == 3 ? FindCheat(tokens[1]) : nullptr;
			if (!cheat)
				return Fail(command, "cheat needs the id of a cheat and on, off or a number");
			command.Target = tokens[1];
			if (tokens[2] == "on" || tokens[2] == "off")
				command.Number = tokens[2] == "on" ? 1 : 0;
			else
			{
				RemoteArg value;
				value.Kind = 'n';
				if (!HasNumber(cheat->Kind) || !ParseNumber(tokens[2], value.Number) || !std::isfinite(value.Number))
					return Fail(command, HasNumber(cheat->Kind) ? "cheat needs on, off or a finite number" : "cheat " + tokens[1] + " takes only on or off");
				command.Number = 1;
				command.Args.push_back(value);
			}
		}

		void ParsePageLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			if (count != 2 || !FindArea(tokens[1]))
				return Fail(command, "page needs the key of an area (explorer, economy, build, ...)");
			command.Target = tokens[1];
		}

		void ParseCallLine(const std::vector<std::string>& tokens, RemoteCommand& command)
		{
			const size_t count = tokens.size();
			const std::string& verb = command.Verb;
			if (count < 2 || (verb == "method" && !GoodPath(tokens[1], true)))
				return Fail(command, verb == "call" ? "call needs a script name" : "method needs the path of a method");
			command.Target = tokens[1];
			for (size_t i = 2; i < count; i++)
			{
				RemoteArg arg;
				if (!ParseArg(tokens[i], arg))
					return Fail(command, "cannot read the argument: " + tokens[i]);
				command.Args.push_back(arg);
			}
		}

	}

	// 한 줄을 읽는다: 동사를 보고 그 동사의 읽기(위의 Parse…Line)에 넘긴다.
	RemoteCommand ParseRemoteLine(const std::string& Line)
	{
		RemoteCommand command;
		const std::string line = Trim(Line);
		if (line.empty() || line[0] == '#')
			return command;

		const std::vector<std::string> tokens = Split(line);
		command.Verb = tokens[0];
		const std::string& verb = command.Verb;

		if (verb == "state")
			ParseStateLine(tokens, command);
		else if (verb == "ask" || verb == "about" || verb == "list" || verb == "tree" || verb == "statics")
			ParseQueryLine(tokens, command);
		else if (verb == "find")
			ParseFindLine(tokens, command);
		else if (verb == "refine")
			ParseRefineLine(tokens, command);
		else if (verb == "write" || verb == "poke")
			ParseWriteLine(tokens, command);
		else if (verb == "record" || verb == "unrecord")
			ParseRecordLine(tokens, command);
		else if (verb == "records")
			ParseRecordsLine(tokens, command);
		else if (verb == "shot")
			ParseShotLine(tokens, command);
		else if (verb == "window")
			ParseWindowLine(tokens, command);
		else if (verb == "ui")
			ParseUiLine(tokens, command);
		else if (verb == "treecall")
			ParseTreeCallLine(tokens, command);
		else if (verb == "override")
			ParseOverrideLine(tokens, command);
		else if (verb == "unoverride")
			ParseUnoverrideLine(tokens, command);
		else if (verb == "economy")
			ParseEconomyLine(tokens, command);
		else if (verb == "person")
			ParsePersonLine(tokens, command);
		else if (verb == "preset")
			ParsePresetLine(tokens, command);
		else if (verb == "time")
			ParseTimeLine(tokens, command);
		else if (verb == "crime")
			ParseCrimeLine(tokens, command);
		else if (verb == "world")
			ParseWorldLine(tokens, command);
		else if (verb == "diplomacy")
			ParseDiplomacyLine(tokens, command);
		else if (verb == "court")
			ParseCourtLine(tokens, command);
		else if (verb == "traits")
			ParseTraitsLine(tokens, command);
		else if (verb == "cheat")
			ParseCheatLine(tokens, command);
		else if (verb == "page")
			ParsePageLine(tokens, command);
		else if (verb == "call" || verb == "method")
			ParseCallLine(tokens, command);
		else
			command.Error = "unknown command: " + verb;
		return command;
	}
}
